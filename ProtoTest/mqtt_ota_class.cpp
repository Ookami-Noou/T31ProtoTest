#include "mqtt_ota_class.h"
#include "util.h"
#include "json.h"

hecsion::MqttOtaTask::MqttOtaTask(
	string user, 
	string password, 
	string sn, 
	string name, 
	string version, 
	int qos
) {
	this->user = user;
	this->password = password;
	this->sn = sn;
	this->name = name;
	this->version = version;
	this->qos = qos;
	this->callback = nullptr;
	this->client_id = mg_mprintf(client_id_fmt, name.c_str(), sn.c_str());
	this->mqtt_pub_topic = mg_mprintf(mqtt_sub_topic_fmt, client_id.c_str());
	this->mqtt_result_sub_topic = mg_mprintf(mqtt_result_pub_topic_fmt, client_id.c_str());
	this->mqtt_on_success_pub_topic = mg_mprintf(mqtt_on_success_pub_topic_fmt, client_id.c_str());
	this->mqtt_on_fail_pub_topic = mg_mprintf(mqtt_on_fail_pub_topic_fmt, client_id.c_str());
	this->mqtt_url = "mqtt://36.137.92.217:1007";
	this->mqtt_conn = null;
	this->state = STATE_RUNNING;
	mg_mgr_init(&mgr);
}

hecsion::MqttOtaTask::~MqttOtaTask()
{
	mg_mgr_free(&mgr);
}

void hecsion::MqttOtaTask::connect(std::function<void(bool, const char*)> callback)
{
	this->callback = std::move(callback);
	this->state = STATE_RUNNING;
	struct mg_mqtt_opts opts;
	memset(&opts, 0, sizeof(opts));
	opts.clean = true;
	opts.user = mg_str(this->user.c_str());
	opts.pass = mg_str(this->password.c_str());
	opts.qos = this->qos;
	opts.topic = mg_str(this->mqtt_result_sub_topic.c_str());
	opts.client_id = mg_str(this->client_id.c_str());
	mg_timer_add(&mgr, 3000, MG_TIMER_REPEAT | MG_TIMER_RUN_NOW, reconnect_callback, this);
	if (this->mqtt_conn == null) this->mqtt_conn = mg_mqtt_connect(&mgr, this->mqtt_url.c_str(), &opts, hecsion::MqttOtaTask::mqtt_callback_fn, this);

	while (this->state == STATE_RUNNING) mg_mgr_poll(&mgr, 1000);
}

void hecsion::MqttOtaTask::disconnect()
{
	this->state = STATE_DONE;
	this->callback = nullptr;
}

void hecsion::MqttOtaTask::send_ota_request_pub_message() {
	if (mqtt_conn == null) {
		MG_INFO(("MQTT connection is not established.\n"));
		return;
	}
	const char* msg = mg_mprintf(
		"{"
			"\"messageType\": \"OTA\","
			"\"clientId\":\"%s\","
			"\"messageId\":null,"
			"\"properties\":{"
				"\"deviceSN\":\"%s\","
				"\"deviceName\":\"%s\","
				"\"otaVersion\":\"%s\","
				"\"userName\":\"%s\""
			"}"
		"}", this->client_id.c_str(), this->sn.c_str(), this->name.c_str(), this->version.c_str(), this->user.c_str()
	);
	struct mg_mqtt_opts pub_opts;
	memset(&pub_opts, 0, sizeof(pub_opts));
	pub_opts.topic = mg_str(mqtt_pub_topic.c_str());
	pub_opts.message = mg_str(msg);
	pub_opts.qos = qos;
	pub_opts.retain = false;
	pub_opts.user = mg_str(user.c_str());
	pub_opts.pass = mg_str(password.c_str());
	pub_opts.client_id = mg_str(client_id.c_str());
	mg_mqtt_pub(mqtt_conn, &pub_opts);
	MG_INFO(("sent message: ### %s ### to topic *** %s ***", msg, mqtt_pub_topic.c_str()));
}

