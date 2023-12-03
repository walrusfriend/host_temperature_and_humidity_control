#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

struct WiFi_Config {
    char ssid[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    char pass[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    byte mode;
};

struct RemoteServerConfig {
    char url[32];
    uint32_t hub_id;
    uint32_t sensor_id;
    uint32_t establishment_id;
};

class Network {
public:
    Network();
    ~Network();

    bool wifi_connect();
    void handle_disconnect();

    bool load_settings();
    bool save_settings();
    
    // REST API functions
    void POST_log(const std::string_view& log_string);
    void POST_temp(const uint8_t& temperature_value);
    void POST_hum(const uint8_t& humidity_value);
    void GET_hub();

    WiFi_Config wifi_cfg;
    RemoteServerConfig server_cfg;

    // const char *ssid = "Redmi10";
    // const char *password = "123456789q";
    // const char *url = "https://serverpd.ru";

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

    static const uint8_t MAX_WIFI_CONNECTION_TRIES = 20;
};