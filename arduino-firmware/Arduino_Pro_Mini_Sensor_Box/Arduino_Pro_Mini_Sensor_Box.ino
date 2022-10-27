#include <Adafruit_Sensor.h>
#include "Adafruit_TSL2591.h"

#include <arduinoFFT.h>

#include <avr/sleep.h>
#include <RTClib.h>

#include <SPI.h>
#include <SD.h>

#include <Wire.h>
#include <SHT21.h>

#define SAMPLES 64              //SAMPLES-pt FFT. Must be a base 2 number. Max 128 for Arduino Uno.
#define SAMPLING_FREQUENCY 8192 //Ts = Based on Nyquist, must be 2 times the highest expected frequency.

RTC_DS3231 rtc;
arduinoFFT FFT = arduinoFFT();
Adafruit_TSL2591 tsl = Adafruit_TSL2591(2591);

const byte pinMicrophoneRight = A1;
const byte pinMicrophoneLeft = A0;
const byte pinLedGreen = 5;
const byte pinLedRed = 4;
const byte pinInterrupt = 2;
const byte pinSD = 10;
const short sampleWindow = 1000;

double vReal[SAMPLES]; //create vector of size SAMPLES to hold real values
double vImag[SAMPLES]; //create vector of size SAMPLES to hold imaginary values

unsigned int sample;
unsigned int samplingPeriod;
unsigned long microSeconds;

bool initializationError = false;

// Method to initialize device and test all external components
// An initializationError will lead to constant flashing of the red LED
// Successful setup will lead to the green LED blinking a few times
void setup() {
  Wire.begin();//I2C begin
  Serial.begin(115200);
  samplingPeriod = round(1000000*(1.0/SAMPLING_FREQUENCY)); //Period in microseconds

  while (!Serial);

  Serial.println(F("STARTING"));
  delay(1000);

  if (configureRTC()) {
    Serial.println(F("RTC OK"));
  }
  else {
    Serial.println(F("RTC FAILED"));
    initializationError = true;
  }

  if (configureSD()) {
    Serial.println(F("SD OK"));
  }
  else {
    Serial.println(F("SD FAILED"));
    initializationError = true;
  }

  if (configureLightSensor()) {
    Serial.println(F("Light sensor OK"));
  }
  else {
    Serial.println(F("Light sensor FAILED"));
    initializationError = true;
  }

  if (initializationError) {
    flashError();
  }
  else {
    flashOk();
  }

  readSensors();
}

// The loop method will trigger a read of all sensors and then go to sleep until the next interval is triggered
void loop() {
  if (rtc.alarmFired(1)) {
    readSensors();
  }

  sleep();
}

// Read sensor information and persist to the SD Card
void readSensors() {
  Serial.println(F("Reading sensor data"));
  File myFile = SD.open("datav2.txt", FILE_WRITE);
  DateTime now = rtc.now() + TimeSpan(15);

    if(myFile) {
      Serial.print(F("Write to file:"));

      myFile.print(now.day(), DEC);
      myFile.print('.');
      myFile.print(now.month(), DEC);
      myFile.print('.');
      myFile.print(now.year(), DEC);
      myFile.print(';');
      myFile.print(now.hour(), DEC);
      myFile.print(':');
      myFile.print(now.minute(), DEC);
      myFile.print(':');
      myFile.print(now.second(), DEC);
      myFile.print(';');

      uint16_t ir, full;
      float lux = readLight(&ir, &full);

      if(lux == -1) { //Measure again if sensor was too sensitive...
        tsl.setGain(TSL2591_GAIN_MED);
        lux = readLight(&ir, &full);
        if(lux == -1) { //Write 1000 instead of -1 because sensor limit is 1k and our requirement is 100
          lux = 1000;
        }
        tsl.setGain(TSL2591_GAIN_HIGH);
      }

      if (isnan(lux)) { //Write 0 instead of nan;
        lux = 0;
      }

      uint16_t visible = full - ir;

      myFile.print(lux, 3);
      myFile.print(';');
      myFile.print(ir);
      myFile.print(';');
      myFile.print(full);
      myFile.print(';');
      myFile.print(visible);
      myFile.print(';');

      Serial.print(lux, 3);
      Serial.print(F("lx; "));
      Serial.print(ir);
      Serial.print(F("IR; "));
      Serial.print(full);
      Serial.print(F("full; "));
      Serial.print(visible);
      Serial.print(F("visible; "));

      float freqLeft;
      float freqRight;
      float sigLeft = readMicrophone(pinMicrophoneLeft, &freqLeft);
      float sigRight = readMicrophone(pinMicrophoneRight, &freqRight);

      myFile.print(sigLeft, 4);
      myFile.print(';');
      myFile.print(freqLeft, 2);
      myFile.print(';');
      myFile.print(sigRight, 4);
      myFile.print(';');
      myFile.print(freqRight, 2);
      myFile.print(';');

      Serial.print(sigLeft, 4);
      Serial.print(F(" | "));
      Serial.print(freqLeft, 2);
      Serial.print(F("Hz / "));
      Serial.print(sigRight, 4);
      Serial.print(F(" | "));
      Serial.print(freqRight, 2);
      Serial.println(F("Hz; "));

      myFile.println();
      myFile.close();
    }
    else{
      flashError();
    }
}

