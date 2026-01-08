/**
 * @file config.h
 * @brief Configuración de pines y constantes globales
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/**
 * @brief Pin GPIO para control de dispositivo (señal LOW)
 */
const int DEVICE_ENABLE_PIN_LOW = GPIO_NUM_7;

/**
 * @brief Pin GPIO para control de dispositivo (señal HIGH)
 */
const int DEVICE_ENABLE_PIN_HIGH = GPIO_NUM_3;

/**
 * @brief Pin SDA para comunicación I2C
 */
const int I2C_SDA_PIN = GPIO_NUM_6;

/**
 * @brief Pin SCL para comunicación I2C
 */
const int I2C_SCL_PIN = GPIO_NUM_12;

/**
 * @brief Frecuencia del bus I2C en Hz
 */
const long I2C_FREQUENCY = 100000;

#endif
