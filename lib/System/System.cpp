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
String System::fallbackTopic;
String System::cmdTopic;
String System::statusTopic;

// receive buffer
char System::rcv_buffer[200];

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
            if(selectedNetwork < 2)
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
            cmdTopic = "Gates/" + espid + "/cmd";
            statusTopic = "Gates/" + espid + "/status";

            fallbackTopic = "Gates/" + fallbackId + "/cmd";

            mqttClient.setServer(persistentData->networks[selectedNetwork].mqtt, 1883);
            break;
        }
    }
}

void System::reconnect()
{
    // Loop until we're reconnected
    while (!mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        String clientName = espid + "_Client";
        if (mqttClient.connect(clientName.c_str()))
        {
            Serial.println("connected");
            // Subscribe
            mqttClient.subscribe("Gates/cmd");
            Serial.println("subscribed to: Gates/cmd");
            mqttClient.subscribe(System::fallbackTopic.c_str());
            Serial.print("subscribed to: ");
            Serial.println(System::fallbackTopic);
            mqttClient.subscribe(System::cmdTopic.c_str());
            Serial.print("subscribed to: ");
            Serial.println(System::cmdTopic);
            mqttClient.publish(System::statusTopic.c_str(), "Ready to Receive");
            Serial.print("published to: ");
            Serial.println(System::statusTopic);
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
        if (json == NULL)
            printf("downloaded file is not a valid json, aborting...\n");
        else
        {
            cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
            cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");

            // check the version
            if (!cJSON_IsNumber(version))
                printf("unable to read new version, aborting...\n");
            else
            {
                double new_version = version->valuedouble;
                if (new_version > FIRMWARE_VERSION)
                {
                    printf("current firmware version (%.1f) is lower than the available one (%.1f), upgrading...\n", FIRMWARE_VERSION, new_version);
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
    mqttClient.publish("jryesp32/output", "Starting Update");

    esp_http_client_config_t config = {};
    config.url = url;
    config.cert_pem = cert;

    esp_err_t ret = esp_https_ota(&config);

    if (ret == ESP_OK)
    {
        Serial.println("OTA OK, restarting...");
        mqttClient.publish("jryesp32/output", "Update Done");
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