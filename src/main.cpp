// #include <regex>
// #include <functional>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include <HardwareSerial.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

/**
 * TODO: 
 * - Messages from phone dublicates for every 'read' operation
 * - Add check for all!
 * - Replace delay in loop func to interrupt handler behaviour
*/

#define DEBUG 1

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define TO_PHONE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define TO_SENSOR_CHARACTERISTIC_UUID "bea03c8c-2cc5-11ee-be56-0242ac120002"

BLEServer *pServer;
BLECharacteristic *p_to_phone_characteristic;
BLECharacteristic *p_to_sensor_characteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;

static constexpr uint8_t RELAY_PIN = 16;
uint8_t hum_min = 10;
uint8_t hum_max = 70;
void compare_hum();

uint8_t curr_hum_value = 72;
uint8_t curr_temp_value = 15;
std::string BLE_reply;
std::string BLE_reply_to_sensor;

const uint8_t HUMIDITY_SENSOR_ACCURACY = 2;

bool is_compressor_start = false;
bool is_relay_controlled_by_user = false;

struct Command {
	std::string name;
	std::function<void(const std::string&)> handler;

	Command(std::string&& command_name, void(*command_handler)(const std::string& message)) {
		name = command_name;
		handler = command_handler;
	}
	
	~Command() {

	}
};

std::vector<Command> command_list;

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

void add_commands();

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
			// Serial.print("Received Value: ");

			// Find an Error
			// auto error_pos = rxValue.find("Error");
			// if (error_pos != std::string::npos) {
			// 	BLE_reply = rxValue;
			// 	Serial.print(BLE_reply.c_str());
			// 	Serial.println("*********");
			// 	return;
			// }

			// auto relay_control = rxValue.find("relay");
			
			// if (relay_control != std::string::npos) {
			// 	std::string tmp = rxValue.substr(relay_control + 6, 2);

			// 	Serial.printf("%s\n", tmp.c_str());

			// 	if (tmp == "on") {
			// 		is_compressor_start = true;
			// 		is_relay_controlled_by_user = true;
			// 		BLE_reply = "Set the relay status to ON\n";
			// 	}
			// 	else if (tmp == "of") {
			// 		is_compressor_start = false;
			// 		is_relay_controlled_by_user = true;
			// 		BLE_reply = "Set the relay status to OFF\n";
			// 	}
			// 	else if (tmp == "au") {
			// 		is_relay_controlled_by_user = false;
			// 	}
			// 	else {
			// 		BLE_reply = "ERROR: Unknown argument!\n";
			// 	}
				
			// 	Serial.print(BLE_reply.c_str());
			// 	Serial.println("*********");
			// 	return;
			// }

			// auto curr_hum_command = rxValue.find("curr_hum");

			// if (curr_hum_command != std::string::npos) {
			// 	if (curr_hum_value == 0xFF)
			// 		BLE_reply = "ERROR: No humidity value from sensor!\n";
			// 	else
			// 		BLE_reply = std::to_string(curr_hum_value) + '\n';

			// 	Serial.print(BLE_reply.c_str());
			// 	Serial.println("*********");
			// 	return;
			// }

			// // Parse input from PC
			// auto PC_input_pos = rxValue.find("hum");

			// if (PC_input_pos == std::string::npos) {
			// 	// TODO Handler the error
			// }
			// else {
			// 	// TODO Add checks

			// 	hum_min = std::stoi(rxValue.substr(PC_input_pos + 4, 2));
			// 	hum_max = std::stoi(rxValue.substr(PC_input_pos + 7, 2));
			// 	BLE_reply += "New humidity border values is " + std::to_string(hum_min) + " and " + std::to_string(hum_max) + '\n';
			// 	// BLE_reply = rxValue;
			// 	Serial.print(BLE_reply.c_str());
			// 	Serial.println("*********");
			// 	return;
			// }

			// // TODO Assume that the message arrives entirely in one package!

			// // Parse hum and temp values
			// // TODO Try to find '.' to locate the mantissa of the float values or just send float
			// std::string hum_str = "Humidity: ";
			// auto pos = rxValue.find(hum_str);

			// if (pos == std::string::npos) {
			// 	// TODO Handler error
			// }
			// else {
			// 	// TODO Add check for digit
			// 	curr_hum_value = std::stoi(rxValue.substr(pos + hum_str.size(), 2)) - 1;
			// }

			// std::string temp_str = "Temperature: ";
			// pos = rxValue.find(temp_str);

			// if (pos == std::string::npos) {
			// 	// TODO Handler error
			// }
			// else {
			// 	curr_temp_value = std::stoi(rxValue.substr(pos + temp_str.size(), 2));
			// }

			// BLE_reply = rxValue;
			// Serial.printf("%s\n", rxValue.c_str());
			// // Serial.printf("Readed hum: %d and readed temp: %d\n", curr_hum_value, curr_temp_value);
			// // for (int i = 0; i < rxValue.length(); i++)
			// // 	Serial.print(rxValue[i]);

			// Serial.println("*********");

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
};

