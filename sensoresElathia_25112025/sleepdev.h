/**
 * @file sleepdev.h
 * @brief Sistema de gestión de energía y modo deep sleep para ESP32-S3
 * @author Elathia
 * @date 2025
 * @version 2.0
 * 
 * @details Módulo para control de bajo consumo mediante deep sleep.
 * Implementa control de GPIO Hold, gestión de periféricos y timer de wakeup.
 * Consumo en deep sleep: ~10μA
 */

#ifndef SLEEPDEV_H
#define SLEEPDEV_H

#include <stdint.h>
#include "Arduino.h"

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 600

/**
 * @brief Configura los pines GPIO y periféricos del sistema
 * @details Inicializa serial, I2C y pines de control. Libera GPIO Hold después de deep sleep.
 * @note GPIO3=Sensores, GPIO7=Low Power, GPIO6=SDA, GPIO12=SCL
 */
void setupGPIO();

/**
 * @brief Prepara y ejecuta la entrada en modo deep sleep
 * @details Desactiva periféricos, configura GPIO Hold y timer de wakeup.
 * @warning Esta función no retorna hasta el próximo despertar del ESP32
 */
void sleepIOT();

#endif