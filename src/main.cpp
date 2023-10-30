#include <algorithm>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include <HardwareSerial.h>
#include "RelayController.h"


#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Ping.h>
#include <time.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
/**
 * TODO: Create standart UUID for all data between sensor and host and
 * host and phone (if this feature will be available)
*/

/**
 * TODO: 
 * - Add checks for all!
 * - Настройка частоты передачи
 * - Включить и выключить передачу
*/

#define DEBUG 1

// #define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b" 
// #define TO_PHONE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
// #define TO_SENSOR_CHARACTERISTIC_UUID "bea03c8c-2cc5-11ee-be56-0242ac120002"

#define ENV_SERVICE_UUID "181A"
#define TEMP_CHAR_UUID "2A6E"
#define HUM_CHAR_UUID "2A6F"

#define BATTERY_SERVICE_UUID "180F"
#define BATTERY_CHAR_UUID "2A19"

#define SERIAL_SERVICE_UUID "FFF0"
#define SERIAL_CHAR_UUID "FFF1"

// Set UUID's same as UUID on BLE module (MLT-BT05)
// #define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
// #define TO_PHONE_CHARACTERISTIC_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"

BLEServer *p_server;
BLECharacteristic *p_hum_characteristic;
BLECharacteristic *p_temp_characteristic;
BLECharacteristic *p_battery_characteristic;
BLECharacteristic *p_serial_characteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

uint8_t hum_min = 53;
uint8_t hum_max = 80;
void compare_hum();

uint8_t curr_hum_value = 0xff;
uint8_t curr_temp_value = 15;
uint8_t curr_battery_value = 100;

std::string BLE_reply;
std::string BLE_reply_to_sensor;

const uint8_t HUMIDITY_SENSOR_ACCURACY = 2;

bool is_compressor_start = false;
bool is_relay_controlled_by_user = false;


/** TODO: Move this to RelayController */
bool inflow_low_state = false;
bool inflow_high_state = false;
bool exhaust_low_state = false;
bool exhaust_high_state = false;

struct Command {
	std::string name;
	std::function<void(const std::string&)> handler;

	Command(std::string&& command_name, std::function<void(const std::string&)> command_handler) {
		name = command_name;
		handler = command_handler;
	}
	
	~Command() {}
};

/**
 * TODO: Split commands by user and sensor or something else
*/
// Command handler function list
void hum_handler(const std::string& message);
void curr_hum_handler(const std::string& message);
void error_handler(const std::string& message);
void relay_handler(const std::string& message);
void humidity_handler(const std::string& message);
void temperature_handler(const std::string& message);
void inflow_low_handler(const std::string& message);
void inflow_high_handler(const std::string& message);
void exhaust_low_handler(const std::string& message);
void exhaust_high_handler(const std::string& message);
void get_handler(const std::string& message);
void post_handler(const std::string& message);
void rand_handler(const std::string& message);
void start_handler(const std::string& message);
void stop_handler(const std::string& message);
void period_handler(const std::string& message);
void ble_handler(const std::string& message);

void parse_message(const std::string& message); 

void debug(const std::string& debug_info);

bool is_number(const std::string& s);

static const std::vector<Command> command_list = {
												  Command("curr_hum", curr_hum_handler),	
												  Command("Error", error_handler),	
												  Command("relay", relay_handler),	
												  Command("Humidity: ", humidity_handler),	
												  Command("Temperature: ", temperature_handler),
												  Command("hum", hum_handler),
												  Command("inflow_low", inflow_low_handler),
												  Command("inflow_high", inflow_high_handler),
												  Command("exhaust_low", exhaust_low_handler),
												  Command("exhaust_high", exhaust_high_handler),
												  Command("get", get_handler),
												  Command("post", post_handler),
												  Command("rand", rand_handler),
												  Command("start", start_handler),
												  Command("stop", stop_handler),
												  Command("period", period_handler),
												  Command("ble", ble_handler)
												  };	

class MyServerCallbacks : public BLEServerCallbacks
{
	void onConnect(BLEServer *pServer)
	{
		deviceConnected = true;

		Serial.println("DEBUG: New device connected");

		BLEDevice::startAdvertising();
	};

