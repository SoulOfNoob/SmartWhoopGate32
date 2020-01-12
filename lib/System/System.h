#include <Arduino.h>
#include <config.h>
#include <WiFi.h>
#include <esp_https_ota.h>
#include <PubSubClient.h>

struct NetworkData
{
    char ssid[32];
    char pass[32];
    char mqtt[32];
};

struct PersistentData
{
    String espid;
    NetworkData networks[3];
};

class System
{
public:
    static void init(PersistentData *persistentData);

    static void setup_wifi(PersistentData *persistentData);
    static void reconnect();
    static esp_err_t do_firmware_upgrade(const char *url, const char *cert);

    static WiFiClient wifiClient;
    static PubSubClient mqttClient;

    static String espid;
    static String cmdTopic;
    static String statusTopic;

private:
    static PersistentData *_persistentData;
};