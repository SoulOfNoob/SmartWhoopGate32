#include <Arduino.h>
#include <config.h>
#include <Animations.h>
#include <FastLED.h>

const CRGB Animations::standbyColor = CRGB::White;

const CRGB Animations::channelColors[8] = {
    CRGB::Green,
    CRGB::Red,
    CRGB::Blue,
    CRGB::Yellow,
    CRGB::Pink,
    CRGB::Purple,
    CRGB::YellowGreen,
    CRGB::Magenta
};

unsigned int Animations::ledTime = 0;
uint8_t Animations::brightness = 200;
bool Animations::offset = false;

void Animations::startup(CRGB *leds)
{
    Serial.print("Boot Animation");
    Animations::circle(leds, CRGB::Green);
    Serial.println(" Done");
}
void Animations::update(CRGB *leds)
{
    Serial.print("Update Animation");
    Animations::circle(leds, CRGB::Red);
    Serial.println(" Done");
}

void Animations::standby(CRGB *leds)
{
    EVERY_N_MILLISECONDS(500)
    {
        FastLED.clear();
        for (int led = offset; led < NUM_LEDS; led += 2)
        {
            leds[led] = CRGB::White;
        }
        FastLED.setBrightness(brightness);
        FastLED.show();
        offset = !offset;
    }
}

void Animations::off(CRGB *leds)
{
    FastLED.clear();
    for (int led = 0; led < NUM_LEDS; led++)
    {
        leds[led] = CRGB::Black;
    }
    FastLED.setBrightness(0);
    FastLED.show();
}

void Animations::circle(CRGB *leds, CRGB color)
{
    delay(50);
    FastLED.clear();
    FastLED.setBrightness(brightness);
    for (int led = 0; led < NUM_LEDS; led++)
    {
        Serial.print(".");
        leds[led] = color;
        FastLED.show();
        delay(5);
    }
    for (int led = 0; led < NUM_LEDS; led++)
    {
        Serial.print(".");
        leds[led] = CRGB::Black;
        FastLED.show();
        delay(5);
    }
    FastLED.clear();
    delay(50);
}

// void clearArray(CRGB *leds)
// {
//     for (int led = 0; led < NUM_LEDS; led++)
//     {
//         leds[led] = CRGB::Black;
//     }
// }

void Animations::setChannelColor(CRGB *leds, uint8_t channel)
{
    if (channel == 0)
    {
        Animations::standby(leds);
    }
    else
    {
        FastLED.clear();
        for (int led = 0; led < NUM_LEDS; led++)
        {
            leds[led] = channelColors[channel];
        }
        FastLED.show();
    }
}