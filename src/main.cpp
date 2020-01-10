#include <Arduino.h>
#include <FastLED.h>
#include <RX5808.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <esp_https_ota.h>
#include <config.h>

#define NUM_LEDS 90
#define DATA_PIN 13

#define CONFIG_FIRMWARE_UPGRADE_URL "https://raw.githubusercontent.com/SoulOfNoob/SmartWhoopGate32/master/ota/esp32/firmware.bin"

extern const char github_pem_start[] asm("_binary_certs_github_pem_start");
extern const char digicert_pem_start[] asm("_binary_certs_digicert_pem_start");

CRGB leds[NUM_LEDS];

// const char *ssid = "SSID";
// const char *password = "PASS";
// const char *mqtt_server = "broker.hivemq.com";

#ifndef WIFI_SSID
    #define WIFI_SSID "BBT";
#endif

#ifndef WIFI_PASS
    #define WIFI_PASS "Gerhofhh19!";
#endif

#ifndef MQTT_BROKER
    #define MQTT_BROKER "broker.hivemq.com";
#endif

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASS;
const char *mqtt_server = MQTT_BROKER;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

bool ledOn = false;

    TaskHandle_t Task1;
TaskHandle_t Task2;

void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
void setup_wifi();
void callback(char *topic, byte *message, unsigned int length);
void evaluateMessage(String message);
esp_err_t do_firmware_upgrade();

void setup()
{
    // put your setup code here, to run once:
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    Serial.begin(115200);
    
    Serial.println();
    Serial.println();
    Serial.println();

    for (uint8_t t = 4; t > 0; t--)
    {
        Serial.printf("[SETUP] WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }
    Serial.println("Firmware Version: 0.2");

    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    RX5808::init();

    //do_firmware_upgrade();

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

    evaluateMessage(messageTemp);
}

void evaluateMessage(String message)
{   
    if (message.indexOf("ON") >= 0)
    {
        Serial.println("Turning LEDs ON");
        ledOn = true;
    }
    else if (message.indexOf("OFF") >= 0)
    {
        Serial.println("Turning LEDs OFF");
        ledOn = false;
    }
    else if (message.indexOf("UPDATE") >= 0)
    {
        Serial.println("Trying Update");
        do_firmware_upgrade();
    }
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
            client.subscribe("jryesp32/cmd");
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

        if(ledOn){
            RX5808::checkRssi();
            RX5808::checkDroneNear();
            RX5808::setDroneColor(NUM_LEDS, leds);
            FastLED.setBrightness(RX5808::brightness);
            FastLED.show();
        }else{
            FastLED.setBrightness(0);
            FastLED.show();
        }
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

        EVERY_N_SECONDS(30) 
        {
            Serial.println("sending mqtt");
            client.publish("jryesp32/output", "still alive");
        }
        EVERY_N_HOURS(24)
        {
            do_firmware_upgrade();
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
    config.url = CONFIG_FIRMWARE_UPGRADE_URL;
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