#include <Arduino.h>
#include <config.h>
#include <FastLED.h>

#define DATA_PIN 13
#define NUM_LEDS 90

class Animations
{
public:
    static void on();
    static void off();
    static void startup(CRGB *leds);
    static void update(CRGB *leds);
    static void standby(CRGB *leds);
    static void initEEPROM(CRGB *leds);
    static void circle(CRGB *leds, CRGB color);
    static void party(CRGB *leds);
    static void pulseCircleRGB(CRGB *leds);
    static void rainbow(CRGB *leds);
    static void rainbowWithGlitter(CRGB *leds);
    static void addGlitter(CRGB *leds, fract8 chanceOfGlitter);
    static void confetti(CRGB *leds);
    static void sinelon(CRGB *leds);
    static void bpm(CRGB *leds);
    static void juggle(CRGB *leds);

    static void setChannelColor(CRGB *leds, uint8_t channel);

private:
    static const CRGB standbyColor;
    static const CRGB channelColors[8];
    static unsigned int ledTime;
    static uint8_t defaultBrightness;
    static uint8_t brightness;

    static bool offset;
};