	void onDisconnect(BLEServer *pServer)
	{
		deviceConnected = false;
	}
};

class HumidityCharacteristicCallbacks : public BLECharacteristicCallbacks
{
	void onWrite(BLECharacteristic *pCharacteristic)
	{
		std::string rxValue = pCharacteristic->getValue();

		if (rxValue.length() > 0)
		{
			Serial.println(rxValue.c_str());
			// parse_message(rxValue);
			humidity_handler(rxValue);

			// Calculate border values
			uint8_t step_value = (hum_max - hum_min) * 0.2;

			if (step_value < HUMIDITY_SENSOR_ACCURACY) 
				step_value = HUMIDITY_SENSOR_ACCURACY;
			// Serial.printf("DEBUG: step_value: %d\n", step_value);

			if ((curr_hum_value < hum_min + step_value) or (curr_hum_value > hum_max - step_value)) {
				BLE_reply_to_sensor = "S5\n";
			}
			else {
				BLE_reply_to_sensor = "S30\n";
			}
			// Serial.printf("DEBUG: reply %s\n",
			// BLE_reply_to_sensor.c_str());

		}
	}

	void onRead(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply);
		Serial.println("Read callback was called");
		BLE_reply.clear();
	}

	void onNotify(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply);
		Serial.println("Notify callback was called");
		BLE_reply.clear();
	}

	void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
		Serial.print("On status called with status: ");
		Serial.println(std::to_string(s).c_str());
	}
};

class TemperatureCharacteristicCallbacks : public BLECharacteristicCallbacks
{
	void onWrite(BLECharacteristic *pCharacteristic)
	{
		std::string rxValue = pCharacteristic->getValue();

		if (rxValue.length() > 0)
		{
			Serial.println(rxValue.c_str());
			parse_message(rxValue);

			// Calculate border values
			uint8_t step_value = (hum_max - hum_min) * 0.2;

			if (step_value < HUMIDITY_SENSOR_ACCURACY) 
				step_value = HUMIDITY_SENSOR_ACCURACY;
			// Serial.printf("DEBUG: step_value: %d\n", step_value);

			if ((curr_hum_value < hum_min + step_value) or (curr_hum_value > hum_max - step_value)) {
				BLE_reply_to_sensor = "S5\n";
			}
			else {
				BLE_reply_to_sensor = "S30\n";
			}
			// Serial.printf("DEBUG: reply %s\n",
			// BLE_reply_to_sensor.c_str());

		}
	}

	void onRead(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply);
		Serial.println("Read callback was called");
		BLE_reply.clear();
	}

	void onNotify(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply);
		Serial.println("Notify callback was called");
		BLE_reply.clear();
	}

	void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
		Serial.print("On status called with status: ");
		Serial.println(std::to_string(s).c_str());
	}
};

class BatteryCharacteristicCallbacks : public BLECharacteristicCallbacks
{
	void onWrite(BLECharacteristic *pCharacteristic)
	{
		std::string&& rx_value = pCharacteristic->getValue();

		if (rx_value.length() > 0)
		{
			Serial.println(rx_value.c_str());

			if (is_number(rx_value)) {
				curr_battery_value = std::stoi(rx_value);

				/** TODO: Do need to add an actual data check? */
			}

		}
	}

	void onRead(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply);
		Serial.println("Read callback was called");
		BLE_reply.clear();
	}

	void onNotify(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply);
		Serial.println("Notify callback was called");
		BLE_reply.clear();
	}

	void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
		Serial.print("On status called with status: ");
		Serial.println(std::to_string(s).c_str());
	}
};

class SerialCharacteristicCallbacks : public BLECharacteristicCallbacks
{
	void onRead(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply);
		Serial.println("Read callback was called");
		BLE_reply.clear();
	}

	void onWrite(BLECharacteristic* pCharacteristic) {
		std::string rxValue = pCharacteristic->getValue();

		if (rxValue.length() > 0)
		{
			Serial.println(rxValue.c_str());
		}

		Serial.println("Write callback was called");
	}

	void onNotify(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply);
		Serial.println("Notify callback was called");
		BLE_reply.clear();
	}

	void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
		Serial.print("On status called with status: ");
		Serial.println(std::to_string(s).c_str());
	}
};


