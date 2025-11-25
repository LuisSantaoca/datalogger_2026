// =============================================================================
// ARCHIVO: type_def.h
// DESCRIPCIÓN: Definiciones de tipos de datos para el sistema de monitoreo ambiental
// FUNCIONALIDADES:
//   - Estructura principal de datos de sensores
//   - Definiciones de tipos para almacenamiento eficiente
//   - Formato de datos compatible con transmisión y almacenamiento
// =============================================================================

#ifndef TYPE_DEF_H
#define TYPE_DEF_H

#include <stdint.h>
#include "Arduino.h"

// =============================================================================
// ESTRUCTURA PRINCIPAL DE DATOS
// =============================================================================

/**
 * Estructura principal para almacenar todos los datos del sistema
 * Utiliza formato de bytes para optimizar transmisión y almacenamiento
 * Todos los valores se almacenan como pares de bytes (High/Low) para mayor precisión
 */
typedef struct {
  // =============================================================================
  // DATOS DE SUELO (Sondas RS485)
  // =============================================================================
  byte H_temperatura_suelo;
  byte L_temperatura_suelo;
  byte H_humedad_suelo;
  byte L_humedad_suelo;
  byte H_conductividad_suelo;
  byte L_conductividad_suelo;
  byte H_ph_suelo;
  byte L_ph_suelo;
  
  // =============================================================================
  // DATOS AMBIENTALES (Sensores I2C)
  // =============================================================================
  byte H_humedad_relativa;
  byte L_humedad_relativa;
  byte H_temperatura_ambiente;
  byte L_temperatura_ambiente;
  
  // =============================================================================
  // DATOS DEL SISTEMA
  // =============================================================================
  byte H_bateria;
  byte L_bateria;
  
  // =============================================================================
  // DATOS DE TIEMPO Y FECHA
  // =============================================================================
  byte H_ano;
  byte L_ano;
  byte H_mes;
  byte H_dia;
  byte H_hora;
  byte H_minuto;
  byte H_segundo;
  
  // =============================================================================
  // DATOS DE COMUNICACIÓN
  // =============================================================================
  byte H_rsi;
  
  // =============================================================================
  // DATOS GPS (Coordenadas en formato IEEE 754)
  // =============================================================================
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

  // =============================================================================
  // DATOS DE HEALTH / DIAGNÓSTICO (FIX-004)
  // =============================================================================
  byte health_checkpoint;        // Último checkpoint alcanzado
  byte health_crash_reason;      // Código de motivo de crash
  byte H_health_boot_count;      // Boot count (byte alto)
  byte L_health_boot_count;      // Boot count (byte bajo)
  byte H_health_crash_ts;        // Timestamp crash (byte alto)
  byte L_health_crash_ts;        // Timestamp crash (byte medio)

  // =============================================================================
  // VERSIONAMIENTO DE FIRMWARE (REQ-004)
  // =============================================================================
  byte fw_major;                 // Versión major (X.0.0)
  byte fw_minor;                 // Versión minor (0.Y.0)
  byte fw_patch;                 // Versión patch (0.0.Z)

} sensordata_type;

// =============================================================================
// NOTAS DE IMPLEMENTACIÓN
// =============================================================================

/*
 * FORMATO DE DATOS:
 * 
 * - Los valores numéricos se almacenan como pares de bytes (High/Low)
 * - Para convertir a valor real: valor = (H_byte << 8) | L_byte
 * - Las coordenadas GPS se almacenan en formato IEEE 754 (float de 32 bits)
 * - La fecha y hora se almacenan como bytes individuales separados
 * - El año se almacena como par de bytes (H/L) para mayor rango
 * 
 * EJEMPLO DE CONVERSIÓN:
 * 
 * Temperatura del suelo:
 * - H_temperatura_suelo = 0x1A (26 en decimal)
 * - L_temperatura_suelo = 0x2B (43 en decimal)
 * - Valor real = (0x1A << 8) | 0x2B = 0x1A2B = 6699
 * - Temperatura = 6699 / 100 = 66.99°C
 * 
 * Fecha y hora:
 * - Año: (H_ano << 8) | L_ano (ej: 2024 = 0x07E8)
 * - Mes: H_mes (1-12)
 * - Día: H_dia (1-31)
 * - Hora: H_hora (0-23)
 * - Minuto: H_minuto (0-59)
 * - Segundo: H_segundo (0-59)
 * 
 * COORDENADAS GPS:
 * - Los 4 bytes se combinan para formar un float de 32 bits
 * - Se puede usar union para conversión directa
 * 
 * RSI (Calidad de señal):
 * - Solo se almacena el byte alto (H_rsi)
 * - Valor típico: -120 a -50 dBm
 */

#endif