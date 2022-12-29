#include <Arduino.h>
#include <FastLED.h>

class RX5808
{
public:
    static void init();
    static void loop();
    static void resetMaxRssi(uint8_t channel);
    static void checkRssi();
    static void checkDroneNear();
    static int getNearestDrone();
    static void setDroneColor();
    static void setDebugMode(bool state);

    static byte defaultBrightness;
    static bool autoReset;

    static int maxRssi[8];
    static int rssi[8];

private:
    static void setupSPIpins();
    static void SERIAL_SENDBIT1();
    static void SERIAL_SENDBIT0();
    static void SERIAL_ENABLE_LOW();
    static void SERIAL_ENABLE_HIGH();
    static void setModuleFrequency(uint16_t frequency);

    static const int rssiMinimum = 2500;
    static uint16_t frequency;
    static bool droneNear[8];
    static int droneNearTime[8];
    static bool debug;
    static int maxRssiTime[8];
    static int ledTime;
    static bool offset;
    static bool debugMode;
};