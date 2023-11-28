#include "Network.h"

#include <string>

Network::Network() {

}

Network::~Network() {

}

bool Network::wifi_connect()
{
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);

	uint8_t wifi_connection_tries = 0;
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.printf(".");

		if (wifi_connection_tries >= MAX_WIFI_CONNECTION_TRIES) {
			Serial.println("ERROR: Couldn't connect to Wi-Fi network!"
							"Please restart the device or set other Wi-Fi SSID and password!");

			is_wifi_settings_initialized = false;
			return false;
		}

		delay(500);
	}

	WiFiClientSecure *client = new WiFiClientSecure;

	if (client)
	{
		// set secure client without certificate
		client->setInsecure();
	}
	else
	{
		Serial.printf("ERROR: [HTTPS] Unable to connect\n");
		return false;
	}

	Serial.println("Connected");
	is_wifi_connection_establish = true;
	return true;
}

bool Network::check_wifi_parameters() {
	// Check if Wi-Fi parameters are initialized
	if (ssid == blank_ssid && password == blank_pass) {
		Serial.println("ERROR: Wi-Fi settings are not initialized!\n"
					   "Please use set_wifi command to setup Wi-Fi connection.");
		is_wifi_settings_initialized = false;
		return false;
	}

	is_wifi_settings_initialized = true;
	return true;
}

bool Network::check_wifi_connection() {
	return (WiFi.status() == WL_CONNECTED) ? true : false;
}

void Network::POST_log(const std::string_view& log_string) {
	StaticJsonDocument<128> query;

	query["log_value"] = log_string;
	query["hub_id"] = hub_id;

	String serialized_query;
	serializeJson(query, serialized_query);

	Serial.println(serialized_query);

	https.begin(url + log_endpoint);
	int httpCode = https.POST(serialized_query);

	if (httpCode > 0)
	{
		String payload = https.getString();
		Serial.println(httpCode);
		Serial.println(payload);

		// StaticJsonDocument<128> reply;
		// DeserializationError error = deserializeJson(reply, payload);

		// if (error) {
		// 	Serial.println("Deserialization error!");
		// 	return;
		// }

		// if (reply["status"] != "OK") {
		// 	Serial.println(httpCode);
		// 	Serial.println(payload);
		// }
		// else {
		// 	Serial.println("OK");
		// }
	}
	else
	{
		Serial.println("HTTP-request error");
	}

	https.end();
}

void Network::POST_temp(const uint8_t& temperature_value) {
	StaticJsonDocument<128> query;

	query["temperature_value"] = temperature_value;
	query["hub_id"] = hub_id;
	query["sensor_id"] = sensor_id;

	String serialized_query;
	serializeJson(query, serialized_query);

	Serial.println(serialized_query);

	https.begin(url + temp_endpoint);
	int httpCode = https.POST(serialized_query);

	if (httpCode > 0)
	{
		String payload = https.getString();

		Serial.println(httpCode);
		Serial.println(payload);

		// StaticJsonDocument<128> reply;
		// DeserializationError error = deserializeJson(reply, payload);

		// if (error) {
		// 	Serial.println("Deserialization error!");
		// 	return;
		// }

		// if (reply["status"] != "OK") {
		// 	Serial.println(httpCode);
		// 	Serial.println(payload);
		// }
		// else {
		// 	Serial.println("OK");
		// }
	}
	else
	{
		Serial.println("HTTP-request error");
	}

	https.end();
	Serial.println();
	Serial.println();
}

void Network::POST_hum(const uint8_t& humidity_value) {
	StaticJsonDocument<128> query;

	// query["humidity_value"] = String(random(20, 81));
	query["humidity_value"] = String(humidity_value);
	query["hub_id"] = hub_id;
	query["sensor_id"] = sensor_id;

	String serialized_query;
	serializeJson(query, serialized_query);

	Serial.println(serialized_query);

	bool status = https.begin(url + hum_endpoint);

	if(status == false) {
		Serial.println("Couldn't start GET hub https session!");
		https.end();
		return;
	}

	int httpCode = https.POST(serialized_query);

	if (httpCode > 0)
	{
		String payload = https.getString();

		Serial.println(httpCode);
		Serial.println(payload);

		// StaticJsonDocument<128> reply;
		// DeserializationError error = deserializeJson(reply, payload);

		// if (error) {
		// 	Serial.println("Deserialization error!");
		// 	return;
		// }

		// if (reply["status"] != "OK") {
		// 	Serial.println(httpCode);
		// 	Serial.println(payload);
		// }
		// else {
		// 	Serial.println("OK");
		// }
	}
	else
	{
		Serial.printf("HTTPS POST humidiy ERROR: %d\n", httpCode);
	}

	https.end();
	Serial.println();
	Serial.println();
}

void Network::GET_hub() {
	StaticJsonDocument<1024> reply;

	bool status = https.begin(url + hub_get_endpoint + String(establishment_id));

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
		Serial.println(payload);

		DeserializationError error = deserializeJson(reply, payload);

		if (error)
		{
			Serial.printf("Deserialization error: %d!\n", error);
			return;
		}

		bool relay_status = reply["compressor_relay_status"];

		Serial.println(relay_status);

		digitalWrite(22, relay_status);
		digitalWrite(LED_BUILTIN, relay_status);
	}
	else {
		Serial.printf("HTTPS GET hub ERROR: %d\n", httpCode);
	}

	https.end();

	Serial.println();
	Serial.println();
}