/*
    Wi-Fi part
*/
//====================================================================
// Вводим имя и пароль точки доступа
const char *ssid = "AKADO-9E4C";
const char *password = "90704507";

const String url = "http://84.201.156.30:8000/";

HTTPClient http;
//====================================================================

hw_timer_t *tim1;
hw_timer_t* tim2;

bool is_tim = false;

void IRAM_ATTR onTimer() {
	is_tim = true;
}

uint8_t prescaler = 10;
void IRAM_ATTR onLongTimer() {
	if (prescaler > 1) {
		--prescaler;
	}

	timerAlarmDisable(tim1);
	timerAlarmWrite(tim1, prescaler * 1000 - 1, true);
	timerAlarmEnable(tim1);
}

// Endpoints
String temp_endpoint("hub/temperature");
String hum_endpoint("hub/humidity");
String log_endpoint("hub/log");

// REST API functions
void POST_log();
void POST_temp();
void POST_hum();

constexpr uint8_t hub_id = 22;
constexpr uint8_t sensor_id = hub_id;

void setup()
{
	Serial.begin(115200);
	Serial.println("Starting BLE work!");

	BLEDevice::init("DewPoint_host");
	p_server = BLEDevice::createServer();
	p_server->setCallbacks(new MyServerCallbacks());

	BLEService *p_env_service = p_server->createService(ENV_SERVICE_UUID);

	p_hum_characteristic = p_env_service->createCharacteristic(
		HUM_CHAR_UUID,
		BLECharacteristic::PROPERTY_WRITE |
		BLECharacteristic::PROPERTY_NOTIFY
	);

	p_hum_characteristic->setCallbacks(new HumidityCharacteristicCallbacks());
	p_hum_characteristic->addDescriptor(new BLE2902());
	
	p_temp_characteristic = p_env_service->createCharacteristic(
		TEMP_CHAR_UUID,
		BLECharacteristic::PROPERTY_WRITE |
		BLECharacteristic::PROPERTY_NOTIFY
	);

	p_temp_characteristic->setCallbacks(new TemperatureCharacteristicCallbacks());
	p_temp_characteristic->addDescriptor(new BLE2902());


	BLEService *p_battery_service = p_server->createService(BATTERY_SERVICE_UUID);

	p_battery_characteristic = p_battery_service->createCharacteristic(
		BATTERY_CHAR_UUID,
		BLECharacteristic::PROPERTY_WRITE |
		BLECharacteristic::PROPERTY_NOTIFY
	);

	p_battery_characteristic->setCallbacks(new BatteryCharacteristicCallbacks());
	p_battery_characteristic->addDescriptor(new BLE2902());


	BLEService *p_serial_service = p_server->createService(SERIAL_SERVICE_UUID);

	p_serial_characteristic = p_serial_service->createCharacteristic(
		SERIAL_CHAR_UUID,
		BLECharacteristic::PROPERTY_READ |
		BLECharacteristic::PROPERTY_WRITE |
		BLECharacteristic::PROPERTY_NOTIFY
	);

	p_serial_characteristic->setCallbacks(new SerialCharacteristicCallbacks());
	p_serial_characteristic->addDescriptor(new BLE2902());

	p_env_service->start();
	p_battery_service->start();
	p_serial_service->start();

	// this still is working for backward compatibility BLEAdvertising
	// *pAdvertising = pServer->getAdvertising();
	BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
	// pAdvertising->addServiceUUID(SERVICE_UUID);
	pAdvertising->addServiceUUID(ENV_SERVICE_UUID);
	pAdvertising->setScanResponse(true);
	pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
	pAdvertising->setMinPreferred(0x12);
	BLEDevice::startAdvertising();
	Serial.println("Characteristic defined! Now you can read it in your phone!");


	// подключаемся к Wi-Fi сети
	WiFi.begin(ssid, password);

	/** TODO: Написать нормальный обработчик команд */

	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		Serial.println("Connecting to Wi-Fi..");
	}

	Serial.println("The Wi-Fi connection is established");

	/** TOOD: Create defines to timer period values and rename timers */
	// Set timer to 30 mins
	// tim2 = timerBegin(1, 8000 - 1, true);
	// timerAttachInterrupt(tim2, &onLongTimer, true);
	// timerAlarmWrite(tim2, 18000000 - 1, true);
	// timerAlarmEnable(tim2);

	// tim1 = timerBegin(0, 8000 - 1, true);
	// timerAttachInterrupt(tim1, &onTimer, true);
	// timerAlarmWrite(tim1, 6000000 - 1, true);
	// timerAlarmEnable(tim1);
	// tim1 = timerBegin(0, 8000 - 1, true);
	// timerAttachInterrupt(tim1, &onTimer, true);
	// timerAlarmWrite(tim1, 10000 - 1, true);
	// timerAlarmEnable(tim1);

	// Send data on start to locate that the MCU is started normally
	// POST_hum();
	// delay(100);
	// POST_temp();
}

