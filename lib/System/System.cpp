#include <Arduino.h>
#include <config.h>
#include <System.h>
#include <WiFi.h>
#include "cJSON.h"
#include <esp_https_ota.h>
#include <PubSubClient.h>

PersistentData System::persistentData;

WiFiClient System::wifiClient;
PubSubClient System::mqttClient(System::wifiClient);

String System::espid;
String System::fallbackId;
String System::MQTTPrefix = "gates";

String System::genericCmndTopic;
String System::fallbackCmndTopic;
String System::specificCmndTopic;
String System::genericBacklogTopic;
String System::fallbackBacklogTopic;
String System::specificBacklogTopic;
String System::genericStatTopic;
String System::fallbackStatTopic;
String System::specificStatTopic;
String System::genericTeleTopic;
String System::fallbackTeleTopic;
String System::specificTeleTopic;
String System::specificRssiTopic;

uint8_t System::logLevel;

    // receive buffer
    char System::rcv_buffer[200];

void System::init()
{
    persistentData = loadEEPROM();
}

void System::loop()
{
    if (!mqttClient.connected()) reconnect();
    mqttClient.loop();
}

// esp_http_client event handler
esp_err_t System::_http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        break;
    case HTTP_EVENT_ON_CONNECTED:
        break;
    case HTTP_EVENT_HEADER_SENT:
        break;
    case HTTP_EVENT_ON_HEADER:
        break;
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            strncpy(rcv_buffer, (char *)evt->data, evt->data_len);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        break;
    case HTTP_EVENT_DISCONNECTED:
        break;
    }
    return ESP_OK;
}

void System::setup_wifi()
{
    uint8_t selectedNetwork = 0;    // manual override
    while (WiFi.status() != WL_CONNECTED)
    {
        sendDebugMessage("Info", (String)__FUNCTION__, "Connecting to " + (String)persistentData.networks[selectedNetwork].ssid);
        WiFi.begin(persistentData.networks[selectedNetwork].ssid, persistentData.networks[selectedNetwork].pass);

        int counter = 0;
        while (WiFi.status() != WL_CONNECTED)
        {
            //if (counter > 10) esp_restart(); // if it doesn't connect in 5 seconds it never will so restart
            if (counter > 10) break;
            delay(500);
            sendDebugMessage("Info", (String)__FUNCTION__, ".");
            counter++;
        }

        if(WiFi.status() != WL_CONNECTED)
        {
            sendDebugMessage("Info", (String)__FUNCTION__, "Unable to connect trying next Network");
            if(selectedNetwork < 3)
            {
                selectedNetwork++;
            }
            else
            {
                sendDebugMessage("Info", (String)__FUNCTION__, "All Connections failed, restarting.");
                esp_restart();
            }
        }
        else
        {
            sendDebugMessage("Info", (String)__FUNCTION__, "WiFi connected");
            sendDebugMessage("Info", (String)__FUNCTION__, "IP address: " + (String)WiFi.localIP());
            sendDebugMessage("Info", (String)__FUNCTION__, "MAC address: " + (String)WiFi.macAddress());
            
            fallbackId = WiFi.macAddress();
            fallbackId.replace(":", "");

            if (persistentData.espid.indexOf("NONAME") >= 0)
            {
                persistentData.espid = fallbackId;
                sendDebugMessage("Info", (String)__FUNCTION__, "fallback Name: " + (String)persistentData.espid);
            }

            espid = persistentData.espid;

            genericCmndTopic = MQTTPrefix + "/all/cmnd/#";
            fallbackCmndTopic = MQTTPrefix + "/" + fallbackId + "/cmnd/#";
            specificCmndTopic = MQTTPrefix + "/" + espid + "/cmnd/#";

            genericBacklogTopic = MQTTPrefix + "/all/backlog";
            fallbackBacklogTopic = MQTTPrefix + "/" + fallbackId + "/backlog";
            specificBacklogTopic = MQTTPrefix + "/" + espid + "/backlog";

            genericStatTopic = MQTTPrefix + "/all/stat";
            fallbackStatTopic = MQTTPrefix + "/" + fallbackId + "/stat";
            specificStatTopic = MQTTPrefix + "/" + espid + "/stat";

            genericTeleTopic = MQTTPrefix + "/all/tele";
            fallbackTeleTopic = MQTTPrefix + "/" + fallbackId + "/tele";
            specificTeleTopic = MQTTPrefix + "/" + espid + "/tele";

            specificRssiTopic = MQTTPrefix + "/" + espid + "/rssi";

            // ToDo: MAKE BETTER
            char hostname_c[64];
            String hostname_s = "Drone_" + espid;
            hostname_s.toCharArray(hostname_c, 64);
            WiFi.setHostname(hostname_c);

            mqttClient.setServer(persistentData.networks[selectedNetwork].mqtt, 1883);
            break;
        }
    }
}

