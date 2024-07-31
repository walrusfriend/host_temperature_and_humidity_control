#include "Network.h"

#include <string>

/** TODO: Добавить проверки во всех json (см. доку) */

extern hw_timer_t *status_timer;

Network::Network() {
}

Network::~Network() {
}

void Network::connect_to_wifi() {
	/** TODO: Reconnect does not working */
	Serial.printf("INFO: Try to connect to Wi-Fi with ssid: %s and password: %s\n",
				  wifi_cfg.ssid, wifi_cfg.pass);

	if (WiFi.mode(WIFI_STA)) {
		Serial.println("INFO: Mode changed successfully!");
	}
	else {
		Serial.println("ERROR: Mode changing failed!");
	}

	uint8_t status = WiFi.begin(wifi_cfg.ssid, wifi_cfg.pass);

	Serial.printf("INFO: Initial status code - %d\n", status);

	uint8_t wifi_connection_tries = 0;
	bool is_connection_successful = true;

	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.printf("INFO: Wi-Fi status code - %d\n", status);

		if (wifi_connection_tries >= Network::MAX_WIFI_CONNECTION_TRIES)
		{
			Serial.printf("\nERROR: Couldn't connect to Wi-Fi network with ssid: %s and password: %s!\n"
						  "Please restart the device or set other Wi-Fi SSID and password!\n",
						  wifi_cfg.ssid, wifi_cfg.pass);
			wifi_connection_tries = 0;
			is_connection_successful = false;
			handle_disconnect();
			break;
		}

		++wifi_connection_tries;
		delay(1000);
	};

	if (is_connection_successful)
	{
		client.setInsecure();

		Serial.println("INFO: Wi-Fi connected");
		Serial.println();

		do_wifi_connect = false;

		// Connect to NTP server
		ntp_client.begin();

		timerAlarmEnable(status_timer);

		POST_log("INFO", "Wi-Fi connected");
	}
}

void Network::handle_disconnect() {
	/** TODO: Disable compressor if Wi-Fi disconnected */
	/** TODO: Add error to queue and then send it to the server when network will appear */
	Serial.println("ERROR: Wi-Fi connection lost");
	do_wifi_connect = true;

	if (WiFi.disconnect()) {
		Serial.println("INFO: Disconnected successfully!");
	}
	else {
		Serial.println("INFO: Disconnection failed!");
	}

	ntp_client.end();
}

void Network::get_status(UserDefinedParameters& params) {
	// Check the current connection status
	if ((WiFi.status() == WL_CONNECTED))
	{
		Serial.println("DEBUG: On status handler");

		GET_hub(params);
		Serial.println();
		// POST_log("INFO", "A GET HUB request has been sent");
	}
	else
	{
		handle_disconnect();
	}
}

void Network::change_wifi_cfg(const std::string_view& SSID, const std::string_view& pass) {
	/** TODO: Add check for too short pass or ssid */

	if (SSID.size() > WIFI_SSID_SIZE) {
		Serial.printf("ERROR: The SSID size must be less than %d\n", WIFI_SSID_SIZE);
		return;
	}

	if (pass.size() > WIFI_PASS_SIZE) {
		Serial.printf("ERROR: The password size must be less than %d\n", WIFI_PASS_SIZE);
		return;
	}

	Serial.printf("INFO: Parsed SSID: %s and password: %s\n", SSID.data(), pass.data());


	strcpy(wifi_cfg.ssid, SSID.data());
	strcpy(wifi_cfg.pass, pass.data());

	EEPROM.put(1, wifi_cfg);
	EEPROM.commit();

	WiFi.reconnect();
	do_wifi_connect = true;
}

void Network::update_schedule(std::vector<Calendar::Unit>& list) {
	GET_schedule(list);
}

