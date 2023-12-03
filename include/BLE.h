#pragma once

#include <HardwareSerial.h>
#include <BLEDevice.h>
#include <BLE2902.h>

class BLE {
public:
    BLE();
    ~BLE();

    BLEUUID serial_service_uuid = std::string("0000ffe0-0000-1000-8000-00805f9b34fb");
    BLEUUID serial_characteristic_uuid = std::string("0000ffe1-0000-1000-8000-00805f9b34fb");

    BLEClient *pClient;
    BLERemoteCharacteristic *p_serial_characteristic;

    BLEAddress sensor_MAC_address = std::string("3C:A3:08:0D:75:41");

    static bool do_BLE_connect;
    static bool connected;
    static bool doScan;

    static bool is_data_from_BLE_received;

    std::string BLE_reply;
    std::string BLE_reply_to_sensor;

    static uint8_t curr_hum_value;
    static uint8_t curr_temp_value;
    static uint8_t curr_battery_value;

    bool connectToServer();
    
    static void notifyCallback(
        BLERemoteCharacteristic *pBLERemoteCharacteristic,
        uint8_t *pData,
        size_t length,
        bool isNotify);
};

class MyClientCallback : public BLEClientCallbacks
{
	void onConnect(BLEClient *pclient)
	{
		BLE::connected = false;
	}

	void onDisconnect(BLEClient *pclient)
	{
		BLE::connected = false;
		Serial.println("onDisconnect");
	}
};