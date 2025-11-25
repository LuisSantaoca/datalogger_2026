/**
 * @file type_def.h
 * @brief Definiciones de tipos de datos para el sistema de monitoreo
 * @author Elathia
 * @date 2025
 * @version 2.0
 * 
 * @details Estructura principal de datos optimizada para transmisión.
 * Formato de bytes (High/Low) para valores numéricos.
 * Total: ~34 bytes por transmisión.
 */

#ifndef TYPE_DEF_H
#define TYPE_DEF_H

#include <stdint.h>
#include "Arduino.h"

/**
 * @struct sensordata_type
 * @brief Estructura principal de datos del sistema
 * 
 * @details Almacena todos los datos de sensores en formato byte optimizado.
 * - Valores numéricos: pares H/L (High/Low byte)
 * - Conversión: valor = (H << 8) | L
 * - GPS: formato IEEE 754 (4 bytes por coordenada)
 */
typedef struct {
  byte H_temperatura_suelo;
  byte L_temperatura_suelo;
  byte H_humedad_suelo;
  byte L_humedad_suelo;
  byte H_conductividad_suelo;
  byte L_conductividad_suelo;
  byte H_ph_suelo;
  byte L_ph_suelo;
  
  byte H_humedad_relativa;
  byte L_humedad_relativa;
  byte H_temperatura_ambiente;
  byte L_temperatura_ambiente;
  
  byte H_bateria;
  byte L_bateria;
  
  byte H_ano;
  byte L_ano;
  byte H_mes;
  byte H_dia;
  byte H_hora;
  byte H_minuto;
  byte H_segundo;
  
  byte H_rsi;
  
  byte lat0;
  byte lat1;
  byte lat2;
  byte lat3;
  byte lon0;
  byte lon1;
  byte lon2;
  byte lon3;
  byte alt0;
  byte alt1;
  byte alt2;
  byte alt3;

} sensordata_type;

#endif