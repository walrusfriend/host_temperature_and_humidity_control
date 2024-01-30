#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

constexpr uint8_t WIFI_SSID_SIZE = 16;
constexpr uint8_t WIFI_PASS_SIZE = 16;

struct WiFi_Config {
    char ssid[WIFI_SSID_SIZE];
    char pass[WIFI_PASS_SIZE];
    byte mode = WIFI_MODE_STA;
};

struct RemoteServerConfig {
    char url[32] = {0};
    uint32_t hub_id = 0;
    uint32_t sensor_id = 0;
    uint32_t establishment_id = 0;
};

class Network {
public:
    Network();
    ~Network();

    void handle_disconnect();

    // REST API functions
    void POST_log(const std::string_view& log_string);
    void POST_temp(const uint8_t& temperature_value);
    void POST_hum(const uint8_t& humidity_value);
    void GET_hub();

    WiFi_Config wifi_cfg;
    RemoteServerConfig server_cfg;

    bool do_wifi_connect = true;

    bool is_wifi_settings_initialized = false;
    bool is_wifi_connection_establish = false;

    HTTPClient https;
    WiFiClientSecure *client;

    // Endpoints
    String temp_endpoint = String("/hub/temperature");
    String hum_endpoint = String("/hub/humidity");
    String log_endpoint = String("/hub/log");

    String hub_get_endpoint = String("/hub/hub?establishment_id=");

    static const uint8_t hub_id = 3;
    static const uint8_t sensor_id = 1;
    static const uint8_t establishment_id = 2;

    static const uint8_t MAX_WIFI_CONNECTION_TRIES = 5;

    int16_t hum_max = 32000;
    int16_t hum_min = -32000;
    bool relay_status = 0;
};