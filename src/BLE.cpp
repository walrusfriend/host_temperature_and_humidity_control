#include "BLE.h"

BLE::BLE() {
	pinMode(power_pin, OUTPUT);
	pinMode(wakeup_pin, OUTPUT);
	
	digitalWrite(power_pin, HIGH);
	digitalWrite(wakeup_pin, HIGH);

	Serial2.begin(230400); // D17 - TX, D16 - RX
	// Check BLE connection


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