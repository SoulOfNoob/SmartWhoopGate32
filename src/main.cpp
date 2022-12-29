// Includes
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <RX5808.h>
#include <Animations.h>
#include <System.h>
#include <ArduinoJson.h>

// Macros
#define EEPROM_SIZE 512

// Constants
extern const char github_pem_start[] asm("_binary_certs_github_pem_start");
extern const char digicert_pem_start[] asm("_binary_certs_digicert_pem_start");

// Variables
TaskHandle_t Task1;
TaskHandle_t Task2;

uint8_t mode = 0;
String commands[] = {
    "power",
    "mode",
    "brightness",
    "restart",
    "update",
    "autoUpdate",
    "resetRSSI",
    "autoresetRSSI",
    "maxRSSI",
    "logRSSI",
    "name",
    "debug"
};
uint8_t commandsLength = sizeof(commands)/sizeof(commands[0]);

/*
    0 = all
    10 = DebugHigh, DebugLow, Info, Warning, Error
    20 = DebugLow, Info, Warning, Error
    30 = Info, Warning, Error
    40 = Warning, Error
    50 = Error
    200 = none
*/

// Switches
bool power = 0;
bool autoUpdate = 1;
bool logRSSI = 0;
bool demoMode = 0;
bool debugMode = 0;

// Flags
bool bootFlag = 1;
bool powerFlag = 1;

// 
uint8_t checkConnectionDelayS = 5;

// Declarations
void Task1code(void *pvParameters);
void handleCommand(String command, String message);
void handleSerial();
void handleBacklog(String input);
void handleMQTT(char *topic, byte *message, unsigned int length);
void checkUpdate();
void logRssi();
void logMaxRssi();
uint8_t getBoolFromString(String input);
String getParameterFromString(String input, String parameter);
String getCommandTopic(String topic);

// Functions
void setup()
{
    System::logLevel = 0;
    Serial.begin(115200);
    Serial.print("\n\n\n");
    for (uint8_t t = 3; t > 0; t--)
    {
        System::sendDebugMessage("Info", (String)__FUNCTION__, "WAIT " + (String)t + "...");
        Serial.flush();
        delay(1000);
    }
    System::sendDebugMessage("DebugLow", (String)__FUNCTION__, "running on core " + (String)xPortGetCoreID());
    System::sendDebugMessage("Info", (String)__FUNCTION__, "Firmware Version: " + (String)FIRMWARE_VERSION);

    Animations::init();
    if (FIRMWARE_VERSION == 0.0) System::initCustomEEPROM();
    // init librarys
    System::init();
    RX5808::init();

    System::mqttClient.setCallback(handleMQTT);
    
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

    mode = 1;
}

void handleCommand(String command, String message)
{
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Started");
    System::sendDebugMessage("DebugLow", (String)__FUNCTION__, "command: " + command + " value: " + message);
    for (uint8_t i = 0; i < commandsLength; i++)
    {
        if(commands[i] == command)
        {
            String sValue = "";
            // Bool
            debugMode = false;
            RX5808::setDebugMode(debugMode);
            if(command == "power" || command == "autoUpdate" || command == "autoReset" || command == "logRSSI" || command == "demoMode")
            {
                uint8_t dstValue = getBoolFromString(message);
                uint8_t curValue = 0;

                if (command == "power") curValue = power;
                else if (command == "autoUpdate") curValue = autoUpdate;
                else if (command == "autoReset") curValue = RX5808::autoReset;
                else if (command == "logRSSI") curValue = logRSSI;
                else if (command == "logRSSI") curValue = demoMode;

                if (dstValue == 2) curValue = !curValue;
                else curValue = dstValue;

                if (command == "power") power = curValue;
                else if (command == "autoUpdate") autoUpdate = curValue;
                else if (command == "autoReset") RX5808::autoReset = curValue;
                else if (command == "logRSSI") logRSSI = curValue;
                else if (command == "logRSSI") demoMode = curValue;

                sValue = (String)curValue;        
            }
            // Int
            else if (command == "mode" || command == "brightness" || command == "logLevel")
            {
                uint8_t dstValue = message.toInt();
                if(dstValue >= 0)
                {
                    if(command == "mode") mode = dstValue;
                    else if(command == "brightness") Animations::brightness = dstValue;
                    else if(command == "logLevel") System::logLevel = dstValue;
                }

                sValue = (String)dstValue;
            }
            // String
            else if (command == "name")
            {
                String name = message;

                if (name.length() > 0)
                {
                    System::persistentData.espid = name;
                    System::saveEEPROM(System::persistentData);
                    System::sendStat(command, (String)System::persistentData.espid);
                    esp_restart();
                }
                else
                {
                    System::sendStat(command, "Invalid value");
                }
            }
            // Commands
            else if (command == "restart")
            {
                System::sendStat(command, "OK");
                esp_restart();
            }
            else if (command == "update")
            {
                System::sendStat(command, "OK");
                checkUpdate();
            }
            else if (command == "resetRSSI")
            {
                for (int i = 0; i < 8; i++) RX5808::resetMaxRssi(i);
                System::sendStat(command, "OK");
            }
            else if (command == "debug")
            {
                debugMode = true;
                RX5808::setDebugMode(debugMode);
                System::sendStat(command, "OK");
            }
            else
            {
                sValue = (String)false;
            }
            
            if(sValue.length() > 0) System::sendStat(command, sValue);
        }
    }
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Done");
}

