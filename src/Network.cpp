#include "Network.h"

#include <string>

extern hw_timer_t *status_timer;
extern hw_timer_t *sensor_timer;

Network::Network() {

}

Network::~Network() {

}

bool Network::wifi_connect()
{
	WiFi.mode(WIFI_STA);
	WiFi.begin(wifi_cfg.ssid, wifi_cfg.pass);

	return WiFi.status() == WL_CONNECTED;
}

void Network::handle_disconnect() {
	Serial.println("Wi-Fi connection lost");
	timerAlarmDisable(status_timer);
	timerAlarmDisable(sensor_timer);
	do_wifi_connect = true;

	delete client;
	client = nullptr;

	WiFi.disconnect(true, true);
}

bool Network::load_settings() {

}

bool Network::save_settings() {
	
}

void Network::POST_log(const std::string_view& log_string) {
	StaticJsonDocument<128> query;

	query["log_value"] = log_string;
	query["hub_id"] = hub_id;

	String serialized_query;
	serializeJson(query, serialized_query);

	https.begin(server_cfg.url + log_endpoint);
	int httpCode = https.POST(serialized_query);

	if (httpCode > 0)
	{
		String payload = https.getString();

		StaticJsonDocument<128> reply;
		DeserializationError error = deserializeJson(reply, payload);

		if (error) {
			Serial.println("Deserialization error!");
			return;
		}

		if (reply["status"] != "OK") {
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else {
			Serial.println("Log: OK");
		}
	}
	else
	{
		Serial.println("HTTP-request error");
	}

	https.end();
	Serial.println();
}

void Network::POST_temp(const uint8_t& temperature_value) {
	StaticJsonDocument<128> query;

	query["temperature_value"] = String(temperature_value);
	query["hub_id"] = hub_id;
	query["sensor_id"] = sensor_id;

	String serialized_query;
	serializeJson(query, serialized_query);

	https.begin(server_cfg.url + temp_endpoint);
	int httpCode = https.POST(serialized_query);

	if (httpCode > 0)
	{
		String payload = https.getString();

		StaticJsonDocument<128> reply;
		DeserializationError error = deserializeJson(reply, payload);

		if (error) {
			Serial.println("Deserialization error!");
			return;
		}

		if (reply["status"] != "OK") {
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else {
			Serial.println("Temperature: OK");
		}
	}
	else
	{
		Serial.println("HTTP-request error");
	}

	https.end();
	Serial.println();
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
		Serial.println("Couldn't start GET hub https session!");
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
			Serial.println("Deserialization error!");
			return;
		}

		if (reply["status"] != "OK") {
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else {
			Serial.println("Humidity: OK");
		}
	}
	else
	{
		Serial.printf("HTTPS POST humidiy ERROR: %d\n", httpCode);
	}

	https.end();
	Serial.println();
}

void Network::GET_hub() {
	StaticJsonDocument<1024> reply;

	bool status = https.begin(server_cfg.url + hub_get_endpoint + String(establishment_id));

	if(status == false) {
		Serial.println("Couldn't start GET hub https session!");
		https.end();
		return;
	}

	int httpCode = https.GET();

	if (httpCode > 0)
	{
		String &&payload = https.getString();

		Serial.printf("HTTP Status code: %d\n", httpCode);

		DeserializationError error = deserializeJson(reply, payload);

		if (error)
		{
			Serial.printf("Deserialization error: %d!\n", error);
			return;
		}

		uint8_t hum_max_value = reply["humidity_upper_limit"];
		uint8_t hum_min_value = reply["humidity_lower_limit"];
		bool relay_status = reply["compressor_relay_status"];

		Serial.printf("GET hub:\n\thum_max: %d\n\thum_min: %d\n\trelay_status: %d\n",
					   hum_max_value, hum_min_value, relay_status);

		digitalWrite(22, relay_status);
		digitalWrite(LED_BUILTIN, relay_status);
	}
	else {
		Serial.printf("HTTPS GET hub ERROR: %d\n", httpCode);
	}

	https.end();
	Serial.println();
}