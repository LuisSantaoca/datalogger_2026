#ifndef CONFIG_DATA_SLEEP_H
#define CONFIG_DATA_SLEEP_H

#include <Arduino.h>

/**
 * @file config_data_sleep.h
 * @brief Constantes globales para configuración de sleep/wakeup en ESP32-S3.
 */

/** @brief Tiempo por defecto antes de dormir (ms) en pruebas. */
static const uint32_t DEFAULT_AWAKE_TIME_MS = 2000;

/** @brief Tiempo por defecto de deep sleep (us). */
static const uint64_t DEFAULT_SLEEP_TIME_US = 10ULL * 1000ULL * 1000ULL;

#endif
