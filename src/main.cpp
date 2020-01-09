#include <Arduino.h>
#include <FastLED.h>

#define NUM_LEDS 90
#define DATA_PIN 13
#define spiDataPin 16
#define slaveSelectPin 4
#define spiClockPin 2
#define rssiPin A0

#define MIN_TUNE_TIME 50

const uint16_t channelFreqTable[] PROGMEM = {
    //     // Channel 1 - 8
    5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917, // Raceband
    5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725, // Band A
    5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866, // Band B
    5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, // Band E
    5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880, // Band F / Airwave
    5362, 5399, 5436, 5473, 5510, 5547, 5584, 5621, // Band D / 5.3
    5180, 5200, 5220, 5240, 5745, 5765, 5785, 5805, // connex
    5825, 5845, 5845, 5845, 5845, 5845, 5845, 5845  // even more connex, last 6 unused!!!
};

const int rssiMinimum = 2500;
CRGB leds[NUM_LEDS];
uint16_t frequency = 0;
int rssi[8];
bool droneNear[8];
int droneNearTime[8];
bool debug = false;
int maxRssi[8];
int maxRssiTime[8];
int ledTime = 0;

void setupSPIpins()
{
  // SPI pins for RX control
  pinMode(slaveSelectPin, OUTPUT);
  pinMode(spiDataPin, OUTPUT);
  pinMode(spiClockPin, OUTPUT);
}

void SERIAL_SENDBIT1()
{
  //digitalLow(spiClockPin);
  digitalWrite(spiClockPin, 0);
  delayMicroseconds(1);

  //digitalHigh(spiDataPin);
  digitalWrite(spiDataPin, 1);
  delayMicroseconds(1);
  //digitalHigh(spiClockPin);
  digitalWrite(spiClockPin, 1);
  delayMicroseconds(1);

  //digitalLow(spiClockPin);
  digitalWrite(spiClockPin, 0);
  delayMicroseconds(1);
}

void SERIAL_SENDBIT0()
{
  //digitalLow(spiClockPin);
  digitalWrite(spiClockPin, 0);
  delayMicroseconds(1);

  //digitalLow(spiDataPin);
  digitalWrite(spiDataPin, 0);
  delayMicroseconds(1);
  //digitalHigh(spiClockPin);
  digitalWrite(spiClockPin, 1);
  delayMicroseconds(1);

  //digitalLow(spiClockPin);
  digitalWrite(spiClockPin, 0);
  delayMicroseconds(1);
}

void SERIAL_ENABLE_LOW()
{
  delayMicroseconds(1);
  //digitalLow(slaveSelectPin);
  digitalWrite(slaveSelectPin, 0);
  delayMicroseconds(1);
}

void SERIAL_ENABLE_HIGH()
{
  delayMicroseconds(1);
  //digitalHigh(slaveSelectPin);
  digitalWrite(slaveSelectPin, 1);
  delayMicroseconds(1);
}

void setModuleFrequency(uint16_t frequency)
{
  uint8_t i;
  uint16_t channelData;

  channelData = frequency - 479;
  channelData /= 2;
  i = channelData % 32;
  channelData /= 32;
  channelData = (channelData << 7) + i;

  // bit bang out 25 bits of data
  // Order: A0-3, !R/W, D0-D19
  // A0=0, A1=0, A2=0, A3=1, RW=0, D0-19=0
  cli();
  SERIAL_ENABLE_HIGH();
  delayMicroseconds(1);
  SERIAL_ENABLE_LOW();

  SERIAL_SENDBIT0();
  SERIAL_SENDBIT0();
  SERIAL_SENDBIT0();
  SERIAL_SENDBIT1();

  SERIAL_SENDBIT0();

  // remaining zeros
  for (i = 20; i > 0; i--)
  {
    SERIAL_SENDBIT0();
  }

  // Clock the data in
  SERIAL_ENABLE_HIGH();
  delayMicroseconds(1);
  SERIAL_ENABLE_LOW();

  // Second is the channel data from the lookup table
  // 20 bytes of register data are sent, but the MSB 4 bits are zeros
  // register address = 0x1, write, data0-15=channelData data15-19=0x0
  SERIAL_ENABLE_HIGH();
  SERIAL_ENABLE_LOW();

  // Register 0x1
  SERIAL_SENDBIT1();
  SERIAL_SENDBIT0();
  SERIAL_SENDBIT0();
  SERIAL_SENDBIT0();

  // Write to register
  SERIAL_SENDBIT1();

  // D0-D15
  //   note: loop runs backwards as more efficent on AVR
  for (i = 16; i > 0; i--)
  {
    // Is bit high or low?
    if (channelData & 0x1)
    {
      SERIAL_SENDBIT1();
    }
    else
    {
      SERIAL_SENDBIT0();
    }
    // Shift bits along to check the next one
    channelData >>= 1;
  }

  // Remaining D16-D19
  for (i = 4; i > 0; i--)
  {
    SERIAL_SENDBIT0();
  }

  // Finished clocking data in
  SERIAL_ENABLE_HIGH();
  delayMicroseconds(1);

  //digitalLow(slaveSelectPin);
  digitalWrite(slaveSelectPin, 0);
  //digitalLow(spiClockPin);
  digitalWrite(spiClockPin, 0);
  //digitalLow(spiDataPin);
  digitalWrite(spiDataPin, 0);
  sei();

  delay(MIN_TUNE_TIME);
}

