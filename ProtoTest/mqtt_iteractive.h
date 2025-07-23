#ifndef PROTO_MQTT_INTERACTIVE_H
#define PROTO_MQTT_INTERACTIVE_H

#include "mqtt_iteractive.h"
#include "util.h"
#include "json.h"

#include "mongoose.h"
#include <time.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
        const char* type;         // Type of command
        const char* client_id;    // Client ID
        const char* message_id;   // Message ID for this communication, should be used in reply messages
        const char* msg;          // Extra message of command, can be null when sent from server
        struct {
            int32_t funcId;       // Function ID of the command
            json_value_t* data;   // a json data for this `funcId`
        } properties;             // extra data of command
    } command_t;

	/**
	 * send a message to publish topic.
	 *
	 * @param [in] msg - must be a string which ends with `\0`
	 */
	void send_pub_message(const char* msg);


    void send_pub_message_mg_str(const struct mg_str* msg);

	/**
	 * set current device info of available disk and available voltage.
	 *
	 * you need call this function from time to time;
	 *
	 * @param [in] available_disk - current available disk space in bytes. Cannot little than 0 or bigger than total disk size.
	 * @param [in] voltage - current voltage level. Cannot little than 0 or bigger than 100.
	 */
	void set_device_info(long available_disk, int voltage);


	/**
	 * main entry of mqtt interactive
	 *
	 * @param [in] argc - argument count of `argv`
	 * @param [in] argv - argument vector of `argc` like command line
	 *				    Options:
	 *                    -user <user>          Required. Set the user account (base64 encoded).
	 *                    -password <password>  Required. Set the user password.
	 *                    -dev_name <name>      Required. Set the device name.
	 *                    -dev_os <os>          Required. Set the device OS (e.g., Linux Zeratul).
	 *                    -sn <serial_number>   Required. Set the device serial number.
	 *                    -hw_ver <version>     Required. Set the hardware version.
	 *                    -sw_ver <version>     Required. Set the software version.
	 *                    -wifi_mac <mac>       Required. Set the WiFi MAC address.
	 *                    -bt_mac <mac>         Required. Set the Bluetooth MAC address.
	 *                    -bt_ver <version>     Required. Set the Bluetooth version.
	 *                    -total_disk <size>    Required. Set the total disk size in bytes.
	 *                    -webrtc <code>        Required. Set the WebRTC code.
	 *                    -qos <0|1|2>          Set the MQTT QoS level (default is 1).
	 *	                  -ca <path>            Optional. Set the CA certificate path for TLS connection.
	 *	                  -avail_disk <size>    Optional. Set the available disk space in bytes (default is 0).
	 *	                  -voltage <value>      Optional. Set the voltage level (default is 0).
     *                    -upload <interval>    Optional. Set the upload interval in milliseconds (default is 180000 ms).
	 *                    -h, -help, -?         Show this help message.
	 * @return 0 on success, or non-zero value on error.
	 */
	int mqtt_interactive_main(int argc, char* argv[]);













    static const char* const report_property_message_type = "report_property";  // Message type for report_property

    // in case of some attributes are constant for each device, they're defined here

    // region const device attributes

    static const char* user = null;             // user account
    static const char* password = null;         // user password
    static const char* dev_name = null;         // device name
    static const char* dev_os = null;           // device OS
    static const char* sn = null;               // device sn
    static const char* hw_ver = null;           // hardware version of device
    static const char* sw_ver = null;           // T31 / T32 version
    static const char* wifi_mac = null;         // wifi mac address
    static const char* bt_mac = null;           // bluetooth mac address
    static const char* bt_ver = null;           // bluetooth fireware version
    static const char* webrtc = null;           // webrtc code. format: 1234-12-1234-1234-1234567
    static long total_disk = 0;                 // total disk size. Unit: Byte
    static const char* client_id = "%s~%s";     // client id for communicating with server. ## init later in main function

    static const char* ca_path = null;          // CA certificate path, if needed

    static const char* s_mqtt_url = "mqtt://36.137.92.217:1007";                 // server url for MQTT-connection
    static const char* s_sub_topic = "earphone_f1/%s~%s/cmd";               // subscribe topic, ## init later in main function
    static const char* s_pub_topic = "earphone_f1/%s~%s/status";            // publish topic,   ## init later in main function
    static int s_qos = 1;                                                   // MQTT QoS
    // endregion

    static long available_disk = 0;          // Available disk space in bytes, used for report_property message
    static int voltage = 0;                  // Voltage level as a string, used for report_property message

    static struct mg_connection* s_conn;              // Client connection

