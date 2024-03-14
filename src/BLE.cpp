#include "BLE.h"

BLE::BLE() {
	pinMode(power_pin, OUTPUT);
	pinMode(wakeup_pin, OUTPUT);
	
	digitalWrite(power_pin, HIGH);
	digitalWrite(wakeup_pin, HIGH);

	Serial2.begin(230400); // D17 - TX, D16 - RX
	// Check BLE connection

	Serial2.print("AT");

	// Wait for reply
	bool is_reply_received = false;
	uint8_t MAX_CONNECTION_TRIES = 50;
	uint8_t connection_tries = 0;

	while(is_reply_received == false) {
		uint16_t size = Serial2.available();
		if (size >= 1) {
			char buff[128];
			Serial2.readBytes(buff, size);
			is_reply_received = true;

			Serial.println("DEBUG: BLE module responded");
		}

		Serial.println("Try to connect to BLE module");

		delay(100);

		if (connection_tries >= MAX_CONNECTION_TRIES) {
			break;
		}
	}

	if (is_reply_received == false) {
		/** TODO: Handle an error */
		Serial.println("ERROR: Couldn't to BLE module!");
	}
}

BLE::~BLE() {

}

void BLE::power(const bool& status) const {
	// The ternary operation handles the case if true is not equal to 1
	digitalWrite(power_pin, status ? HIGH : LOW);
}

void BLE::wake_up() const {
	Serial.println("DEBUG: Wake up");
	digitalWrite(wakeup_pin, LOW);
	delay(1100);
	digitalWrite(wakeup_pin, HIGH);
}