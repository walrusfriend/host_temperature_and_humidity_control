#pragma once

#include <BluetoothSerial.h>

#include "RingBuffer.h"
#include "UART_API.h"

bool is_number(const std::string& s);

class Parser {
public:
    Parser();
    ~Parser();

    void parse_serial();
    void parse_BT(uint8_t& hum_value);

private:
    void init_BT_parser();
    void init_serial_parser();

    // Serial command handlers
    void unknown_command_handler();
    void max_size_reached_handler();
    void help_handler();
    void hum_handler();

    // BT command handlers
    void raw_hum_data_handler(uint8_t& hum_value);
    void raw_temp_data_handler();

private:
    std::string curr_BT_command;
    std::string curr_serial_command;

    static constexpr uint8_t MAX_SIZE = 128;

    UART_API BT_cli = MAX_SIZE;
    UART_API serial_cli = MAX_SIZE;
};