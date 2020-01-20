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
bool power = 1;
bool powerFlag = 1;
bool logRSSI = 0;

PersistentData persistentData;

// Declarations
void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
void handleCommand(char *topic, byte *message, unsigned int length);
void checkUpdate();
void saveEEPROM(PersistentData argument);
PersistentData loadEEPROM();
void logRssi();
void logMaxRssi();
void printEEPROM(PersistentData persistentData);
void initCustomEEPROM();

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
    Serial.print("Firmware Version: ");
    Serial.println(FIRMWARE_VERSION);

    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    Animations::init(leds);
    if (FIRMWARE_VERSION == 0.0) initCustomEEPROM();
    persistentData = loadEEPROM();

    // init librarys
    System::init(&persistentData);
    System::setup_wifi(&persistentData);

    System::mqttClient.setCallback(handleCommand);
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
            //mode = 99;
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

void handleCommand(char *topic, byte *message, unsigned int length)
{
    Serial.print("Message arrived on topic: ");
    Serial.print(topic);
    Serial.print(". Message: ");
    String messageTemp;
    String topicTemp = topic;

    for (int i = 0; i < length; i++)
    {
        Serial.print((char)message[i]);
        messageTemp += (char)message[i];
    }
    Serial.println();

    if (topicTemp.indexOf("power") >= 0)
    {
        if (messageTemp.indexOf("0") >= 0 || messageTemp.indexOf("OFF") >= 0 || messageTemp.indexOf("off") >= 0)
        {
            power = false;
        }
        else if (messageTemp.indexOf("1") >= 0 || messageTemp.indexOf("ON") >= 0 || messageTemp.indexOf("on") >= 0)
        {
            power = true;
        }
        else if (messageTemp.indexOf("2") >= 0 || messageTemp.indexOf("TOGGLE") >= 0 || messageTemp.indexOf("toggle") >= 0)
        {
            power = !power;
        }

        System::sendStat("power", (String) power);
    }
    else if (topicTemp.indexOf("mode") >= 0)
    {
        if (messageTemp.toInt() > 0) mode = messageTemp.toInt();
        System::sendStat("mode", (String) mode);
    }
    else if (topicTemp.indexOf("brightness") >= 0)
    {
        if (messageTemp.toInt() >= 0) Animations::brightness = messageTemp.toInt();
        System::sendStat("brightness", (String) Animations::brightness);
    }
    else if (topicTemp.indexOf("restart") >= 0)
    {
        System::sendStat("restart", "OK");
        esp_restart();
    }
    else if (topicTemp.indexOf("update") >= 0)
    {
        System::sendStat("update", "OK");
        checkUpdate();
    }
    else if (topicTemp.indexOf("resetRSSI") >= 0)
    {
        for (int i = 0; i < 8; i++) RX5808::resetMaxRssi(i);
        System::sendStat("resetRSSI", "OK");
    }
    else if (topicTemp.indexOf("autoresetRSSI") >= 0)
    {
        if (messageTemp.indexOf("0") >= 0 || messageTemp.indexOf("OFF") >= 0 || messageTemp.indexOf("off") >= 0)
        {
            RX5808::autoReset = false;
        }
        else if (messageTemp.indexOf("1") >= 0 || messageTemp.indexOf("ON") >= 0 || messageTemp.indexOf("on") >= 0)
        {
            RX5808::autoReset = true;
        }
        else if (messageTemp.indexOf("2") >= 0 || messageTemp.indexOf("TOGGLE") >= 0 || messageTemp.indexOf("toggle") >= 0)
        {
            RX5808::autoReset = !RX5808::autoReset;
        }
        System::sendStat("autoresetRSSI", (String) RX5808::autoReset);
    }
    else if (topicTemp.indexOf("maxRSSI") >= 0)
    {
        // ToDo
        logMaxRssi();
    }
    else if (topicTemp.indexOf("logRSSI") >= 0)
    {
        if (messageTemp.indexOf("0") >= 0 || messageTemp.indexOf("OFF") >= 0 || messageTemp.indexOf("off") >= 0)
        {
            logRSSI = false;
        }
        else if (messageTemp.indexOf("1") >= 0 || messageTemp.indexOf("ON") >= 0 || messageTemp.indexOf("on") >= 0)
        {
            logRSSI = true;
        }
        else if (messageTemp.indexOf("2") >= 0 || messageTemp.indexOf("TOGGLE") >= 0 || messageTemp.indexOf("toggle") >= 0)
        {
            logRSSI = !logRSSI;
        }

        System::sendStat("logRSSI", (String)logRSSI);
        logRssi();
    }
    else if (topicTemp.indexOf("name") >= 0)
    {
        String name = messageTemp;
        if (name.length() > 0)
        {
            persistentData.espid = name;
            saveEEPROM(persistentData);
            System::sendStat("name", (String) persistentData.espid);
            esp_restart();
        }
        else
        {
            System::sendStat("name", "Invalid value");
        }
    }
    else if (topicTemp.indexOf("network") >= 0)
    {
        uint8_t networkStart = topicTemp.indexOf("network");
        uint8_t network = topicTemp.substring(networkStart + 7, networkStart + 8).toInt();

        uint8_t seperator1 = messageTemp.indexOf(";");
        uint8_t seperator2 = messageTemp.indexOf(";", seperator1 + 1);

        String ssid = messageTemp.substring(0, seperator1);
        String pass = messageTemp.substring(seperator1 + 1, seperator2);
        String mqtt = messageTemp.substring(seperator2 + 1, messageTemp.length());

        if (ssid.length() > 0 && pass.length() > 0 && mqtt.length() > 0)
        {
            ssid.toCharArray(persistentData.networks[network].ssid, 32);
            Serial.print("ssid: ");
            Serial.println(ssid);
            pass.toCharArray(persistentData.networks[network].pass, 32);
            Serial.print("pass: ");
            Serial.println(pass);
            mqtt.toCharArray(persistentData.networks[network].mqtt, 32);
            Serial.print("mqtt: ");
            Serial.println(mqtt);
            saveEEPROM(persistentData);

            System::sendStat("network", "OK");
        }
        else
        {
            System::sendStat("network", "Invalid value");
        }
    }
    
    // ToDo: make AutoUpdate optional
}

