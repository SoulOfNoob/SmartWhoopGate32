#include <Arduino.h>
#include <FastLED.h>
#include <RX5808.h>
#include <WiFi.h>
#include <WebServer.h>
#include <AutoConnect.h>
#include <PubSubClient.h>
#include <WebOTA.h>

#define NUM_LEDS 90
#define DATA_PIN 13

CRGB leds[NUM_LEDS];

// const char *ssid = "SSID";
// const char *password = "PASS";
const char *mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;

void Task1code(void *pvParameters);
void Task2code(void *pvParameters);
void setup_wifi();
void rootPage();
void callback(char *topic, byte *message, unsigned int length);

WebServer Server; // Replace with WebServer for ESP32
AutoConnect Portal(Server);

void setup()
{
    // put your setup code here, to run once:
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
    Serial.begin(115200);

    setup_wifi();
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

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
    Serial.println("new wifi stuff");
    Server.on("/", rootPage);
    if (Portal.begin())
    {
        Serial.println("WiFi connected: " + WiFi.localIP().toString());
    }
    Serial.println("new wifi stuff done");

    // // We start by connecting to a WiFi network
    // Serial.println();
    // Serial.print("Connecting to ");
    // Serial.println(ssid);

    // WiFi.begin(ssid, password);

    // while (WiFi.status() != WL_CONNECTED)
    // {
    //     delay(500);
    //     Serial.print(".");
    // }

    // Serial.println("");
    // Serial.println("WiFi connected");
    // Serial.println("IP address: ");
    // Serial.println(WiFi.localIP());
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
            client.subscribe("jryesp32/input");
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

void rootPage()
{
    char content[] = "Hello, world";
    Server.send(200, "text/plain", content);
}

//Task1code: blinks an LED every 1000 ms
void Task1code(void *pvParameters)
{
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());

    for (;;)
    {
        RX5808::checkRssi();
        RX5808::checkDroneNear();
        RX5808::setDroneColor(NUM_LEDS, leds);
        FastLED.setBrightness(RX5808::brightness);
        FastLED.show();
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

        EVERY_N_MILLISECONDS(5000) 
        {
            client.publish("jryesp32/output", "still alive");
        }
    }
}

void loop()
{
    Portal.handleClient();
}