#ifndef PROTO_MQTT_OTA
#define PROTO_MQTT_OTA

#include "mongoose.h"
#include "util.h"
#include "http_download.h"

/**
 * in this file , i'll just ensure weather the device needs update. if needs, provide the url of new OTA package.
 * and then, user should call `http_download_main` to download the OTA package.
 */

static int64_t timeout_ms = 10 * 1000L;		   // timeout. default 10 seconds
static const char* user = null;				   // username provided by GuiZhou mobel
static const char* password = null;			   // password provided by GuiZhou mobel
static const char* device_sn = null;
static const char* device_name = null;
static const char* device_version = null;

static const char* client_id = null;

static void mqtt_ota_callback_fn(struct mg_connection* c, int ev, void* ev_data);
static void print_mqtt_ota_usage();


int mqtt_ota_main(int argc, char* argv[]) {
	for (int i = 1; i < argc - 1; i++) {
		if (!strcmp(argv[i], "-u")) {
			user = argv[++i];
		}
		else if (!strcmp(argv[i], "-p")) {
			password = argv[++i];
		}
		else if (!strcmp(argv[i], "-sn")) {
			device_sn = argv[++i];
		}
		else if (!strcmp(argv[i], "-name")) {
			device_name = argv[++i];
		}
		else if (!strcmp(argv[i], "-ver")) {
			device_version = argv[++i];
		}
		else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-help") || !strcmp(argv[i], "-?")) {
			print_mqtt_ota_usage();
			return 0;
		}
		else if (!strcmp(argv[i], "-t")) {
			bool success = false;
			int64_t timeout = string_to_long(argv[++i], strlen(argv[i]), &success);
			if (!success || timeout <= 0) {
				printf("Invalid timeout value: must be a positive integer, but received: %s\n", argv[i]);
				return 1;
			}
			if (timeout > 0) {
				timeout_ms = timeout;  // Update the timeout if provided
			}
		}
		else {
			printf("Unknown option: %s\n", argv[i]);
			return 1;
		}
	}

	if (user == null || password == null || device_sn == null || device_name == null || device_version == null) {
		printf("Missing required parameters. Please provide user, password, device_sn, device_name, and device_version.\n");
		print_mqtt_ota_usage();
		return 1;
	}

	client_id = mg_mprintf("%s~%s", device_name, device_sn);  // Initialize client ID

	return 0;
}







static void print_mqtt_ota_usage() {
	printf("Usage: mqtt_ota [options]\n");
	printf("Options:\n");
	printf("  -u <user>           Required. Set the user account (base64 encoded).\n");
	printf("  -p <password>       Required. Set the user password.\n");
	printf("  -sn <serial_number> Required. Set the device serial number.\n");
	printf("  -name <name>        Required. Set the device name.\n");
	printf("  -ver <version>      Required. Set the device version.\n");
	printf("  -t <timeout>        Optional. Set the connection timeout in milliseconds (default is 10000 ms).\n");
	printf("  -h, -help, -?       Show this help message.\n");
}

#endif // !PROTO_MQTT_OTA
