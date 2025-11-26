/**
 * @file sensores.h
 * @brief Sistema de lectura de sensores ambientales y de suelo
 * @author Elathia
 * @date 2025
 * @version 2.0
 * 
 * @details Módulo para lectura de sensores I2C (AHT10/20) y RS485 (sondas de suelo).
 * Soporta múltiples tipos de sondas y sensores ambientales configurables.
 */

#ifndef sensores_H
#define sensores_H

#include <stdint.h>
#include "Arduino.h"
#include "type_def.h"

#define RS485_TX 16
#define RS485_RX 15
#define ADC_VOLT_BAT 13
#define RS485_BAUD 9600
#define RS485_CFG SWSERIAL_8N1

#define SONDA_DFROBOT_EC 1
#define SONDA_DFROBOT_EC_PH 2
#define SONDA_SEED_EC 3
#define TYPE_SONDA 3 //seleccion de sonda 3 elathia

#define AHT10_SEN 1
#define AHT20_SEN 2
#define TYPE_AHT 2

/**
 * @brief Inicializa y lee todos los sensores del sistema
 * @param data Estructura donde almacenar los datos leídos
 */
void setupSensores(sensordata_type* data);

/**
 * @brief Lee datos de una sonda RS485 con protocolo Modbus
 * @param request Buffer de petición Modbus
 * @param requestLength Longitud de la petición
 * @param response Buffer para almacenar respuesta
 * @param responseLength Longitud esperada de respuesta
 * @param sondaName Nombre de la sonda para logging
 * @return true si la lectura es exitosa
 */
bool readSondaRS485(byte* request, int requestLength, byte* response, int responseLength, const char* sondaName);

/**
 * @brief Lee datos de sonda DFRobot EC (conductividad)
 */
void read_dfrobot_sonda_ec();

/**
 * @brief Lee datos de sonda DFRobot EC+pH
 */
void read_dfrobot_sonda_ecph();

/**
 * @brief Lee datos de sonda Seed EC
 */
void read_seed_sonda_ec();

/**
 * @brief Lee sensor de temperatura y humedad AHT10
 */
void readAht10();

/**
 * @brief Lee sensor de temperatura y humedad AHT20
 */
void readAht20();

/**
 * @brief Lee el nivel de batería mediante ADC
 */
void readBateria();

#endif