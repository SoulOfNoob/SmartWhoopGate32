#include <Arduino.h>
#include <config.h>
#include <FastLED.h>

#define DATA_PIN 13
#define NUM_LEDS 90

class Animations
{
public:
    static void init(CRGB *leds);
    static void loop();
    static void on();
    static void off();
    static void setAnimation(void (*f)());

    static void startup();
    static void update();
    static void standby();
    static void initEEPROM();
    static void circle(CRGB color);
    static void party();
    static void pulseCircleRGB();
    static void wingRotationRGB();
    static void rainbow();
    static void rainbowWithGlitter();
    static void addGlitter(fract8 chanceOfGlitter);
    static void confetti();
    static void sinelon();
    static void bpm();
    static void juggle();

    static void setChannelColor(uint8_t channel);
    static uint8_t doOverflow(uint8_t value, uint8_t min, uint8_t max);

    static void (*animation)();

private:
    static CRGB *_leds;

    static const CRGB standbyColor;
    static const CRGB channelColors[8];
    static unsigned int ledTime;
    static uint8_t defaultBrightness;
    static uint8_t brightness;

    static bool offset;
};