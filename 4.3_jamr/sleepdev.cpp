/**
 * @file sleepdev.cpp
 * @brief Implementación del sistema de gestión de energía y deep sleep
 * @author Elathia
 * @date 2025
 */

#include "sleepdev.h"
#include "Wire.h"
#include "esp_system.h"
#include "gsmlte.h"

void printWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}

void setupGPIO() {
  Serial.begin(115200);

  gpio_hold_dis(GPIO_NUM_7);
  gpio_hold_dis(GPIO_NUM_3);
  gpio_hold_dis(GPIO_NUM_9);

  Wire.begin(GPIO_NUM_6, GPIO_NUM_12, 100000);

  pinMode(GPIO_NUM_7, OUTPUT);
  pinMode(GPIO_NUM_3, OUTPUT);
  pinMode(GPIO_NUM_9, OUTPUT);
}

void sleepIOT() {
  digitalWrite(GPIO_NUM_3, LOW);
  digitalWrite(GPIO_NUM_7, HIGH);
  digitalWrite(GPIO_NUM_9, LOW);

  Wire.end();

  gpio_hold_en(GPIO_NUM_7);
  gpio_hold_en(GPIO_NUM_3);
  gpio_hold_en(GPIO_NUM_9);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " seconds");
  Serial.println("Going to sleep now");
  
  esp_deep_sleep_start();
}
