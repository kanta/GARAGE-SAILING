///
/// \file SerialRadio.ino
/// \brief Radio implementation using the Serial communication.
///
/// \author Matthias Hertel, http://www.mathertel.de
/// \copyright Copyright (c) by Matthias Hertel.\n
/// This work is licensed under a BSD 3-Clause license.\n
/// See http://www.mathertel.de/License.aspx
///
/// \details
/// This is a Arduino sketch radio implementation that can be controlled using commands on the Serial input.
/// It can be used with various chips after adjusting the radio object definition.\n
/// Open the Serial console with 115200 baud to see current radio information and change various settings.
///
/// Wiring
/// ------
/// The necessary wiring of the various chips are described in the Testxxx example sketches.
/// No additional components are required because all is done through the serial interface.
///
/// More documentation and source code is available at http://www.mathertel.de/Arduino
///
/// History:
/// --------
/// * 05.08.2014 created.
/// * 04.10.2014 working.

#include <Arduino.h>
#include <Wire.h>

#include <radio.h>

// all possible radio chips included.
#include <RDA5807M.h>
#include <SI4703.h>
#include <SI4705.h>
#include <SI4721.h>
#include <TEA5767.h>

// #include <RDSParser.h>

#define NUM_OF_SW 4
constexpr uint8_t swPin[] = {13, 26, 14, 25};
#define LED_POWER_PIN 2

// Define some stations available at your locations here:
// 89.40 MHz as 8940

RADIO_FREQ preset[] = {
    8770,
    8810, // hr1
    8820,
    8850, // Bayern2
    8890, // ???
    8930, // * hr3
    8980,
    9180,
    9220, 9350,
    9440, // * hr1
    9510, // - Antenne Frankfurt
    9530,
    9560, // Bayern 1
    9680, 9880,
    10020, // planet
    10090, // ffh
    10110, // SWR3
    10030, 10260, 10380, 10400,
    10500 // * FFH
};

int i_sidx = 5; ///< Start at Station with index=5

RDA5807M radio; ///< Create an instance of a RDA5807 chip radio

/// State of Keyboard input for this radio implementation.
enum RADIO_STATE
{
  STATE_PARSECOMMAND, ///< waiting for a new command character.
  STATE_PARSEINT,     ///< waiting for digits for the parameter.
  STATE_EXEC          ///< executing the command.
};

RADIO_STATE kbState; ///< The state of parsing input characters.
char kbCommand;
int16_t kbValue;

bool lowLevelDebug = false;

/// Update the Frequency on the LCD display.
void DisplayFrequency(RADIO_FREQ f)
{
  char s[12];
  radio.formatFrequency(s, sizeof(s));
  Serial.print("FREQ:");
  Serial.println(s);
} // DisplayFrequency()

/// Update the ServiceName text on the LCD display.
void DisplayServiceName(char *name)
{
  Serial.print("RDS:");
  Serial.println(name);
} // DisplayServiceName()

// - - - - - - - - - - - - - - - - - - - - - - - - - -

// void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4)
// {
//   rds.processData(block1, block2, block3, block4);
// }

/// Execute a command identified by a character and an optional number.
/// See the "?" command for available commands.
/// \param cmd The command character.
/// \param value An optional parameter for the command.
void runSerialCommand(char cmd, int16_t value)
{
  if (cmd == '?')
  {
    Serial.println();
    Serial.println("? Help");
    Serial.println("+ increase volume");
    Serial.println("- decrease volume");
    Serial.println("> next preset");
    Serial.println("< previous preset");
    Serial.println(". scan up   : scan up to next sender");
    Serial.println(", scan down ; scan down to next sender");
    Serial.println("fnnnnn: direct frequency input");
    Serial.println("i station status");
    Serial.println("s mono/stereo mode");
    Serial.println("b bass boost");
    Serial.println("u mute/unmute");
  }

  // ----- control the volume and audio output -----

  else if (cmd == '+')
  {
    // increase volume
    int v = radio.getVolume();
    if (v < 15)
      radio.setVolume(++v);
  }
  else if (cmd == '-')
  {
    // decrease volume
    int v = radio.getVolume();
    if (v > 0)
      radio.setVolume(--v);
  }

  else if (cmd == 'u')
  {
    // toggle mute mode
    radio.setMute(!radio.getMute());
  }

  // toggle stereo mode
  else if (cmd == 's')
  {
    radio.setMono(!radio.getMono());
  }

  // toggle bass boost
  else if (cmd == 'b')
  {
    radio.setBassBoost(!radio.getBassBoost());
  }

  // ----- control the frequency -----

  else if (cmd == '>')
  {
    // next preset
    if (i_sidx < (sizeof(preset) / sizeof(RADIO_FREQ)) - 1)
    {
      i_sidx++;
      radio.setFrequency(preset[i_sidx]);
    } // if
  }
  else if (cmd == '<')
  {
    // previous preset
    if (i_sidx > 0)
    {
      i_sidx--;
      radio.setFrequency(preset[i_sidx]);
    } // if
  }
  else if (cmd == 'f')
  {
    radio.setFrequency(value);
  }

  else if (cmd == '.')
  {
    radio.seekUp(false);
  }
  else if (cmd == ':')
  {
    radio.seekUp(true);
  }
  else if (cmd == ',')
  {
    radio.seekDown(false);
  }
  else if (cmd == ';')
  {
    radio.seekDown(true);
  }

  else if (cmd == '!')
  {
    // not in help
    RADIO_FREQ f = radio.getFrequency();
    if (value == 0)
    {
      radio.term();
    }
    else if (value == 1)
    {
      radio.init();
      radio.setBandFrequency(RADIO_BAND_FM, f);
    }
  }
  else if (cmd == 'i')
  {
    // info
    char s[12];
    radio.formatFrequency(s, sizeof(s));
    Serial.print("Station:");
    Serial.println(s);
    Serial.print("Radio:");
    radio.debugRadioInfo();
    Serial.print("Audio:");
    radio.debugAudioInfo();
  }
  else if (cmd == 'x')
  {
    radio.debugStatus(); // print chip specific data.
  }
  else if (cmd == '*')
  {
    lowLevelDebug = !lowLevelDebug;
    radio._wireDebug(lowLevelDebug);
  }
} // runSerialCommand()

