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
 * FIX-V3: [Reservado]
 * Sistema: [Por definir]
 * Descripción: [Por definir]
 */
#define ENABLE_FIX_V3_PLACEHOLDER       0

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
    
    #if ENABLE_FIX_V3_PLACEHOLDER
    Serial.println(F("  [X] FIX-V3: Placeholder"));
    #else
    Serial.println(F("  [ ] FIX-V3: Placeholder"));
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
