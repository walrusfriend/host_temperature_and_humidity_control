#pragma once

class UserDefinedParameters {
public:
	int hum_max = 0;
    int hum_min = 0;
    bool relay_status = 0;
};

class SensorParameters {
public:
    int temp = 0;
    int hum = 100;
    int battery_charge = 100;
    bool relay_status = false;
};