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
static const char* user_ota = null;				   // username provided by GuiZhou mobel
static const char* password_ota = null;			   // password provided by GuiZhou mobel
static const char* device_sn_ota = null;
static const char* device_name_ota = null;
static const char* device_version_ota = null;

static const char* client_id_ota = null;
static const char* mqtt_ota_pub_tpoic = "earphone_f1/%s/ota";					// MQTT topic for OTA updates
static const char* mqtt_ota_result_sub_topic = "earphone_f1/%s/serverResultMsg";// MQTT topic for OTA result subscription
static const char* mqtt_ota_on_success_pub_topic = "earphone_f1/%s/otaSucceed";	// MQTT topic to publish success messages
static const char* mqtt_ota_on_fail_pub_topic = "earphone_f1/%s/otaFail";		// MQTT topic to publish fail messages

static const char* s_mqtt_ota_url = "mqtt://36.137.92.217:1007";
static struct mg_connection* s_mqtt_conn = null;
static int s_ota_qos = 1;

static void mqtt_ota_callback_fn(struct mg_connection* c, int ev, void* ev_data);
static void print_mqtt_ota_usage();
static const char* generate_mqtt_ota_replay() {
	return mg_mprintf(
		"{"
			"\"messageType\": \"OTA\", "
			"\"clientId\": \"WESEE_E01~24121210002803\", "
			"\"messageId\":null,"
			"\"properties\": {"
				"\"deviceSN\":\"%s\","
				"\"deviceName\":\"%s\","
				"\"otaVersion\":\"%s\","
				"\"userName\": \"%s\""
			"}"
		"}", device_sn_ota, device_name_ota, device_version_ota, user_ota
	);
}


int mqtt_ota_main(int argc, char* argv[], int* can_update, const char* *ota_url) {
	for (int i = 1; i < argc - 1; i++) {
		if (!strcmp(argv[i], "-u")) {
			user_ota = argv[++i];
		}
		else if (!strcmp(argv[i], "-p")) {
			password_ota = argv[++i];
		}
		else if (!strcmp(argv[i], "-sn")) {
			device_sn_ota = argv[++i];
		}
		else if (!strcmp(argv[i], "-name")) {
			device_name_ota = argv[++i];
		}
		else if (!strcmp(argv[i], "-ver")) {
			device_version_ota = argv[++i];
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
		else if (!strcmp(argv[i], "-q") || !strcmp(argv[i], "-qos")) {
			bool success = false;
			s_ota_qos = (int)string_to_long(argv[++i], strlen(argv[i]), &success);
			if (!success || s_ota_qos < 0 || s_ota_qos > 2) {
				printf("Invalid QoS value: must be an integer between 0 and 2, but received: %s\n", argv[i]);
				return 1;
			}
		}
		else if (!strcmp(argv[i], "-url")) {
			s_mqtt_ota_url = argv[++i];
			if (s_mqtt_ota_url == null || strlen(s_mqtt_ota_url) == 0) {
				printf("Invalid MQTT URL: must be a non-empty string.\n");
				return 1;
			}
		}
		else {
			printf("Unknown option: %s\n", argv[i]);
			return 1;
		}
	}

	if (user_ota == null || password_ota == null || device_sn_ota == null || device_name_ota == null || device_version_ota == null || s_mqtt_ota_url == null) {
		printf("Missing required parameters. Please provide user, password, device_sn, device_name, and device_version.\n");
		print_mqtt_ota_usage();
		return 1;
	}

	client_id_ota = mg_mprintf("%s~%s", device_name_ota, device_sn_ota);  // Initialize client ID
	mqtt_ota_pub_tpoic = mg_mprintf(mqtt_ota_pub_tpoic, client_id_ota);
	mqtt_ota_result_sub_topic = mg_mprintf(mqtt_ota_result_sub_topic, client_id_ota);
	mqtt_ota_on_success_pub_topic = mg_mprintf(mqtt_ota_on_success_pub_topic, client_id_ota);
	mqtt_ota_on_fail_pub_topic = mg_mprintf(mqtt_ota_on_fail_pub_topic, client_id_ota);

	struct mg_mgr mgr;
	mg_mgr_init(&mgr);

	bool done = false;struct mg_mqtt_opts opts;
	memset(&opts, 0, sizeof(opts));
	opts.clean = true;
	opts.user = mg_str(user);
	opts.pass = mg_str(password);
	opts.qos = s_qos;
	opts.topic = mg_str(s_pub_topic);
	opts.client_id = mg_str(client_id);
	if (s_conn == null) s_conn = mg_mqtt_connect(mgr, s_url, &opts, mqtt_ota_callback_fn, null);

	while (!done) mg_mgr_poll(&mgr, 1000);  // Event loop, 1s timeout
	mg_mgr_free(&mgr);                  // Finished, cleanup

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
