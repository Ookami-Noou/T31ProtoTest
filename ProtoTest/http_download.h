#ifndef PROTO_HTTP_DOWNLOAD_H
#define PROTO_HTTP_DOWNLOAD_H

#include "mongoose.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"

// Configuration defaults
static int64_t max_size_per_piece = 1 * 1024 * 1024; // 1MB per chunk
static uint64_t s_timeout_ms = 10000;                // 10s connection timeout
static uint64_t s_transfer_timeout_ms = 30000;       // 30s transfer timeout
static const char* url = NULL;
static const char* download_path = NULL;

// Download state
static FILE* download_file_p = NULL;
static char* download_buffer = NULL;
static int64_t download_buffer_len = 0;
static int64_t download_offset = 0;
static int64_t download_all_size = 0;
static int retry_count = 0;
static const int max_retries = 3;

// Status codes
#define STETE_RUNNING 0
#define STATE_SUCCESS 1
#define STATE_ERROR   2

// Forward declarations
static void http_download_callback_fn(struct mg_connection* c, int ev, void* ev_data);
static void parse_content_range_value(const char* str, int len, int64_t* start_pos, int64_t* end_pos, int64_t* all_size, bool* success);
static void write_to_file();
static void print_http_download_usage();
static void cleanup_resources();

/** Main entry point */
int http_download_main(int argc, char* argv[]) {
    int ret = 0;

    // Parse command line arguments
    for (int i = 1; i < argc - 1; i++) {
        if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]) || !strcmp("-?", argv[i])) {
            print_http_download_usage();
            return 0;
        }
        if (!strcmp("-u", argv[i])) {
            url = argv[++i];
        }
        else if (!strcmp("-p", argv[i])) {
            download_path = argv[++i];
        }
        else if (!strcmp("-t", argv[i])) {
            bool success = false;
            s_timeout_ms = string_to_long(argv[++i], strlen(argv[i]), &success);
            if (!success || s_timeout_ms <= 0) {
                printf("Invalid timeout value: must be positive integer\n");
                print_http_download_usage();
                return 1;
            }
        }
        else if (!strcmp("-s", argv[i])) {
            bool success = false;
            max_size_per_piece = string_to_long(argv[++i], strlen(argv[i]), &success);
            if (!success || max_size_per_piece <= 0) {
                printf("Invalid size value: must be positive integer\n");
                print_http_download_usage();
                return 1;
            }
        }
        else {
            printf("Unknown option: %s\n", argv[i]);
            print_http_download_usage();
            return 1;
        }
    }

    // Validate required parameters
    if (url == NULL || download_path == NULL) {
        printf("URL and download path are required\n");
        print_http_download_usage();
        return 1;
    }

    // Allocate download buffer
    download_buffer = (char*)malloc(max_size_per_piece);
    if (download_buffer == NULL) {
        printf("Failed to allocate download buffer\n");
        return 1;
    }

    // Open output file (truncate if exists)
    download_file_p = fopen(download_path, "wb+");
    if (!download_file_p) {
        printf("Failed to open file: %s\n", download_path);
        ret = 1;
        goto cleanup;
    }

    // Main download loop
    int state = STETE_RUNNING;
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    do {
        printf("Downloading range: %lld-%lld\n",
            download_offset, download_offset + max_size_per_piece - 1);

        mg_http_connect(&mgr, url, http_download_callback_fn, &state);

        // Event polling loop
        while (state == STETE_RUNNING) {
            mg_mgr_poll(&mgr, 1000);
            printf("Progress: %.1f%%\r",
                download_all_size > 0 ? (double)download_offset / download_all_size * 100 : 0.0);
            fflush(stdout);
        }

        write_to_file();

        // Handle retry logic
        if (state == STATE_ERROR && retry_count++ < max_retries) {
            printf("\nRetrying (%d/%d)...\n", retry_count, max_retries);
            state = STETE_RUNNING;
            continue;
        }

        // Continue if more data available
        if (state == STATE_SUCCESS && download_all_size > 0 && download_offset < download_all_size) {
            state = STETE_RUNNING;
        }

    } while (state == STETE_RUNNING && download_offset < download_all_size);

    // Final status
    if (state == STATE_SUCCESS) {
        printf("\nDownload completed. Total size: %lld bytes\n", download_offset);
    }
    else {
        printf("\nDownload failed\n");
        ret = 1;
    }

    mg_mgr_free(&mgr);

cleanup:
    cleanup_resources();
    return ret;
}

