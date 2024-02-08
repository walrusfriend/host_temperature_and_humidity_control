#include "BLE.h"

bool BLE::is_data_from_BLE_received = false;
bool BLE::do_BLE_connect = true;
bool BLE::connected = false;
bool BLE::doScan = false;
uint8_t BLE::curr_hum_value = 0xff;
uint8_t BLE::curr_temp_value = 0xff;
uint8_t BLE::curr_battery_value = 100;
bool BLE::block_relay = true;

BLE::BLE() {
	// Serial2.begin(9600);
	// Check BLE connection


}

BLE::~BLE() {

}

bool BLE::connectToServer()
{
	// Serial.printf("INFO: Forming a connection to %s", sensor_MAC_address.toString().c_str());

	// pClient = BLEDevice::createClient();
	// Serial.println("INFO:  - Created client");

	// pClient->setClientCallbacks(new MyClientCallback());

	// if (pClient->connect(sensor_MAC_address) == false)
	// {
	// 	Serial.println("ERROR: Couldn't connect to remote BLE server!");
	// 	return false;
	// }

	// Serial.println("INFO:  - Connected to server");
	// pClient->setMTU(517); // set client to request maximum MTU from server (default is 23 otherwise)

	// BLERemoteService *p_serial_service = pClient->getService(serial_service_uuid);
	// if (p_serial_service == nullptr)
	// {
	// 	Serial.print("ERROR: Failed to find our service UUID: ");
	// 	Serial.println(serial_service_uuid.toString().c_str());
	// 	pClient->disconnect();
	// 	return false;
	// }
	// Serial.println("INFO:  - Found serial service");

	// p_serial_characteristic = p_serial_service->getCharacteristic(serial_characteristic_uuid);
	// if (p_serial_characteristic == nullptr)
	// {
	// 	Serial.print("ERROR: Failed to find our characteristic UUID: ");
	// 	Serial.println(serial_characteristic_uuid.toString().c_str());
	//     pClient->disconnect();
	// 	return false;
	// }
	// Serial.println("INFO:  - Found service characteristic");

	// if (p_serial_characteristic->canNotify())
	// 	p_serial_characteristic->registerForNotify(notifyCallback);

	// connected = true;
	// return true;
}

// void BLE::notifyCallback(
// 	BLERemoteCharacteristic *pBLERemoteCharacteristic,
// 	uint8_t *pData,
// 	size_t length,
// 	bool isNotify)
// {
// 	Serial.print("INFO: Notify callback for characteristic ");
// 	Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
// 	Serial.print(" of data length ");
// 	Serial.println(length);
// 	Serial.print("data: ");
// 	Serial.write(pData, length);
// 	Serial.println();

// 	if (pData[0] == 'd')
// 	{
// 		if (pData[2] == 't' && pData[6] == 'h') {
// 			std::string temp_str;
// 			temp_str += pData[3];
// 			temp_str += pData[4];

// 			/** TODO: Add check for number */
// 			curr_temp_value = std::stoi(temp_str);

// 			std::string hum_str;
// 			hum_str += pData[7];
// 			hum_str += pData[8];

// 			/** TODO: Add check for number */
// 			curr_hum_value = std::stoi(hum_str);

// 			Serial.printf("INFO: Parsed temp value: %d\n"
// 						  "\t  Parsed hum value %d\n\n", curr_temp_value, curr_hum_value);

// 			is_data_from_BLE_received = true;
// 		}
// 		else {
// 			Serial.println("ERROR: Invalid message!");
// 			Serial.println((char*)pData);
// 		}
// 		Serial.println();
// 	}
// }