void scanSw()
{
  static bool swState[NUM_OF_SW] = {1};
  int v;
  for (uint8_t i = 0; i < NUM_OF_SW; i++)
  {
    if (digitalRead(swPin[i]) != swState[i])
    {
      swState[i] = !swState[i];
      Serial.printf("%d,%d\n", i, swState[i]);
      if (!swState[i])
      {
        switch (i)
        {
        case 0:
          // increase volume
          v = radio.getVolume();
          if (v < 15)
            radio.setVolume(++v);
          break;
        case 1:
          v = radio.getVolume();
          if (v > 0)
            radio.setVolume(--v);
          break;
        case 2:
          Serial.println("Seek Up.");
          radio.seekUp(true);
          break;
        case 3:
          break;
        default:
          break;
        }
      }
    }
  }
  touch_value_t t = touchRead(T5);
  Serial.printf("Touch: %d\n", t);
  digitalWrite(LED_POWER_PIN, (t<40));
}
/// Setup a FM only radio configuration with I/O for commands and debugging on the Serial port.
void setup()
{
  // open the Serial port
  Serial.begin(115200);
  Serial.print("Radio...");
  for (uint8_t i = 0; i < NUM_OF_SW; i++)
  {
    pinMode(swPin[i], INPUT_PULLUP);
  }
  pinMode(LED_POWER_PIN, OUTPUT);
  delay(500);

#ifdef ESP8266
  // For ESP8266 boards (like NodeMCU) the I2C GPIO pins in use
  // need to be specified.
  Wire.begin(D2, D1); // a common GPIO pin setting for I2C
#endif

  // Enable information to the Serial port
  radio.debugEnable(true);
  radio._wireDebug(lowLevelDebug);

  // Initialize the Radio
  radio.init();

  radio.setBandFrequency(RADIO_BAND_FMWORLD, preset[i_sidx]); // 5. preset.

  // delay(100);
  radio.setMono(false);
  radio.setMute(false);
  radio.setVolume(3);

  Serial.write('>');

  // setup the information chain for RDS data.
  // radio.attachReceiveRDS(RDS_process);
  // rds.attachServicenNameCallback(DisplayServiceName);

  runSerialCommand('?', 0);
  kbState = STATE_PARSECOMMAND;
} // Setup

void loop()
{
  static uint32_t lastSwPollTime = 0;
  uint32_t currentTime = millis();
  if ((uint32_t)(currentTime-lastSwPollTime) >= 30)
  {
    scanSw();
    lastSwPollTime = currentTime;
  }
}

/// Constantly check for serial input commands and trigger command execution.
void _loop()
{
  int newPos;
  unsigned long now = millis();
  static unsigned long nextFreqTime = 0;
  static unsigned long nextRadioInfoTime = 0;

  // some internal static values for parsing the input
  static RADIO_FREQ lastf = 0;
  RADIO_FREQ f = 0;

  if (Serial.available() > 0)
  {
    // read the next char from input.
    char c = Serial.read();

    if ((kbState == STATE_PARSECOMMAND) && (c < 0x20))
    {
      // ignore unprintable chars
      // Serial.read();
    }
    else if (kbState == STATE_PARSECOMMAND)
    {
      // read a command.
      kbCommand = c; // Serial.read();
      kbState = STATE_PARSEINT;
    }
    else if (kbState == STATE_PARSEINT)
    {
      if ((c >= '0') && (c <= '9'))
      {
        // build up the value.
        // c = Serial.read();
        kbValue = (kbValue * 10) + (c - '0');
      }
      else
      {
        // not a value -> execute
        runSerialCommand(kbCommand, kbValue);
        kbCommand = ' ';
        kbState = STATE_PARSECOMMAND;
        kbValue = 0;
      } // if
    }   // if
  }     // if

  // check for RDS data
  // radio.checkRDS();

  // update the display from time to time
  if (now > nextFreqTime)
  {
    f = radio.getFrequency();
    if (f != lastf)
    {
      // print current tuned frequency
      DisplayFrequency(f);
      lastf = f;
    } // if
    nextFreqTime = now + 400;
  } // if

} // loop

// End.