/** Callback function for mongoose events */
static void http_download_callback_fn(struct mg_connection* c, int ev, void* ev_data) {
    int* pstate = (int*)c->fn_data;

    switch (ev) {
    case MG_EV_OPEN:
        *(uint64_t*)c->data = mg_millis() + s_timeout_ms + s_transfer_timeout_ms;
        break;

    case MG_EV_CONNECT: {
        struct mg_str host = mg_url_host(url);
        mg_printf(c,
            "GET %s HTTP/1.1\r\n"
            "Host: %.*s\r\n"
            "Connection: close\r\n"
            "Range: bytes=%lld-%lld\r\n"
            "\r\n",
            mg_url_uri(url),
            host.len, host.buf,
            download_offset,
            download_offset + max_size_per_piece - 1);
        break;
    }

    case MG_EV_HTTP_MSG: {
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        int state_start = -1, state_end = -1;
        for (size_t i = 0; i < hm->head.len; i++) {
            if (hm->head.buf[i] == ' ') {
                if (state_start < 0) {
                    state_start = i + 1;
                }
                else if (state_end < 0) {
                    state_end = i;
                    break;
                }
            }
        }
        bool get_state_success = false;
        int resp_code = (int)string_to_long(hm->head.buf + state_start, state_end - state_start, &get_state_success);

        // Check HTTP status code
        if (resp_code != 200 && resp_code != 206) {
            printf("\nHTTP error: %d\n", resp_code);
            *pstate = STATE_ERROR;
            c->is_closing = 1;
            return;
        }

        // Handle full file response (no range support)
        if (resp_code == 200) {
            const struct mg_str* cl = mg_http_get_header(hm, "Content-Length");
            if (cl) {
                bool ok = false;
                download_all_size = string_to_long(cl->buf, cl->len, &ok);
                if (ok) download_offset = download_all_size;
            }
        }

        // Validate body size
        if (hm->body.len > max_size_per_piece) {
            printf("\nBody too large (%lld > %lld)\n", hm->body.len, max_size_per_piece);
            *pstate = STATE_ERROR;
            c->is_closing = 1;
            return;
        }

        // Store received data
        memcpy(download_buffer, hm->body.buf, hm->body.len);
        download_buffer_len = hm->body.len;

        // Parse Content-Range header
        struct mg_str* range_hdr = mg_http_get_header(hm, "Content-Range");
        if (range_hdr != NULL) {
            bool success = false;
            int64_t start = -1, end = -1, total = -1;
            parse_content_range_value(range_hdr->buf, range_hdr->len, &start, &end, &total, &success);

            if (success) {
                download_offset = end + 1;
                download_all_size = total;
                printf("\nReceived bytes %lld-%lld/%lld\n", start, end, total);
            }
        }

        c->is_closing = 1;
        break;
    }

    case MG_EV_ERROR:
        printf("\nError: %s\n", (char*)ev_data);
        *pstate = STATE_ERROR;
        break;

    case MG_EV_CLOSE:
        if (*pstate == STETE_RUNNING) *pstate = STATE_SUCCESS;
        break;

    case MG_EV_POLL:
        if (mg_millis() > *(uint64_t*)c->data) {
            *pstate = STATE_ERROR;
            mg_error(c, "Operation timed out");
        }
        break;
    }
}

/** Parse Content-Range header */
static void parse_content_range_value(const char* str, int len,
    int64_t* start_pos, int64_t* end_pos,
    int64_t* all_size, bool* success) {
    *success = false;
    if (!str || len < 11 || !start_pos || !end_pos || !all_size) return;

    // Create null-terminated copy for safety
    char* tmp = (char*)malloc(len + 1);
    if (!tmp) return;

    memcpy(tmp, str, len);
    tmp[len] = '\0';

    // Parse different Content-Range formats
    if (sscanf(tmp, "bytes %lld-%lld/%lld", start_pos, end_pos, all_size) == 3) {
        *success = true;
    }
    else if (sscanf(tmp, "bytes %lld-%lld/*", start_pos, end_pos) == 2) {
        *success = true;  // Server didn't provide total size
    }

    free(tmp);
}

/** Write buffered data to file */
static void write_to_file() {
    if (!download_file_p || !download_buffer) {
        printf("\nInvalid file or buffer state\n");
        exit(2);
    }

    if (download_buffer_len > 0) {
        size_t written = fwrite(download_buffer, 1, download_buffer_len, download_file_p);
        if (written != (size_t)download_buffer_len) {
            printf("\nWrite failed: expected %lld, wrote %zu\n",
                download_buffer_len, written);
            exit(3);
        }
        fflush(download_file_p);
        download_buffer_len = 0;
    }
}

/** Clean up resources */
static void cleanup_resources() {
    if (download_buffer) {
        free(download_buffer);
        download_buffer = NULL;
    }

    if (download_file_p) {
        fclose(download_file_p);
        download_file_p = NULL;
    }
}

/** Print usage instructions */
static void print_http_download_usage() {
    printf("HTTP Download Tool\n");
    printf("Usage:\n");
    printf("  -u <url>       Download URL (required)\n");
    printf("  -p <path>      Output file path (required)\n");
    printf("  -t <timeout>   Connection timeout in ms (default: 10000)\n");
    printf("  -s <size>      Chunk size in bytes (default: 1048576)\n");
    printf("  -h             Show this help\n");
}

#endif // PROTO_HTTP_DOWNLOAD_H