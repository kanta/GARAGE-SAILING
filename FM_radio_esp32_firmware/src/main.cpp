#include <Arduino.h>
#include <Wire.h>

#include <TaskManager.h>
#include <radio.h>
#include <RDA5807M.h>
// #include <RDSParser.h>

#define LED_POWER_PIN 4U
constexpr uint8_t freqLedPin[3] = {15, 14, 13}; // RGB
#define scanSwPin 26U
#define NUM_OF_TOUCH  3
constexpr uint8_t touchPin[NUM_OF_TOUCH] = {T9, T8, T7};// 32:AUX, 33:Vol Up, 27:Vol Down

float touchBaseValue[NUM_OF_TOUCH] = { 0.0 };
constexpr float touchRatioThreshold[NUM_OF_TOUCH] = { 0.5, 0.5, 0.5 };

#define DEFAULT_FREQ  8730U

RDA5807M radio; ///< Create an instance of a RDA5807 chip radio

bool lowLevelDebug = false;


void scanSw()
{
  static bool touchState[NUM_OF_TOUCH] = { true }, swState = true;
  touch_value_t t;
  float ratio;
  int v;
  for (uint8_t i = 0; i < NUM_OF_TOUCH; i++)
  {
    ratio = (float)touchRead(touchPin[i])/touchBaseValue[i];
    if ((ratio < touchRatioThreshold[i]) != touchState[i] )
    {
      touchState[i] = !touchState[i];
      Serial.printf("Touch%d: %d\n", i, touchState[i]);
      if (!touchState[i])
      {
        switch (i)
        {
        case 1:
          // increase volume
          v = radio.getVolume();
          if (v < 15)
            radio.setVolume(++v);
          break;
        case 2:
          v = radio.getVolume();
          if (v > 0)
            radio.setVolume(--v);
          break;
        case 0:
          // Serial.println("Seek Up.");
          // radio.seekUp(true);
          break;
        default:
          break;
        }
      }
    }
  }
  if (digitalRead(scanSwPin) != swState)
  {
    swState = !swState;
    if (swState)
    {
      Serial.println("Seek Up.");
      radio.seekUp(true);
    }
  }
}

void updateRadioFreq()
{
  static uint16_t freq = 0;
  uint16_t t = radio.getFrequency();
  if (t != freq)
  {
    float realf = (float)t/100.0f;
    Serial.printf("Frequency:%.1fMHz\n", realf);
    freq = t;
  }
  else
  {
    //
  }
}

void setup()
{
  uint8_t i;
  // open the Serial port
  Serial.begin(115200);
  pinMode(RX, INPUT_PULLUP);
  Serial.print("Radio...");
  pinMode(scanSwPin, INPUT_PULLUP);
  pinMode(LED_POWER_PIN, OUTPUT);
  for (i=0; i<3; i++)
    pinMode(freqLedPin[i], OUTPUT);
  for (i = 0; i < NUM_OF_TOUCH; i++)
  {
    touchBaseValue[i] = touchRead(touchPin[i]);
  }
  
  delay(500);

  // Enable information to the Serial port
  radio.debugEnable(true);
  radio._wireDebug(lowLevelDebug);

  // Initialize the Radio
  radio.init();

  radio.setBandFrequency(RADIO_BAND_FMWORLD, DEFAULT_FREQ); 

  // delay(100);
  radio.setMono(false);
  radio.setMute(false);
  radio.setVolume(3);

  Tasks.add([] {
    scanSw();
  })->startIntervalMsec(30);

  Tasks.add([] {
    updateRadioFreq();
  })->startIntervalMsec(400);

  Tasks.add([] {
    digitalWrite(LED_POWER_PIN, !digitalRead(LED_POWER_PIN));
  })->startIntervalMsecForCount(250,6);

} // Setup

void loop()
{
  Tasks.update();
}
