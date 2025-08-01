//#include "http_download.h"
//#include "mqtt_iteractive.h"
//#include "json.h"
//
//int test_mqtt_interactive() {
//	char* argv[] = {
//		"mqtt_interactive", 
//		"-user", "bWFzdGVyLeiAs+acuua1i+ivlQ==",
//		"-password", "595911d4-341d-436a-9611-2e8791e8bf44",
//		"-dev_name", "WESEE_E01",
//		"-dev_os", "Linux Zeratul",
//		"-sn", "24092510001308",
//		"-hw_ver", "1.0",
//		"-sw_ver", "E01_ZC_WEBRTC_20250102_1740",
//		"-wifi_mac", "b8:2d:28:c7:77:d5",
//		"-bt_mac", "f1:d2:11:00:18:f7",
//		"-bt_ver", "V9.1.1.51",
//		"-total_disk", "16777216", // 16 * 1024 * 1024
//		"-webrtc", "HCSX-00-NB23-7FHK-00000013",
//		"-avail_disk", "8388608", // 8 * 1024 * 1024
//		"-voltage", "33",
//		"-qos", "1",
//		"-upload", "180000", // 3 minutes
//		"-ca", null
//	};
//
//	return mqtt_interactive_main(sizeof(argv) / sizeof(argv[0]), argv);
//}
//
//int test_http_download() {
//	char* argv[] = {
//		"http_download",
//		"-u", "http://ylxiot-bucket.eos-guiyang-4.cmecloud.cn/ota/e2c87511-6972-403f-b497-50b83337481a-%E6%B5%8B%E8%AF%95ota%E5%8C%85.zip?AWSAccessKeyId=qFvAMy4cfaChR0IoRSa0bw5HXEps&Expires=33300345599&Signature=7JpxllsPtbDOwRYtz0YyJzN0%2Bwk%3D",
//		"-p", "download1.zip",
//		"-t", "10000", // 10 seconds
//		"-s", "1048576", // 1 MB
//	};
//	return http_download_main(sizeof(argv) / sizeof(argv[0]), argv);
//}
//
//void print_json_value(struct json_value_s* root) {
//	if (root->type == json_type_object) {
//		struct json_object_s* obj = (struct json_object_s*)root->payload;
//		struct json_object_element_s* elem = obj->start;
//
//		while (elem) {
//			struct json_string_s* key = elem->name;
//			struct json_value_s* value = elem->value;
//
//			if (value->type == json_type_string) {
//				struct json_string_s* str_val = (struct json_string_s*)value->payload;
//				printf("Key: %.*s, Value: %.*s\n",
//					(int)key->string_size, key->string,
//					(int)str_val->string_size, str_val->string);
//			}
//			else if (value->type == json_type_number) {
//				struct json_number_s* num_val = (struct json_number_s*)value->payload;
//				printf("Key: %.*s, Value: %lld\n",
//					(int)key->string_size, key->string,
//					num_val->number);
//			}
//			else if (value->type == json_type_true) {
//				struct json_boolean_s* bool_val = (struct json_boolean_s*)value->payload;
//				printf("Key: %.*s, Value: true\n",
//					(int)key->string_size, key->string);
//			}
//			else if (value->type == json_type_false) {
//				struct json_boolean_s* bool_val = (struct json_boolean_s*)value->payload;
//				printf("Key: %.*s, Value: false\n",
//					(int)key->string_size, key->string);
//			}
//			else if (value->type == json_type_null) {
//				printf("Key: %.*s, Value: null\n", (int)key->string_size, key->string);
//			}
//			else if (value->type == json_type_object) {
//				print_json_value(value);
//			}
//			else if (value->type == json_type_array) {
//				struct json_array_s* array = json_value_as_array(root);
//				if (!array) {
//					printf("Not an array!\n");
//					return;
//				}
//				struct json_array_element_s* element = array->start;
//				size_t index = 0;
//				while (element) {
//					struct json_value_s* value = element->value;
//					print_json_value(value);
//					element = element->next;
//					index++;
//				}
//			}
//			else {
//				printf("Key: %.*s, Value: Unexpected type %lld\n", (int)key->string_size, key->string, value->type);
//			}
//			elem = elem->next;
//		}
//	}
//	else if (root->type == json_type_array) {
//		struct json_array_s* arr = (struct json_array_s*)root->payload;
//		for (size_t i = 0; i < arr->length; i++) {
//			struct json_array_element_s value = arr->start[i];
//			printf(value.value);
//		}
//	}
//	else {
//		printf("Unexpected JSON type: %lld\n", root->type);
//	}
//}
//
//int test_json() {
//	const char* val = "{"
//			"\"code\":200,"
//			"\"message\" : null,"
//			"\"messageType\" : \"OTA未用到可以不填或不写\","
//			"\"clientId\" : \"WESEE_E01~24121210002803\","
//			"\"messageId\" : \"OTA未用到可以不填或不写\","
//			"\"properties\" : {"
//				"\"hasNew\": true, "
//				"\"remoteUrl\" : \"http://192.168.1.1/ota.zip\""
//			"}"
//		"}";
//	json_value_t *root = json_parse(val, strlen(val));
//	if (root == NULL) {
//		printf("Failed to parse JSON\n");
//		return 1;
//	}
//	print_json_value(root);
//
//	free(root);
//}
//
//int main(int argc, char* argv[]) {
//	return test_http_download();
//}