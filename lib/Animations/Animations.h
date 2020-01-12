#include <Arduino.h>
#include <config.h>
#include <FastLED.h>

#define DATA_PIN 13
#define NUM_LEDS 90

class Animations
{
public:
    static void startup(CRGB *leds);
    static void update(CRGB *leds);
    static void standby(CRGB *leds);
    static void circle(CRGB *leds, CRGB color);
    static void off(CRGB *leds);

    static void setChannelColor(CRGB *leds, uint8_t channel);

private:
    static const CRGB standbyColor;
    static const CRGB channelColors[8];
    static unsigned int ledTime;
    static uint8_t brightness;
    static bool offset;
};