void hecsion::MqttOtaTask::send_ota_state_message(bool success, string ver)
{
	if (mqtt_conn == null) {
		MG_INFO(("MQTT connection is not established.\n"));
		return;
	}
	const char* msg = mg_mprintf(
		"{"
			"\"messageType\": \"OTA\","
			"\"clientId\":\"%s\","
			"\"messageId\":null,"
			"\"properties\":{"
				"\"deviceSN\":\"%s\","
				"\"deviceName\":\"%s\","
				"\"otaVersion\":\"%s\","
				"\"userName\": %s"
			"}"
		"}", this->client_id.c_str(), this->sn.c_str(), this->name.c_str(), ver.c_str(), this->user.c_str()
	);
	struct mg_mqtt_opts pub_opts;
	memset(&pub_opts, 0, sizeof(pub_opts));
	pub_opts.topic = mg_str(success ? mqtt_on_success_pub_topic.c_str() : mqtt_on_fail_pub_topic.c_str());
	pub_opts.message = mg_str(msg);
	pub_opts.qos = qos;
	pub_opts.retain = false;
	pub_opts.user = mg_str(user.c_str());
	pub_opts.pass = mg_str(password.c_str());
	pub_opts.client_id = mg_str(client_id.c_str());
	mg_mqtt_pub(mqtt_conn, &pub_opts);
	MG_INFO(("sent message: ### %s ### to topic *** %s ***", msg, success ? mqtt_on_success_pub_topic.c_str() : mqtt_on_fail_pub_topic.c_str()));
}

void hecsion::MqttOtaTask::process_received_data(const mg_str* data)
{
	ota_response_t response;
	bool success = parse_response(data->buf, data->len, &response);
	if (!success) {
		MG_INFO(("Failed to parse response data: %.*s", (int)data->len, data->buf));
		return;
	}
	if (response.code == 200) {
		MG_INFO(("OTA update available: %s", response.properties.remoteUrl ? response.properties.remoteUrl : "No URL provided"));
		if (response.properties.hasNew && response.properties.remoteUrl) {
			if (callback != nullptr) {
				callback(true, response.properties.remoteUrl);
			}
		}
		else {
			MG_INFO(("No new OTA update available."));
			if (callback != nullptr) {
				callback(false, nullptr);
			}
		}
	}
	else {
		MG_INFO(("OTA command failed with code: %d, message: %s", response.code, response.message ? response.message : "No message"));
		this->state = STATE_ERROR;
		return;
	}
}

#ifndef IS_KEY
#define IS_KEY(json_obj, key) (strncmp(json_obj->name->string, key, json_obj->name->string_size) == 0)
#endif

#ifndef IS_VALUE_TYPE
#define IS_VALUE_TYPE(json_obj, target_type) (json_obj->value->type == target_type)
#endif

bool hecsion::MqttOtaTask::parse_response(const char* json_data, size_t len, ota_response_t* cmd) {
	MG_INFO(("start to parse response: %s", json_data));
	json_value_t* json_obj = json_parse(json_data, len);
	if (json_obj == null) {
		MG_INFO(("Failed to parse JSON data: %s", json_data));
		return false;
	}
	memset(cmd, 0, sizeof(ota_response_t));
	if (json_obj->type != json_type_object) {
		MG_INFO(("JSON data is not an object: %s", json_data));
		goto on_fail;
	}
	else {
		json_object_t* obj = (json_object_t*)json_obj->payload;
		json_object_element_t* elem = obj->start;
		while (elem) {
			if (IS_KEY(elem, "code") && IS_VALUE_TYPE(elem, json_type_number)) {
				json_number_t* code_num = json_value_as_number(elem->value);
				if (code_num == null) {
					MG_INFO(("Failed to get code from command"));
					goto on_fail;
				}
				bool success = false;
				cmd->code = string_to_long(code_num->number, code_num->number_size, &success);
				if (!success) {
					MG_INFO(("Failed to parse code from command"));
					goto on_fail;
				}
			}
			else if (IS_KEY(elem, "message") && IS_VALUE_TYPE(elem, json_type_string)) {
				json_string_t* message_str = (json_string_t*)elem->value->payload;
				cmd->message = mg_mprintf("%.*s", message_str->string_size, message_str->string);
			}
			else if (IS_KEY(elem, "messageType") && IS_VALUE_TYPE(elem, json_type_string)) {
				json_string_t* type_str = (json_string_t*)elem->value->payload;
				cmd->messageType = mg_mprintf("%.*s", type_str->string_size, type_str->string);
			}
			else if (IS_KEY(elem, "clientId") && IS_VALUE_TYPE(elem, json_type_string)) {
				json_string_t* client_id_str = (json_string_t*)elem->value->payload;
				cmd->clientId = mg_mprintf("%.*s", client_id_str->string_size, client_id_str->string);
			}
			else if (IS_KEY(elem, "messageId") && IS_VALUE_TYPE(elem, json_type_string)) {
				json_string_t* message_id_str = (json_string_t*)elem->value->payload;
				cmd->messageId = mg_mprintf("%.*s", message_id_str->string_size, message_id_str->string);
			}
			else if (IS_KEY(elem, "properties") && IS_VALUE_TYPE(elem, json_type_object)) {
				json_object_t* object = (json_object_t*)elem->value->payload;
				json_object_element_t* cmd_elem = object->start;
				while (cmd_elem) {
					if (IS_KEY(cmd_elem, "hasNew")) {
						if (IS_VALUE_TYPE(cmd_elem, json_type_true)) {
							cmd->properties.hasNew = true;
						}
						else {
							cmd->properties.hasNew = false;
						}
					} 
					else if (IS_KEY(cmd_elem, "remoteUrl") && IS_VALUE_TYPE(cmd_elem, json_type_string)) {
						json_string_t* remoteUrl = (json_string_t*)cmd_elem->value->payload;
						cmd->properties.remoteUrl = mg_mprintf("%.*s", remoteUrl->string_size, remoteUrl->string);
					}
					cmd_elem = cmd_elem->next;
				}
			}

			elem = elem->next;
		}
	}

	free(json_obj);
	return true;

on_fail:
	free(json_obj);
	return false;
}

