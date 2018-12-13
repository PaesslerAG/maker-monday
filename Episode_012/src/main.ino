#include <SigFox.h>
#include <ArduinoLowPower.h>
#include <sds011_mkrfox.h>
 
#define DEBUG       false             // Set DEBUG to false to disable serial prints
#define DEEPSLEEP   15 * 60 * 1000   // Set the delay to 15 minutes (15 min x 60 seconds x 1000 milliseconds)
#define SDSWARMUP   15 * 1000        // Run nova SDS011 for 15 seconds before taking the sensor data
int alarm_source = 0;
 
// create an instance of the sensor
SdsDustSensor sds(Serial1);
 
// define the message payload to send via Sigfox
typedef struct __attribute__ ((packed)) sigfox_message {
  uint8_t status;
  int16_t moduleTemperature;
  uint16_t pm25;
  uint16_t pm10;
} SigfoxMessage;
// Stub for message which will be sent
SigfoxMessage msg;
 
// include some helpers to handle Sigfox convertion, printing module info and sending the message
#include "sigfox_tools.h"
 
void setup() {
    LowPower.attachInterruptWakeup(RTC_ALARM_WAKEUP, alarmEvent0, CHANGE);
 
  pinMode(LED_BUILTIN, OUTPUT);
 
  if (DEBUG) {
    Serial.begin(9600);
    while (!Serial) {}
  }
 
  if (!SigFox.begin()) {
    if (DEBUG) {
      Serial.println("Shield error or not present! Try reboot");
    }
    reboot();
  }
  if (DEBUG) {
    printSigfoxInfo();
  }
  delay(100);
  //Send module to standby until we need to send a message
  SigFox.end();
  if (DEBUG) {
    // Enable DEBUG prints and LED indication if we are testing
    SigFox.debug();
  }
 
  sds.begin(); // this line will begin Serial1 with given baud rate (9600 by default)
  if (DEBUG) {
    printSensorInfo();
  }
  sds.sleep();
}
void loop() {
 
  if (DEBUG) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
  }
 
  querySensor();
  sendSigfoxMessage(&msg);
 
  if (DEBUG) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(DEEPSLEEP - SDSWARMUP);
  } else {
    LowPower.sleep(DEEPSLEEP - SDSWARMUP);
  }
}
 
void alarmEvent0() {
  alarm_source = 0;
}
 
void reboot() {
  NVIC_SystemReset();
  while (1);
}
 
void querySensor() {
  msg.status = 0;
  msg.pm25 = convertoFloatToInt16(0, 1000, 0);
  msg.pm10 = convertoFloatToInt16(0, 1000, 0);
 
  sds.wakeup();
  delay(SDSWARMUP); // working for some seconds
  PmResult pm = sds.queryPm();
  msg.status |= pm.rawStatus;
  if (pm.isOk()) {
    msg.pm25 = convertoFloatToInt16(pm.pm25, 1000, 0);
    msg.pm10 = convertoFloatToInt16(pm.pm10, 1000, 0);
 
    if (DEBUG) {
      Serial.print(pm.pm25);
      Serial.print(" PM2.5, ");
      Serial.print(pm.pm10);
      Serial.println(" PM10");
    }
  } else {
    Serial.print("Could not read values from sensor, reason: ");
    Serial.print(pm.statusToString());
  }
 
  WorkingStateResult state = sds.sleep();
  if (state.isWorking()) {
    Serial.println(" Problem with sleeping the sensor.");
  } else {
    Serial.println(" Sensor is sleeping");
  }
}
 
void printSensorInfo() {
  Serial.println();
  Serial.println("SDS011:");
  Serial.println(sds.queryFirmwareVersion().toString()); // prints firmware version
  Serial.println(sds.setQueryReportingMode().toString()); // ensures sensor is in 'query' reporting mode
  Serial.print("SDS011 warm up:");
  Serial.print(SDSWARMUP);
  Serial.println(" mSec");
  Serial.print("Sending every: ");
  Serial.print(DEEPSLEEP);
  Serial.println(" mSec");
}