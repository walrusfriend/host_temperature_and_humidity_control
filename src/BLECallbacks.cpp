// #include "BLECallbacks.h"

// void MyServerCallbacks::onConnect(BLEServer *pServer)
// {
//     deviceConnected = true;
//     BLEDevice::startAdvertising();
// };

// void MyServerCallbacks::onDisconnect(BLEServer *pServer)
// {
//     deviceConnected = false;
// }

// void HumidityCharacteristicCallbacks::onWrite(BLECharacteristic *pCharacteristic)
// {
//     std::string rxValue = pCharacteristic->getValue();

//     if (rxValue.length() > 0)
//     {
//         Serial.println(rxValue.c_str());
//         parse_message(rxValue);

//         // Calculate border values
//         uint8_t step_value = (hum_max - hum_min) * 0.2;

//         if (step_value < HUMIDITY_SENSOR_ACCURACY) 
//             step_value = HUMIDITY_SENSOR_ACCURACY;
//         // Serial.printf("DEBUG: step_value: %d\n", step_value);

//         if ((curr_hum_value < hum_min + step_value) or (curr_hum_value > hum_max - step_value)) {
//             BLE_reply_to_sensor = "S5\n";
//         }
//         else {
//             BLE_reply_to_sensor = "S30\n";
//         }
//         // Serial.printf("DEBUG: reply %s\n",
//         // BLE_reply_to_sensor.c_str());

//     }
// }

// void HumidityCharacteristicCallbacks::onRead(BLECharacteristic* pCharacteristic) {
//     pCharacteristic->setValue(BLE_reply);
//     Serial.println("Read callback was called");
//     BLE_reply.clear();
// }

// void HumidityCharacteristicCallbacks::onNotify(BLECharacteristic* pCharacteristic) {
//     pCharacteristic->setValue(BLE_reply);
//     Serial.println("Notify callback was called");
//     BLE_reply.clear();
// }

// void HumidityCharacteristicCallbacks::onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
//     Serial.print("On status called with status: ");
//     Serial.println(std::to_string(s).c_str());
// }