void loop()
{
	// if (deviceConnected)
	// {
	//  p_to_phone_characteristic->setValue((uint8_t*)test_reply,
	//  strlen(test_reply)); p_to_phone_characteristic->notify();
	//  txValue++; delay(10); // bluetooth stack will go into
	//  congestion, if too many packets are sent
	// }

	/**
	 * TODO: Disable BLE part to test host-to-server bridge
	*/
	// disconnecting
	if (!deviceConnected && oldDeviceConnected)
	{
		delay(500);					 // give the bluetooth stack the chance to get things ready
		p_server->startAdvertising(); // restart advertising
		Serial.println("start advertising");
		oldDeviceConnected = deviceConnected;
	}	
	// connecting
	if (deviceConnected && !oldDeviceConnected)
	{
		// do stuff here on connecting
		oldDeviceConnected = deviceConnected;
	}

	/** TODO: Make a more intellegance check. For example - starts a
	 * timer and wait for it */
	// if (pServer->getConnectedCount() < 2) { BLE_reply = "ERROR:
	//  Sensor disconnected!\n"; debug(BLE_reply);
	// }
	// compare_hum();
	// delay(1000);

	if (Serial.available() >= 1) {
		char sym[Serial.available()];
		Serial.read(sym, Serial.available());

		parse_message(std::string(sym));
	}

	if (is_tim) {
		// Check the current connection status
		if ((WiFi.status() == WL_CONNECTED))
		{	
			// POST_temp();
			// POST_hum();
		}
		else {
			// WiFi.reconnect();

			/** TODO: Add a timeout */
			// while (WiFi.status() != WL_CONNECTED) {
			// 	delay(1000);
			// }
		}

		// Serial.println("TEST");
		is_tim = false;
	}

}

void compare_hum() {
	if (is_compressor_start) {
		// pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, LOW);
		RelayController::on(RelayController::COMPRESSOR_RELAY);
	}
	else {
		// pinMode(RELAY_PIN, INPUT);
		RelayController::off(RelayController::COMPRESSOR_RELAY);
	}

	if (is_relay_controlled_by_user) {
		return;
	}

	if (curr_hum_value > 80) {
		// Serial.println("ALARM!!!");
		// p_to_phone_characteristic->setValue("ALARM!!!");
	}

	if (curr_hum_value < hum_min) {
		is_compressor_start = true;
	}

	if (curr_hum_value > hum_max) {
		is_compressor_start = false;
	}
}

