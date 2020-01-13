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
uint8_t Animations::defaultBrightness = 200;
uint8_t Animations::brightness = 255;
bool Animations::offset = false;

uint8_t gHue = 0;
uint8_t pulseBrightness = 0;
uint8_t pos = 0;
uint8_t loopvar = 0;

void Animations::on()
{
    brightness = defaultBrightness; 
    FastLED.setBrightness(brightness);
    FastLED.show();
}
void Animations::off() { 
    brightness = 0; 
    FastLED.setBrightness(brightness);
    FastLED.show();
}

void Animations::standby(CRGB *leds)
{
    // EVERY_N_MILLISECONDS(500)
    // {
    //     FastLED.clear();
    //     for (int led = offset; led < NUM_LEDS; led += 2)
    //     {
    //         leds[led] = CRGB::White;
    //     }
    //     FastLED.setBrightness(brightness);
    //     FastLED.show();
    //     offset = !offset;
    // }
    //Animations::pulseCircleRGB(leds);
    Animations::wingRotationRGB(leds);
}

void Animations::setChannelColor(CRGB *leds, uint8_t channel)
{
    if (loopvar < NUM_LEDS)
    {
        EVERY_N_MILLISECONDS(2)
        {
            leds[loopvar] = channelColors[channel];
            FastLED.show();

            loopvar++;
            if (loopvar >= NUM_LEDS) loopvar = 0;
        }
    }
}

void Animations::startup(CRGB *leds)
{
    Serial.print("Boot Animation");
    Animations::circle(leds, CRGB::Green);
    Serial.println(" Done");
}
void Animations::update(CRGB *leds)
{
    Serial.print("EEPROM Animation");
    Animations::circle(leds, CRGB::Red);
    Serial.println(" Done");
}
void Animations::initEEPROM(CRGB *leds)
{
    Serial.print("Update Animation");
    Animations::circle(leds, CRGB::Blue);
    Serial.println(" Done");
}

void Animations::circle(CRGB *leds, CRGB color)
{
    delay(50);
    uint8_t speed = 5;
    //FastLED.clear();
    FastLED.setBrightness(brightness);
    for (int led = 0; led < NUM_LEDS; led++)
    {
        Serial.print(".");
        leds[led] = color;
        FastLED.show();
        delay(speed);
    }
    for (int led = 0; led < NUM_LEDS; led++)
    {
        Serial.print(".");
        leds[led] = CRGB::Black;
        FastLED.show();
        delay(speed);
    }
    FastLED.clear();
    delay(50);
}

void Animations::party(CRGB *leds)
{
    // do cool stuff here
}

uint8_t Animations::doOverflow(uint8_t value, uint8_t minimum, uint8_t maximum)
{
    if (value > maximum)
    {
        return (minimum - 1) + (value - maximum);
    }
    else if (value < minimum)
    {
        return (maximum + 1) - (minimum - value);
    }
    else
    {
        return value;
    }
}

void Animations::pulseCircleRGB(CRGB *leds)
{
    EVERY_N_MILLISECONDS(20) { gHue++; }
    EVERY_N_MILLISECONDS(1000 / 200)
    {
        pulseBrightness++;
        CRGBPalette16 palette = PartyColors_p;
        for (int i = 0; i < NUM_LEDS; i++)
        { //9948
            leds[i] = ColorFromPalette(palette, gHue + (i * 2), pulseBrightness - gHue + (i * 10));
        }
        FastLED.show();
    }
}

void Animations::wingRotationRGB(CRGB * leds)
{
    EVERY_N_MILLISECONDS(1000 / 60) { gHue++; }                                             // color speed
    EVERY_N_MILLISECONDS(1000 / 20) { pos = Animations::doOverflow(pos + 1, 0, NUM_LEDS); } // rotation speed
    EVERY_N_MILLISECONDS(1000 / 60)
    { // render speed
        // a colored dot sweeping back and forth, with fading trails
        fadeToBlackBy(leds, NUM_LEDS, 5);
        for (size_t i = 0; i < 3; i++)
        {
            uint8_t tmpPos = Animations::doOverflow(pos + (i * 30), 0, NUM_LEDS);
            leds[tmpPos] = CHSV(Animations::doOverflow(gHue + (i * (85)), 0, 255), 255, 192);
        }

        //leds[pos] += CHSV(gHue, 255, 192);
        FastLED.show();
    }
}

void Animations::rainbow(CRGB *leds)
{
    EVERY_N_MILLISECONDS(20) { gHue++; }
    // FastLED's built-in rainbow generator
    fill_rainbow(leds, NUM_LEDS, gHue, 7);
    FastLED.show();
}

void Animations::rainbowWithGlitter(CRGB *leds)
{
    // built-in FastLED rainbow, plus some random sparkly glitter
    Animations::rainbow(leds);
    Animations::addGlitter(leds, 80);
}

void Animations::addGlitter(CRGB *leds, fract8 chanceOfGlitter)
{
    if (random8() < chanceOfGlitter)
    {
        leds[random16(NUM_LEDS)] += CRGB::White;
    }
    FastLED.show();
}

void Animations::confetti(CRGB *leds)
{
    EVERY_N_MILLISECONDS(20) { gHue++; }
    // random colored speckles that blink in and fade smoothly
    fadeToBlackBy(leds, NUM_LEDS, 10);
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV(gHue + random8(64), 200, 255);
    FastLED.show();
}

void Animations::sinelon(CRGB *leds)
{
    EVERY_N_MILLISECONDS(20) { gHue++; }
    // a colored dot sweeping back and forth, with fading trails
    fadeToBlackBy(leds, NUM_LEDS, 20);
    int pos = beatsin16(13, 0, NUM_LEDS - 1);
    leds[pos] += CHSV(gHue, 255, 192);
    FastLED.show();
}

void Animations::bpm(CRGB *leds)
{
    EVERY_N_MILLISECONDS(20) { gHue++; }
    // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
    uint8_t BeatsPerMinute = 62;
    CRGBPalette16 palette = PartyColors_p;
    uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);

    for (int i = 0; i < NUM_LEDS; i++)
    { //9948
        leds[i] = ColorFromPalette(palette, gHue + (i * 2), beat - gHue + (i * 10));
    }
    FastLED.show();
}

void Animations::juggle(CRGB *leds)
{
    // eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy(leds, NUM_LEDS, 20);
    byte dothue = 0;
    for (int i = 0; i < 8; i++)
    {
        leds[beatsin16(i + 7, 0, NUM_LEDS - 1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
    FastLED.show();
}