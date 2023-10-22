#pragma once 

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

class MyServerCallbacks : public BLEServerCallbacks
{
	void onConnect(BLEServer *pServer);
	void onDisconnect(BLEServer *pServer);
};

class HumidityCharacteristicCallbacks : public BLECharacteristicCallbacks
{
	void onWrite(BLECharacteristic *pCharacteristic);
	void onRead(BLECharacteristic* pCharacteristic);
	void onNotify(BLECharacteristic* pCharacteristic);
	void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code);
};