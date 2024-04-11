#pragma once

#include <HardwareSerial.h>

class BLE {
public:
    BLE();
    ~BLE();

    void power(const bool& status) const;
    void wake_up() const;
    void check_connection();

    std::string BLE_output_buff = "S:1";
    bool is_connected = false;

private:
    const uint8_t power_pin = 15;
    const uint8_t wakeup_pin = 4;
};