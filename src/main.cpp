// Includes
#include <Arduino.h>
#include <EEPROM.h>
#include <config.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <RX5808.h>
#include <Animations.h>
#include <System.h>

// Macros
#define DATA_PIN 13
#define NUM_LEDS 90
#define EEPROM_SIZE 512

// Constants
extern const char github_pem_start[] asm("_binary_certs_github_pem_start");
extern const char digicert_pem_start[] asm("_binary_certs_digicert_pem_start");

// Variables
TaskHandle_t Task1;
TaskHandle_t Task2;
CRGB leds[NUM_LEDS];

uint8_t mode = 0;

PersistentData persistentData;

// Declarations
void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
void evaluateMQTTMessage(char *topic, byte *message, unsigned int length);
void saveEEPROM(PersistentData argument);
PersistentData loadEEPROM();
void printMaxRssi();
void printEEPROM(PersistentData persistentData);

// Functions
void setup()
{
    Serial.begin(115200);
    Serial.println("\n\n\n");
    for (uint8_t t = 3; t > 0; t--)
    {
        Serial.printf("[SETUP] WAIT %d...\n", t);
        Serial.flush();
        delay(1000);
    }
    Serial.print("setup() running on core ");
    Serial.println(xPortGetCoreID());
    Serial.println("Firmware Version: 0.4");

    persistentData = loadEEPROM();

    // init librarys
    System::init(&persistentData);
    System::setup_wifi(&persistentData);

    System::mqttClient.setCallback(evaluateMQTTMessage);
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    RX5808::init();

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

    ArduinoOTA
        .onStart([]() {
            //mode = 9;
            //Animations::update(leds);
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            Serial.println("Start updating " + type);
        })
        .onEnd([]() {
            Serial.println("\nEnd");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
            Serial.printf("Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR)
                Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR)
                Serial.println("End Failed");
        });

    ArduinoOTA.begin();

    mode = 1;
}

void evaluateMQTTMessage(char *topic, byte *message, unsigned int length)
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

    if (messageTemp.indexOf("LED") >= 0)
    {
        if (messageTemp.indexOf("ON") >= 0)
        {
            Serial.println("Turning LEDs ON");
            mode = 3;
            System::mqttClient.publish(System::statusTopic.c_str(), "LEDs ON");
        }
        else if (messageTemp.indexOf("OFF") >= 0)
        {
            Serial.println("Turning LEDs OFF");
            mode = 2;
            System::mqttClient.publish(System::statusTopic.c_str(), "LEDs OFF");
        }
    }
    // SETMAXRSSI [1] = <1234>"
    else if (messageTemp.indexOf("MAXRSSI") >= 0)
    {
        if (messageTemp.indexOf("AUTORESET") >= 0)
        {
            if (messageTemp.indexOf("ON") >= 0)
            {
                Serial.println("Turning AUTORESET ON");
                RX5808::autoReset = true;
                System::mqttClient.publish(System::statusTopic.c_str(), "AUTORESET ON");
            }
            else if (messageTemp.indexOf("OFF") >= 0)
            {
                Serial.println("Turning AUTORESET OFF");
                RX5808::autoReset = false;
                System::mqttClient.publish(System::statusTopic.c_str(), "AUTORESET OFF");
            }
        }
        else if (messageTemp.indexOf("RESET") >= 0)
        {
            for (int i = 0; i < 8; i++) RX5808::resetMaxRssi(i);
            
        }
        else if (messageTemp.indexOf("SET") >= 0)
        {
            int channelStart = messageTemp.indexOf("[");
            int valueStart = messageTemp.indexOf("<");
            int valueEnd = messageTemp.indexOf(">");

            if (channelStart > 0 && valueStart > 0 && valueEnd > 0)
            {
                int channel = messageTemp.substring(channelStart + 1, channelStart + 2).toInt();
                int value = messageTemp.substring(valueStart + 1, valueEnd).toInt();
                if (channel >= 0 && value >= 0)
                {
                    RX5808::maxRssi[channel] = value;
                    System::mqttClient.publish(System::statusTopic.c_str(), "Set");
                }
                else
                {
                    System::mqttClient.publish(System::statusTopic.c_str(), "Invalid value");
                }
            }
            else
            {
                System::mqttClient.publish(System::statusTopic.c_str(), "Invalid format");
            }
        }
        printMaxRssi();
    }
    else if (messageTemp.indexOf("UPDATE") >= 0)
    {
        mode = 9;
        Animations::off(leds);
        Animations::update(leds);
        const char *url = CONFIG_FIRMWARE_UPGRADE_URL;
        System::do_firmware_upgrade(url, digicert_pem_start);
    }
    else if (messageTemp.indexOf("RESTART") >= 0)
    {
        esp_restart();
    }
}

