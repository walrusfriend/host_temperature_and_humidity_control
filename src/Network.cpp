#include "Network.h"

#include <string>

extern hw_timer_t *status_timer;
extern hw_timer_t *sensor_timer;

Network::Network() {
	// client = new WiFiClientSecure;
	// WiFi.setSleep(false);
	// WiFi.setTxPower(WIFI_POWER_19_5dBm);
	// WiFi.enableLongRange(true);
	// WiFi.useStaticBuffers(true);
	// WiFi.disconnect(true, true);
}

Network::~Network() {

}

void Network::handle_disconnect() {
	Serial.println("ERROR: Wi-Fi connection lost");
	// timerAlarmDisable(status_timer);
	// timerAlarmDisable(sensor_timer);
	do_wifi_connect = true;

	// delete client;
	// client = nullptr;

	if (WiFi.disconnect()) {
		Serial.println("INFO: Disconnected succsessfully!");
	}
	else {
		Serial.println("INFO: Disconnetion failed!");
	}

	// WiFi.disconnect(false, false);
	// WiFi.disconnect(true, true);
}

void Network::POST_log(const std::string_view& log_string) {
	StaticJsonDocument<128> query;

	query["log_value"] = log_string;
	query["hub_id"] = hub_id;

	String serialized_query;
	serializeJson(query, serialized_query);

	bool status = https.begin(server_cfg.url + log_endpoint);

	if(status == false) {
		Serial.println("ERROR: Couldn't start POST log https session!");
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
			Serial.println("ERROR: Deserialization error!");
			return;
		}

		if (reply["status"] != "OK") {
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else {
			Serial.println("INFO: Log: OK");
		}
	}
	else
	{
		Serial.println("ERROR: HTTP-request error");
	}

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
		Serial.println("ERROR: Couldn't start POST temp https session!");
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
			Serial.println("ERROR: Deserialization error!");
			return;
		}

		if (reply["status"] != "OK") {
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else {
			Serial.println("INFO: Temperature: OK");
		}
	}
	else
	{
		Serial.printf("ERROR: HTTPS POST temperature ERROR: %d\n", httpCode);
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
		Serial.println("ERROR: Couldn't start GET hub https session!");
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
			Serial.println("ERROR: Deserialization error!");
			return;
		}

		if (reply["status"] != "OK") {
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else {
			Serial.println("INFO: Humidity: OK");
		}
	}
	else
	{
		Serial.printf("ERROR: HTTPS POST humidiy ERROR: %d\n", httpCode);
	}

	https.end();
}

void Network::GET_hub(UserDefinedParameters& user_defined_params) {
	StaticJsonDocument<1024> reply;

	bool status = https.begin(server_cfg.url + hub_get_endpoint + String(establishment_id));

	if(status == false) {
		Serial.println("ERROR: Couldn't start GET hub https session!");
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
			Serial.printf("ERROR: Deserialization error: %d!\n", error);
			return;
		}

		int tmp_hum_max = reply["humidity_upper_limit"];
		int tmp_hum_min = reply["humidity_lower_limit"];
		user_defined_params.relay_status = reply["compressor_relay_status"];

		// Checking for a fool
		if (tmp_hum_max < tmp_hum_min) {
			/** TODO: Send a log to a server */
			Serial.println("ERROR: A max hum value must be higher than a min hum value!");
			return;
		}

		user_defined_params.hum_max = tmp_hum_max;
		user_defined_params.hum_min = tmp_hum_min;

		Serial.printf("INFO: GET hub:\n\thum_max: %d\n\thum_min: %d\n\trelay_status: %d\n",
					   user_defined_params.hum_max, 
					   user_defined_params.hum_min, 
					   user_defined_params.relay_status);
	}
	else {
		Serial.printf("ERROR: HTTPS GET hub ERROR: %d\n", httpCode);
	}

	https.end();
}