void hum_handler(const std::string& message) {
	// Try to find the 'space' sym between 'min' and 'max' values of the
	// user input
	auto space_pos = message.find_last_of(' ');
	if (space_pos == std::string::npos or space_pos == 3 /* Exclude the first 'space' sym */) {
		BLE_reply = "ERROR: Unknown command format!\n"
					"Command must looks like 'hum xx xx' where xx - a positive number from 0 to 100!\n";
		debug(BLE_reply);
		return;
	}

	// Split min and max values
	std::string&& tmp_hum_min_str = message.substr(strlen("hum "), space_pos - strlen("hum "));
	std::string&& tmp_hum_max_str = message.substr(space_pos + 1, message.size() - space_pos);

	if (tmp_hum_max_str[0] == '-' or tmp_hum_min_str[0] == '-') {
		BLE_reply = "ERROR: The humidity value borders must be a positive number!\n";
		debug(BLE_reply);
		return;
	}

	uint16_t tmp_min_hum_value = 0;
	uint16_t tmp_max_hum_value = 0;

	if (is_number(tmp_hum_min_str)) {
		tmp_min_hum_value = std::stoi(tmp_hum_min_str);
	}
	else {
		BLE_reply = "ERROR: The minimum humidity value is not a number!\n";
		debug(BLE_reply);
		return;
	}

	if (is_number(tmp_hum_max_str)) {
		tmp_max_hum_value = std::stoi(tmp_hum_max_str);
	}
	else {
		BLE_reply = "ERROR: The maximum humidity value is not a number!\n";
		debug(BLE_reply);
		return;
	}

	if (tmp_min_hum_value > tmp_max_hum_value) {
		BLE_reply = "ERROR: The lower border must be less than the higher border!\n";
		debug(BLE_reply);
		return;
	}
	else if (tmp_max_hum_value > 100) {
		BLE_reply = "ERROR: The maximum humidity value must be less or equal than 100!\n";
		debug(BLE_reply);
		return;
	}

	hum_min = tmp_min_hum_value;
	hum_max = tmp_max_hum_value;

	BLE_reply = "New humidity border values is " + std::to_string(hum_min) + 
					" and " + std::to_string(hum_max) + '\n';
	Serial.print(BLE_reply.c_str());
}

void curr_hum_handler(const std::string& message) {
	if (curr_hum_value == 0xFF)
		BLE_reply = "ERROR: No humidity value from sensor!\n";
	else
		BLE_reply = std::to_string(curr_hum_value) + '\n';

	Serial.print(BLE_reply.c_str());
}

void error_handler(const std::string& message) {
	BLE_reply = std::move(message);
	Serial.print(BLE_reply.c_str());
}

void relay_handler(const std::string& message) {
	std::string tmp = message.substr(6, message.size() - 6);

	Serial.printf("%s\n", tmp.c_str());

	if (tmp == "on") {
		is_compressor_start = true;
		is_relay_controlled_by_user = true;
		BLE_reply = "Set the relay status to ON\n";
	}
	else if (tmp == "off") {
		is_compressor_start = false;
		is_relay_controlled_by_user = true;
		BLE_reply = "Set the relay status to OFF\n";
	}
	else if (tmp == "auto") {
		is_relay_controlled_by_user = false;
		BLE_reply = "The relay change its state automatically!\n";
	}
	else {
		BLE_reply = "ERROR: Unknown argument!\n";
	}
	
	Serial.print(BLE_reply.c_str());
}

/** TODO: Remake a algorythm - sensor sends a headher of the message,
 * humidity and temperature handlers will be united to one*/
void humidity_handler(const std::string& message) {
	/** TODO: Now algorythm support only two-digit numbers, fix it */
	// std::string&& tmp_str = message.substr(strlen("Humidity: "), 2);
	std::string&& tmp_str = message.substr(strlen("Humidity: "), 2);
	if (is_number(tmp_str)) {
		curr_hum_value = std::stoi(tmp_str);
		BLE_reply = message;
		debug(BLE_reply);
	}
	else {
		BLE_reply = "ERROR: Check the raw input from the sensor:\n" + message + '\n' + tmp_str;
		debug(BLE_reply);
		return;
	}
}

void temperature_handler(const std::string& message) {
	/** TODO: Now algorythm support only two-digit numbers, fix it */
	std::string&& tmp_str = message.substr(strlen("Temperature: "), 2);
	if (is_number(tmp_str)) {
		curr_temp_value = std::stoi(tmp_str);
	}
	else {
		BLE_reply = "ERROR: Check the raw input from the sensor:\n" + message;
		debug(BLE_reply);
		return;
	}
}

