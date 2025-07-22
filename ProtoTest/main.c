#include "http_download.h"
#include "mqtt_iteractive.h"

int test_mqtt_interactive() {
	char* argv[] = {
		"mqtt_interactive", 
		"-user", "bWFzdGVyLeiAs+acuua1i+ivlQ==",
		"-password", "595911d4-341d-436a-9611-2e8791e8bf44",
		"-dev_name", "WESEE_E01",
		"-dev_os", "Linux Zeratul",
		"-sn", "24092510001308",
		"-hw_ver", "1.0",
		"-sw_ver", "E01_ZC_WEBRTC_20250102_1740",
		"-wifi_mac", "b8:2d:28:c7:77:d5",
		"-bt_mac", "f1:d2:11:00:18:f7",
		"-bt_ver", "V9.1.1.51",
		"-total_disk", "16777216", // 16 * 1024 * 1024
		"-webrtc", "HCSX-00-NB23-7FHK-00000013",
		"-avail_disk", "8388608", // 8 * 1024 * 1024
		"-voltage", "33",
		"-qos", "1",
		"-upload", "180000", // 3 minutes
		"-ca", null
	};

	return mqtt_interactive_main(sizeof(argv) / sizeof(argv[0]), argv);
}

int test_http_download() {
	char* argv[] = {
		"http_download",
		"-u", "http://ylxiot-bucket.eos-guiyang-4.cmecloud.cn/ota/e2c87511-6972-403f-b497-50b83337481a-%E6%B5%8B%E8%AF%95ota%E5%8C%85.zip?AWSAccessKeyId=qFvAMy4cfaChR0IoRSa0bw5HXEps&Expires=33300345599&Signature=7JpxllsPtbDOwRYtz0YyJzN0%2Bwk%3D",
		"-p", "download.zip",
		"-t", "10000", // 10 seconds
		"-s", "1048576", // 1 MB
	};
	return http_download_main(sizeof(argv) / sizeof(argv[0]), argv);
}

int main(int argc, char* argv[]) {
	return test_mqtt_interactive();
}