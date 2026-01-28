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

/**
 * FIX-V4: Apagado robusto de modem antes de deep sleep
 * Sistema: LTE / Sleep
 * Archivo: LTEModule.cpp, AppController.cpp
 * Descripción: 
 *   1. Mejora powerOff() para esperar URC "NORMAL POWER DOWN"
 *   2. Garantiza apagado ANTES de entrar a deep sleep
 *   Evita estados corruptos persistentes (modem zombi).
 * Referencia: SIM7080G Hardware Design v1.05, Page 23, 27
 * Estado: Implementado
 */
#define ENABLE_FIX_V4_MODEM_POWEROFF_SLEEP    1

// ============================================================
// FIX-V4: PARÁMETROS DE APAGADO ROBUSTO
// ============================================================

/** @brief Timeout para esperar URC "NORMAL POWER DOWN" (ms) */
#define FIX_V4_URC_TIMEOUT_MS                 5000

/** @brief Tiempo de espera después de PWRKEY fallback (ms) */
#define FIX_V4_PWRKEY_WAIT_MS                 3000

/** @brief Tiempo de espera adicional post-apagado antes de sleep (ms) */
#define FIX_V4_POST_POWEROFF_DELAY_MS         500

/**
 * FIX-V5: Watchdog de Sistema
 * Sistema: Core/AppController
 * Archivo: AppController.cpp
 * Descripción: Task Watchdog Timer (TWDT) para recovery automático
 *              de cuelgues de software (módem zombie, I2C lock, etc.).
 *              Si AppLoop() no hace "feed" en 60 segundos, reset automático.
 * NOTA: ESP-IDF v5.x ya tiene TWDT habilitado por defecto.
 *       Deshabilitar para evitar conflicto y boot loops.
 * Estado: Deshabilitado (sistema ya lo provee)
 */
#define ENABLE_FIX_V5_WATCHDOG    0

// ============================================================
// FIX-V5: PARÁMETROS DE WATCHDOG
// ============================================================

/** @brief Timeout del watchdog en segundos (60s = balance entre recovery y falsos positivos) */
#define FIX_V5_WATCHDOG_TIMEOUT_S             60

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

/**
 * FEAT-V4: Reinicio periódico preventivo (24h)
 * Sistema: Core/AppController/Sleep
 * Archivo: AppController.cpp
 * Descripción: Reinicio limpio cada N horas para prevenir degradación
 *              a largo plazo (memory leaks, fragmentación, estados corruptos).
 *              - Acumula microsegundos reales de sleep (inmune a cambios de TIEMPO_SLEEP)
 *              - Restart en punto seguro (después de FIX-V4, antes de deep sleep)
 *              - Protección anti boot-loop con flag RTC
 *              - Kill switch NVS: dis_feat4=true deshabilita sin reflashear
 * Dependencias: FEAT-V1, FEAT-V3, FIX-V4
 * Estado: Propuesto
 */
#define ENABLE_FEAT_V4_PERIODIC_RESTART       1

// ============================================================
// FEAT-V4: PARÁMETROS DE REINICIO PERIÓDICO
// ============================================================

/** @brief Horas entre reinicios preventivos (24 = producción, 1 = pruebas) */
#define FEAT_V4_RESTART_HOURS                 24

/** @brief Threshold calculado en microsegundos */
#define FEAT_V4_THRESHOLD_US  ((uint64_t)FEAT_V4_RESTART_HOURS * 3600ULL * 1000000ULL)

// Valores para g_last_restart_reason_feat4
#define FEAT4_RESTART_NONE                    0   // No hay restart pendiente
#define FEAT4_RESTART_PERIODIC                1   // Restart por tiempo >= 24h
#define FEAT4_RESTART_EXECUTED                2   // Restart fue ejecutado

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
    
    #if ENABLE_FIX_V4_MODEM_POWEROFF_SLEEP
    Serial.println(F("  [X] FIX-V4: Modem PowerOff Sleep (URC)"));
    #else
    Serial.println(F("  [ ] FIX-V4: Modem PowerOff Sleep (URC)"));
    #endif
    
    #if ENABLE_FIX_V5_WATCHDOG
    Serial.print(F("  [X] FIX-V5: Watchdog ("));
    Serial.print(FIX_V5_WATCHDOG_TIMEOUT_S);
    Serial.println(F("s)"));
    #else
    Serial.println(F("  [ ] FIX-V5: Watchdog"));
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
    
    #if ENABLE_FEAT_V4_PERIODIC_RESTART
    Serial.print(F("  [X] FEAT-V4: Periodic Restart ("));
    Serial.print(FEAT_V4_RESTART_HOURS);
    Serial.println(F("h)"));
    #else
    Serial.println(F("  [ ] FEAT-V4: Periodic Restart"));
    #endif
    
    Serial.println(F("====================="));
}

#endif // FEATURE_FLAGS_H
