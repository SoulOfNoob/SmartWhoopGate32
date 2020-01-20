#include <Arduino.h>
#include <config.h>
#include <System.h>
#include <WiFi.h>
#include "cJSON.h"
#include <esp_https_ota.h>
#include <PubSubClient.h>

PersistentData *System::_persistentData;

WiFiClient System::wifiClient;
PubSubClient System::mqttClient(System::wifiClient);

String System::espid;
String System::fallbackId;
String System::MQTTPrefix = "gates";

String System::genericCmndTopic;
String System::fallbackCmndTopic;
String System::specificCmndTopic;
String System::genericStatTopic;
String System::fallbackStatTopic;
String System::specificStatTopic;
String System::genericTeleTopic;
String System::fallbackTeleTopic;
String System::specificTeleTopic;
String System::specificRssiTopic;

    // receive buffer
    char System::rcv_buffer[200];

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

void System::init(PersistentData *persistentData)
{
    
}

void System::setup_wifi(PersistentData *persistentData)
{
    uint8_t selectedNetwork = 0;    // manual override
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.println();
        Serial.print("Connecting to ");
        Serial.println(persistentData->networks[selectedNetwork].ssid);

        WiFi.begin(persistentData->networks[selectedNetwork].ssid, persistentData->networks[selectedNetwork].pass);

        int counter = 0;
        while (WiFi.status() != WL_CONNECTED)
        {
            //if (counter > 10) esp_restart(); // if it doesn't connect in 5 seconds it never will so restart
            if (counter > 10) break;
            delay(500);
            Serial.print(".");
            counter++;
        }

        if(WiFi.status() != WL_CONNECTED)
        {
            Serial.println("");
            Serial.println("Unable to connect trying next Network");
            if(selectedNetwork < 3)
            {
                selectedNetwork++;
            }
            else
            {
                Serial.println("All Connections failed, restarting.");
                esp_restart();
            }
        }
        else
        {
            Serial.println("");
            Serial.println("WiFi connected");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());
            Serial.println("MAC address: ");
            Serial.println(WiFi.macAddress());
            fallbackId = WiFi.macAddress();
            fallbackId.replace(":", "");

            if (persistentData->espid.indexOf("NONAME") >= 0)
            {
                persistentData->espid = fallbackId;
                Serial.print("fallback Name: ");
                Serial.println(persistentData->espid);
            }

            espid = persistentData->espid;

            genericCmndTopic = MQTTPrefix + "/all/cmnd/#";
            fallbackCmndTopic = MQTTPrefix + "/" + fallbackId + "/cmnd/#";
            specificCmndTopic = MQTTPrefix + "/" + espid + "/cmnd/#";

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

            mqttClient.setServer(persistentData->networks[selectedNetwork].mqtt, 1883);
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
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        String clientName = espid + "_Client";
        if (mqttClient.connect(fallbackId.c_str()))
        {
            Serial.println("connected");
            // Subscribe
            mqttClient.subscribe(genericCmndTopic.c_str());
            Serial.println(("Subscribed: " + genericCmndTopic).c_str());
            mqttClient.subscribe(fallbackCmndTopic.c_str());
            Serial.println(("Subscribed: " + fallbackCmndTopic).c_str());
            mqttClient.subscribe(specificCmndTopic.c_str());
            Serial.println(("Subscribed: " + specificCmndTopic).c_str());

            sendTele("Ready to Receive");
        }
        else
        {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
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
    printf("Looking for a new firmware...\n");

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
            printf("downloaded file is not a valid json, aborting...\n");
            sendTele("downloaded file is not a valid json, aborting...");
        }
        else
        {
            cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
            cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");

            // check the version
            if (!cJSON_IsNumber(version))
            {
                printf("unable to read new version, aborting...\n");
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
                if (new_version > FIRMWARE_VERSION)
                {
                    printf("current firmware version (%.1f) is lower than the available one (%.1f), upgrading...", FIRMWARE_VERSION, new_version);
                    if (cJSON_IsString(file) && (file->valuestring != NULL))
                    {
                        //System::do_firmware_upgrade(file->valuestring, cert);
                        downloadUrl = file->valuestring;
                    }
                    else
                        printf("unable to read the new file name, aborting...\n");
                }
                else
                    printf("current firmware version (%.1f) is greater or equal to the available one (%.1f), nothing to do...\n", FIRMWARE_VERSION, new_version);
            }
        }
    }
    else
        printf("unable to download the json file, aborting...\n");

    // cleanup
    esp_http_client_cleanup(client);

    printf("\n");
    //vTaskDelay(30000 / portTICK_PERIOD_MS);
    return downloadUrl;
}

esp_err_t System::do_firmware_upgrade(const char *url, const char *cert)
{
    Serial.println("downloading and installing new firmware ...");
    sendTele("Starting Update");

    esp_http_client_config_t config = {};
    config.url = url;
    config.cert_pem = cert;

    esp_err_t ret = esp_https_ota(&config);

    if (ret == ESP_OK)
    {
        Serial.println("OTA OK, restarting...");
        sendTele("Update Done");
        delay(1000);
        esp_restart();
    }
    else
    {
        Serial.println("OTA failed...");
        return ESP_FAIL;
    }
    return ESP_OK;
}