void setup()
{
	Serial.begin(115200);
	Serial.println("Starting BLE work!");

	add_commands();

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
		BLECharacteristic::PROPERTY_READ);

	p_to_sensor_characteristic->setCallbacks(new MyToSensorCallbacks());
	p_to_sensor_characteristic->addDescriptor(new BLE2902());

	pService->start();

	// BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
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

	compare_hum();
	delay(1000);
}

void compare_hum() {
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

	if (is_compressor_start) {
		pinMode(RELAY_PIN, OUTPUT);
		digitalWrite(RELAY_PIN, LOW);
	}
	else {
		pinMode(RELAY_PIN, INPUT);
	}

}

void add_commands() {
	command_list.emplace_back("hum", hum_handler);
	command_list.emplace_back("curr_hum", curr_hum_handler);
	command_list.emplace_back("Error", error_handler);
	command_list.emplace_back("relay", relay_handler);
	command_list.emplace_back("Humidity: ", humidity_handler);
	command_list.emplace_back("Temperature: ", temperature_handler);
}

void hum_handler(const std::string& message) {
	if (message.size() > strlen("hum xx xx\n")) {
		BLE_reply = "ERROR: Unknown command format!\n";
		debug(BLE_reply);
		return;
	}

	// Try to find the 'space' sym between 'min' and 'max' values of the user input
	auto space_pos = message.find_last_of(' ', strlen("hum "));
	if (space_pos == std::string::npos) {
		BLE_reply = "ERROR: Unknown command format!\n"
					"Command must have the 'space' symbol between the minimum and the maximum values!\n";
		debug(BLE_reply);
	}

	// Split min and max values
	// std::string&& tmp_hum_min_str = message.substr(strlen("hum "), space_pos - strlen("hum "));
	// std::string&& tmp_hum_max_str = message.substr(space_pos + 1, message.size() - space_pos);

	// debug(tmp_hum_min_str);
	// debug(tmp_hum_max_str);

	// hum_min = std::stoi(message.substr(4, 2));
	// hum_max = std::stoi(message.substr(7, 2));
	// BLE_reply += "New humidity border values is " + std::to_string(hum_min) + " and " + std::to_string(hum_max) + '\n';
	BLE_reply = message;
	Serial.print(BLE_reply.c_str());
	Serial.println("*********");
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
	}
	else {
		BLE_reply = "ERROR: Unknown argument!\n";
	}
	
	Serial.print(BLE_reply.c_str());
}

void humidity_handler(const std::string& message) {
	/**
	 * TODO: Check string for containig a number
	*/
	// curr_hum_value = std::stoi(rxValue.substr(pos + hum_str.size(), 2)) - 1;
}

void temperature_handler(const std::string& message) {
	// curr_temp_value = std::stoi(rxValue.substr(pos + temp_str.size(), 2));
}

void parse_message(const std::string& message) {
	for (uint8_t i = 0; i < command_list.size(); ++i) {
		if (message.find(command_list[i].name) != std::string::npos) {
			debug("Command finded!");
			command_list[i].handler(message);
			debug("Exit from handler");
			return;
		}
	}
	debug("Coundn't find the command!");
}

#if DEBUG == 1
void debug(const std::string& debug_info) {
	Serial.printf("DEBUG: %s\n", debug_info.c_str());
}
#else
void debug(const std::string& debug_info) {}
#endif