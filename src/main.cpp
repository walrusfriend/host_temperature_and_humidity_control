#include <algorithm>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include <HardwareSerial.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

/**
 * TODO: 
 * - Add checks for all!
*/

#define DEBUG 1

// #define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
// #define TO_PHONE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TO_SENSOR_CHARACTERISTIC_UUID "bea03c8c-2cc5-11ee-be56-0242ac120002"

// Set UUID's same as UUID on BLE module (MLT-BT05)
#define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define TO_PHONE_CHARACTERISTIC_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"

BLEServer *pServer;
BLECharacteristic *p_to_phone_characteristic;
BLECharacteristic *p_to_sensor_characteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

static constexpr uint8_t RELAY_PIN = 16;
uint8_t hum_min = 53;
uint8_t hum_max = 80;
void compare_hum();

uint8_t curr_hum_value = 0xff;
uint8_t curr_temp_value = 15;
std::string BLE_reply;
std::string BLE_reply_to_sensor;

const uint8_t HUMIDITY_SENSOR_ACCURACY = 2;

bool is_compressor_start = false;
bool is_relay_controlled_by_user = false;

struct Command {
	std::string name;
	std::function<void(const std::string&)> handler;

	Command(std::string&& command_name, std::function<void(const std::string&)> command_handler) {
		name = command_name;
		handler = command_handler;
	}
	
	~Command() {

	}
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

void parse_message(const std::string& message); 

void debug(const std::string& debug_info);

bool is_number(const std::string& s);

static const std::vector<Command> command_list = {Command("hum", hum_handler),	
												  Command("curr_hum", curr_hum_handler),	
												  Command("Error", error_handler),	
												  Command("relay", relay_handler),	
												  Command("Humidity: ", humidity_handler),	
												  Command("Temperature: ", temperature_handler)};	

class MyServerCallbacks : public BLEServerCallbacks
{
	void onConnect(BLEServer *pServer)
	{
		deviceConnected = true;
		BLEDevice::startAdvertising();
	};

	void onDisconnect(BLEServer *pServer)
	{
		deviceConnected = false;
	}
};

class MyCallbacks : public BLECharacteristicCallbacks
{
	void onWrite(BLECharacteristic *pCharacteristic)
	{
		std::string rxValue = pCharacteristic->getValue();

		if (rxValue.length() > 0)
		{
			Serial.println("*********");
			Serial.println(rxValue.c_str());
			parse_message(rxValue);
			Serial.println("*********");

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
			// Serial.printf("DEBUG: reply %s\n", BLE_reply_to_sensor.c_str());

		}
	}

	void onRead(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply);
		BLE_reply.clear();
	}
};

class MyToSensorCallbacks : public BLECharacteristicCallbacks
{
	void onRead(BLECharacteristic* pCharacteristic) {
		pCharacteristic->setValue(BLE_reply_to_sensor);
		BLE_reply_to_sensor.clear();
	}

	void onWrite(BLECharacteristic* pCharacteristic) {
		Serial.printf("%s\n", pCharacteristic->getValue().c_str());
	}
};

void setup()
{
	Serial.begin(115200);
	Serial.println("Starting BLE work!");

	BLEDevice::init("ESP-32-BLE-Server");
	pServer = BLEDevice::createServer();
	pServer->setCallbacks(new MyServerCallbacks());

	BLEService *pService = pServer->createService(SERVICE_UUID);
	p_to_phone_characteristic = pService->createCharacteristic(
		TO_PHONE_CHARACTERISTIC_UUID,
		BLECharacteristic::PROPERTY_READ |
		BLECharacteristic::PROPERTY_WRITE);
		
	// p_to_phone_characteristic->setValue("Hello World says Neil");
	p_to_phone_characteristic->setCallbacks(new MyCallbacks());
	p_to_phone_characteristic->addDescriptor(new BLE2902());

	p_to_sensor_characteristic = pService->createCharacteristic(
		TO_SENSOR_CHARACTERISTIC_UUID,
		BLECharacteristic::PROPERTY_READ |
		BLECharacteristic::PROPERTY_WRITE);

	p_to_sensor_characteristic->setCallbacks(new MyToSensorCallbacks());
	p_to_sensor_characteristic->addDescriptor(new BLE2902());

	pService->start();

	// this still is working for backward compatibility
	// BLEAdvertising *pAdvertising = pServer->getAdvertising();
	BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
	pAdvertising->addServiceUUID(SERVICE_UUID);
	// pAdvertising->setScanResponse(false);
	pAdvertising->setScanResponse(true);
	pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
	pAdvertising->setMinPreferred(0x12);
	BLEDevice::startAdvertising();
	Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop()
{
	// if (deviceConnected)
	// {
	// 	p_to_phone_characteristic->setValue((uint8_t*)test_reply, strlen(test_reply));
	// 	p_to_phone_characteristic->notify();
	// 	txValue++;
	// 	delay(10); // bluetooth stack will go into congestion, if too many packets are sent
	// }

	// disconnecting
	if (!deviceConnected && oldDeviceConnected)
	{
		delay(500);					 // give the bluetooth stack the chance to get things ready
		pServer->startAdvertising(); // restart advertising
		Serial.println("start advertising");
		oldDeviceConnected = deviceConnected;
	}	
	// connecting
	if (deviceConnected && !oldDeviceConnected)
	{
		// do stuff here on connecting
		oldDeviceConnected = deviceConnected;
	}

	/** TODO: Make a more intellegance check. For example - starts a timer and wait for it */
	// if (pServer->getConnectedCount() < 2) {
	// 	BLE_reply = "ERROR: Sensor disconnected!\n";
	// 	debug(BLE_reply);
	// }
	compare_hum();
	delay(1000);
}

void compare_hum() {
	if (is_compressor_start) {
		pinMode(RELAY_PIN, OUTPUT);
		digitalWrite(RELAY_PIN, LOW);
	}
	else {
		pinMode(RELAY_PIN, INPUT);
	}

	if (is_relay_controlled_by_user) {
		return;
	}

	if (curr_hum_value > 80) {
		// Serial.println("ALARM!!!");
		p_to_phone_characteristic->setValue("ALARM!!!");
	}

	if (curr_hum_value < hum_min) {
		is_compressor_start = true;
	}

	if (curr_hum_value > hum_max) {
		is_compressor_start = false;
	}
}

void hum_handler(const std::string& message) {
	// Try to find the 'space' sym between 'min' and 'max' values of the user input
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

/** TODO: Remake a algorythm - sensor sends a headher of the message, humidity and temperature handlers will be united to one*/
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