void inflow_low_handler(const std::string& message) {
	uint8_t header_size = strlen("inflow_low ");
	std::string tmp = message.substr(header_size, message.size() - header_size);

	debug(tmp);

	if (tmp == "on") {
		if (inflow_high_state == true) {
			RelayController::off(RelayController::INFLOW_RELAY_HIGH);
			inflow_high_state = false;
		}

		RelayController::on(RelayController::INFLOW_RELAY_LOW);

		inflow_low_state = true;

		BLE_reply = "Inflow low changed status to ON\n";
		debug(BLE_reply);
	}
	else if (tmp == "off") {
		RelayController::off(RelayController::INFLOW_RELAY_LOW);

		inflow_high_state = false;

		BLE_reply = "Inflow low changed status to OFF\n";
		debug(BLE_reply);
	}
	else {
		BLE_reply = "ERROR: Invalid argument!\n";
		debug(BLE_reply);
	}
}

void inflow_high_handler(const std::string& message) {
	uint8_t header_size = strlen("inflow_high ");
	std::string tmp = message.substr(header_size, message.size() - header_size);

	if (tmp == "on") {
		if (inflow_low_state == true) {
			RelayController::off(RelayController::INFLOW_RELAY_LOW);
			inflow_low_state = false;
		}

		RelayController::on(RelayController::INFLOW_RELAY_HIGH);

		inflow_high_state = true;

		BLE_reply = "Inflow high changed status to ON\n";
		debug(BLE_reply);
	}
	else if (tmp == "off") {
		RelayController::off(RelayController::INFLOW_RELAY_HIGH);

		inflow_high_state = false;

		BLE_reply = "Inflow high changed status to OFF\n";
		debug(BLE_reply);
	}
	else {
		BLE_reply = "ERROR: Invalid argument!\n";
		debug(BLE_reply);
	}
}

void exhaust_low_handler(const std::string& message) {
	uint8_t header_size = strlen("exhaust_low ");
	std::string tmp = message.substr(header_size, message.size() - header_size);

	if (tmp == "on") {
		if (exhaust_high_state == true) {
			RelayController::off(RelayController::EXHAUST_RELAY_HIGH);
			exhaust_high_state = false;
		}

		RelayController::on(RelayController::EXHAUST_RELAY_LOW);

		exhaust_low_state = true;

		BLE_reply = "Exhaust low changed status to ON\n";
		debug(BLE_reply);
	}
	else if (tmp == "off") {
		RelayController::off(RelayController::EXHAUST_RELAY_LOW);

		exhaust_low_state = false;

		BLE_reply = "Exhaust low changed status to OFF\n";
		debug(BLE_reply);
	}
	else {
		BLE_reply = "ERROR: Invalid argument!\n";
		debug(BLE_reply);
	}
}

void exhaust_high_handler(const std::string& message) {
	uint8_t header_size = strlen("exhaust_high ");
	std::string tmp = message.substr(header_size, message.size() - header_size);

	if (tmp == "on") {
		if (exhaust_low_state == true) {
			RelayController::off(RelayController::EXHAUST_RELAY_LOW);
			exhaust_low_state = false;
		}

		RelayController::on(RelayController::EXHAUST_RELAY_HIGH);

		exhaust_high_state = true;

		BLE_reply = "Exhaust high changed status to ON\n";
		debug(BLE_reply);
	}
	else if (tmp == "off") {
		RelayController::off(RelayController::EXHAUST_RELAY_HIGH);

		exhaust_high_state = false;
		
		BLE_reply = "Exhaust high changed status to OFF\n";
		debug(BLE_reply);
	}
	else {
		BLE_reply = "ERROR: Invalid argument!\n";
		debug(BLE_reply);
	}
}

void get_handler(const std::string& message) {
	Serial.print("ECHO: ");
	Serial.println(message.c_str());

	// std::string args;
	// uint8_t header_size = strlen("get ");
	// if (header_size < message.size()) {
	// 	args = message.substr(header_size, message.size() - header_size);
	// }

	String endpoint;

	// Check the current connection status
	if ((WiFi.status() == WL_CONNECTED))
	{ 
		HTTPClient http;

		http.begin(url);
		int httpCode = http.GET(); // Делаем запрос
		
		if (httpCode > 0)
		{
			String payload = http.getString();
			Serial.println(httpCode);
			Serial.println(payload);
		}
		else
		{
			Serial.println("HTTP-request error");
		}

		http.end();
	}
}