void checkRssi()
{
  for (int i = 0; i < 8; i++)
  {
    setModuleFrequency(pgm_read_word_near(channelFreqTable + i));
    analogRead(rssiPin);
    rssi[i] = analogRead(rssiPin); // * 0.01 + rssi[i] * 0.99;
    if (rssi[i] > maxRssi[i])
    {
      maxRssi[i] = rssi[i];
    }
    if (millis() > maxRssiTime[i] + 30000)
    {
      maxRssiTime[i] = millis();
      maxRssi[i] = 3000;
    }
    if ((rssi[i] >= 2000 && debug))
    {
      Serial.print("Channel: ");
      Serial.print(i);
      Serial.print(" Rssi: ");
      Serial.println(rssi[i]);
    }
  }
}

void checkDroneNear()
{
  for (int i = 0; i < 8; i++)
  {
    if (rssi[i] > (maxRssi[i] - 80))
    {
      droneNear[i] = true;
      droneNearTime[i] = millis();
      if (debug)
      {
        Serial.print("Channel: ");
        Serial.print(i);
        Serial.print(" Time: ");
        Serial.println(droneNearTime[i]);
      }
    }
    else if (droneNearTime[i] < millis() - 5000)
    {
      droneNear[i] = false;
      droneNearTime[i] = 0;
    }
  }
}

void setup()
{
  // put your setup code here, to run once:
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  Serial.begin(115200);
  Serial.println(F("START:"));
  setupSPIpins();
  maxRssi[0] = 3000;
  maxRssi[1] = 3000;
  maxRssi[2] = 3000;
  maxRssi[3] = 3000;
  maxRssi[4] = 3000;
  maxRssi[5] = 3000;
  maxRssi[6] = 3000;
  maxRssi[7] = 3000;
  maxRssi[8] = 3000;

  analogReadResolution(12); // Sets the sample bits and read resolution, default is 12-bit (0 - 4095), range is 9 - 12 bits
  analogSetWidth(12);       // Sets the sample bits and read resolution, default is 12-bit (0 - 4095), range is 9 - 12 bits
  //  9-bit gives an ADC range of 0-511
  // 10-bit gives an ADC range of 0-1023
  // 11-bit gives an ADC range of 0-2047
  // 12-bit gives an ADC range of 0-4095

  //analogSetAttenuation(ADC_0db);  //For all pins
  //analogSetPinAttenuation(A18, ADC_0db); //0db attenuation on pin A18
  //analogSetPinAttenuation(A19, ADC_2_5db); //2.5db attenuation on pin A19
  //analogSetPinAttenuation(A6, ADC_6db); //6db attenuation on pin A6
  analogSetPinAttenuation(A0, ADC_2_5db); //11db attenuation on pin A7
                                          /*
  for(int i=0; i<8; i++)
  {
    setModuleFrequency(pgm_read_word_near(channelFreqTable + i));
    analogRead(rssiPin);
    rssi[i] = analogRead(rssiPin);
    maxRssi[i] = 2000;
    maxRssiTime[i] = 0;
  }
  for(int x = 0; x < NUM_LEDS; x++)
  {
    if(x%2==0)
    {
      leds[x] = CRGB::White;
    }
  }
  FastLED.setBrightness(200);
*/
  setModuleFrequency(pgm_read_word_near(channelFreqTable + 0));
}
void setDroneColor()
{
  int newest = 0;
  for (int i = 0; i < 8; i++)
  {
    if (droneNear[i] & (droneNearTime[i] > newest))
    {
      if (debug)
      {
        Serial.print("Channel: ");
        Serial.print(i);
        Serial.println(" Dronenear! ");
      }
      newest = droneNearTime[i];
      for (int x = 0; x < NUM_LEDS; x++)
      {
        if (i == 0)
        {
          leds[x] = CRGB::Green;
        }
        else if (i == 1)
        {
          leds[x] = CRGB::Red;
        }
        else if (i == 2)
        {
          leds[x] = CRGB::Blue;
        }
        else if (i == 3)
        {
          leds[x] = CRGB::Yellow;
        }
        else if (i == 4)
        {
          leds[x] = CRGB::Pink;
        }
        else if (i == 5)
        {
          leds[x] = CRGB::Purple;
        }
        else if (i == 6)
        {
          leds[x] = CRGB::YellowGreen;
        }
        else if (i == 7)
        {
          leds[x] = CRGB::Magenta;
        }
      }
    }
  }
  if (newest == 0)
  {
    if (ledTime + 500 < millis())
    {
      for (int x = 0; x < NUM_LEDS; x++)
      {
        if (x % 2 == 0)
        {
          leds[x] = CRGB::White;
          FastLED.setBrightness(200);
        }
        else
        {
          leds[x] = CRGB::Black;
        }
      }
      ledTime = millis();
    }
    else
    {
      for (int x = 0; x < NUM_LEDS; x++)
      {
        if (x % 2 == 1)
        {
          leds[x] = CRGB::White;
          FastLED.setBrightness(200);
        }
        else
        {
          leds[x] = CRGB::Black;
        }
      }
    }
  }
  else
  {
    FastLED.setBrightness(255);
  }
}

void loop()
{
  checkRssi();
  checkDroneNear();
  setDroneColor();
  FastLED.show();
}