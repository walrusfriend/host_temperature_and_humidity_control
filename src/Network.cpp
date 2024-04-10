#include "Network.h"

#include <string>

extern hw_timer_t *status_timer;
extern hw_timer_t *sensor_timer;

Network::Network() {
}

Network::~Network() {
}

void Network::handle_disconnect() {
	/** TODO: Disable compressor if Wi-Fi disconnected */
	Serial.println("ERROR: Wi-Fi connection lost");
	do_wifi_connect = true;

	if (WiFi.disconnect()) {
		Serial.println("INFO: Disconnected successfully!");
	}
	else {
		Serial.println("INFO: Disconnection failed!");
	}
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
			return;
		}

		int tmp_hum_max = reply["humidity_upper_limit"];
		int tmp_hum_min = reply["humidity_lower_limit"];
		bool relay_status = reply["compressor_relay_status"];

		// Checking for a fool
		if (tmp_hum_max < tmp_hum_min) {
			/** TODO: Send a log to a server */
			Serial.println("ERROR: GET_hub() - A max hum value must be higher than a min hum value!");
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