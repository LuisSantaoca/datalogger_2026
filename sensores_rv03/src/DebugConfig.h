#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

#include <Arduino.h>

/**
 * @file DebugConfig.h
 * @brief Configuración global de debug para todos los módulos
 */

// =============================================================================
// CONFIGURACIÓN GLOBAL DE DEBUG
// =============================================================================

/**
 * @brief Habilitar/deshabilitar debug global en todos los módulos
 * true = debug habilitado, false = debug deshabilitado
 */
#define DEBUG_GLOBAL_ENABLED true

/**
 * @brief Niveles de debug disponibles
 */
#define DEBUG_LEVEL_NONE    0
#define DEBUG_LEVEL_ERROR   1
#define DEBUG_LEVEL_WARNING 2
#define DEBUG_LEVEL_INFO    3
#define DEBUG_LEVEL_VERBOSE 4

/**
 * @brief Nivel de debug activo (0-4)
 * 0 = Sin debug
 * 1 = Solo errores
 * 2 = Errores y advertencias
 * 3 = Errores, advertencias e información
 * 4 = Todo (modo verbose)
 */
#define DEBUG_LEVEL DEBUG_LEVEL_INFO

// =============================================================================
// CONFIGURACIÓN POR MÓDULO (independiente del global)
// =============================================================================

#define DEBUG_ADC_ENABLED    true
#define DEBUG_I2C_ENABLED    true
#define DEBUG_RS485_ENABLED  true
#define DEBUG_GPS_ENABLED    true
#define DEBUG_LTE_ENABLED    true
#define DEBUG_BUFFER_ENABLED true
#define DEBUG_BLE_ENABLED    true
#define DEBUG_FORMAT_ENABLED true
#define DEBUG_SLEEP_ENABLED  true
#define DEBUG_RTC_ENABLED    true
#define DEBUG_APP_ENABLED    true

// =============================================================================
// MACROS DE DEBUG
// =============================================================================

/**
 * @brief Macro para imprimir mensajes de error
 */
#if DEBUG_GLOBAL_ENABLED && (DEBUG_LEVEL >= DEBUG_LEVEL_ERROR)
  #define DEBUG_ERROR(module, msg) \
    if (DEBUG_##module##_ENABLED) { \
      Serial.print("[ERROR]["); \
      Serial.print(#module); \
      Serial.print("] "); \
      Serial.println(msg); \
    }
#else
  #define DEBUG_ERROR(module, msg)
#endif

/**
 * @brief Macro para imprimir mensajes de advertencia
 */
#if DEBUG_GLOBAL_ENABLED && (DEBUG_LEVEL >= DEBUG_LEVEL_WARNING)
  #define DEBUG_WARN(module, msg) \
    if (DEBUG_##module##_ENABLED) { \
      Serial.print("[WARN]["); \
      Serial.print(#module); \
      Serial.print("] "); \
      Serial.println(msg); \
    }
#else
  #define DEBUG_WARN(module, msg)
#endif

/**
 * @brief Macro para imprimir mensajes de información
 */
#if DEBUG_GLOBAL_ENABLED && (DEBUG_LEVEL >= DEBUG_LEVEL_INFO)
  #define DEBUG_INFO(module, msg) \
    if (DEBUG_##module##_ENABLED) { \
      Serial.print("[INFO]["); \
      Serial.print(#module); \
      Serial.print("] "); \
      Serial.println(msg); \
    }
#else
  #define DEBUG_INFO(module, msg)
#endif

/**
 * @brief Macro para imprimir mensajes verbose
 */
#if DEBUG_GLOBAL_ENABLED && (DEBUG_LEVEL >= DEBUG_LEVEL_VERBOSE)
  #define DEBUG_VERBOSE(module, msg) \
    if (DEBUG_##module##_ENABLED) { \
      Serial.print("[VERB]["); \
      Serial.print(#module); \
      Serial.print("] "); \
      Serial.println(msg); \
    }
#else
  #define DEBUG_VERBOSE(module, msg)
#endif

/**
 * @brief Macro para imprimir mensaje con valor
 */
#if DEBUG_GLOBAL_ENABLED && (DEBUG_LEVEL >= DEBUG_LEVEL_INFO)
  #define DEBUG_PRINT_VALUE(module, msg, value) \
    if (DEBUG_##module##_ENABLED) { \
      Serial.print("[INFO]["); \
      Serial.print(#module); \
      Serial.print("] "); \
      Serial.print(msg); \
      Serial.println(value); \
    }
#else
  #define DEBUG_PRINT_VALUE(module, msg, value)
#endif

#endif // DEBUG_CONFIG_H
