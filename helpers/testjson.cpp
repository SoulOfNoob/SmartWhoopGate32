#include "Arduino.h"
#include <WiFi.h>
#include <esp_https_ota.h>
#include "cJSON.h"

#define UPDATE_JSON_URL "https://raw.githubusercontent.com/SoulOfNoob/SmartWhoopGate32/master/ota/esp32/firmware.json"

WiFiClient wifiClient;

// receive buffer
char rcv_buffer[200];

// esp_http_client event handler
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
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

void setup_wifi()
{

    Serial.println();
    Serial.print("Connecting to ");
    Serial.println("ssid");

    WiFi.begin("ssid", "pass");

    int counter = 0;
    while (WiFi.status() != WL_CONNECTED)
    {
        //if (counter > 10) esp_restart(); // if it doesn't connect in 5 seconds it never will so restart
        if (counter > 10)
            break;
        delay(500);
        Serial.print(".");
        counter++;
    }
}

// ToDo: return version number and save url globally
char *checkForUpdate(const char *cert)
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
        }
        else
        {
            cJSON *version = cJSON_GetObjectItemCaseSensitive(json, "version");
            cJSON *file = cJSON_GetObjectItemCaseSensitive(json, "file");

            // check the version
            if (!cJSON_IsNumber(version))
            {
                printf("unable to read new version, aborting...\n");
                
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