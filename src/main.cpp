#include <algorithm>

#include <BLEDevice.h>
// #include <BLEUtils.h>
#include <BLE2902.h>

#include <HardwareSerial.h>
#include "RelayController.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Ping.h>
#include <time.h>

#define DEBUG 1

/**
 * TODO: 
 * - Add checks for all!
 * - Настройка частоты передачи
 * - Включить и выключить передачу
 * - Count the number of connetion tries and handle infinity connetion
 * - Rename variables
 * - Try send data and only then reconnect to BLE server
 * - Refactor code:
 * 	 - move code parts to *.h/*.cpp files
 * 	 - 
*/

static BLEUUID serial_service_uuid("0000ffe0-0000-1000-8000-00805f9b34fb");
static BLEUUID serial_characteristic_uuid("0000ffe1-0000-1000-8000-00805f9b34fb");

static BLEClient *pClient;
static BLERemoteCharacteristic *p_serial_characteristic;

BLEAddress sensor_MAC_address("3C:A3:08:0D:75:41");

bool doConnect = true;
bool connected = false;
bool doScan = false;

uint8_t txValue = 0;

uint8_t hum_min = 53;
uint8_t hum_max = 80;

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

static void notifyCallback(
	BLERemoteCharacteristic *pBLERemoteCharacteristic,
	uint8_t *pData,
	size_t length,
	bool isNotify)
{
	Serial.print("Notify callback for characteristic ");
	Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
	Serial.print(" of data length ");
	Serial.println(length);
	Serial.print("data: ");
	Serial.write(pData, length);
	Serial.println();
}

class MyClientCallback : public BLEClientCallbacks
{
	void onConnect(BLEClient *pclient)
	{
		connected = false;
	}

	void onDisconnect(BLEClient *pclient)
	{
		connected = false;
		Serial.println("onDisconnect");
	}
};

void compare_hum();

bool connectToServer()
{
	Serial.printf("Forming a connection to %s", sensor_MAC_address.toString().c_str());

	pClient = BLEDevice::createClient();
	Serial.println(" - Created client");

	pClient->setClientCallbacks(new MyClientCallback());

	if(pClient->connect(sensor_MAC_address) == false) {
		Serial.println("Couldn't connect to remote BLE server!");
		return false;
	}

	Serial.println(" - Connected to server");
	pClient->setMTU(517); // set client to request maximum MTU from server (default is 23 otherwise)

	BLERemoteService *p_serial_service = pClient->getService(serial_service_uuid);
	if (p_serial_service == nullptr)
	{
		Serial.print("Failed to find our service UUID: ");
		Serial.println(serial_service_uuid.toString().c_str());
		pClient->disconnect();
		return false;
	}
	Serial.println(" - Found serial service");

	p_serial_characteristic = p_serial_service->getCharacteristic(serial_characteristic_uuid);
	if (p_serial_service == nullptr)
	{
		Serial.print("Failed to find our characteristic UUID: ");
		Serial.println(serial_characteristic_uuid.toString().c_str());
		pClient->disconnect();
		return false;
	}
	Serial.println(" - Found service characteristic");

	// Read the value of the characteristic.
	// if (pRemoteCharacteristic->canRead())
	// {
	// 	std::string value = pRemoteCharacteristic->readValue();
	// 	Serial.print("The characteristic value was: ");
	// 	Serial.println(value.c_str());
	// }

	if (p_serial_characteristic->canNotify())
		p_serial_characteristic->registerForNotify(notifyCallback);

	connected = true;
	return true;
}

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

/** TODO: Move this code to the right place */
TaskHandle_t connect_task_handle;
TaskHandle_t input_serial_task_handle;

void task_connect(void* param) {

	for(;;) {
		// If the flag "doConnect" is true then we have scanned for and found the desired
		// BLE Server with which we wish to connect.  Now we connect to it.  Once we are
		// connected we set the connected flag to be true.
		if (doConnect == true)
		{
			Serial.println("Try to connect");
			// uint8_t number_of_unsuccessful_connections = 0;
			while (connectToServer() == false) {
				// ++number_of_unsuccessful_connections;

				// if (number_of_unsuccessful_connections > 5) {
				// 	Serial.println("ERROR: Failed to connect to the server!");
				// 	break;
				// }
				vTaskDelay(500);
			};
			doConnect = false;

			Serial.println("Connected!");
		}

		vTaskDelay(500);
	}

}

void task_input_serial(void* param) {
	for(;;) {
		// Handle COM port input data
		if (Serial.available() >= 1) {
			char sym[Serial.available()];
			Serial.read(sym, Serial.available());

			parse_message(std::string(sym));
		}

		vTaskDelay(1);
	}
}

void setup()
{
	Serial.begin(115200);

	Serial.println("Start program!");
	BLEDevice::init("DewPoint");

	// Retrieve a Scanner and set the callback we want to use to be informed when we
	// have detected a new device.  Specify that we want active scanning and start the
	// scan to run for 5 seconds.
	// BLEScan *pBLEScan = BLEDevice::getScan();
	// pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
	// pBLEScan->setInterval(1349);
	// pBLEScan->setWindow(449);
	// pBLEScan->setActiveScan(true);
	// pBLEScan->start(5, false);

	Serial.println("Starting BLE work!");

	// подключаемся к Wi-Fi сети
	// WiFi.begin(ssid, password);

	/** TODO: Написать нормальный обработчик команд */

	// while (WiFi.status() != WL_CONNECTED)
	// {
		// delay(1000);
		// Serial.println("Connecting to Wi-Fi..");
	// }

	// Serial.println("The Wi-Fi connection is established");

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

	xTaskCreate(task_connect, "task_connect", 2048, nullptr, 1, &connect_task_handle);
	xTaskCreate(task_input_serial, "task_input_serial", 2048, nullptr, 1, &input_serial_task_handle);
}

void loop()
{
	// if (is_tim) {
	// 	// Check the current connection status
	// 	if ((WiFi.status() == WL_CONNECTED))
	// 	{	
	// 		// POST_temp();
	// 		// POST_hum();
	// 	}
	// 	else {
	// 		// WiFi.reconnect();

	// 		/** TODO: Add a timeout */
	// 		// while (WiFi.status() != WL_CONNECTED) {
	// 		// 	delay(1000);
	// 		// }
	// 	}

	// 	// Serial.println("TEST");
	// 	is_tim = false;
	// }


	// If we are connected to a peer BLE Server, update the characteristic each time we are reached
	// with the current time since boot.
	// if (connected)
	// {
	// 	if (Serial.available()) {
	// 		p_serial_characteristic->writeValue(Serial.read());
	// 	}
	// }
	// else if (doScan)
	// {
	// 	Serial.println("Try to find BLE devices");
	// 	BLEDevice::getScan()->start(0); // this is just example to start scan after disconnect, most likely there is better way to do it in arduino
	// }

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
	// uint8_t header_size = strlen("ble ");
	// std::string tmp = message.substr(header_size, (message.size() - 2) - header_size);

	p_serial_characteristic->writeValue("test");
}

/** TODO: This will be work only if the message starts with a command*/
void parse_message(const std::string& message) {
	for (uint8_t i = 0; i < command_list.size(); ++i) {
		if (message.find(command_list[i].name) != std::string::npos) {
			command_list[i].handler(message);
			return;
		}
	}

	Serial.println("ERROR: Unknown command!");
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