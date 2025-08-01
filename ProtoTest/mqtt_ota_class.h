#pragma once
#ifndef HECSION_MQTT_OTA_CLASS
#define HECSION_MQTT_OTA_CLASS

#include "mongoose.h"
#include <cctype>
#include <cstring>
#include <string>
#include <cstdio>
#include <functional>

using namespace std;

namespace hecsion {

	typedef struct {
		int code;					// Response code, 200 for success
		const char* message;		// Optional message, can be null
		const char* messageType;	// Message type, can be "OTA" or other types
		const char* clientId;		// Client ID of the device
		const char* messageId;		// Message ID, can be null if not used
		struct {
			bool hasNew;			// Indicates if there is a new OTA update available
			const char* remoteUrl;	// URL to download the OTA update
		} properties;				// Properties related to the OTA update
	} ota_response_t;

	class MqttOtaTask {
	private :
		static constexpr const char* const client_id_fmt = "%s~%s";
		static constexpr const char* const mqtt_sub_topic_fmt = "earphone_f1/%s/ota";
		static constexpr const char* const mqtt_result_pub_topic_fmt = "earphone_f1/%s/serverResultMsg";
		static constexpr const char* const mqtt_on_success_pub_topic_fmt = "earphone_f1/%s/otaSucceed";
		static constexpr const char* const mqtt_on_fail_pub_topic_fmt = "earphone_f1/%s/otaFail";

		static const char STATE_RUNNING = 0;
		static const char STATE_DONE    = 1;
		static const char STATE_ERROR   = 2;

	private :
		string user;
		string password;
		string sn;
		string name;
		string version;
		int qos;

		string mqtt_url;
		string client_id;
		string mqtt_pub_topic;
		string mqtt_result_sub_topic;
		string mqtt_on_success_pub_topic;
		string mqtt_on_fail_pub_topic;
		struct mg_connection* mqtt_conn;
		struct mg_mgr mgr;

		char state; // Flag to indicate if the request is done

		std::function<void(bool, const char*)> callback;

	public:
		/**
		 * Constructor for MqttOtaTask.
		 * 
		 * @param [in] user : User account for MQTT connection
		 * @param [in] password : User password for MQTT connection
		 * @param [in] sn : Device serial number
		 * @param [in] name : Device name
		 * @param [in] version : Current device version
		 * @param [in] qos : MQTT Quality of Service level (default is 1)
		 * @param [in] timeout_ms : Timeout for the MQTT connection in milliseconds (default is 10000 ms)
		 */
		MqttOtaTask(string user, string password, string sn, string name, string version, int qos = 1);

		~MqttOtaTask();

		/**
		 * Create a long connection with MQTT server.
		 * You can call `disconnect` to close the connection.
		 * 
		 * @param [in] callback : Callback function to handle OTA update availability.
		 *		In this function, `can_update` indicates whether an update is available,
		 *		if it's `true`, `ota_url` will contain the URL to download the OTA package.
		 *		And then, you can download the OTA package for updating here.
		 *		After updating, you should call `send_ota_state_message` to notify the server
		 */
		void connect(std::function<void(bool, const char*)> callback);

		/**
		 * Disconnect from MQTT server.
		 */
		void disconnect();

		/**
		 * Send to server Whether the OTA update is success or not.
		 * 
		 * @param [in] success : true if the OTA update is successful, false otherwise
		 * @param [in] ver : The newest version of device if success; otherwise the current version of device
		 */
		void send_ota_state_message(bool success, string ver);

	private:
		void send_ota_request_pub_message();
		void process_received_data(const struct mg_str* data);

	private:
		static void mqtt_callback_fn(struct mg_connection* c, int ev, void* ev_data);

		static bool parse_response(const char* json_data, size_t len, ota_response_t* cmd);

		static void reconnect_callback(void* arg);
	};
}

#endif // !HECSION_MQTT_OTA_CLASS