//Task1code: blinks an LED every 1000 ms
void Task1code(void *pvParameters)
{
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {        
        System::loop();
        ArduinoOTA.handle();
        Animations::loop();

        /*
            ToDo: create loop tasks for librarys
                Animations::loop();
            and put here
        */

        int nearest = RX5808::getNearestDrone();
        
        if (power)
        {
            if (!powerFlag)
            {
                Animations::on();
                powerFlag = 1;
            }
            
            if (mode == 2)
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
            else if (nearest != 0)
            {
                Animations::setChannelColor(nearest);
                //System::sendTele("saw [" + (String)nearest + "] maxrssi = " + (String)RX5808::maxRssi[nearest]);
            }
            // WhoopMode
            else if (mode == 3)
            {
                Animations::standby();
            }
            // PartyMode
            else if (mode == 10)
            {
                Animations::party();
            }
            // rainbow
            else if (mode == 11)
            {
                Animations::rainbow();
            }
            // rainbowWithGlitter
            else if (mode == 12)
            {
                Animations::rainbowWithGlitter();
            }
            // confetti
            else if (mode == 13)
            {
                Animations::confetti();
            }
            // sinelon
            else if (mode == 14)
            {
                Animations::sinelon();
            }
            // juggle
            else if (mode == 15)
            {
                Animations::juggle();
            }
            // bpm
            else if (mode == 16)
            {
                Animations::bpm();
            }
            // bpm new
            else if (mode == 17)
            {
                Animations::animation = &Animations::bpm;
            }

            // Debug StartupAnimation
            else if (mode == 95)
            {
                Animations::startup();
            }
            // Debug UpdateAnimation
            else if (mode == 96)
            {
                Animations::update();
            }
            // Debug EepromAnimation
            else if (mode == 97)
            {
                Animations::initEEPROM();
            }
            // Debug StandbyAnimation
            else if (mode == 98)
            {
                Animations::standby();
            }
            else
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        }
        else
        {
            if (powerFlag)
            {
                Animations::off();
                powerFlag = 0;
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
}

void loop()
{
    //EVERY_N_SECONDS(1) printMaxRssi();
    EVERY_N_MINUTES(10) checkUpdate();
    // BootMode
    if (mode == 1)
    {
        checkUpdate();
        Animations::startup(); // caution: BLOCKING!!
        power = 0;
        mode = 3;
    }
    // SleepMode?
    else
    {
        /*
            ToDo: create loop tasks for library
            RX5808::loop();
            and put here
        */
        RX5808::checkRssi();      // caution: BLOCKING!!
        if (logRSSI)
        {
            logRssi();
        }
        RX5808::checkDroneNear(); // caution: BLOCKING!!
    }
}
// ToDo: put inside system and call Animations::changeAnimation();
void checkUpdate()
{
    String message;
    message += "uptime: ";
    message += millis();
    message += " ms, FW: ";
    message += FIRMWARE_VERSION;
    message += ". Checking for updates..";
    System::sendTele(message);
    char *url = System::checkForUpdate(digicert_pem_start);
    if (strlen(url) != 0)
    {
        //update available
        mode = 2;
        power = 1;
        Animations::update();
        System::do_firmware_upgrade(url, digicert_pem_start);
        power = 0;
        mode = 3;
    }
    else
    {
        // no update
        Serial.println("No File");
    }
}

void logRssi()
{
    int *rssi = RX5808::rssi;
    String values = "Values: ";
    for (int i = 0; i < 8; i++)
    {
        values += i + 1;
        values += ": ";
        values += rssi[i];
        values += ", ";
    }
    System::sendRssi(values);
}

void logMaxRssi()
{
    int *maxRssi = RX5808::maxRssi;
    String values = "Values: ";
    for (int i = 0; i < 8; i++)
    {
        values += i + 1;
        values += ": ";
        values += maxRssi[i];
        values += ", ";
    }
    System::sendRssi(values);
}

// ToDo: EEPROM stuff in system.cpp
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
    Serial.println(eData.espid);
    if (String(ok) != String("OK"))
    {
        Serial.println("No OK!");
        initCustomEEPROM();
    }
    return eData;
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

// Default config
// put in extra file
void initCustomEEPROM()
{
    //update available
    mode = 2;
    power = 1;
    Animations::circle(CRGB::Blue);
    power = 0;
    mode = 3;
    // Put your WiFi Settings here:
    PersistentData writeData = {
        "NONAME", // MQTT Topic
        {
            {"SSID", "PASS", "MQTT"}, // WiFi 1
            {"SSID", "PASS", "MQTT"}, // WiFi 2
            {"SSID", "PASS", "MQTT"}, // WiFi 3
            {"SSID", "PASS", "MQTT"}  // WiFi 4
        }};
    saveEEPROM(writeData);
}