/**
 * @file timedata.h
 * @brief Sistema de gestión de tiempo y fecha con RTC
 * @author Elathia
 * @date 2025
 * @version 2.0
 * 
 * @details Módulo para manejo de RTC (Real Time Clock) con reintentos automáticos,
 * validación de datos y sistema de logging estructurado.
 */

#ifndef TIMEDATA_H  
#define TIMEDATA_H

#include "type_def.h"
#include <Arduino.h>

/**
 * @brief Configura e inicializa el sistema de tiempo
 * @param data Estructura donde almacenar la información de tiempo
 * @return true si la inicialización es exitosa
 */
bool setupTimeData(sensordata_type* data);



#endif 