float readLight(uint16_t *ir, uint16_t *full) {
  uint32_t lum = tsl.getFullLuminosity();
  uint16_t tempIr, tempFull;
  tempIr = lum >> 16;
  tempFull = lum & 0xFFFF;
  float lux = tsl.calculateLux(tempFull, tempIr);
  *ir = tempIr;
  *full = tempFull;

  return lux;
}

float readMicrophone(byte pinMicrophone, float *freq) {
  unsigned long startMillis = millis();
  unsigned int peakToPeak = 0;
  unsigned int signalMax = 0;
  unsigned int signalMin = 1024;
  int i = 0;

  while(millis() - startMillis < sampleWindow && i < SAMPLES) {

    //Returns the number of microseconds since the Arduino board began running the current script.
    microSeconds = micros();

    sample = analogRead(pinMicrophone);

    vReal[i] = sample;
    vImag[i] = 0;

    if (sample < 1024) {
      if (sample > signalMax) {
        signalMax = sample;
      }

      if (sample < signalMin) {
        signalMin = sample;
      }
    }

    while(micros() < (microSeconds + samplingPeriod)) {
      // Do nothing and wait for sample period to finish
    }

    i++;
  }

  FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);

  float peak = FFT.MajorPeak(vReal, SAMPLES, SAMPLING_FREQUENCY);

  peakToPeak = signalMax - signalMin;
  double sig = (peakToPeak * 5.0) / 1024;

  *freq = peak;

  return sig;
}

bool configureRTC() {
  if (!rtc.begin()) return false;

  // Disable the 32K Pin
  rtc.disable32K();

  // Stop oscillating signals at SQW Pin
  rtc.writeSqwPinMode(DS3231_OFF);

  // Clear alarms 1 & alarm 2
  rtc.clearAlarm(1);
  rtc.clearAlarm(2);

  // Disable alarm 2
  rtc.disableAlarm(2);

  // Configure interrupt pin
  pinMode (pinInterrupt, INPUT_PULLUP);
  attachInterrupt (digitalPinToInterrupt (pinInterrupt), onWakeUp, FALLING);

  // Enable sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);

  return true;
}

bool configureSD() {
  if(SD.begin(pinSD)) {
    /*
    File myFile = SD.open("datav2.txt", FILE_WRITE); //Try opening main file
    if (myFile) {
      myFile.close();
      myFile = SD.open("check.txt", FILE_WRITE); //Try opening check file and writing '1' in it
      if (myFile) {
        myFile.println('1');
        myFile.close();
        myFile = SD.open("check.txt", FILE_READ); //Try opening check file again and check if '1' was written
        if (myFile && myFile.read() == '1') {
          myFile.close();
          return true; //Return true if everything works
        }
      }
    }
    */

    return true;
  }

  return false; //Return false if one of those didn't work
}

bool configureLightSensor() {
  if (tsl.begin()) {
    tsl.setGain(TSL2591_GAIN_HIGH);
    tsl.setTiming(TSL2591_INTEGRATIONTIME_600MS);

    return true;
  }

  return false;
}


void sleep() {
  Serial.println(F("SLEEP!"));
  Serial.flush();

  enqueueInterrupt();

  sleep_enable();
  sleep_cpu();
}

void onWakeUp() {
  Serial.println(F("WAKEUP!"));
  sleep_disable();
}

void enqueueInterrupt() {
  rtc.clearAlarm(1);

  // Set alarm for next measuring interval
  rtc.setAlarm1(rtc.now() + TimeSpan(118), DS3231_A1_Second);
}

void flashOk() {
  unsigned long ms = 500;

  pinMode (pinLedGreen, OUTPUT);

  for (byte i = 0; i < 4; i++) {
    digitalWrite(pinLedGreen, HIGH);
    delay(ms);
    digitalWrite(pinLedGreen, LOW);
    delay(ms);
  }
}

void flashError() {
  pinMode(pinLedRed, OUTPUT);
  digitalWrite(pinLedRed, HIGH);
}
