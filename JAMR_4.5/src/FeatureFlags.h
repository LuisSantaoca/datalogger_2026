/**
 * @file FeatureFlags.h
 * @brief Sistema de Feature Flags para compilación condicional
 * @version FEAT-V1
 * @date 2026-01-07
 * 
 * Este archivo centraliza todas las banderas para habilitar o deshabilitar
 * fixes y features en tiempo de compilación.
 * 
 * USO:
 *   1 = Habilitado (código nuevo activo)
 *   0 = Deshabilitado (comportamiento original)
 * 
 * CONVENCIÓN DE NOMBRES:
 *   ENABLE_FIX_Vn_DESCRIPCION  - Para correcciones de bugs
 *   ENABLE_FEAT_Vn_DESCRIPCION - Para nuevas funcionalidades
 * 
 * ROLLBACK:
 *   Para revertir un fix, cambiar su valor de 1 a 0 y recompilar.
 *   El código original se mantiene en los bloques #else.
 */

#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

#include <Arduino.h>

// ============================================================
// FIX FLAGS - Correcciones de bugs
// ============================================================

/**
 * FIX-V1: Skip Reset en configureOperator()
 * Sistema: LTE/Modem
 * Archivo: LTEModule.cpp, LTEModule.h, AppController.cpp
 * Descripción: Evita resetModem() cuando ya hay operadora guardada,
 *              reduciendo eventos PDP de 3+ a 1 por ciclo.
 * Estado: Implementado
 */
#define ENABLE_FIX_V1_SKIP_RESET_PDP    1

/**
 * FIX-V2: Fallback a escaneo cuando falla operadora guardada
 * Sistema: LTE/Modem - Selección de Operadora
 * Archivo: AppController.cpp
 * Descripción: Si configureOperator() falla con operadora de NVS,
 *              borra NVS y ejecuta escaneo completo de todas las operadoras.
 * Mitigaciones:
 *   - skipScanCycles: Evita bucle infinito en zonas sin cobertura
 *   - Validación de score mínimo antes de reintentar
 * Requisito: RF-12
 * Estado: Implementado
 */
#define ENABLE_FIX_V2_FALLBACK_OPERADORA  1

/** @brief Ciclos a saltar tras escaneo fallido (protección anti-bucle) */
#define FIX_V2_SKIP_CYCLES_ON_FAIL        3

/**
 * FIX-V3: Modo reposo por baja batería
 * Sistema: Energía / LTE
 * Archivo: AppController.cpp
 * Descripción: Si batería <= UTS_LOW_ENTER (3.20V), entra a modo reposo.
 *              Solo sale cuando batería >= UTS_LOW_EXIT (3.80V) de forma estable.
 *              Bloquea transmisión LTE para evitar pico de 2A que causa brownout.
 * Requisitos: RF-06, RF-09
 * Estado: Implementado
 */
#define ENABLE_FIX_V3_LOW_BATTERY_MODE    1

// ============================================================
// FIX-V3: UMBRALES DE BATERÍA (CONTRATO v1.0)
// ============================================================
// NOTA: Valores configurables por revisión de hardware.
//       El comportamiento (histéresis + estabilidad) es OBLIGATORIO.

/** @brief Umbral de entrada a modo reposo (voltios) */
#define FIX_V3_UTS_LOW_ENTER              3.20f

/** @brief Umbral de salida de modo reposo (voltios) */
#define FIX_V3_UTS_LOW_EXIT               3.80f

// ============================================================
// FIX-V3: PARÁMETROS DE FILTRADO
// ============================================================

/** @brief Número de muestras para promediar vBat */
#define FIX_V3_VBAT_FILTER_SAMPLES        10

/** @brief Delay entre muestras ADC (ms) */
#define FIX_V3_VBAT_FILTER_DELAY_MS       50

// ============================================================
// FIX-V3: PARÁMETROS DE ESTABILIDAD
// ============================================================

/** @brief Ciclos consecutivos requeridos para considerar "estable" */
#define FIX_V3_STABLE_CYCLES_REQUIRED     3

// ============================================================
// FEAT FLAGS - Nuevas funcionalidades
// ============================================================

/**
 * FEAT-V2: Cycle Timing - Medición de tiempos de ejecución
 * Sistema: Diagnóstico/Performance
 * Archivo: CycleTiming.h, AppController.cpp
 * Descripción: Mide y reporta tiempos de cada fase del ciclo FSM.
 *              Cero overhead cuando está deshabilitado (macros vacías).
 * Estado: Implementado
 */
#define ENABLE_FEAT_V2_CYCLE_TIMING     1

/**
 * FEAT-V3: Crash Diagnostics - Sistema de diagnóstico post-mortem
 * Sistema: Diagnóstico/Estabilidad
 * Archivo: src/data_diagnostics/CrashDiagnostics.h, .cpp
 * Descripción: Captura contexto de crashes para análisis post-mortem.
 *              Usa RTC memory + NVS + LittleFS para persistencia.
 *              Reporte vía Serial cuando se conecta dispositivo.
 * Estado: Implementado
 */
#define ENABLE_FEAT_V3_CRASH_DIAGNOSTICS  1

// ============================================================
// FUNCIÓN DE DEBUG: Imprimir flags activos
// ============================================================

/**
 * @brief Imprime el estado de todos los feature flags al Serial
 * 
 * Útil para diagnóstico: permite saber qué fixes están activos
 * en el firmware compilado sin revisar el código fuente.
 */
inline void printActiveFlags() {
    Serial.println(F("=== FEATURE FLAGS ==="));
    
    // FIX Flags
    #if ENABLE_FIX_V1_SKIP_RESET_PDP
    Serial.println(F("  [X] FIX-V1: Skip Reset PDP"));
    #else
    Serial.println(F("  [ ] FIX-V1: Skip Reset PDP"));
    #endif
    
    #if ENABLE_FIX_V2_FALLBACK_OPERADORA
    Serial.println(F("  [X] FIX-V2: Fallback Operadora"));
    #else
    Serial.println(F("  [ ] FIX-V2: Fallback Operadora"));
    #endif
    
    #if ENABLE_FIX_V3_LOW_BATTERY_MODE
    Serial.print(F("  [X] FIX-V3: Low Battery Mode ("));
    Serial.print(FIX_V3_UTS_LOW_ENTER, 2);
    Serial.print(F("V/"));
    Serial.print(FIX_V3_UTS_LOW_EXIT, 2);
    Serial.println(F("V)"));
    #else
    Serial.println(F("  [ ] FIX-V3: Low Battery Mode"));
    #endif
    
    // FEAT Flags
    #if ENABLE_FEAT_V2_CYCLE_TIMING
    Serial.println(F("  [X] FEAT-V2: Cycle Timing"));
    #else
    Serial.println(F("  [ ] FEAT-V2: Cycle Timing"));
    #endif
    
    #if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
    Serial.println(F("  [X] FEAT-V3: Crash Diagnostics"));
    #else
    Serial.println(F("  [ ] FEAT-V3: Crash Diagnostics"));
    #endif
    
    Serial.println(F("====================="));
}

#endif // FEATURE_FLAGS_H
