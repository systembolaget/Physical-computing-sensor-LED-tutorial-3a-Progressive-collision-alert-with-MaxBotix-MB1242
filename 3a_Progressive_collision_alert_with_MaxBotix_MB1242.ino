// Tutorial 3a. Progressive collision alert with MaxBotix MB1242

// Main parts: Adafruit Metro Mini, MaxBotix MB1242, NeoPixel Ring
// 24 x 5050 RGB LED, 8 Ohm loudspeaker, ON Semiconductor TIP120

// Libraries required to interface with the sensor via I2C and to
// drive RGB LEDs; the RunningMedian library was developed by Rob
// Tillaart; use the latest versions
#include <Wire.h>
#include "RunningMedian.h"
#include <Adafruit_NeoPixel.h>

// MaxBotix MB1242 connected to pins A4 (data) and A5 (clock)

// Variables that remain constant
const byte pinData = 4; // Digital output pin to LED ring
const byte numLeds = 24; // Number of LEDs
const int colourMin = 0; // Minimum single value HSV colour (red)
const int colourMax = 32767; // Maximum single value HSV colour (cyan)
const byte speakerPin = 8; // Digital output pin to loudspeaker
const int toneFrequency = 880; // Note A5 in Hz
const byte toneDuration = 50; // Tone duration in milliseconds
const int toneIntervalMin = 75; // Minimum time to pass until the next tone
const int toneIntervalMax = 750; // Maximum time to pass until the next tone
const int distanceMin = 20; // Minimum sensing distance in cm, as per MaxBotix
const int distanceMax = 200; // Maximum sensing distance in cm, up to 765 cm possible
const byte sensorReadInterval = 100; // Time to pass until the next sensor reading
// Instances a Strip object from the library with the number of
// LEDs in the ring, the digital output pin the LED ring is wired
// to and the type of programmable RGB LED used
Adafruit_NeoPixel strip(numLeds, pinData, NEO_GRB + NEO_KHZ800);
// Instances a samples object from the library that holds 5 sensor
// readings, refreshed at each reading cycle (= FIFO order)
RunningMedian samples = RunningMedian(5);

// Variables that can change
int toneInterval = 0; // Time to pass until the next tone
unsigned long timeNowTone = 0; // Timestamp that updates each loop() iteration
unsigned long timeNowSensor = 0; // Timestamp that updates each loop() iteration
word distance = 0; // Sensor's distance reading in cm
int colourAlertHSV = 0; // Single value HSV colour calculated from distance measured

void setup()
{
  // Initialise loudspeaker pin
  pinMode(speakerPin, OUTPUT);

  // Initialise the strip object (= LED array)
  strip.begin();
  // Set the LED ring's overall brightness
  strip.setBrightness(32);
  // Initialize all LEDs to off
  strip.show();

  // Initialise the I2C library to read from the sensor
  Wire.begin();
  // To enter the millis() timed reading cycle, take a single
  // reading with 100ms delay (10Hz), as per MaxBotix
  readDistance();
  delay(100);
}

void loop()
{
  // Check if it is time to read from the sensor
  if (millis() - timeNowSensor >= sensorReadInterval)
  {
    // Create a new timestamp for the next time = loop() execution
    timeNowSensor = millis();

    // A call to this function fetches a new distance reading
    distance = readDistance();

    // Add it to the running median calculation (= FIFO queue)
    samples.add(distance);

    // Now calculate the running median to remove spikes
    float distanceMedian = samples.getMedian();

    // Keep the result within the specified range
    distanceMedian = constrain(distanceMedian, distanceMin, distanceMax);

    // Instead of the faulty Arduino map() implementation, we use the
    // affine transformation f(t) = (t - a) * ((d - c) / (b - a)) + c
    // with t = distanceMedian: calculate the interval between tones
    // and the LED ring's colour, based on distance. To avoid division
    // in speed critical applications, the constant ((d - c) / (b - a))
    // can be pre-computed and stored as an integer
    toneInterval = (distanceMedian - distanceMin) * ((toneIntervalMax - toneIntervalMin) / (distanceMax - distanceMin)) + toneIntervalMin;
    colourAlertHSV = (distanceMedian - distanceMin) * ((colourMax - colourMin) / (distanceMax - distanceMin)) + colourMin;

    // A call to this function commands the sensor to take a
    // reading. The reading will be processed only when the if
    // conditional is true (= next 100ms interval has passed)
    sendRangeCommand();
  }

  // Check if it is time to make a tone
  if (millis() - timeNowTone >= toneInterval)
  {
    // Create a new timestamp for the next time = loop() execution
    timeNowTone = millis();

    // In case the distance is within the specified range
    if ((distance >= distanceMin) && (distance <= distanceMax))
    {
      // Make a tone
      tone(speakerPin, toneFrequency, toneDuration);

      // Also, convert the corresponding colour from HSV to RGB
      uint32_t colourAlertRGB = strip.ColorHSV(colourAlertHSV);
      // And change the LEDs accordingly (0 = all LEDs)
      strip.fill(colourAlertRGB, 0);
      strip.show();
    }

    // In case the distance is not within the specified range
    else
    {
      // Make no tone
      noTone(speakerPin);

      // And switch LEDs off
      strip.clear();
      strip.show();
    }
  }
}

void sendRangeCommand()
{
  // The Arduino Wire library uses 7-bit addressing; to begin
  // a transmission, use 0x70 instead of the 8‑bit value 0xE0
  Wire.beginTransmission(0x70);
  // Send the range command (= 0x51)
  Wire.write(0x51);
  Wire.endTransmission();
}

word readDistance()
{
  // Ask the sensor for two bytes that represent the distance
  Wire.requestFrom(0x70, byte(2));
  // Check if it responds correctly
  if (Wire.available() >= 2)
  {
    // If it did, read the high and low byte
    byte high_byte = Wire.read();
    byte low_byte = Wire.read();
    // Header Arduino.h defines macro word(high_byte, low_byte)
    word distance = word(high_byte, low_byte);
    return distance;
  }

  // If there was an error, return 0
  else
  {
    return word(0);
  }
}
