#include <Arduino.h>
#include <Wire.h>

#include <TaskManager.h>
#include <radio.h>
#include <RDA5807M.h>
// #include <RDSParser.h>

#define LED_POWER_PIN 4U
constexpr uint8_t freqLedPin[3] = {13, 15, 14}; // RGB
#define scanSwPin 26U
#define NUM_OF_TOUCH 3
constexpr uint8_t touchPin[NUM_OF_TOUCH] = {T9, T8, T7}; // 32:AUX, 33:Vol Up, 27:Vol Down

float touchBaseValue[NUM_OF_TOUCH] = {0.0};
constexpr float touchRatioThreshold[NUM_OF_TOUCH] = {0.5, 0.5, 0.5};

#define FREQ_LOW_LIMIT 7600u
#define FREQ_HIGH_LIMIT 10800U //9500U
#define DEFAULT_FREQ 8730U
uint16_t currentFreq = DEFAULT_FREQ;

RDA5807M radio; ///< Create an instance of a RDA5807 chip radio

bool lowLevelDebug = false;

void updatePowerLed()
{
  static uint16_t count = 0;
  digitalWrite(LED_POWER_PIN, (count++ < 1));
  if (count >= 120)
    count = 0;
}

void testTouchRgbLed()
{
  static uint8_t count = 0;
  digitalWrite(freqLedPin[count], LOW);
  count++;
  if (count >= 3)
    count = 0;
  Serial.printf("LED#:%d\n", count);
  digitalWrite(freqLedPin[count], HIGH);
}

void updateRgbLed(float spectrum)
{
  float r, g, b, scale;
  if (spectrum < 0.4f)
  {
    scale = spectrum * (1.0f / 0.4f);
    r = 1.0f - scale;
    g = scale;
    b = 0.0f;
  }
  else if (spectrum < 0.8f)
  {
    scale = (spectrum-0.4f) * (1.0f/0.4f);
    r = 0.0f;
    g = 1.0f - scale;
    b = scale;
  }
  else
  {
    scale = (spectrum-0.8f) * (1.0f/0.2f);
    r = scale/1.5f;
    g = 0.0f;
    b = 1.0f - (scale/2.0f);
  } 
  ledcWrite(0, r*255.0f);
  ledcWrite(1, g*100.0f);
  ledcWrite(2, b*255.0f);

}

void testRgbLed()
{
  static float phase = 0.0f;
  updateRgbLed(phase);
  phase += 0.01;
  if (phase>1.0f)
    phase = 0.0f;
}

void ghostRadio()
{
  uint16_t newFreq = (random(FREQ_LOW_LIMIT, FREQ_HIGH_LIMIT + 1) / 10) * 10;
  radio.setFrequency(newFreq);
      float s = (float)(newFreq-FREQ_LOW_LIMIT)/(float)(FREQ_HIGH_LIMIT-FREQ_LOW_LIMIT);
    updateRgbLed(s);
}

void scanSw()
{
  static bool touchState[NUM_OF_TOUCH] = {true}, swState = true;
  touch_value_t t;
  float ratio;
  int v;
  for (uint8_t i = 0; i < NUM_OF_TOUCH; i++)
  {
    ratio = (float)touchRead(touchPin[i]) / touchBaseValue[i];
    if ((ratio < touchRatioThreshold[i]) != touchState[i])
    {
      touchState[i] = !touchState[i];
      Serial.printf("Touch%d: %d\n", i, touchState[i]);
      digitalWrite(freqLedPin[i], touchState[i]);
      switch (i)
      {
      case 0:
        if (touchState[0])
        {
          Tasks["GhostRadio"]->startIntervalMsec(200);
        }
        else
        {
          Tasks["GhostRadio"]->stop();
        }
        break;
      case 1:
        if (touchState[1])
        {
          // increase volume
          v = radio.getVolume();
          if (v < 15)
            radio.setVolume(++v);
        }
        break;
      case 2:
        if (touchState[2])
        {
          v = radio.getVolume();
          if (v > 0)
            radio.setVolume(--v);
        }
        break;
      default:
        break;
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
    float realf = (float)t / 100.0f;
    Serial.printf("Frequency:%.1fMHz\n", realf);
    freq = t;
    float s = (float)(t-FREQ_LOW_LIMIT)/(float)(FREQ_HIGH_LIMIT-FREQ_LOW_LIMIT);
    updateRgbLed(s);
  }
  else
  {
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    ledcWrite(2, 0);
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
  digitalWrite(LED_POWER_PIN, HIGH);
  for (i = 0; i < 3; i++)
  {
    // pinMode(freqLedPin[i], OUTPUT);
    ledcSetup(i, 12800, 8); 
    ledcAttachPin(freqLedPin[i], i);
  }
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

  Tasks.add([]
            { scanSw(); })
      ->startIntervalMsec(30);

  Tasks.add([]
            { updateRadioFreq(); })
      ->startIntervalMsec(400);

  // LED: power on indicator
  Tasks.add([]
            { updatePowerLed(); })
      ->startIntervalFromMsec(30, 2000);

  // Ghost Radio
  Tasks.add("GhostRadio", []
            { ghostRadio(); });

  // Tasks.add([]
  //           { testRgbLed(); })
  //     ->startIntervalMsec(40);
} // Setup

void loop()
{
  Tasks.update();
}