//Task1code: blinks an LED every 1000 ms
void Task1code(void *pvParameters)
{
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {
        if(mode == 1)
        {
            Animations::startup(leds);
            mode = 2;
        }
        else if(mode == 2)
        {
            Animations::off(leds);
            mode = 9;
        }
        else if(mode == 3)
        {
            RX5808::checkRssi();
            RX5808::checkDroneNear();
            Animations::setChannelColor(leds, RX5808::getNearestDrone());
            RX5808::setDroneColor(leds);
        }
        else
        {
            delay(10);
        }
    }
}

void loop()
{
    if (!System::mqttClient.connected())
    {
        System::reconnect();
    }
    System::mqttClient.loop();

    // EVERY_N_SECONDS(5)
    // {
    //     Serial.print("loop() running on core ");
    //     Serial.println(xPortGetCoreID());
    // }
    if(mode == 3){
        EVERY_N_SECONDS(1)
        {
            printMaxRssi();
        }
    }
    EVERY_N_MINUTES(5)
    {
        Serial.println("sending mqtt");
        System::mqttClient.publish("smartwhoopgates32/output", "still alive");
    }
    EVERY_N_HOURS(24)
    {
        //mode = 9;
        //Animations::update(leds);
        const char *url = CONFIG_FIRMWARE_UPGRADE_URL;
        System::do_firmware_upgrade(url, digicert_pem_start);
    }

    ArduinoOTA.handle();
}

void saveEEPROM(PersistentData eData)
{
    Serial.print("Writing ");
    Serial.print(sizeof(eData));
    Serial.println(" Bytes to EEPROM.");
    char ok[2 + 1] = "OK";
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, eData);
    EEPROM.put(0 + sizeof(eData), ok);
    EEPROM.commit();
    EEPROM.end();
}

PersistentData loadEEPROM()
{
    PersistentData eData;
    char ok[2 + 1];
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, eData);
    EEPROM.get(0 + sizeof(eData), ok);
    EEPROM.end();
    if (String(ok) != String("OK"))
    {
        eData = {};
    }
    return eData;
}

void printMaxRssi()
{
    int *maxRssi = RX5808::maxRssi;
    String values = "Values: ";
    for( int i = 0 ; i < 8 ; i++ )
    {
        values += i;
        values += ": ";
        values += maxRssi[i];
        values += ", ";
    }
    System::mqttClient.publish(System::statusTopic.c_str(), values.c_str());
}

void printEEPROM(PersistentData eData)
{
    Serial.println("EEPROM Data:");
    Serial.print("espid: ");
    Serial.println(eData.espid);

    Serial.print("ssid1: ");
    Serial.println(eData.networks[0].ssid);
    Serial.print("pass1: ");
    Serial.println(eData.networks[0].pass);
    Serial.print("mqtt1: ");
    Serial.println(eData.networks[0].mqtt);

    Serial.print("ssid2: ");
    Serial.println(eData.networks[1].ssid);
    Serial.print("pass2: ");
    Serial.println(eData.networks[1].pass);
    Serial.print("mqtt2: ");
    Serial.println(eData.networks[1].mqtt);

    Serial.print("ssid3: ");
    Serial.println(eData.networks[2].ssid);
    Serial.print("pass3: ");
    Serial.println(eData.networks[2].pass);
    Serial.print("mqtt3: ");
    Serial.println(eData.networks[2].mqtt);
}