void Network::POST_log(const std::string_view& tag, const std::string_view& log_string) {
	StaticJsonDocument<128> query;

	query["tag"] = tag;
	query["log_value"] = log_string;
	query["hub_id"] = hub_id;

	String serialized_query;
	serializeJson(query, serialized_query);

	bool status = https.begin(server_cfg.url + log_endpoint);

	if(status == false) {
		Serial.println("ERROR: POST_log() - Couldn't start https session!");
		https.end();
		return;
	}

	int httpCode = https.POST(serialized_query);

	if (httpCode > 0)
	{
		String payload = https.getString();

		StaticJsonDocument<128> reply;
		DeserializationError error = deserializeJson(reply, payload);

		if (error) {
			Serial.println("ERROR: POST_log() - Deserialization error!");
			https.end();
			return;
		}

		if (reply["status"] != "OK") {
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else {
			Serial.println("INFO: Log -  OK");
		}
	}
	else
	{
		Serial.println("ERROR: POST_log() - HTTP-request error");
	}

	Serial.println();
	https.end();
}

void Network::POST_temp(const uint8_t& temperature_value) {
	StaticJsonDocument<128> query;

	query["temperature_value"] = String(temperature_value);
	query["hub_id"] = hub_id;
	query["sensor_id"] = sensor_id;

	String serialized_query;
	serializeJson(query, serialized_query);

	bool status = https.begin(server_cfg.url + temp_endpoint);

	if(status == false) {
		Serial.println("ERROR: POST_temp() - Couldn't start https session!");
		https.end();
		return;
	}

	int httpCode = https.POST(serialized_query);

	if (httpCode > 0)
	{
		String payload = https.getString();

		StaticJsonDocument<128> reply;
		DeserializationError error = deserializeJson(reply, payload);

		if (error) {
			Serial.println("ERROR: POST_temp() - Deserialization error!");
			https.end();
			return;
		}

		if (reply["status"] != "OK") {
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else {
			Serial.println("INFO: Temperature -  OK");
		}
	}
	else
	{
		Serial.printf("ERROR: POST_temp() - HTTPS ERROR: %d\n", httpCode);
	}

	https.end();
}

void Network::POST_hum(const uint8_t& humidity_value) {
	StaticJsonDocument<128> query;

	query["humidity_value"] = String(humidity_value);
	query["hub_id"] = hub_id;
	query["sensor_id"] = sensor_id;

	String serialized_query;
	serializeJson(query, serialized_query);

	bool status = https.begin(server_cfg.url + hum_endpoint);

	if(status == false) {
		Serial.println("ERROR: POST_hum() - Couldn't start https session!");
		https.end();
		return;
	}

	int httpCode = https.POST(serialized_query);

	if (httpCode > 0)
	{
		String payload = https.getString();

		StaticJsonDocument<128> reply;
		DeserializationError error = deserializeJson(reply, payload);

		if (error) {
			Serial.println("ERROR: POST_hum() -Deserialization error!");
			https.end();
			return;
		}

		if (reply["status"] != "OK") {
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else {
			Serial.println("INFO: Humidity - OK");
		}
	}
	else
	{
		Serial.printf("ERROR: POST_hum() - HTTPS ERROR: %d\n", httpCode);
	}

	https.end();
}

void Network::GET_schedule(std::vector<Calendar::Unit>& list) {
	StaticJsonDocument<2048> reply;

	bool status = https.begin(server_cfg.url + get_schedule_endpoint + String(hub_id));

	if(status == false) {
		Serial.println("ERROR: GET_schedule() - Couldn't start https session!");
		https.end();
		return;
	}

	int httpCode = https.GET();

	if (httpCode > 0)
	{
		String &&payload = https.getString();
		// Serial.printf("HTTP Status code: %d\n", httpCode);

		DeserializationError error = deserializeJson(reply, payload);

		if (error)
		{
			Serial.printf("ERROR: GET_schedule() - Deserialization error: %d!\n", error);
			https.end();
			return;
		}

		size_t nesting_level = reply.nesting();
		size_t reply_size = reply.size();

		if (reply_size == 0) {
			Serial.println("ERROR: GET_schedule() - reply size is 0!");
			https.end();
			return;
		}

		list.clear();

		Serial.println("INFO: GET_schedule:");

		for (uint8_t i = 0; i < reply_size; ++i) {
			JsonObject root = reply[i];
			JsonArray days = root["day"];

			Calendar::Unit unit;
			unit.id = root["id"].as<int>();
			unit.start.from_string(root["start_time"].as<std::string_view>());
			unit.stop.from_string(root["stop_time"].as<std::string_view>());

			for (auto day : days) {
				unit.days[day.as<int>()] = true;
			}

			list.emplace_back(unit);

			// Debug output
			String day_list;
			for (auto day : days)
				day_list += String(day.as<int>()) + ' ';

			Serial.printf("Print values with id %d\n"
			              "\tstart_time: %d:%d\n"
						  "\tstop_time: %d:%d\n"
						  "\tdays: %s\n\n",
						  unit.id,
						  unit.start.hour, unit.start.min,
						  unit.stop.hour, unit.stop.min,
						  day_list);
			
		}
	}
	else {
		Serial.printf("ERROR: GET_hub() - HTTPS ERROR: %d\n", httpCode);
	}

	https.end();
}

void Network::GET_hub(UserDefinedParameters& user_defined_params) {
	StaticJsonDocument<1024> reply;

	bool status = https.begin(server_cfg.url + hub_get_endpoint + String(establishment_id));

	if(status == false) {
		Serial.println("ERROR: GET_hub() - Couldn't start https session!");
		https.end();
		return;
	}

	int httpCode = https.GET();

	if (httpCode > 0)
	{
		String &&payload = https.getString();

		// Serial.printf("HTTP Status code: %d\n", httpCode);

		DeserializationError error = deserializeJson(reply, payload);

		if (error)
		{
			Serial.printf("ERROR: GET_hub() - Deserialization error: %d!\n", error);
			https.end();
			return;
		}

		int tmp_hum_max = reply["humidity_upper_limit"];
		int tmp_hum_min = reply["humidity_lower_limit"];
		bool relay_status = reply["compressor_relay_status"];

		// Checking for a fool
		if (tmp_hum_max < tmp_hum_min) {
			/** TODO: Send a log to a server */
			Serial.println("ERROR: GET_hub() - A max hum value must be higher than a min hum value!");
			https.end();
			return;
		}

		user_defined_params.hum_max = tmp_hum_max;
		user_defined_params.hum_min = tmp_hum_min;
		user_defined_params.relay_status = relay_status;

		Serial.printf("INFO: GET hub:\n\thum_max: %d\n\thum_min: %d\n\trelay_status: %d\n",
					   user_defined_params.hum_max, 
					   user_defined_params.hum_min, 
					   user_defined_params.relay_status);
	}
	else {
		Serial.printf("ERROR: GET_hub() - HTTPS ERROR: %d\n", httpCode);
	}

	https.end();
}