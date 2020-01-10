#include <Arduino.h>
#include <FastLED.h>
#include <RX5808.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_https_ota.h>

#define NUM_LEDS 90
#define DATA_PIN 13

#define SOFTWARE_VERSION 1

// Provide server name, path to metadata file and polling interval for OTA updates.
#define OTA_SERVER_HOST_NAME "https://github.com/SoulOfNoob/SmartWhoopGate32/raw/master"
#define OTA_SERVER_METADATA_PATH "/esp32/ota.txt"
#define OTA_POLLING_INTERVAL_S 5
#define OTA_AUTO_REBOOT 1

#define CONFIG_FIRMWARE_UPGRADE_URL "https://github.com/SoulOfNoob/SmartWhoopGate32/raw/master/compiled/firmware.bin"

extern const char github_pem_start[] asm("_binary_certs_github_pem_start");
extern const char digicert_pem_start[] asm("_binary_certs_digicert_pem_start");

//const char *fwurl = "https://github.com/SoulOfNoob/SmartWhoopGate32/raw/master/compiled/firmware.bin";

CRGB leds[NUM_LEDS];

const char *ssid = "SSID";
const char *password = "PASS";
const char *mqtt_server = "broker.hivemq.com";


WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;

void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
void setup_wifi();
void callback(char *topic, byte *message, unsigned int length);
esp_err_t do_firmware_upgrade();

void setup()
{
    // put your setup code here, to run once:
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    Serial.begin(115200);

    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    RX5808::init();

    //Serial.println((char *)github_pem_start);
    do_firmware_upgrade();

    //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
    xTaskCreatePinnedToCore(
        Task1code, /* Task function. */
        "Task1",   /* name of task. */
        10000,     /* Stack size of task */
        NULL,      /* parameter of the task */
        1,         /* priority of the task */
        &Task1,    /* Task handle to keep track of created task */
        0);        /* pin task to core 0 */
    delay(500);

    //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
    xTaskCreatePinnedToCore(
        Task2code, /* Task function. */
        "Task2",   /* name of task. */
        10000,     /* Stack size of task */
        NULL,      /* parameter of the task */
        1,         /* priority of the task */
        &Task2,    /* Task handle to keep track of created task */
        1);        /* pin task to core 1 */
    delay(500);
}

void setup_wifi()
{
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void callback(char *topic, byte *message, unsigned int length)
{
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    String messageTemp;

    for (int i = 0; i < length; i++)
    {
        Serial.print((char)message[i]);
        messageTemp += (char)message[i];
    }
    Serial.println();
}

void reconnect()
{
    // Loop until we're reconnected
    while (!client.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (client.connect("jryesp32Client"))
        {
            Serial.println("connected");
            // Subscribe
            client.subscribe("jryesp32/input");
            Serial.println("subscribed");
            client.publish("jryesp32/output", "bin da");
            Serial.println("published");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

//Task1code: blinks an LED every 1000 ms
void Task1code(void *pvParameters)
{
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {
        RX5808::checkRssi();
        RX5808::checkDroneNear();
        RX5808::setDroneColor(NUM_LEDS, leds);
        FastLED.setBrightness(RX5808::brightness);
        FastLED.show();
    }
}

//Task2code: blinks an LED every 700 ms
void Task2code(void *pvParameters)
{
    Serial.print("Task2 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {
        if (!client.connected())
        {
            reconnect();
        }
        client.loop();

        EVERY_N_MILLISECONDS(5000) 
        {
            Serial.println("sending mqtt");
            client.publish("jryesp32/output", "still alive");
        }
    }
}

void loop()
{
    
}

esp_err_t do_firmware_upgrade()
{
    Serial.println("downloading and installing new firmware ...");

    esp_http_client_config_t config = { };
    config.url = "https://raw.githubusercontent.com/SoulOfNoob/SmartWhoopGate32/master/compiled/esp32/firmware.bin";
    config.cert_pem = digicert_pem_start;

    esp_err_t ret = esp_https_ota(&config);

    if (ret == ESP_OK)
    {
        Serial.println("OTA OK, restarting...");
        esp_restart();
    }
    else
    {
        Serial.println("OTA failed...");
        return ESP_FAIL;
    }
    return ESP_OK;
}