void System::sendStat(String command, String message)
{
    mqttClient.publish((genericStatTopic + "/" + command).c_str(), ("[" + espid + "] " + message).c_str());
    mqttClient.publish((fallbackStatTopic + "/" + command).c_str(), ("[" + fallbackId + "] " + message).c_str());
    mqttClient.publish((specificStatTopic + "/" + command).c_str(), ("[" + espid + "] " + message).c_str());
}
void System::sendTele(String message)
{
    mqttClient.publish(genericTeleTopic.c_str(), ("[" + espid + "] " + message).c_str());
    mqttClient.publish(fallbackTeleTopic.c_str(), ("[" + fallbackId + "] " + message).c_str());
    mqttClient.publish(specificTeleTopic.c_str(), ("[" + espid + "] " + message).c_str());
}
void System::sendRssi(String message)
{
    mqttClient.publish(specificRssiTopic.c_str(), ("[" + espid + "] " + message).c_str());
}

void System::reconnect()
{
    // Loop until we're reconnected
    while (!mqttClient.connected())
    {
        sendDebugMessage("Info", (String)__FUNCTION__, "Attempting MQTT connection...");
        // Attempt to connect
        String clientName = espid + "_Client";
        if (mqttClient.connect(fallbackId.c_str()))
        {
            sendDebugMessage("Info", (String)__FUNCTION__, "connected");
            // Subscribe
            mqttClient.subscribe(genericCmndTopic.c_str());
            sendDebugMessage("Info", (String)__FUNCTION__, "Subscribed: " + genericCmndTopic);
            mqttClient.subscribe(fallbackCmndTopic.c_str());
            sendDebugMessage("Info", (String)__FUNCTION__, "Subscribed: " + fallbackCmndTopic);
            mqttClient.subscribe(specificCmndTopic.c_str());
            sendDebugMessage("Info", (String)__FUNCTION__, "Subscribed: " + specificCmndTopic);
            mqttClient.subscribe(genericBacklogTopic.c_str());
            sendDebugMessage("Info", (String)__FUNCTION__, "Subscribed: " + genericBacklogTopic);
            mqttClient.subscribe(fallbackBacklogTopic.c_str());
            sendDebugMessage("Info", (String)__FUNCTION__, "Subscribed: " + fallbackBacklogTopic);
            mqttClient.subscribe(specificBacklogTopic.c_str());
            sendDebugMessage("Info", (String)__FUNCTION__, "Subscribed: " + specificBacklogTopic);

            sendTele("Ready to Receive");
        }
        else
        {
            sendDebugMessage("Error", (String)__FUNCTION__, "failed, rc=" + (String)mqttClient.state() + " try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}


// ToDo: return version number and save url globally
char *System::checkForUpdate(const char *cert)
{
    //UPDATE_JSON_URL
    char *downloadUrl = "";
    sendDebugMessage("Info", (String)__FUNCTION__, "Looking for a new firmware...");

    // configure the esp_http_client
    esp_http_client_config_t config = {};
    config.url = UPDATE_JSON_URL;
    config.event_handler = _http_event_handler;

    esp_http_client_handle_t client = esp_http_client_init(&config);

    // downloading the json file
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        // parse the json file
        cJSON *json = cJSON_Parse(rcv_buffer);
        if (json == NULL){
            sendDebugMessage("Error", (String)__FUNCTION__, "downloaded file is not a valid json, aborting...");
            sendTele("downloaded file is not a valid json, aborting...");
        }
        else
        {
            cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
            cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");

            // check the version
            if (!cJSON_IsNumber(version))
            {
                sendDebugMessage("Error", (String)__FUNCTION__, "unable to read new version, aborting...");
                sendTele("unable to read new version, aborting...");
            }
            else
            {
                double new_version = version->valuedouble;
                String message;
                message += "FW: ";
                message += FIRMWARE_VERSION;
                if(new_version > FIRMWARE_VERSION) message += " < ";
                if(new_version <= FIRMWARE_VERSION) message += " >= ";
                message += new_version;
                if(new_version > FIRMWARE_VERSION) message += ". upgrading...";
                if(new_version <= FIRMWARE_VERSION) message += ". nothing to do...";
                sendTele(message);
                sendDebugMessage("Info", (String)__FUNCTION__, message);
                if (new_version > FIRMWARE_VERSION)
                {
                    if (cJSON_IsString(file) && (file->valuestring != NULL))
                    {
                        //System::do_firmware_upgrade(file->valuestring, cert);
                        downloadUrl = file->valuestring;
                    }
                    else
                    {
                        sendDebugMessage("Error", (String)__FUNCTION__, "unable to read the new file name, aborting...");
                    }
                }
            }
        }
    }
    else
        sendDebugMessage("Error", (String)__FUNCTION__, "unable to download the json file, aborting...");

    // cleanup
    esp_http_client_cleanup(client);

    //vTaskDelay(30000 / portTICK_PERIOD_MS);
    return downloadUrl;
}

esp_err_t System::do_firmware_upgrade(const char *url, const char *cert)
{
    sendDebugMessage("Info", (String)__FUNCTION__, "downloading and installing new firmware ...");
    sendTele("Starting Update");

    esp_http_client_config_t config = {};
    config.url = url;
    config.cert_pem = cert;

    esp_err_t ret = esp_https_ota(&config);

    if (ret == ESP_OK)
    {
        sendDebugMessage("Info", (String)__FUNCTION__, "OTA OK, restarting...");
        sendTele("Update Done");
        delay(1000);
        esp_restart();
    }
    else
    {
        sendDebugMessage("Error", (String)__FUNCTION__, "OTA failed, aborting...");
        return ESP_FAIL;
    }
    return ESP_OK;
}

void System::saveEEPROM(PersistentData eData)
{
    sendDebugMessage("DebugLow", (String)__FUNCTION__, "Writing " + (String)sizeof(eData) + " Bytes to EEPROM.");
    char ok[2 + 1] = "OK";
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.put(0, eData);
    EEPROM.put(0 + sizeof(eData), ok);
    EEPROM.commit();
    EEPROM.end();
}

PersistentData System::loadEEPROM()
{
    PersistentData eData;
    char ok[2 + 1];
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(0, eData);
    EEPROM.get(0 + sizeof(eData), ok);
    EEPROM.end();
    if (String(ok) != String("OK"))
    {
        sendDebugMessage("Warning", (String)__FUNCTION__, "No EEPROM data found, initializing.");
        initCustomEEPROM();
    }
    return eData;
}

// Default config
// put in extra file
void System::initCustomEEPROM()
{
    // ToDo: animation here
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
/**
 * Post debug message to Serial.println().
 *
 * @param level debug level
 * @param position current function
 * @param message message to print
 * @param newline end with new line
 * @param sendTele send debug message also to mqtt
 */
void System::sendDebugMessage(String level, String position, String message, bool sendTele)
{
    uint8_t iLevel = 0;
    if(level ==      "DebugHigh")
    {
        iLevel = 10;
    }
    else if(level == "DebugLow")
    {
        iLevel = 20;
    }
    else if(level == "Info")
    {
        iLevel = 30;
    }
    else if(level == "Warning")
    {
        iLevel = 40;
    }
    else if(level == "Error")
    {
        iLevel = 50;
    }
    else
    {
        iLevel = 200;
    }
    if(iLevel >= logLevel)
    {
        String tmpMessage = "[" + level + "]";
        
        for(uint8_t i = 0; i < 9 - level.length(); i++)
        {
            tmpMessage += " ";
        }

        if(logLevel <= 10 || level == "DebugHigh" || level == "Error")
        {
            tmpMessage += "[" + position + "]";
            if(position.length() < 22) 
            {
                for(int8_t i = 0; i < 22 - position.length(); i++)
                {
                    tmpMessage += " ";
                }
            }
        }
        tmpMessage += " " + message + "";
        Serial.println(tmpMessage.c_str());
    }
}