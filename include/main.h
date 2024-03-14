#pragma once

class UserDefinedParameters {
public:
	int hum_max = 0;
    int hum_min = 0;
    bool relay_status = 0;
};

class SensorParameters {
public:
    int temp;
    int hum;
    int battery_charge;
    bool relay_status;
};