void handleSerial()
{
    
    if (Serial.available() > 0)
    {
        System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Started");
        System::sendDebugMessage("Info", (String)__FUNCTION__, "Message received, processing.");

        String input = Serial.readString();
        System::sendDebugMessage("Info", (String)__FUNCTION__, "Message is: '" + input + "'");

        handleBacklog(input);
        System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Done");
    }
    
}

void handleBacklog(String input)
{
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Started");
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "commandsLength: " + (String)commandsLength);
    for (uint8_t i = 0; i < commandsLength; i++)
    {
        System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "i: " + (String)i);
        String parameter = commands[i];
        System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "parameter: " + (String)parameter);
        String value = getParameterFromString(input, parameter);
        System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "value: " + (String)value);
        if (value != "N/A")
        {
            handleCommand(parameter, value);
        }
    }
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Done");
}

void handleMQTT(char *cTopic, byte *bMessage, unsigned int length)
{
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Started");
    // ToDo: save all settings in Struct and consider EEPROM
    String sMessage;
    String sTopic = (String)cTopic;
    for (int i = 0; i < length; i++)
    {
        sMessage += (char)bMessage[i];
    }
    System::sendDebugMessage("Info", (String)__FUNCTION__, "Message arrived on topic: " + sTopic + ". Message: " + sMessage);
    if (sTopic.indexOf("backlog") >= 0) {
        handleBacklog(sMessage);
    }
    else
    {
        handleCommand(getCommandTopic((String)cTopic), sMessage);
    }
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Done");
}

// ToDo: put inside system and call Animations::changeAnimation();
void checkUpdate()
{
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Started");
    System::sendTele("uptime: " + (String)millis() + " ms, FW: " + (String)FIRMWARE_VERSION + ". Checking for updates..");
    char *url = System::checkForUpdate(digicert_pem_start);
    if (strlen(url) != 0)
    {
        //update available
        mode = 0;
        power = 1;
        Animations::update();
        if( System::do_firmware_upgrade(url, digicert_pem_start) == ESP_OK) {
            Animations::updateDone();
            esp_restart();
        } else {
            Animations::error();
        }
        power = 0;
        mode = 10;
    }
    else
    {
        // no update
        System::sendDebugMessage("Warning", (String)__FUNCTION__, "No File");
    }
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Done");
}

