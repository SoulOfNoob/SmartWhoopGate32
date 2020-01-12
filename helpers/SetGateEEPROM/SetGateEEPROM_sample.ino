#include <EEPROM.h>

#define EEPROM_SIZE 512

struct NetworkData
{
    char ssid[32];
    char pass[32];
    char mqtt[32];
};

struct PersistentData
{
    String espid;
    NetworkData networks[3];
};

PersistentData writeData = {
  "Name",
  {
    {"SSID1", "PASS1", "MQTT"}, 
    {"SSID2", "PASS2", "MQTT2"}, 
    {"SSID3", "PASS3", "MQTT3"}
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("\n\n\n");
  for (uint8_t t = 3; t > 0; t--)
  {
      Serial.printf("[SETUP] WAIT %d...\n", t);
      Serial.flush();
      delay(1000);
  }
  saveEEPROM(writeData);
}

void loop() {
  printEEPROM(loadEEPROM());
  delay(1000);
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
