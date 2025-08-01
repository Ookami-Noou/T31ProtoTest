#include "mqtt_ota_class.h"
#include <string>
#include <functional>

int main(int argc, char* argv[]) {
	hecsion::MqttOtaTask task(
//		-user bWFzdGVyLeiAs+acuua1i+ivlQ==
//		-password 595911d4-341d-436a-9611-2e8791e8bf44
		"lXgoML7wky6+svn05Oo0FQ==", // user
		"595911d4-341d-436a-9611-2e8791e8bf44", // password
		"24092510001308", // sn
		"WESEE_E01", // name
		"E01_ZC_WEBRTC_20250102_1740", // version
		1 // qos
	);
	task.connect([&] (bool can_update, const char* ota_url) {
		if (can_update) {
			printf("OTA update available at: %s\n", ota_url);
			size_t len = strlen(ota_url);
			char* ota_url_copy = (char*)malloc(len);
			if (ota_url_copy == nullptr) {
				printf("Failed to allocate memory for OTA URL copy.\n");

				task.send_ota_state_message(false, "E01_ZC_WEBRTC_20250102_1740"); // Assuming the current version is the same
				return;
			}
			memset(ota_url_copy, 0, len);
			memcpy(ota_url_copy, ota_url, 4);
			memcpy(ota_url_copy + 4, ota_url + 5, len - 5);
			printf("OTA URL copy: %s\n", ota_url_copy);
			void* res = malloc(1);
			if (!((int64_t)res / 10 % 2)) {
				printf("OTA update failed with success code: %d\n", res);
				task.send_ota_state_message(true, "E01_ZC_WEBRTC_20250102_1740"); // Assuming the update is successful
			}
			else {
				printf("OTA update failed with error code: %d\n", res);
				task.send_ota_state_message(false, "E01_ZC_WEBRTC_20250102_1740"); // Assuming the current version is the same
			}
			if (res != NULL) {
				free(res);
			}
		}
		else {
			printf("No OTA update available.\n");
		}
	});
	return 0;
}