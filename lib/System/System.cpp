#include <Arduino.h>
#include <config.h>
#include <System.h>
#include <WiFi.h>
#include <esp_https_ota.h>
#include <PubSubClient.h>

const char *System::_ssid;
const char *System::_password;
const char *System::_mqtt_server_ip;

WiFiClient System::wifiClient;
PubSubClient System::mqttClient(System::wifiClient);

void System::init(const char *ssid, const char *password, const char *mqtt_server_ip)
{
    _ssid = ssid;
    _password = password;
    _mqtt_server_ip = mqtt_server_ip;
    mqttClient.setServer(_mqtt_server_ip, 1883);
}

void System::setup_wifi()
{
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(_ssid);
    Serial.print("with password ");
    Serial.println(_password);

    WiFi.begin(_ssid, _password);
    //WiFi.begin(char *ssid, char *pass);

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

void System::reconnect()
{
    // Loop until we're reconnected
    while (!System::mqttClient.connected())
    {
        Serial.print("Attempting MQTT connection...");
        // Attempt to connect
        if (mqttClient.connect("jryesp32Client"))
        {
            Serial.println("connected");
            // Subscribe
            mqttClient.subscribe("jryesp32/cmd");
            Serial.println("subscribed");
            mqttClient.publish("jryesp32/output", "Ready to Receive");
            Serial.println("published");
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