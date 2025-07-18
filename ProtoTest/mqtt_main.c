// Copyright (c) 2023 Cesanta Software Limited
// All rights reserved
//
// Example MQTT client. It performs the following steps:
//    1. Connects to the MQTT server specified by `s_url` variable
//    2. When connected, subscribes to the topic `s_sub_topic`
//    3. Publishes message `hello` to the `s_pub_topic`
//    4. Receives that message back from the subscribed topic and closes
//    5. Timer-based reconnection logic revives the connection when it is down
//
// To enable SSL/TLS, see https://mongoose.ws/tutorials/tls/#how-to-build

#include "mongoose.h"
#include "json_util.h"
#include <time.h>
#include <stdio.h>

#include "controller.h"

static const char* const report_property_message_type = "report_property";  // Message type for report_property

// in case of some attributes are constant for each device, they're defined here
// region const device attributes

static const char* user = "bWFzdGVyLeiAs+acuua1i+ivlQ==";               // user account
static const char* password = "595911d4-341d-436a-9611-2e8791e8bf44";   // user password
static const char* dev_name = "WESEE_E01";                              // device name
static const char* dev_os = "Linux Zeratul";                            // device OS
static const char* sn = "24092510001308";                               // device sn
static const char* hw_ver = "1.0";                                      // hardware version of device
static const char* sw_ver = "E01_ZC_WEBRTC_20250102_1740";              // T31 / T32 version
static const char* wifi_mac = "b8:2d:28:c7:77:d5";                      // wifi mac address
static const char* bt_mac = "f1:d2:11:00:18:f7";                        // bluetooth mac address
static const char* bt_ver = "V9.1.1.51";                                // bluetooth fireware version
static const char* webrtc = "HCSX-00-NB23-7FHK-00000013";               // webrtc code. format: 1234-12-1234-1234-1234567
static long total_disk = 16 * 1024 * 1024L;                             // total disk size. Unit: Byte
static const char* client_id = "%s~%s";                                 // client id for communicating with server. ## init later in main function

static const char* s_url = "mqtt://36.137.92.217:1007";                 // server url for MQTT-connection
static const char* s_sub_topic = "earphone_f1/%s~%s/cmd";               // subscribe topic, ## init later in main function
static const char* s_pub_topic = "earphone_f1/%s~%s/status";            // publish topic,   ## init later in main function
static int s_qos = 1;                                                   // MQTT QoS
// endregion


static struct mg_connection* s_conn;              // Client connection

void format_current_time(char* buffer) {
    time_t rawtime;
    struct tm* timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    sprintf(buffer, "%02d:%02d:%02d",
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec);
}

void send_pub_message(struct mg_str* msg) {
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
struct mg_str get_report_property_msg(const long avail_disk, const char* const voltage) {
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
        "\"voltage\":\"%s\","
        "\"webrtc\":\"%s\""
        "}"
        "}",
        report_property_message_type, client_id, dev_name, dev_os,
        sn, hw_ver, sw_ver, total_disk, avail_disk, wifi_mac, bt_mac, bt_ver, voltage, webrtc);

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

void process_received_data(const struct mg_str* const data) {
    if (data == null || data->len == 0 || data->buf == null) return;
    if (!is_json_string(data->buf, data->len)) {
        MG_ERROR(("Received data is not a valid JSON string: %.*s", (int)data->len, data->buf));
        return;
    }
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
    const char* replay_type = null;
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
        "\n\tcmd: %s,"
        "\n}\n",
        cmd->type, cmd->client_id, cmd->message_id, cmd->msg, cmd->cmd));

    struct mg_str replay = get_result_replay_msg(replay_type, cmd->message_id, "{\"funcId\":16386,\"data\":{\"default\":\"\"}}");

    send_pub_message(&replay);
    // Free the command structure after processing
    free_command(cmd);

}

/* callback for mqtt commection */
static void fn(struct mg_connection* c, int ev, void* ev_data) {
    char time[10] = { 0 };
    format_current_time(time);
    if (ev == MG_EV_OPEN) {
        printf("⇱↗→↘↡↙←↖↟↗→↘↡↙←↖↟↗→↘↡↙←↖↟↗⇲\n");
        MG_INFO(("%s \t%lu CREATED", time, c->id));
        // c->is_hexdumping = 1;
    }
    else if (ev == MG_EV_CONNECT) {
        MG_INFO(("%s \t%lu Connect to %s", time, c->id, s_url));
        // TODO("Handle connection options here, like TLS");
        /*if (c->is_tls) {
            struct mg_tls_opts opts = { .ca = mg_unpacked("/certs/ca.pem"),
                                       .name = mg_url_host(s_url) };
            mg_tls_init(c, &opts);
        }*/
    }
    else if (ev == MG_EV_ERROR) {
        // On error, log error message
        MG_ERROR(("%s \t%lu ERROR %s", time, c->id, (char*)ev_data));
        // TODO("Handle error here, like reconnecting");
    }
    else if (ev == MG_EV_MQTT_OPEN) {
        // MQTT connect is successful
        struct mg_str subt = mg_str(s_sub_topic);
        MG_INFO(("%s \t%lu CONNECTED to %s", time, c->id, s_url));
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
        struct mg_str msg = get_report_property_msg(15 * 1024 * 1024, "99");
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
        s_conn = null;  // Mark that we're closed
    }
}

// Timer function - recreate client connection if it is closed
static void timer_reconn_fn(void* arg) {
    struct mg_mgr* mgr = (struct mg_mgr*)arg;
    struct mg_mqtt_opts opts = {
        .clean = true,
        .user = mg_str(user),
        .pass = mg_str(password),
        .qos = s_qos,
        .topic = mg_str(s_pub_topic),
        .client_id = mg_str(client_id),
    };
    if (s_conn == null) s_conn = mg_mqtt_connect(mgr, s_url, &opts, fn, null);
}

/* upload device info timer */
static void timer_upload_fun(void* arg) {
    // TODO("replace this as the real upload function");
    if (s_conn == null) return;

    struct mg_str msg = get_report_property_msg(5 * 1024 * 1024, "25");
    send_pub_message(&msg);
}

int mqtt_main(int argc, char* argv[]) {
	// TODO set params in region: const device attributes

    s_sub_topic = mg_mprintf(s_sub_topic, dev_name, sn);  // Initialize subscription topic
    s_pub_topic = mg_mprintf(s_pub_topic, dev_name, sn);  // Initialize publication topic
    client_id = mg_mprintf(client_id, dev_name, sn);  // Initialize client ID

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_timer_add(&mgr, 3000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_reconn_fn, &mgr);
    mg_timer_add(&mgr, 3 * 60 * 1000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, timer_upload_fun, null);
    while (1) mg_mgr_poll(&mgr, 1000);  // Event loop, 1s timeout
    mg_mgr_free(&mgr);                  // Finished, cleanup

    return 0;
}

#if MQTT_MAIN == ENTRY
int main(int argc, char* argv[]) {
    return mqtt_main(argc, argv);
}
#endif