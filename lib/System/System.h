#include <Arduino.h>
#include <config.h>
#include <WiFi.h>
#include <esp_https_ota.h>
#include <PubSubClient.h>


class System
{
public:
    static void init(const char *ssid, const char *password, const char *mqtt_server_ip);

    static void setup_wifi();
    static void reconnect();
    static esp_err_t do_firmware_upgrade(const char *url, const char *cert);

    static WiFiClient wifiClient;
    static PubSubClient mqttClient;

private:
    static const char *_ssid;
    static const char *_password;
    static const char *_mqtt_server_ip;
};