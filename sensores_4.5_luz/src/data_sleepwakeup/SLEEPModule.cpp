#include "SLEEPModule.h"
#include "../data_buffer/config_data_buffer.h"
#include "../DebugConfig.h"


bool SleepModule::begin() {

  Serial.begin(SERIAL_BAUD_RATE);
  DEBUG_INFO(SLEEP, "SleepModule initializing...");
  gpio_hold_dis(GPIO_NUM_3);
  gpio_hold_dis(GPIO_NUM_9);


  delay(2000);

  return true;
}

void SleepModule::clearWakeupSources() {
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
}

bool SleepModule::enableTimerWakeup() {
  esp_err_t res = esp_sleep_enable_timer_wakeup(DEFAULT_SLEEP_TIME_US);
  return res == ESP_OK;
}

void SleepModule::enterDeepSleep() {

  digitalWrite(GPIO_NUM_3, LOW);
  digitalWrite(GPIO_NUM_9, LOW);


  gpio_hold_en(GPIO_NUM_3);
  gpio_hold_en(GPIO_NUM_9);

  DEBUG_INFO(SLEEP, "Entering deep sleep NOW...");
  Serial.flush();
  delay(100);

  esp_deep_sleep_start();
}

esp_sleep_wakeup_cause_t SleepModule::getWakeupCause() const {
  return esp_sleep_get_wakeup_cause();
}

const char* SleepModule::wakeupCauseToString(esp_sleep_wakeup_cause_t cause) const {
  switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER:
      return "TIMER";
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      return "UNDEFINED";
  }
}