void hecsion::MqttOtaTask::reconnect_callback(void* arg)
{
	hecsion::MqttOtaTask* task = (hecsion::MqttOtaTask*)arg;
	struct mg_mgr* mgr = &task->mgr;
	struct mg_mqtt_opts opts;
	memset(&opts, 0, sizeof(opts));
	opts.clean = true;
	opts.user = mg_str(task->user.c_str());
	opts.pass = mg_str(task->password.c_str());
	opts.qos = task->qos;
	opts.topic = mg_str(task->mqtt_result_sub_topic.c_str());
	opts.client_id = mg_str(task->client_id.c_str());
	if (task->mqtt_conn == null) {
		task->mqtt_conn = mg_mqtt_connect(mgr, task->mqtt_url.c_str(), &opts, mqtt_callback_fn, task);
	}
}

void hecsion::MqttOtaTask::mqtt_callback_fn(mg_connection* c, int ev, void* ev_data)
{
	hecsion::MqttOtaTask* task = (hecsion::MqttOtaTask*) c->fn_data;
	char time[10] = { 0 };
	format_current_time(time);
	if (ev == MG_EV_OPEN) {
		printf("#######################################################################\n");
		MG_INFO(("%s \t%lu CREATED, ready to connect url: %s, topic: %s", time, c->id, task->mqtt_url.c_str(), task->mqtt_result_sub_topic.c_str()));
		//c->is_hexdumping = 1;
	}
	else if (ev == MG_EV_CONNECT) {
		MG_INFO(("%s \t%lu Connect to %s", time, c->id, task->mqtt_url.c_str()));
	}
	else if (ev == MG_EV_ERROR) {
		// On error, log error message
		MG_INFO(("%s \t%lu ERROR %s", time, c->id, (char*)ev_data));
	}
	else if (ev == MG_EV_MQTT_OPEN) {
		// MQTT connect is successful
		struct mg_str subt = mg_str(task->mqtt_result_sub_topic.c_str());
		MG_INFO(("%s \t%lu CONNECTED to %s", time, c->id, task->mqtt_url.c_str()));
		task->send_ota_request_pub_message();
	}
	else if (ev == MG_EV_MQTT_MSG) {
		// When we get echo response, print it
		struct mg_mqtt_message* mm = (struct mg_mqtt_message*)ev_data;
		MG_INFO(("%s \t%lu RECEIVED %.*s <- %.*s", time, c->id, (int)mm->data.len, mm->data.buf, (int)mm->topic.len, mm->topic.buf));
		task->process_received_data(&(mm->data));
	}
	else if (ev == MG_EV_CLOSE) {
		MG_INFO(("%s \t%lu CLOSED\n", time, c->id));
		task->mqtt_conn = (struct mg_connection*)null;
	}
}



