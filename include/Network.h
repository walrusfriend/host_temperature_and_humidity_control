#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

class Network {
public:
    Network();
    ~Network();

    bool wifi_connect();
    bool check_wifi_parameters();
    bool check_wifi_connection();
    
    // REST API functions
    void POST_log(const std::string_view& log_string);
    void POST_temp(const uint8_t& temperature_value);
    void POST_hum(const uint8_t& humidity_value);
    void GET_hub();

    const String blank_ssid = "blank_ssid";
    const String blank_pass = "blank_pass";
    const String blank_url = "blank_url";

    String ssid = blank_ssid;
    String password = blank_pass;
    String url = blank_url;

    // const char *ssid = "Redmi 10";
    // const char *password = "123456789q";
    // const char *url = "https://serverpd.ru";

    bool do_wifi_connect = false;

    bool is_wifi_settings_initialized = false;
    bool is_wifi_connection_establish = false;

    HTTPClient https;

    // Endpoints
    String temp_endpoint = String("/hub/temperature");
    String hum_endpoint = String("/hub/humidity");
    String log_endpoint = String("/hub/log");

    String hub_get_endpoint = String("/hub/hub?establishment_id=");

    static const uint8_t hub_id = 3;
    static const uint8_t sensor_id = 1;
    static const uint8_t establishment_id = 2;

    static const uint8_t MAX_WIFI_CONNECTION_TRIES = 20;
};