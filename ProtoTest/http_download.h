#ifndef PROTO_HTTP_DOWNLOAD_H
#define PROTO_HTTP_DOWNLOAD_H
#include "mongoose.h"
#include <stdio.h>
#include <string.h>
#include "util.h"

static int64_t max_size_per_piece = 1 * 1024 * 1024; // how many bytes to download per piece. set by  -s

static uint64_t s_timeout_ms = 10000; // timeout in milliseconds for connection, default is 10 seconds  set by  -t
static const char* url = null;  // the url download from. set by  -u

static const char* download_path = null; // path to save downloaded file. including filename. set by  -p

static FILE* file = (FILE*) null; // File pointer to write to

static char* buffer = (char*)null; // buffer used for saving stream downloaded
int64_t buffer_len = 0;      // how many bytes is effectively downloaded into buffer
int64_t download_offset = 0; // the offset to download from, in bytes
int64_t all_size = 0;        // how many bytes of the whole file in server

static void http_download_callback_fn(struct mg_connection* c, int ev, void* ev_data);

static void parse_content_range_value(const char* str, int len, int64_t* start_pos, int64_t* end_pos, int64_t* all_size, bool* success);

static void write_to_file();

static void print_http_download_usage();

#define RUNNING 0
#define SUCCESS 1
#define ERROR   2

