/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 * Set PC to UTC!!
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

#include <RTClib.h>

#include <avr/sleep.h>
#include <RTClib.h>

#include <SPI.h>
#include <SD.h>

#include <Wire.h>

RTC_DS3231 rtc;

const byte pinLedGreen = 5;
const byte pinLedRed = 4;
const byte pinInterrupt = 2;

bool initializationError = false;

void  setup(){
  Serial.begin(115200);

  if(configureRTC()) {
    Serial.println("RTC OK");
  }
  else {
    Serial.println("RTC FAILED");
    initializationError = true;
  }

  if(initializationError) {
    flashError();
  }
  else {
    flashOk();
  }
}

void loop() {
  showTime();
  delay(5000);
}

bool configureRTC()
{
  if(!rtc.begin()) {
    return false;
  }

  // Uncomment here to set time, comment to just test the currently saved time
  ////rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  return true;
}

void showTime() {
  DateTime now = rtc.now() + TimeSpan(15);
  Serial.print(now.day(), DEC);
  Serial.print('.');
  Serial.print(now.month(), DEC);
  Serial.print('.');
  Serial.print(now.year(), DEC);
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);
  delay(1000);
}

void flashOk() {
  unsigned long ms = 500;

  pinMode(pinLedGreen, OUTPUT);
  for(byte i=0; i<5; i++){
    digitalWrite(pinLedGreen, HIGH);
    delay(ms);
    digitalWrite(pinLedGreen, LOW);
    delay(ms);
  }
}

void flashError(){
  pinMode(pinLedRed, OUTPUT);
  digitalWrite(pinLedRed, HIGH);
}
