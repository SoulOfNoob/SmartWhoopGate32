// Includes
#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <FastLED.h>
#include <RX5808.h>
#include <Animations.h>
#include <System.h>

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
    "name"
};
uint8_t commandsLength = sizeof(commands)/sizeof(commands[0]);

// Switches
bool power = 0;
bool autoUpdate = 1;
bool logRSSI = 0;
bool debug = 1;

// Flags
bool bootFlag = 1;
bool powerFlag = 1;

// Declarations
void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
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
void sendDebugMessage(String position, String message);

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

    Animations::init();
    if (FIRMWARE_VERSION == 0.0) System::initCustomEEPROM();
    // init librarys
    System::init();
    System::setup_wifi();
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
    sendDebugMessage((String)__FUNCTION__, "Started");
    sendDebugMessage((String)__FUNCTION__, "command: " + command + " value: " + message);
    for (uint8_t i = 0; i < commandsLength; i++)
    {
        if(commands[i] == command)
        {
            String sValue = "";
            // Bool
            if(command == "power" || command == "autoUpdate" || command == "autoReset" || command == "logRSSI")
            {
                uint8_t dstValue = getBoolFromString(message);
                uint8_t curValue = 0;

                if (command == "power") curValue = power;
                else if (command == "autoUpdate") curValue = autoUpdate;
                else if (command == "autoReset") curValue = RX5808::autoReset;
                else if (command == "logRSSI") curValue = logRSSI;

                if (dstValue == 2) curValue = !curValue;
                else curValue = dstValue;

                if (command == "power") power = curValue;
                else if (command == "autoUpdate") autoUpdate = curValue;
                else if (command == "autoReset") RX5808::autoReset = curValue;
                else if (command == "logRSSI") logRSSI = curValue;

                sValue = (String)curValue;        
            }
            // Int
            else if (command == "mode" || command == "brightness")
            {
                uint8_t dstValue = message.toInt();
                if(dstValue >= 0)
                {
                    if(command == "mode") mode = dstValue;
                    else if(command == "brightness") Animations::brightness = dstValue;
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
            else
            {
                sValue = (String)false;
            }
            
            if(sValue.length() > 0) System::sendStat(command, sValue);
        }
    }
    sendDebugMessage((String)__FUNCTION__, "Done");
}

void handleSerial()
{
    
    if (Serial.available() > 0)
    {
        sendDebugMessage((String)__FUNCTION__, "Started");
        Serial.println("Message received, processing.");
        String input = Serial.readString();
        Serial.println(("Message is: '" + input + "'").c_str());

        handleBacklog(input);
        sendDebugMessage((String)__FUNCTION__, "Done");
    }
    
}

void handleBacklog(String input)
{
    sendDebugMessage((String)__FUNCTION__, "Started");
    for (uint8_t i = 0; i < commandsLength; i++)
    {
        String parameter = commands[i];
        String value = getParameterFromString(input, parameter);
        if (value != "N/A")
        {
            handleCommand(parameter, value);
        }
    }
    sendDebugMessage((String)__FUNCTION__, "Done");
}

void handleMQTT(char *cTopic, byte *bMessage, unsigned int length)
{
    // ToDo: save all settings in Struct and consider EEPROM
    Serial.print(("Message arrived on topic: " + (String)cTopic + ". Message: ").c_str());
    String sMessage;
    String sTopic = (String)cTopic;
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)bMessage[i]);
        sMessage += (char)bMessage[i];
    }
    Serial.println();

    if (sTopic.indexOf("backlog") >= 0) {
        handleBacklog(sMessage);
    }
    else
    {
        handleCommand(getCommandTopic((String)cTopic), sMessage);
    }
}

//Task1code: blinks an LED every 1000 ms
void Task1code(void *pvParameters)
{
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {        
        System::loop(); // includes mqtt loop
        handleSerial();
        Animations::loop();

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
                switch(mode){
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
    }
}

void loop()
{
    if(autoUpdate)
    {
        EVERY_N_MINUTES(10) checkUpdate();
    }
    // BootMode
    if (bootFlag)
    {
        mode = 0;
        power = 1;
        if(autoUpdate) checkUpdate();
        Animations::startup(); // caution: BLOCKING!!
        power = 0;
        mode = 10;

        bootFlag = 0;
    }
    RX5808::loop(); // blocking ca 500ms
    if (logRSSI)
        logRssi();
}
// ToDo: put inside system and call Animations::changeAnimation();
void checkUpdate()
{
    System::sendTele("uptime: " + (String)millis() + " ms, FW: " + (String)FIRMWARE_VERSION + ". Checking for updates..");
    char *url = System::checkForUpdate(digicert_pem_start);
    if (strlen(url) != 0)
    {
        //update available
        mode = 0;
        power = 1;
        Animations::update();
        System::do_firmware_upgrade(url, digicert_pem_start);
        power = 0;
        mode = 10;
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

uint8_t getBoolFromString(String input)
{
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
}

String getParameterFromString(String input, String parameter)
{
    sendDebugMessage((String)__FUNCTION__, "Started");
    // input = "power: 1; mode: 15; brightness: 150; autoUpdate: OFF; logRSSI = on";
    // parameter = "restart";
    String value = "N/A";

    int8_t parameterIndex = input.indexOf(parameter);
    //Serial.println(("Parameter: " + parameter + " position is: " + parameterIndex).c_str());
    if (parameterIndex >= 0)
    {
        int8_t beginIndex = input.indexOf(":", parameterIndex);
        int8_t endIndex = input.indexOf(";", beginIndex);
        if (beginIndex > parameterIndex && beginIndex < endIndex)
        {
            value = input.substring(beginIndex + 2, endIndex);
        }
    }
    sendDebugMessage((String)__FUNCTION__, "Parameter '" + parameter + "' is: '" + value + "'");
    sendDebugMessage((String)__FUNCTION__, "Done");
    return value;
}

String getCommandTopic(String topic)
{
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

    return command;
}

void sendDebugMessage(String position, String message)
{
    if(debug)
    {
        Serial.println(("[" + position + "] " + message).c_str());
    }
}