#include "BLE.h"

#include "Network.h"
extern Network network;

BLE::BLE() {
	pinMode(power_pin, OUTPUT);
	pinMode(wakeup_pin, OUTPUT);
	
	digitalWrite(power_pin, HIGH);
	digitalWrite(wakeup_pin, HIGH);

	Serial2.begin(230400); // D17 - TX, D16 - RX
	// Check BLE connection

	check_connection();
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

void BLE::check_connection() {
	// Wait for reply
	uint8_t MAX_CONNECTION_TRIES = 10;
	uint8_t connection_tries = 0;

	// Send request
	Serial2.print("AT");

	while(connection_tries < MAX_CONNECTION_TRIES) {
		uint16_t size = Serial2.available();
		if (size >= 1) {
			char buff[128];
			Serial2.readBytes(buff, size);
			is_connected = true;

			Serial.println("DEBUG: The BLE module on the host is working normally!");
			network.POST_log("INFO", "The BLE module on the host is working normally!");
			return;
		}

		Serial.println("Try to connect to BLE module");
		++connection_tries;
		delay(100);
	}

	Serial.println("ERROR: Couldn't connect to BLE module on the host: the connection time has expired");
	network.POST_log("ERROR", "Couldn't connect to BLE module on the host: the connection time has expired");
	is_connected = false;
}