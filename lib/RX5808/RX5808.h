#include <Arduino.h>
#include <FastLED.h>

class RX5808
{
public:
    static void init();
    static void checkRssi();
    static void checkDroneNear();
    static int getNearestDrone();
    static void setDroneColor(CRGB *leds);

    static byte defaultBrightness;

private:
    static void setupSPIpins();
    static void SERIAL_SENDBIT1();
    static void SERIAL_SENDBIT0();
    static void SERIAL_ENABLE_LOW();
    static void SERIAL_ENABLE_HIGH();
    static void setModuleFrequency(uint16_t frequency);

    static const int rssiMinimum = 2500;
    static uint16_t frequency;
    static int rssi[8];
    static bool droneNear[8];
    static int droneNearTime[8];
    static bool debug;
    static int maxRssi[8];
    static int maxRssiTime[8];
    static int ledTime;
    static bool offset;
};