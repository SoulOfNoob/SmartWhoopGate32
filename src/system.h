#include <Arduino.h>
#include <config.h>
#include <WiFi.h>
#include <esp_https_ota.h>
#include <PubSubClient.h>


class System
{
public:
    static void init();

    static void setup_wifi();
    static void reconnect();
    static esp_err_t do_firmware_upgrade(const char *url, const char *cert);

    static WiFiClient wifiClient;
    static PubSubClient mqttClient;
};