void logRssi()
{
    
    StaticJsonDocument<250> doc;
    doc["sensor"] = "rssi";
    doc["time"] = millis();
    JsonArray data = doc.createNestedArray("data");
    
    int *rssi = RX5808::rssi;
    String values = "Values: ";
    for (int i = 0; i < 8; i++)
    {
        data.add(rssi[i]);
        values += i + 1;
        values += ": ";
        values += rssi[i];
        values += ", ";
    }
    String serializedJson;
    serializeJson(doc, serializedJson);
    Serial.println(serializedJson.c_str());
    System::sendRssi(serializedJson);
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

uint8_t getBoolFromString(String input)
{
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Started");
    if (input.indexOf("0") >= 0 || input.indexOf("OFF") >= 0 || input.indexOf("off") >= 0)
    {
        return 0;
    }
    else if (input.indexOf("1") >= 0 || input.indexOf("ON") >= 0 || input.indexOf("on") >= 0)
    {
        return 1;
    }
    else if (input.indexOf("2") >= 0 || input.indexOf("TOGGLE") >= 0 || input.indexOf("toggle") >= 0)
    {
        return 2;
    }
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Done");
}

String getParameterFromString(String input, String parameter)
{
    System::sendDebugMessage("DebugHigh", "getParameterFromString", "Started");
    String value = "N/A";
    int8_t parameterIndex = input.indexOf(parameter);
    if (parameterIndex >= 0)
    {
        int8_t beginIndex = input.indexOf(":", parameterIndex);
        int8_t endIndex = input.indexOf(";", beginIndex);
        if (beginIndex > parameterIndex && beginIndex < endIndex)
        {
            value = input.substring(beginIndex + 2, endIndex);
        }
    }
    System::sendDebugMessage("DebugHigh", "getParameterFromString", "Parameter '" + parameter + "' is: '" + value + "'");
    System::sendDebugMessage("DebugHigh", "getParameterFromString", "Done");
    return value;
}

String getCommandTopic(String topic)
{
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Started");
    String command = "N/A";

    uint8_t parameterIndex = topic.indexOf("cmnd/");
    if (parameterIndex >= 0)
    {
        int beginIndex = topic.indexOf("/", parameterIndex);
        if(topic.indexOf("/", beginIndex + 1) == -1)
        {
            command = topic.substring(beginIndex + 1);
        }
    }
    System::sendDebugMessage("DebugHigh", (String)__FUNCTION__, "Done");
    return command;
}


void Task1code(void *pvParameters)
{
    System::sendDebugMessage("Info", (String)__FUNCTION__, "Task1 running on core " + (String)xPortGetCoreID());

    for (;;)
    {
        RX5808::loop(); // blocking ca 500ms
        if (logRSSI) logRssi();
    }
}

void loop()
{
    // BootMode
    if (bootFlag)
    {
        System::setup_wifi(); // todo: do reconnects every 5 seconds to free rx scanning
        System::reconnect();
        mode = 0;
        power = 1;
        if (autoUpdate)
            checkUpdate();
        Animations::startup(); // caution: BLOCKING!!
        power = 0;
        mode = 10;

        bootFlag = 0;
    }

    if(demoMode)
    {
        mode = 10;
        power = 1;
        if (!powerFlag)
        {
            Animations::on();
            powerFlag = 1;
        }
        EVERY_N_MINUTES(1)
        {
            mode = Animations::doOverflow(mode + 1, 10, 16);
        }
    }

    if (power)
    {
        if (!powerFlag)
        {
            Animations::on();
            powerFlag = 1;
        }

        int nearest = RX5808::getNearestDrone();
        if (mode == 0)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        else if (nearest != 0)
        {
            Animations::setChannelColor(nearest);
            //System::sendTele("saw [" + (String)nearest + "] maxrssi = " + (String)RX5808::maxRssi[nearest]);
        }
        else
        {
            switch (mode)
            {
                case 10: Animations::wingRotationRGB(); break;
                case 11: Animations::rainbow(); break;
                case 12: Animations::rainbowWithGlitter(); break;
                case 13: Animations::confetti(); break;
                case 14: Animations::sinelon(); break;
                case 15: Animations::juggle(); break;
                case 16: Animations::bpm(); break;
                case 17: Animations::animation = &Animations::bpm; break;
                case 95: Animations::startup(); break;
                case 96: Animations::update(); break;
                case 97: Animations::initEEPROM(); break;
                case 98: Animations::standby(); break;
                default: vTaskDelay(100 / portTICK_PERIOD_MS); break;
            }
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
    // loop tasks
    System::loop();
    handleSerial();
    Animations::loop();

    // Periodic tasks
    if(autoUpdate)
    {
        EVERY_N_MINUTES(10) checkUpdate();
    }
    EVERY_N_SECONDS(checkConnectionDelayS)
    {
        if (WiFi.status() != WL_CONNECTED) System::setup_wifi(); // BLOCKING
        if (!System::mqttClient.connected()) System::reconnect(); // BLOCKING // todo: do reconnects every 5 seconds to free rx scanning
    }
}