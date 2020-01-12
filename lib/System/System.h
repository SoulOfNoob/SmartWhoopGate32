#include <Arduino.h>
#include <config.h>
#include <WiFi.h>
#include <esp_https_ota.h>
#include <PubSubClient.h>

#define UPDATE_JSON_URL "https://raw.githubusercontent.com/SoulOfNoob/SmartWhoopGate32/master/ota/esp32/firmware.json"
#define FIRMWARE_VERSION 0.7

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
    static char *checkForUpdate(const char *cert);
    static esp_err_t do_firmware_upgrade(const char *url, const char *cert);

    static WiFiClient wifiClient;
    static PubSubClient mqttClient;

    static String espid;
    static String cmdTopic;
    static String statusTopic;

    static char rcv_buffer[200];
    static esp_err_t _http_event_handler(esp_http_client_event_t *evt);

private:
    static PersistentData *_persistentData;
};