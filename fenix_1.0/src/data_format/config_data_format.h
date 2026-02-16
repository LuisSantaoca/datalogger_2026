#ifndef CONFIG_DATA_FORMAT_H
#define CONFIG_DATA_FORMAT_H

#include <Arduino.h>

/**
 * @file config_data_format.h
 * @brief Constantes globales para el formato de trama.
 */

/** @brief Longitud del ICCID (sin terminador nulo). */
static const uint8_t ICCID_LEN = 20;

/** @brief Longitud del epoch (sin terminador nulo). */
static const uint8_t EPOCH_LEN = 10;

/** @brief Longitud de latitud y longitud (sin terminador nulo). */
static const uint8_t COORD_LEN = 12;

/** @brief Longitud de altitud (sin terminador nulo). */
static const uint8_t ALT_LEN = 4;

/** @brief Longitud de variables var1..var7 (sin terminador nulo). */
static const uint8_t VAR_LEN = 4;

/** @brief Número de variables var1..var7. */
static const uint8_t VAR_COUNT = 7;

/** @brief Longitud máxima de la trama completa incluyendo '\0'. */
static const uint8_t FRAME_MAX_LEN = 102;

/** @brief Longitud máxima de la trama Base64 incluyendo '\0'. */
static const uint8_t FRAME_BASE64_MAX_LEN = 200;

#endif