#define IS_KEY(json_obj, key) (strncmp(json_obj->name->string, key, json_obj->name->string_size) == 0)
#define IS_VALUE_TYPE(json_obj, target_type) (json_obj->value->type == target_type)

    static void free_command(command_t* cmd);
    static command_t* parse_command(const char* json_data);

    static command_t* parse_command(const char* json_data) {
		json_value_t* json_obj = json_parse(json_data, strlen(json_data));
		if (json_obj == null) {
			MG_ERROR(("Failed to parse JSON data: %s", json_data));
			return null;
		}
		command_t* cmd = (command_t*)malloc(sizeof(command_t));
		if (cmd == null) {
			MG_ERROR(("Failed to allocate memory for command_t"));
            goto on_fail;
		}
		memset(cmd, 0, sizeof(command_t));
        if (json_obj->type != json_type_object) {
			MG_ERROR(("JSON data is not an object: %s", json_data));
        }
        else {
			json_object_t* obj = (json_object_t*)json_obj->payload;
			json_object_element_t* elem = obj->start;
            while (elem) {
				if (IS_KEY(elem, "type") && IS_VALUE_TYPE(elem, json_type_string)) {
					json_string_t* string_str = (json_string_t*)elem->value->payload;
					char* str = (const char*)malloc(string_str->string_size + 1);
					if (str) {
						strncpy(str, string_str->string, string_str->string_size);
                        str[string_str->string_size] = '\0';
						cmd->type = str;
                    }
                    else {
						MG_ERROR(("Failed to allocate memory for command type"));
                        goto on_fail;
                    }
				}
                else if (IS_KEY(elem, "clientId") && IS_VALUE_TYPE(elem, json_type_string)) {
                    json_string_t* string_str = (json_string_t*)elem->value->payload;
                    char* str = (const char*)malloc(string_str->string_size + 1);
                    if (str) {
                        strncpy(str, string_str->string, string_str->string_size);
                        str[string_str->string_size] = '\0';
                        cmd->client_id = str;
                    }
                    else {
                        MG_ERROR(("Failed to allocate memory for command type"));
                        goto on_fail;
                    }
                }
				else if (IS_KEY(elem, "messageId") && IS_VALUE_TYPE(elem, json_type_string)) {
					json_string_t* string_str = (json_string_t*)elem->value->payload;
					char* str = (const char*)malloc(string_str->string_size + 1);
					if (str) {
						strncpy(str, string_str->string, string_str->string_size);
						str[string_str->string_size] = '\0';
						cmd->message_id = str;
					}
					else {
						MG_ERROR(("Failed to allocate memory for command message_id"));
                        goto on_fail;
					}
				}
                else if (IS_KEY(elem, "msg")) {
                    if (IS_VALUE_TYPE(elem, json_type_string)) {
						json_string_t* string_str = (json_string_t*)elem->value->payload;
						char* str = (const char*)malloc(string_str->string_size + 1);
						if (str) {
							strncpy(str, string_str->string, string_str->string_size);
							str[string_str->string_size] = '\0';
							cmd->msg = str;
						}
						else {
							MG_ERROR(("Failed to allocate memory for command msg"));
							goto on_fail;
						}
					}
                    else if (IS_VALUE_TYPE(elem, json_type_null)) {
                        cmd->msg = null;
                    }
                }
                else if (IS_KEY(elem, "cmd") && IS_VALUE_TYPE(elem, json_type_object)) {
                    json_object_t* object = (json_object_t*)elem->value->payload;
					json_object_element_t* cmd_elem = object->start;
                    while (cmd_elem) {
                        if (IS_KEY(cmd_elem, "funcId") && IS_VALUE_TYPE(cmd_elem, json_type_number)) {
                            json_number_t* cmd_func_id = json_value_as_number(cmd_elem->value);
                            if (cmd_func_id == null) {
								MG_ERROR(("Failed to get funcId from command"));
								goto on_fail;
                            }
                            bool success = false;
							cmd->properties.funcId = string_to_long(cmd_func_id->number, cmd_func_id->number_size, &success);
                            if (!success) {
								MG_ERROR(("Failed to parse funcId from command"));
                                goto on_fail;
                            }
						}
						else if (IS_KEY(cmd_elem, "data") && IS_VALUE_TYPE(cmd_elem, json_type_object)) {
                            cmd->properties.data = json_extract_value(cmd_elem->value);
						}
                        cmd_elem = cmd_elem->next;
                    }
                }

                elem = elem->next;
            }
        }
		
		free(json_obj);
		return cmd;

    on_fail:
        free(json_obj);
        free_command(cmd);
        return null;
    }

    static void free_command(command_t* cmd) {
		if (cmd == null) return;
		if (cmd->type) free((void*)cmd->type);
		if (cmd->client_id) free((void*)cmd->client_id);
		if (cmd->message_id) free((void*)cmd->message_id);
		if (cmd->msg) free((void*)cmd->msg);
		if (cmd->properties.data) free(cmd->properties.data);
		free(cmd);
    }


    void set_device_info(long disk, int vol) {
        if (disk < 0) {
            available_disk = 0;
        }
        else if (disk > total_disk) {
            available_disk = total_disk;
        }
        else {
            available_disk = disk;
        }
        if (vol < 0) {
            voltage = 0;
        }
        else if (vol > 100) {
            voltage = 100;
        }
        else {
            voltage = vol;
        }
    }

    void send_pub_message(const char* msg) {
        struct mg_str msg_str = mg_str(msg);
        send_pub_message_mg_str(&msg_str);
    }

    inline void send_pub_message_mg_str(const struct mg_str* msg) {
        struct mg_str pubt = mg_str(s_pub_topic);

        struct mg_mqtt_opts pub_opts;
        memset(&pub_opts, 0, sizeof(pub_opts));
        pub_opts.topic = pubt;
        pub_opts.message = *msg;
        pub_opts.qos = s_qos, pub_opts.retain = false;
        pub_opts.user = mg_str(user);
        pub_opts.pass = mg_str(password);
        pub_opts.client_id = mg_str(client_id);
        mg_mqtt_pub(s_conn, &pub_opts);
        char time[10] = { 0 };
        format_current_time(time);
        MG_INFO(("%s \t%lu PUBLISHED %.*s -> %.*s", time, s_conn->id, (int)msg->len, msg->buf, (int)pubt.len, pubt.buf));
    }


    /**
     * Generate a mg_str object which contains a JSON string for the report_property message.
     *
     * @param [in] avail_disk : Available disk space in bytes.
     * @param [in] voltage : Voltage level as a string. it should be percent without the percent sign, e.g. "25" for 25%.
     */
    struct mg_str get_report_property_msg() {
        // Create a JSON string for the report_property message
        const char* const json = mg_mprintf(
            "{"
            "\"messageType\":\"%s\","
            "\"clientId\":\"%s\","
            "\"properties\":{"
            "\"dev_name\":\"%s\","
            "\"dev_os\":\"%s\","
            "\"sn\":\"%s\","
            "\"hw_ver\":\"%s\","
            "\"sw_ver\":\"%s\","
            "\"totaldisk\":%ld,"
            "\"availdisk\":%ld,"
            "\"wifi_mac\":\"%s\","
            "\"bt_mac\":\"%s\","
            "\"bt_ver\":\"%s\","
            "\"voltage\":\"%d\","
            "\"webrtc\":\"%s\""
            "}"
            "}",
            report_property_message_type, client_id, dev_name, dev_os,
            sn, hw_ver, sw_ver, total_disk, available_disk, wifi_mac, bt_mac, bt_ver, voltage, webrtc);

        return mg_str(json);
    }

    /**
     *Generate an offline message for the device.
     */
    struct mg_str get_offline_msg() {
        // Generate an offline message
        const char* const offline_msg = mg_mprintf(
            "{"
            "\"messageType\":\"device_offline\","
            "\"clientId\":\"%s\""
            "}",
            mg_mprintf("%s~%s", dev_name, sn));
        return mg_str(offline_msg);
    }

    /**
     * Generate a replay message for the device.
     *
     * @param [in] message_type : The type of the message (e.g., "report_property").
     * @param [in] message_id : The ID of the message which is received from the message from server
     * @param [in] properties : The properties of the message in JSON format.
     */
    struct mg_str get_result_replay_msg(const char* const message_type, const char* const message_id, const char* const properties) {
        const char* const msg = mg_mprintf(
            "{"
            "\"messageType\":\"%s\","
            "\"clientId\":\"%s\","
            "\"messageId\":\"%s\","
            "\"properties\":%s"
            "}",
            message_type, client_id, message_id, properties);
        return mg_str(msg);
    }

    // TODO use json lib to process data received from server;
    void process_received_data(const struct mg_str* const data) {
        printf("%s:%d TODO use json lib to process data received from server", __FILE__, __LINE__);
        if (data == null || data->len == 0 || data->buf == null) return;
        command_t* cmd = parse_command(data->buf);
        if (cmd == null) {
            MG_ERROR(("Failed to parse command from received data: %.*s", (int)data->len, data->buf));
            return;
        }
        if (strcmp(client_id, cmd->client_id)) {
            // this message should not be received
            MG_ERROR(("Received command for a different client_id: expected %s, got %s", client_id, cmd->client_id));
            return;
        }
        const char* replay_type = (const char*)null;
        if (!strcmp(cmd->type, "command_debug_reply")) {
            replay_type = "result_debug_reply";
        }
        else {
            replay_type = cmd->type;
        }
        // Process the command based on its type
        // TODO("Handle the command as needed");
        // For example, you can print the properties or perform actions based on funcId
        MG_INFO(("Received command_debug_reply for command_t: {"
            "\n\ttype: %s, "
            "\n\tclient_id: %s,"
            "\n\tmessage_id: %s,"
            "\n\tmsg: %s,"
            "\n\tcmd: {,"
            "\n\t\tfuncId: %d,"
            "\n\t\tdata: %s"
            "\n\t}"
            "\n}\n",
            cmd->type, cmd->client_id, cmd->message_id, cmd->msg, cmd->properties.funcId, cmd->properties.data));

        struct mg_str replay = get_result_replay_msg(replay_type, cmd->message_id, "{\"funcId\":16386,\"data\":{\"default\":\"\"}}");

        send_pub_message(&replay);
        // Free the command structure after processing
        free_command(cmd);

    }

    /* callback for mqtt commection */
    static void mqtt_interactive_callback_fn(struct mg_connection* c, int ev, void* ev_data) {
        char time[10] = { 0 };
        format_current_time(time);
        if (ev == MG_EV_OPEN) {
            printf("-/\\-/\\-/\\-/\\-/\\-/\\-/\\-/\\-\n");
            MG_INFO(("%s \t%lu CREATED", time, c->id));
            //c->is_hexdumping = 1;
        }
        else if (ev == MG_EV_CONNECT) {
            MG_INFO(("%s \t%lu Connect to %s", time, c->id, s_mqtt_url));
            if (ca_path != null && c->is_tls) {
                struct mg_tls_opts opts = { .ca = mg_unpacked(ca_path),
                                           .name = mg_url_host(s_mqtt_url) };
                mg_tls_init(c, &opts);
            }
        }
        else if (ev == MG_EV_ERROR) {
            // On error, log error message
            MG_ERROR(("%s \t%lu ERROR %s", time, c->id, (char*)ev_data));
        }
        else if (ev == MG_EV_MQTT_OPEN) {
            // MQTT connect is successful
            struct mg_str subt = mg_str(s_sub_topic);
            MG_INFO(("%s \t%lu CONNECTED to %s", time, c->id, s_mqtt_url));
            struct mg_mqtt_opts sub_opts;
            memset(&sub_opts, 0, sizeof(sub_opts));
            sub_opts.topic = subt;
            sub_opts.qos = s_qos;
            sub_opts.client_id = mg_str(client_id);
            sub_opts.retain = false;
            sub_opts.user = mg_str(user);
            sub_opts.pass = mg_str(password);
            mg_mqtt_sub(c, &sub_opts);
            MG_INFO(("%s \t%lu SUBSCRIBED to %.*s", time, c->id, (int)subt.len, subt.buf));
            struct mg_str msg = get_report_property_msg();
            send_pub_message(&msg);
        }
        else if (ev == MG_EV_MQTT_MSG) {
            // When we get echo response, print it
            struct mg_mqtt_message* mm = (struct mg_mqtt_message*)ev_data;
            MG_INFO(("%s \t%lu RECEIVED %.*s <- %.*s", time, c->id, (int)mm->data.len, mm->data.buf, (int)mm->topic.len, mm->topic.buf));
            process_received_data(&(mm->data));
        }
        else if (ev == MG_EV_CLOSE) {
            MG_INFO(("%s \t%lu CLOSED\n", time, c->id));
            s_conn = (struct mg_connection*)null;  // Mark that we're closed
        }
    }

    // Timer function - recreate client connection if it is closed
    static void timer_reconn_fn(void* arg) {
        struct mg_mgr* mgr = (struct mg_mgr*)arg;
        struct mg_mqtt_opts opts;
        memset(&opts, 0, sizeof(opts));
        opts.clean = true;
        opts.user = mg_str(user);
        opts.pass = mg_str(password);
        opts.qos = s_qos;
        opts.topic = mg_str(s_pub_topic);
        opts.client_id = mg_str(client_id);
        if (s_conn == null) s_conn = mg_mqtt_connect(mgr, s_mqtt_url, &opts, mqtt_interactive_callback_fn, null);
    }

    /* upload device info timer */
    static void timer_upload_fun(void* arg) {
        // TODO("replace this as the real upload function");
        if (s_conn == null) return;

        struct mg_str msg = get_report_property_msg();
        send_pub_message(&msg);
    }

    static void print_mqtt_main_usage() {
        printf("Usage: mqtt_main [options]\n");
        printf("Options:\n");
        printf("  -user <user>          Required. Set the user account (base64 encoded).\n");
        printf("  -password <password>  Required. Set the user password.\n");
        printf("  -dev_name <name>      Required. Set the device name.\n");
        printf("  -dev_os <os>          Required. Set the device OS (e.g., Linux Zeratul).\n");
        printf("  -sn <serial_number>   Required. Set the device serial number.\n");
        printf("  -hw_ver <version>     Required. Set the hardware version.\n");
        printf("  -sw_ver <version>     Required. Set the software version.\n");
        printf("  -wifi_mac <mac>       Required. Set the WiFi MAC address.\n");
        printf("  -bt_mac <mac>         Required. Set the Bluetooth MAC address.\n");
        printf("  -bt_ver <version>     Required. Set the Bluetooth version.\n");
        printf("  -total_disk <size>    Required. Set the total disk size in bytes.\n");
        printf("  -webrtc <code>        Required. Set the WebRTC code.\n");
        printf("  -qos <0|1|2>          Set the MQTT QoS level (default is 1).\n");
        printf("  -ca <path>            Optional. Set the CA certificate path for TLS connection.\n");
        printf("  -avail_disk <size>    Optional. Set the available disk space in bytes (default is 0).\n");
        printf("  -voltage <value>      Optional. Set the voltage level (default is 0).\n");
		printf("  -upload <interval>    Optional. Set the upload interval in milliseconds (default is 180000 ms).\n");
        printf("  -h, -help, -?         Show this help message.\n");
    }

    int mqtt_interactive_main(int argc, char* argv[]) {
        int64_t upload_interval = 3 * 60 * 1000L;
        for (int i = 1; i < argc - 1; i++) {
            if (!strcmp("-user", argv[i])) {
                user = argv[++i];  // Set user account
            }
            else if (!strcmp("-password", argv[i])) {
                password = argv[++i];  // Set user password
            }
            else if (!strcmp("-dev_name", argv[i])) {
                dev_name = argv[++i];  // Set device name
            }
            else if (!strcmp("-dev_os", argv[i])) {
                dev_os = argv[++i];  // Set device OS
            }
            else if (!strcmp("-sn", argv[i])) {
                sn = argv[++i];  // Set device serial number
            }
            else if (!strcmp("-hw_ver", argv[i])) {
                hw_ver = argv[++i];  // Set hardware version
            }
            else if (!strcmp("-sw_ver", argv[i])) {
                sw_ver = argv[++i];  // Set software version
            }
            else if (!strcmp("-wifi_mac", argv[i])) {
                wifi_mac = argv[++i];  // Set WiFi MAC address
            }
            else if (!strcmp("-bt_mac", argv[i])) {
                bt_mac = argv[++i];  // Set Bluetooth MAC address
            }
            else if (!strcmp("-bt_ver", argv[i])) {
                bt_ver = argv[++i];  // Set Bluetooth version
            }
            else if (!strcmp("-total_disk", argv[i])) {
                bool success = false;
                total_disk = string_to_long(argv[++i], strlen(argv[i]), &success);
                if (!success || total_disk <= 0) {
                    printf("Invalid total disk size: must be an integer and bigger than 0, but received: %s\n", argv[i]);
                    return 1;
                }
            }
            else if (!strcmp("-webrtc", argv[i])) {
                webrtc = argv[++i];  // Set WebRTC code
            }
            else if (!strcmp("-qos", argv[i])) {
                bool success = false;
                s_qos = (int)string_to_long(argv[++i], strlen(argv[i]), &success);
                if (!success || s_qos < 0 || s_qos > 2) {
                    printf("Invalid QoS value: must be an integer between 0 and 2, but received: %s\n", argv[i]);
                    return 1;
                }
            }
            else if (!strcmp("-ca", argv[i])) {
                ca_path = argv[++i];  // Set CA certificate path
            }
            else if (!strcmp("-avail_disk", argv[i])) {
                bool success = false;
                long disk = string_to_long(argv[++i], strlen(argv[i]), &success);
                if (!success || disk < 0) {
                    printf("Invalid available disk size: must be a non-negative integer, but received: %s\n", argv[i]);
                    return 1;
                }
                set_device_info(disk, voltage);  // Set available disk space
            }
            else if (!strcmp("-voltage", argv[i])) {
                bool success = false;
                long vol = string_to_long(argv[++i], strlen(argv[i]), &success);
                if (!success || vol < 0) {
                    printf("Invalid available disk size: must be a non-negative integer, but received: %s\n", argv[i]);
                    return 1;
                }
                set_device_info(available_disk, vol);  // Set available disk space
            }
            else if (!strcmp("-h", argv[i]) || !strcmp("-help", argv[i]) || !strcmp("-?", argv[i])) {
                print_mqtt_main_usage();
                return 0;
            }
			else if (!strcmp("-upload", argv[i])) {
                bool success = false;
                upload_interval = string_to_long(argv[++i], strlen(argv[i]), &success);
				if (!success || upload_interval <= 0) {
					printf("Invalid upload interval: must be a positive integer, but received: %s\n", argv[i]);
					return 1;
				}
			}
            else {
                printf("Unknown option: %s\n", argv[i]);
                print_mqtt_main_usage();
                return 1;
            }
        }

        if (!user || !password || !dev_name || !sn || !hw_ver || !sw_ver || !wifi_mac || !bt_mac || !bt_ver || !webrtc) {
            printf("Missing required parameters. Please provide user, password, dev_name, sn, hw_ver, sw_ver, wifi_mac, bt_mac, bt_ver, and webrtc.\n");
            print_mqtt_main_usage();
            return 1;
        }

        s_sub_topic = mg_mprintf(s_sub_topic, dev_name, sn);  // Initialize subscription topic
        s_pub_topic = mg_mprintf(s_pub_topic, dev_name, sn);  // Initialize publication topic
        client_id = mg_mprintf(client_id, dev_name, sn);  // Initialize client ID

        struct mg_mgr mgr;
        mg_mgr_init(&mgr);
        mg_timer_add(&mgr, 3000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_reconn_fn, &mgr);
        mg_timer_add(&mgr, upload_interval, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_upload_fun, null);
        while (1) mg_mgr_poll(&mgr, 1000);  // Event loop, 1s timeout
        mg_mgr_free(&mgr);                  // Finished, cleanup

        return 0;
    }


#ifdef __cplusplus
}
#endif
#endif // !PROTO_MQTT_INTERACTIVE_H
