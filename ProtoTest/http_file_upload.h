#ifndef PROTO_HTTP_FILE_UPLOAD_H
#define PROTO_HTTP_FILE_UPLOAD_H

#include "util.h"
#include "mongoose.h"

static const char* s_url = null; // URL to upload file to, set by -u
static const char* s_file_path = null; // Path to the file to upload, set by -p
static uint64_t s_timeout_ms = 10000; // Timeout in milliseconds for connection, default is 10 seconds, set by -t
static uint64_t max_upload_size_per_piece = 1 * 1024 * 1024; // Maximum size of each upload piece in bytes, default is 1MB, set by -s

static char* upload_buffer = null;

static FILE* file_upload_p = null;
static uint64_t upload_offset = 0; // Offset to upload from, in bytes
static uint64_t upload_all_size = 0; // Total size of the file to upload, in bytes


static void print_file_upload_usage();

static void http_upload_callback_fn(struct mg_connection* c, int ev, void* ev_data);

int http_file_upload_main(int argc, char* argv[]) {
	for (int i = 1; i < argc - 1; i++) {
		if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]) || !strcmp("-?", argv[i])) {
			print_file_upload_usage();
			return 0;
		}
		else if (!strcmp("-u", argv[i])) {
			s_url = argv[++i];
		}
		else if (!strcmp("-p", argv[i])) {
			s_file_path = argv[++i];
		}
		else if (!strcmp("-t", argv[i])) {
			bool success = false;
			s_timeout_ms = string_to_long(argv[++i], strlen(argv[i]), &success);
			if (!success || s_timeout_ms <= 0) {
				printf("Invalid timeout value: must be an integer and bigger than 0, but received: %s\n", argv[i]);
				return 1;
			}
		}
		else if (!strcmp("-s", argv[i])) {
			bool success = false;
			max_upload_size_per_piece = string_to_long(argv[++i], strlen(argv[i]), &success);
			if (!success || max_upload_size_per_piece <= 0) {
				printf("Invalid size value: must be an integer and bigger than 0, but received: %s\n", argv[i]);
				return 1;
			}
		}
		else {
			printf("Unknown option: %s\n", argv[i]);
			print_file_upload_usage();
			return 1;
		}
	}

	if (s_url == null || s_file_path == null) {
		printf("URL or file path is not set.\n");
		print_file_upload_usage();
		return 1;
	}

	upload_buffer = (char*)malloc(max_upload_size_per_piece);
	if (upload_buffer == null) {
		printf("buffer malloc failed");
		return 1;
	}

	printf("ready to write into file: %s", s_file_path);
	file_upload_p = fopen(s_file_path, "ab");
	if (!file_upload_p) {
		printf("Failed to open file for writing.\n");
		return 1;
	}


	int state = 0;
	struct mg_mgr mgr;
	mg_mgr_init(&mgr);

	do {
		mg_http_connect(&mgr, s_url, http_upload_callback_fn, &state);
		while (state == 0) {
			mg_mgr_poll(&mgr, 1000);
		}
		// TODO ÊµÏÖupload

		if (state == 1 && upload_offset < upload_all_size) {
			state = 0;
		}
	} while (state == 0 && upload_offset < upload_all_size);

	mg_mgr_free(&mgr);

	fclose(file_upload_p);
	free(upload_buffer);
	printf("File written successfully, wroten: %lld.\n", upload_offset);
	file_upload_p = (FILE*)null;
	return 0;
}








static void print_file_upload_usage() {
	printf("Usage: http_file_upload -u <url> -p <file_path> [-t <timeout_ms>]\n");
	printf("Options:\n");
	printf("  -u <url>          Required. Set the URL to upload the file to.\n");
	printf("  -p <file_path>    Required. Set the path to the file to upload.\n");
	printf("  -t <timeout_ms>   Set the timeout in milliseconds (default: 10000 ms).\n");
}

#endif // !PROTO_HTTP_FILE_UPLOAD_H
