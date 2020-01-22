#include <Arduino.h>
#include <firmware.h>
#include <EEPROM.h>
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
#define EEPROM_SIZE 512

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
    static void init();
    static void loop();

    static uint8_t logLevel;

    static void setup_wifi();
    static void reconnect();
    static char *checkForUpdate(const char *cert);
    static esp_err_t do_firmware_upgrade(const char *url, const char *cert);

    static WiFiClient wifiClient;
    static PubSubClient mqttClient;

    static String MQTTPrefix;

    static String espid;
    static String fallbackId;

    static void sendStat(String command, String message);
    static void sendTele(String message);
    static void sendRssi(String message);

    static void saveEEPROM(PersistentData eData);
    static PersistentData loadEEPROM();
    static void printEEPROM(PersistentData persistentData);
    static void initCustomEEPROM();
    static void sendDebugMessage(String level, String position, String message);

    static PersistentData persistentData;

private:
    static char rcv_buffer[200];
    static esp_err_t _http_event_handler(esp_http_client_event_t *evt);

    static String genericCmndTopic;
    static String fallbackCmndTopic;
    static String specificCmndTopic;
    static String genericBacklogTopic;
    static String fallbackBacklogTopic;
    static String specificBacklogTopic;
    static String genericStatTopic;
    static String fallbackStatTopic;
    static String specificStatTopic;
    static String genericTeleTopic;
    static String fallbackTeleTopic;
    static String specificTeleTopic;
    static String specificRssiTopic;
};