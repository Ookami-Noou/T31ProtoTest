#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NULL
#define NULL_IN_DEFINED
#define NULL ((void*)0)
#endif // !NULL

bool is_blank(const char ch) {
    return (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
}

bool is_json_string(const char* str, int len) {
    if (str == NULL) return 0;
    if (len == 0) return 0;

    // skipping trailing whitespace in begin
    size_t start = 0;
    while (start < len && is_blank((unsigned char)str[start])) {
        start++;
    }
    if (start >= len) return 0;

    // skipping trailing whitespace in the end
    size_t end = len - 1;
    while (end > start && is_blank((unsigned char)str[end])) {
        end--;
    }

    // check is { or [ the first char
    char first_char = str[start];
    char last_char = str[end];
    if (!(first_char == '{' && last_char == '}') &&
        !(first_char == '[' && last_char == ']')) {
        return 0;
    }

    // used for checking brackets
    char stack[100];
    int top = -1;
    bool in_string = 0;
    int escaped = 0;

    stack[++top] = first_char;

    for (size_t i = start + 1; i < end; i++) {
        char ch = str[i];

        if (escaped) {
            escaped = 0;
            continue;
        }

        if (ch == '\\') {
            if (in_string) {
                escaped = 1;
                continue;
            }
            else {
                // \ is illegal outside of string
                return 0;
            }
        }

        if (ch == '"') {
            in_string = !in_string;
        }
        else if (!in_string) {
            if (ch == '{' || ch == '[') {
                if (top >= 999) return 0; // stack overflow
                stack[++top] = ch;
            }
            else if (ch == '}' || ch == ']') {
                if (top < 0) return 0;
                char open = stack[top--];
                if ((ch == '}' && open != '{') ||
                    (ch == ']' && open != '[')) {
                    return 0;
                }
                if (top < 0 && i < end - 1) return 0;
            }
        }
    }

    if (top != 0 || in_string || escaped) {
        return 0;
    }

    // check the last char
    char open = stack[top];
    if ((last_char == '}' && open != '{') ||
        (last_char == ']' && open != '[')) {
        return 0;
    }

    return 1;
}

typedef struct {
    const char* type;         // type of command
    const char* client_id;    // client id
    const char* message_id;   // message id for this communication. you should replay message with this
    const char* msg;          // extra message of command. can be null when sent from server
    const char* cmd;          // a JSON string which contains properties of the command
} command_t;

static const char* skip_whitespace(const char* p) {
    while (*p && is_blank((unsigned char)*p)) p++;
    return p;
}

static const char* find_key(const char* json, const char* key) {
    size_t key_len = strlen(key);
    const char* p = json;
    while (*p) {
        p = strchr(p, '"');
        if (!p) break;
        p++;
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '"') {
            p += key_len + 1;
            p = skip_whitespace(p);
            if (*p == ':') {
                p++;
                return skip_whitespace(p);
            }
        }
        p++;
    }
    return NULL;
}

static char* parse_json_string(const char* input, const char** end) {
    const char* p = input + 1;
    char* buffer = (char*) malloc(strlen(p) + 1);
    if (!buffer) return NULL;
    char* q = buffer;

    while (*p && *p != '"') {
        if (*p == '\\') {
            p++;
            switch (*p) {
            case '"': *q++ = '"'; p++; break;
            case '\\': *q++ = '\\'; p++; break;
            case '/': *q++ = '/'; p++; break;
            case 'b': *q++ = '\b'; p++; break;
            case 'f': *q++ = '\f'; p++; break;
            case 'n': *q++ = '\n'; p++; break;
            case 'r': *q++ = '\r'; p++; break;
            case 't': *q++ = '\t'; p++; break;
            case 'u':
                *q++ = '\\';
                *q++ = 'u';
                p++;
                for (int i = 0; i < 4 && *p; i++) *q++ = *p++;
                break;
            default: *q++ = *p++;
            }
        }
        else {
            *q++ = *p++;
        }
    }
    *q = '\0';
    if (*p == '"') p++;
    *end = p;
    return buffer;
}

static char* parse_other_value(const char* input, const char** end) {
    const char* start = input;
    const char* p = input;
    int in_string = 0;
    int brace_count = 0;
    int bracket_count = 0;

    while (*p) {
        if (!in_string) {
            if (*p == ',' && brace_count == 0 && bracket_count == 0) break;
            if (*p == '}' || *p == ']') {
                if (brace_count > 0 && *p == '}') brace_count--;
                if (bracket_count > 0 && *p == ']') bracket_count--;
                if (brace_count == 0 && bracket_count == 0) {
                    p++;
                    break;
                }
            }
            if (*p == '{') brace_count++;
            if (*p == '[') bracket_count++;
            if (*p == '"') in_string = 1;
        }
        else {
            if (*p == '"' && (p == start || *(p - 1) != '\\')) in_string = 0;
        }
        p++;
    }

    size_t len = p - start;
    char* value = (char*) malloc(len + 1);
    if (!value) return NULL;
    strncpy_s(value, len + 1, start, len);
    value[len] = '\0';
    *end = p;
    return value;
}

static char* parse_value(const char* input, const char** end) {
    const char* p = skip_whitespace(input);
    char* result = NULL;

    if (*p == '"') {
        result = parse_json_string(p, end);
    }
    else if (strncmp(p, "null", 4) == 0) {
        *end = p + 4;
        result = NULL;
    }
    else {
        result = parse_other_value(p, end);
    }
    return result;
}

command_t* parse_command(const char* json) {
    command_t* cmd = (command_t*)calloc(1, sizeof(command_t));
    if (!cmd) return NULL;

    const char* pos = find_key(json, "type");
    if (pos) cmd->type = parse_value(pos, &pos);

    pos = find_key(json, "clientId");
    if (pos) cmd->client_id = parse_value(pos, &pos);

    pos = find_key(json, "messageId");
    if (pos) cmd->message_id = parse_value(pos, &pos);

    pos = find_key(json, "msg");
    if (pos) cmd->msg = parse_value(pos, &pos);

    pos = find_key(json, "cmd");
    if (pos) cmd->cmd = parse_value(pos, &pos);

    return cmd;
}

void free_command(command_t* cmd) {
    if (!cmd) return;
    free((void*)cmd->type);
    free((void*)cmd->client_id);
    free((void*)cmd->message_id);
    free((void*)cmd->msg);
    free((void*)cmd->cmd);
    free(cmd);
}

#ifdef NULL_IN_DEFINED
#undef NULL_IN_DEFINED
#undef NULL
#endif

#endif // !JSON_UTIL_H