void post_handler(const std::string& message) {
	Serial.print("ECHO: ");
	Serial.println(message.c_str());

	std::string args;
	uint8_t header_size = strlen("post ");
	if (header_size < message.size()) {
		args = message.substr(header_size, message.size() - header_size);
	}

	if (args == "log") {
		POST_log();
	}
	else if (args == "temp") {
		POST_temp();
	}
	else if (args == "hum") {
		POST_hum();
	}
	else {
		Serial.println("Unkwonw argument!");
	}

}

void rand_handler(const std::string& message) {
	Serial.println(random(1, 5));
}

void start_handler(const std::string& message) {
	timerAlarmEnable(tim1);
}

void stop_handler(const std::string& message) {
	timerAlarmDisable(tim1);
}

void period_handler(const std::string& message) {
	uint8_t header_size = strlen("period ");
	std::string tmp = message.substr(header_size, (message.size() - 2) - header_size);

	Serial.println(tmp.c_str());

	if (is_number(tmp)) {
		uint32_t timer_value_ms = std::stoi(tmp);
		timerAlarmWrite(tim1, timer_value_ms * 10 - 1, true);
		Serial.printf("Set timer period to %d ms\n", timer_value_ms);
	}
	else {
		Serial.println("ERROR: Argument must be a number!");
	}

	/** TODO: Calculate timer parameters */
}

void ble_handler(const std::string& message) {
	uint8_t header_size = strlen("ble ");
	std::string tmp = message.substr(header_size, (message.size() - 2) - header_size);

	p_serial_characteristic->setValue(tmp);
	
	if (tmp[0] == 'n') {
		p_serial_characteristic->notify();
	}

}

/** TODO: This will be work only if the message starts with a command*/
void parse_message(const std::string& message) {
	for (uint8_t i = 0; i < command_list.size(); ++i) {
		if (message.find(command_list[i].name) != std::string::npos) {
			command_list[i].handler(message);
			return;
		}
	}
}

#if DEBUG == 1
void debug(const std::string& debug_info) {
	Serial.printf("DEBUG: %s\n", debug_info.c_str());
}
#else
void debug(const std::string& debug_info) {}
#endif

bool is_number(const std::string& s)
{
    return !s.empty() && std::find_if(s.begin(), 
        s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
}

void POST_log() {
	StaticJsonDocument<128> query;

	query["log_value"] = "Just send some value to test Arduino JSON library and Arduino HTTP";
	query["hub_id"] = hub_id; 

	String serialized_query;
	serializeJson(query, serialized_query);

	Serial.println(serialized_query);

	http.begin(url + log_endpoint);
	int httpCode = http.POST(serialized_query);

	if (httpCode > 0) {
		String payload = http.getString();
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
	else {
		Serial.println("HTTP-request error");
	}

	http.end();
}

void POST_temp() {
	StaticJsonDocument<128> query;

	query["temperature_value"] = String(random(-2000, 2100) / 100.);
	query["hub_id"] = hub_id;
	query["sensor_id"] = sensor_id; 

	String serialized_query;
	serializeJson(query, serialized_query);

	Serial.println(serialized_query);

	http.begin(url + temp_endpoint);
	int httpCode = http.POST(serialized_query);

	if (httpCode > 0) {
		String payload = http.getString();
		
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
	else {
		Serial.println("HTTP-request error");
	}

	http.end();
}

void POST_hum() {
	StaticJsonDocument<128> query;

	query["humidity_value"] = String(random(20, 81));
	query["hub_id"] = hub_id;
	query["sensor_id"] = sensor_id; 

	String serialized_query;
	serializeJson(query, serialized_query);

	Serial.println(serialized_query);

	http.begin(url + hum_endpoint);
	int httpCode = http.POST(serialized_query);

	if (httpCode > 0) {
		String payload = http.getString();

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
	else {
		Serial.println("HTTP-request error");
	}

	http.end();	
}