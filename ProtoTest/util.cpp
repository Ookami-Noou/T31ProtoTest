#include "util.h"

int64_t string_to_long(const char* str, int len, bool* success) {
    int64_t value = 0;
    int64_t sign = 1; // Default sign is positive
    int i = 0;
    if (str == NULL || len <= 0) {
        *success = false;
        printf("Invalid string or length: str=%p, len=%d\n", str, len);
        return 0;
    }
    if (*str == '-') {
        sign = -1;
        i++;
    }
    else if (*str == '+') {
        sign = 1;
        i++;
    }
    for (; i < len; i++) {
        if (str[i] == ' ' || str[i] == ',' || str[i] == '_') continue;
        if (str[i] < '0' || str[i] > '9') {
            *success = false;
            printf("Invalid character '%c' in string '%.*s'\n", str[i], len, str);
            return value * sign; // shut down
        }
        value = (value * 10) + (str[i] - '0');
    }
    *success = true;
    return value * sign;
}

void format_current_time(char* buffer) {
    time_t rawtime;

    time(&rawtime);
    struct tm* timeinfo = localtime(&rawtime);

    sprintf(buffer, "%02d:%02d:%02d",
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec);
}