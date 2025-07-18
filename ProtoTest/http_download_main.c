#include "mongoose.h"
#include "controller.h"
#include <stdio.h>
#include <string.h>

// how many bytes to download per piece.
#define DOWNLOAD_SIZE_PER_PIECE (1 * 1024 * 1024LL)


static char* buffer = null; // buffer used for saving stream downloaded
static const uint64_t s_timeout_ms = 10000;
static const char* url = "http://ylxiot-bucket.eos-guiyang-4.cmecloud.cn/"    // scheme://host:port
"ota/e2c87511-6972-403f-b497-50b83337481a-%E6%B5%8B%E8%AF%95ota%E5%8C%85.zip"   // uri
"?AWSAccessKeyId=qFvAMy4cfaChR0IoRSa0bw5HXEps&Expires=33300345599&Signature=7JpxllsPtbDOwRYtz0YyJzN0%2Bwk%3D"; // params

static const char* download_path = "download.zip"; // path to save downloaded file. including filename
static FILE* file = null; // File pointer to write to

int64_t buffer_len = 0;      // how many bytes is effectively downloaded into buffer
int64_t download_offset = 0; // the offset to download from, in bytes
int64_t all_size = 0;        // how many bytes of the whole file in server

static void fn(struct mg_connection* c, int ev, void* ev_data);

static long string_to_long(const char* str, int len, bool* success);

static void parse_content_range_value(const char* str, int len, int64_t* start_pos, int64_t* end_pos, int64_t* all_size, bool* success);

static void write_to_file();

#define RUNNING 0
#define SUCCESS 1
#define ERROR   2

/** the Entry pf HTTP download file module */
int http_download_main(int argc, char* argv[]) {
    buffer = (char*)malloc(DOWNLOAD_SIZE_PER_PIECE);
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
        mg_http_connect(&mgr, url, fn, &state);
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
    printf("File written successfully.\n");

    return 0;
}

#if ENTRY == HTTP_DOWNLOAD_MAIN
int main(int argc, char* argv[]) {
    return http_download_main(argc, argv);
}
#endif









// in this function::
// if u want to disconnect, u can set `c->is_closing` to 1(true)
// when error occurred or request is ended, u should set `*(bool*)c->fn_data` to 1(true) for breaking `mg_mgr_poll` cycle in main function
static void fn(struct mg_connection* c, int ev, void* ev_data) {
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
            mg_url_uri(url), host.len, host.buf, download_offset, download_offset + (int64_t)DOWNLOAD_SIZE_PER_PIECE - 1);

        printf("Sending HTTP request: %s\n", c->send.buf);
    }
    else if (ev == MG_EV_HTTP_MSG) {
        printf("Connection received message\n");
        struct mg_http_message* hm = (struct mg_http_message*)ev_data;
        if (hm->body.len > DOWNLOAD_SIZE_PER_PIECE) {
            printf("Body too large! (%lld > %lld)\n", hm->body.len, DOWNLOAD_SIZE_PER_PIECE);
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

// Convert a string to a long integer, handling signs and invalid characters
// if ivalid characters are found, it prints an error message and returns 0
static long string_to_long(const char* str, int len, bool* success) {
    long value = 0;
    long sign = 1; // Default sign is positive
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