#include <Arduino.h>
#include <firmware.h>
#include <WiFi.h>
#include <esp_https_ota.h>
#include <PubSubClient.h>

/*
    set FIRMWARE_VERSION to 0.0 for initial setup.
    initCustomEEPROM will be called 
    and after setting up wifi and mqtt server,
    current firmware will be loaded without resetting EEPROM.
*/
//#define FIRMWARE_VERSION 1.8

struct NetworkData
{
    char ssid[32];
    char pass[32];
    char mqtt[32];
};

// ToDo: char espid[16]
struct PersistentData
{
    String espid;
    NetworkData networks[4];
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
    static String fallbackId;
    static String fallbackTopic;
    static String cmdTopic;
    static String statusTopic;

    static char rcv_buffer[200];
    static esp_err_t _http_event_handler(esp_http_client_event_t *evt);

private:
    static PersistentData *_persistentData;
};