#ifndef CONFIG_DATA_GPS_H
#define CONFIG_DATA_GPS_H

#include <Arduino.h>

static const uint8_t GPS_PIN_TX = 10;
static const uint8_t GPS_PIN_RX = 11;
static const uint8_t GPS_PWRKEY_PIN = 9;

static const uint32_t GPS_SIM_BAUD = 115200;

static const uint32_t GPS_AT_TIMEOUT_MS = 1500;
static const uint32_t GPS_AT_READY_TIMEOUT_MS = 8000;

static const uint16_t GPS_POWER_ON_ATTEMPTS = 3;

static const bool GPS_PWRKEY_ACTIVE_HIGH = true;
static const uint32_t GPS_PWRKEY_PRE_DELAY_MS = 100;
static const uint32_t GPS_PWRKEY_PULSE_MS = 1200;
static const uint32_t GPS_PWRKEY_POST_DELAY_MS = 3000;

static const uint16_t GPS_GNSS_MAX_RETRIES = 100;
static const uint32_t GPS_GNSS_RETRY_DELAY_MS = 1000;

static const char GPS_POWER_OFF_COMMAND[] = "AT+CPOWD=1";

// Sistema de Debug
#define DEBUG_ENABLED true
#define DEBUG_LEVEL_ERROR 1
#define DEBUG_LEVEL_WARNING 2
#define DEBUG_LEVEL_INFO 3
#define DEBUG_LEVEL_DEBUG 4
#define DEBUG_LEVEL_VERBOSE 5

#define DEBUG_LEVEL DEBUG_LEVEL_VERBOSE

#endif
