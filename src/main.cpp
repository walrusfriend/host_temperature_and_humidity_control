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

uint8_t curr_hum_value = 50;
uint8_t curr_temp_value = 15;
std::string BLE_reply;
std::string BLE_reply_to_sensor;

const uint8_t HUMIDITY_SENSOR_ACCURACY = 2;

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
			Serial.print("Received Value: ");

			// Parse input from PC
			auto PC_input_pos = rxValue.find("hum");

			if (PC_input_pos == std::string::npos) {
				// TODO Handler the error
			}
			else {
				// TODO Add checks
				hum_min = std::stoi(rxValue.substr(PC_input_pos + 4, 2));
				hum_max = std::stoi(rxValue.substr(PC_input_pos + 7, 2));
				BLE_reply += "New humidity border values is " + std::to_string(hum_min) + " and " + std::to_string(hum_max) + '\n';
				Serial.print(BLE_reply.c_str());
				Serial.println("*********");
				return;
			}

			// TODO Assume that the message arrives entirely in one package!

			// Parse hum and temp values
			// TODO Try to find '.' to locate the mantissa of the float values or just send float
			std::string hum_str = "Humidity: ";
			auto pos = rxValue.find(hum_str);

			if (pos == std::string::npos) {
				// TODO Handler error
			}
			else {
				// TODO Add check for digit
				curr_hum_value = std::stoi(rxValue.substr(pos + hum_str.size(), 2)) - 1;
			}

			std::string temp_str = "Temperature: ";
			pos = rxValue.find(temp_str);

			if (pos == std::string::npos) {
				// TODO Handler error
			}
			else {
				curr_temp_value = std::stoi(rxValue.substr(pos + temp_str.size(), 2));
			}

			BLE_reply = rxValue;
			Serial.printf("%s\n", rxValue.c_str());
			// Serial.printf("Readed hum: %d and readed temp: %d\n", curr_hum_value, curr_temp_value);
			// for (int i = 0; i < rxValue.length(); i++)
			// 	Serial.print(rxValue[i]);

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
	if (curr_hum_value > 80) {
		// Serial.println("ALARM!!!");
		p_to_phone_characteristic->setValue("ALARM!!!");
	}

	if (curr_hum_value < hum_max and curr_hum_value > hum_min) {
		pinMode(RELAY_PIN, OUTPUT);
		digitalWrite(RELAY_PIN, LOW);
	}
	else {
		pinMode(RELAY_PIN, INPUT);
	}
}