/**
 * @file RTCModule.cpp
 * @brief Implementación de funciones para gestión del RTC
 */

#include "RTCModule.h"
#include "../DebugConfig.h"

RTC_DS3231 rtc;

bool initializeRTC() {

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
  
  if (!rtc.begin()) {
    DEBUG_ERROR(RTC, "No se pudo encontrar el RTC");
    return false;
  }
  
  if (rtc.lostPower()) {
    DEBUG_WARN(RTC, "RTC perdio energia, configurando hora...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  
  return true;
}

uint32_t getEpochTime() {
  DateTime now = rtc.now();
  return now.unixtime();
}

String getEpochString() {
  uint32_t epoch = getEpochTime();
  return String(epoch);
}

void epochToBytes(uint32_t epoch, byte bytes[4]) {
  bytes[0] = (epoch >> 24) & 0xFF;
  bytes[1] = (epoch >> 16) & 0xFF;
  bytes[2] = (epoch >> 8) & 0xFF;
  bytes[3] = epoch & 0xFF;
}

void printCurrentDateTime() {
  DateTime now = rtc.now();
  
  if (DEBUG_RTC_ENABLED && DEBUG_GLOBAL_ENABLED) {
    Serial.print("[INFO][RTC] ");
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();
  }
}
