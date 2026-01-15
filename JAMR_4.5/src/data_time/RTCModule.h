/**
 * @file RTCModule.h
 * @brief Gestión del módulo RTC y control de dispositivo
 */

#ifndef RTC_MODULE_H
#define RTC_MODULE_H

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include "config_data_time.h"

/**
 * @brief Objeto RTC para comunicación con el módulo DS1307/DS3231
 */
extern RTC_DS1307 rtc;

/**
 * @brief Inicializa el bus I2C y el módulo RTC
 * @return true si la inicialización fue exitosa, false en caso contrario
 */
bool initializeRTC();

/**
 * @brief Habilita el dispositivo mediante los pines de control
 */
void enableDevice();

/**
 * @brief Desactiva el dispositivo mediante los pines de control
 */
void disableDevice();

/**
 * @brief Obtiene el timestamp epoch actual del RTC
 * @return Epoch en segundos (uint32_t)
 */
uint32_t getEpochTime();

/**
 * @brief Obtiene el timestamp epoch actual del RTC como String
 * @return Epoch en segundos como String
 */
String getEpochString();

/**
 * @brief Convierte el epoch a un array de bytes
 * @param epoch Valor epoch a convertir
 * @param bytes Array de 4 bytes donde se almacenará el resultado
 */
void epochToBytes(uint32_t epoch, byte bytes[4]);

/**
 * @brief Imprime la fecha y hora actual en formato legible
 */
void printCurrentDateTime();

#endif
