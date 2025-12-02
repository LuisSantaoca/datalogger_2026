// =============================================================================
// ARCHIVO: timedata.h
// DESCRIPCIÓN: Archivo de cabecera para el sistema de gestión de tiempo y fecha
// FUNCIONALIDADES:
//   - Declaraciones de funciones del sistema de tiempo
//   - Sistema robusto de manejo de errores
//   - Funciones de diagnóstico y estadísticas
//   - Validación de datos de tiempo
// =============================================================================

#ifndef TIMEDATA_H  
#define TIMEDATA_H

#include "type_def.h"
#include <Arduino.h>

// =============================================================================
// FUNCIÓN PRINCIPAL DE CONFIGURACIÓN
// =============================================================================

/**
 * Configura e inicializa el sistema de tiempo con manejo robusto de errores
 * Implementa inicialización del RTC con reintentos automáticos y validación
 * 
 * @param data - Estructura de datos donde almacenar la información de tiempo
 * @return true si la inicialización es exitosa
 */
bool setupTimeData(sensordata_type* data);

// =============================================================================
// FUNCIONES DE LOGGING Y DIAGNÓSTICO
// =============================================================================

/**
 * Sistema de logging estructurado para el módulo de tiempo
 * Proporciona diferentes niveles de logging con timestamps y emojis
 * 
 * @param level - Nivel de log (0=Error, 1=Warning, 2=Info, 3=Debug)
 * @param message - Mensaje a loguear
 */
void logTimeMessage(int level, const String& message);

/**
 * Valida que los valores de tiempo estén en rangos razonables
 * Verifica que la fecha y hora sean coherentes y válidas
 * 
 * @param year - Año a validar
 * @param month - Mes a validar (1-12)
 * @param day - Día a validar (1-31)
 * @param hour - Hora a validar (0-23)
 * @param minute - Minuto a validar (0-59)
 * @param second - Segundo a validar (0-59)
 * @return true si todos los valores son válidos
 */
bool validateTimeValues(int year, int month, int day, int hour, int minute, int second);

/**
 * Verifica el estado de la batería del RTC
 * Comprueba si el módulo RTC tiene batería de respaldo funcional
 * 
 * @return true si la batería está en buen estado
 */
bool checkRtcBattery();

// =============================================================================
// FUNCIONES DE DIAGNÓSTICO Y ESTADÍSTICAS
// =============================================================================

/**
 * Obtiene el estado actual del sistema de tiempo
 * Proporciona información completa del estado del RTC
 * 
 * @return String con estadísticas del sistema de tiempo
 */
String getTimeSystemStats();

/**
 * Verifica la integridad del sistema de tiempo
 * Realiza una lectura de prueba para validar el funcionamiento
 * 
 * @return true si el sistema funciona correctamente
 */
bool verifyTimeSystem();

/**
 * Resetea los contadores de errores del sistema de tiempo
 * Útil para mantenimiento y recuperación del sistema
 */
void resetTimeErrorCounters();

/**
 * Obtiene la fecha y hora actual en formato String legible
 * Útil para logging y debugging
 * 
 * @return String con fecha y hora formateada
 */
String getCurrentTimeString();

#endif 