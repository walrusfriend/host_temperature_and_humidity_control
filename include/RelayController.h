#pragma once

#include <Arduino.h>

class RelayController {
public:
	static constexpr uint8_t COMPRESSOR_RELAY = LED_BUILTIN;
	static constexpr uint8_t INFLOW_RELAY_LOW = 5;
	static constexpr uint8_t INFLOW_RELAY_HIGH = 19;
	static constexpr uint8_t EXHAUST_RELAY_LOW = 12;
	static constexpr uint8_t EXHAUST_RELAY_HIGH = 14;

public:
	static void on (uint8_t relay_pin) {
		pinMode(relay_pin, OUTPUT);
		digitalWrite(relay_pin, LOW);
	}

	static void off (uint8_t relay_pin) {
		/** TODO: Try to set relay pin to HIGH*/
		pinMode(relay_pin, INPUT);
	}
};