/** the Entry pf HTTP download file module */
int http_download_main(int argc, char* argv[]) {

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
                printf("Invalid timeout value: must be an integer and bigger than 0, but recieved: %s\n", argv[i]);
                print_http_download_usage();
                return 1;
            }
        }
        else if (!strcmp("-s", argv[i])) {
            bool success = false;
            max_size_per_piece = string_to_long(argv[++i], strlen(argv[i]), &success);
            if (!success || max_size_per_piece <= 0) {
                printf("Invalid size value: must be an integer and bigger than 0, but received: %s\n", argv[i]);
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
    if (url == null || download_path == null) {
		printf("URL or download path is not set.\n");
		print_http_download_usage();
		return 1;
    }





    buffer = (char*)malloc(max_size_per_piece);
    if (buffer == null) {
        printf("buffer malloc failed");
        return 1;
    }

    printf("ready to write into file: download.zip");
    file = fopen("download.zip", "ab");
    if (!file) {
        printf("Failed to open file for writing.\n");
        return 1;
    }


    int state = RUNNING;
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    do {
        mg_http_connect(&mgr, url, http_download_callback_fn, &state);
        while (state == RUNNING) {
            mg_mgr_poll(&mgr, 1000);
        }
        write_to_file();

        if (state == SUCCESS && download_offset < all_size) {
            state = RUNNING;
        }
    } while (state == RUNNING && download_offset < all_size);

    mg_mgr_free(&mgr);

    fclose(file);
    free(buffer);
    printf("File written successfully, wroten: %lld.\n", download_offset);
    file = (FILE*)null;
    return 0;
}






// in this function::
// if u want to disconnect, u can set `c->is_closing` to 1(true)
// when error occurred or request is ended, u should set `*(bool*)c->fn_data` to 1(true) for breaking `mg_mgr_poll` cycle in main function
static void http_download_callback_fn(struct mg_connection* c, int ev, void* ev_data) {
    if (ev == MG_EV_OPEN) {
        // Connection created. Store connect expiration time in c->data
        printf("Connection opened\n");
        *(uint64_t*)c->data = mg_millis() + s_timeout_ms;
    } if (ev == MG_EV_CONNECT) {
        printf("Connection connected\n");
        // TODO("config TLS options if needed, like this");
        //struct mg_tls_opts opts = { .name = mg_url_host(url) };
        //mg_tls_init(c, &opts);

        // Attention:
        // Here you mast write Host in format: `"%.*s", host.len, host.buf`.
        // Otherwise the host.buf contains all data without scheme, including uri because they uses the same pointer.
        // Or you can parse url by yourself also.
        struct mg_str host = mg_url_host(url);
        mg_printf(c, "GET %s HTTP/1.1\r\n"
            "Host: %.*s\r\n"
            "Connection: close\r\n"
            "Range: bytes=%lld-%lld\r\n"
            "\r\n",
            mg_url_uri(url), host.len, host.buf, download_offset, download_offset + (int64_t)max_size_per_piece - 1);

        printf("Sending HTTP request: %s\n", c->send.buf);
    }
    else if (ev == MG_EV_HTTP_MSG) {
        printf("Connection received message\n");
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        if (hm->body.len > max_size_per_piece) {
            printf("Body too large! (%lld > %lld)\n", hm->body.len, max_size_per_piece);
            *(int*)c->fn_data = ERROR;
            c->is_closing = 1;
            return;
        }
        memcpy(buffer, hm->body.buf, hm->body.len);
        buffer_len = hm->body.len;

        printf("\n");
        // here you can check some headers you need in hm->headers of response, such as:
        // Content-Length
        // Accept-Ranges
        // Range
        // and etc.

        int header_count = sizeof(hm->headers) / sizeof(hm->headers[0]);
        for (int i = 0; i < header_count; i++) {
            struct mg_http_header* header = &hm->headers[i];
            if (header->name.len == 0) continue;
            printf("Header %d ----> %.*s: %.*s\n", i, (int)header->name.len, header->name.buf, (int)header->value.len, header->value.buf);
            if (!strncmp("Content-Range", header->name.buf, header->name.len)) {
                bool success = false;
                int64_t data_start = -1, data_end = -1, total_size = -1;
                parse_content_range_value(header->value.buf, header->value.len, &data_start, &data_end, &total_size, &success);
                if (!success || data_start < 0 || data_end < 0 || total_size < 0) {
                    printf("Failed to parse Content-Range header: %.*s\n", (int)header->value.len, header->value.buf);
                }
                else {
                    download_offset = data_end + 1;
                    all_size = total_size;
                    printf("Parsed Content-Range: start=%lld, end=%lld, total=%lld\n", data_start, data_end, total_size);
                }
            }
        }
        printf("\n");

        c->is_closing = 1;
    }
    else if (ev == MG_EV_ERROR) {
        *(bool*)c->fn_data = ERROR;
        printf("MG_EV_ERROR: %s\n", (char*)ev_data);
    }
    else if (ev == MG_EV_CLOSE) {
        printf("Connection closed\n");
        *(bool*)c->fn_data = SUCCESS;
    }
    else if (ev == MG_EV_POLL) {
        if (mg_millis() > *(uint64_t*)c->data &&
            (c->is_connecting || c->is_resolving)) {
            *(bool*)c->fn_data = ERROR;
            printf("Connect timeout\n");
        }
    }
}

static void parse_content_range_value(const char* str, int len, int64_t* start_pos, int64_t* end_pos, int64_t* all_size, bool* success) {
    if (str == null || len < 11 || start_pos == null || end_pos == null || all_size == null) {
        if (success != null) {
            *success = false;
            return;
        }
        else {
            printf("Invalid parameters for parse_content_range_value\n");
            exit(1);
            return;
        }
    }
    *start_pos = -1;
    *end_pos = -1;
    *all_size = -1;
    const char* p = str;
    if (*str == ' ') {
        p = str + 1;
    }
    // CANNOT use sscanf here!!
    int matched = _snscanf(str, len, "bytes %lld-%lld/%lld", start_pos, end_pos, all_size);
    if (matched != 3) {
        *success = false;
        return;
    }
    *success = true;
}

static void write_to_file() {
    if (file == null || buffer == null) {
        printf("File pointer or buffer is null, cannot write to file.\n");
        exit(2);
        return;
    }
    if (buffer_len > 0) {
        fwrite(buffer, 1, buffer_len, file);
        buffer_len = 0;
    }
}

static void print_http_download_usage() {
    printf("Options:\n");
    printf("  -u <url>       Required. Set the URL to download from. includes scheme, host, port, path, params and etc.\n");
    printf("  -p <path>      Required. Set the path to save the downloaded file, includes filename, absolute path and suffix.\n");
    printf("  -t <timeout>   Set the timeout in milliseconds. it must be integer and larger than 0 (default: 10 seconds)\n");
    printf("  -s <size>      Set the maximum size of each download piece in bytes. it must be integer and larger than 0 (default: 1MB)\n");
    printf("  -?\n");
    printf("  -h\n");
    printf("  -help          Show this help message\n");
}


#endif // !PROTO_HTTP_DOWNLOAD_H