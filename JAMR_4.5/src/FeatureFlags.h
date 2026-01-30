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
// DEBUG FLAGS - SOLO PARA PRUEBAS (CAMBIAR A 0 EN PRODUCCIÓN)
// ============================================================

/** 
 * @brief Modo de prueba de estrés con mocks (sin hardware real)
 * Valida: FSM, memoria, buffer, restart periódico
 * NO valida: comunicación real, EMI, modem
 */
#define DEBUG_STRESS_TEST_ENABLED             0   // 0=off, 1=stress con mocks

/** @brief Simula envío LTE exitoso sin conectar a la red */
#define DEBUG_MOCK_LTE                        0   // 0=LTE real, 1=simulado

/** @brief Usa coordenadas GPS dummy sin encender módulo */
#define DEBUG_MOCK_GPS                        0   // 0=GPS real, 1=simulado

/** @brief Usa ICCID dummy sin encender modem */
#define DEBUG_MOCK_ICCID                      0   // 0=ICCID real, 1=simulado

/**
 * @brief Modo diagnóstico EMI - COMUNICACIÓN REAL con logging detallado
 * Valida: Integridad comunicación UART, detección de ruido EMI
 * Requiere: Modem conectado, SIM insertada
 * Output: Hex dump de respuestas AT, estadísticas de errores
 */
#define DEBUG_EMI_DIAGNOSTIC_ENABLED          0   // 0=off, 1=diagnóstico EMI (PRODUCCIÓN: off)

/** @brief Número de ciclos de diagnóstico EMI antes de generar reporte */
#define DEBUG_EMI_DIAGNOSTIC_CYCLES           20  // ~4 horas por reporte (6 reportes en 24h)

/** @brief Log hex dump de cada respuesta AT (verbose) */
#define DEBUG_EMI_LOG_RAW_HEX                 0   // 0=off, 1=hex dump (PRODUCCIÓN: off)

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
 * FIX-V4: Garantizar apagado de modem antes de deep sleep
 * Sistema: LTE / Sleep
 * Archivo: AppController.cpp
 * Descripción: 
 *   Llama powerOff() ANTES de entrar a deep sleep.
 *   La lógica robusta de powerOff() está en FIX-V6.
 *   Evita estados corruptos persistentes (modem zombi).
 * Referencia: SIM7080G Hardware Design v1.05, Page 23, 27
 * Nota: Usar JUNTO con FIX-V6 para secuencia completa.
 * Estado: Implementado
 */
#define ENABLE_FIX_V4_MODEM_POWEROFF_SLEEP    1

// ============================================================
// FIX-V4: PARÁMETROS (legacy - ahora en FIX-V6)
// ============================================================

/** @brief Timeout para esperar URC "NORMAL POWER DOWN" (ms) - DEPRECADO, usar FIX_V6_URC_WAIT_TIMEOUT_MS */
#define FIX_V4_URC_TIMEOUT_MS                 5000

/** @brief Tiempo de espera después de PWRKEY fallback (ms) - DEPRECADO, usar FIX_V6_PWRKEY_OFF_TIME_MS */
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

/**
 * FIX-V6: Secuencia robusta de power on/off del modem SIM7080G
 * Sistema: LTE/Modem
 * Archivo: LTEModule.cpp
 * Descripción: Implementa especificaciones del datasheet SIMCOM:
 *   - Espera URC "NORMAL POWER DOWN" en powerOff()
 *   - PWRKEY extendido (1.5s) y reset forzado (>12.6s)
 *   - Manejo de PSM con AT+CPSMS=0
 *   - Reintentos múltiples de AT después de wake
 * Documentación: fixs-feats/fixs/FIX_V6_MODEM_POWER_SEQUENCE.md
 * Investigación: calidad/INVESTIGACION_MODEM_ZOMBIE.md
 * Estado: Implementado
 */
#define ENABLE_FIX_V6_MODEM_POWER_SEQUENCE    1

// ============================================================
// FIX-V6: PARÁMETROS DE POWER SEQUENCE (Datasheet SIM7080G)
// ============================================================

/** @brief PWRKEY LOW para encender (>1s, usar 1.5s) */
#define FIX_V6_PWRKEY_ON_TIME_MS              1500

/** @brief PWRKEY LOW para apagar (>1.2s, usar 1.5s) */
#define FIX_V6_PWRKEY_OFF_TIME_MS             1500

/** @brief PWRKEY LOW para reset forzado interno (>12.6s) */
#define FIX_V6_PWRKEY_RESET_TIME_MS           13000

/** @brief Delay para UART ready (1.8s según datasheet + margen) */
#define FIX_V6_UART_READY_DELAY_MS            2500

/** @brief Buffer entre power off y on (datasheet: ≥2s) */
#define FIX_V6_TOFF_TON_BUFFER_MS             2000

/** @brief Timeout para esperar URC "NORMAL POWER DOWN" */
#define FIX_V6_URC_WAIT_TIMEOUT_MS            10000

/** @brief Número de reintentos de AT después de wake */
#define FIX_V6_AT_RETRY_COUNT                 5

/** @brief Delay entre reintentos de AT (ms) */
#define FIX_V6_AT_RETRY_DELAY_MS              500

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

/**
 * FEAT-V7: Diagnóstico de Producción
 * Sistema: Diagnóstico/Estadísticas
 * Archivo: src/data_diagnostics/ProductionDiag.h, .cpp, config_production_diag.h
 * Descripción: Sistema de contadores y eventos persistentes para análisis en campo.
 *              - Contadores: ciclos, envíos LTE OK/FAIL, EMI, batería baja, crashes
 *              - Log de eventos circular con timestamps
 *              - Comandos Serial: STATS, LOG, CLEAR, EXPORT
 *              - Persistencia en LittleFS con CRC16
 * Dependencias: LittleFS, FEAT-V3 (opcional, para registrar crashes)
 * Estado: Implementado
 */
#define ENABLE_FEAT_V7_PRODUCTION_DIAG        1

/**
 * FEAT-V8: Testing System (Comandos seriales)
 * Sistema: Testing/Diagnóstico
 * Archivo: src/data_tests/TestModule.h, .cpp, config_tests.h
 * Descripción: Sistema de tests minimalista ejecutable via monitor serial.
 *              - 4 tests críticos: CRC16, FSM batería, contadores, parsing AT
 *              - Ejecutable sin recompilar (comandos TEST_*)
 *              - Zero overhead cuando flag = 0
 * Comandos: TEST_CRC, TEST_BAT, TEST_COUNT, TEST_PARSE, TEST_HELP
 * Dependencias: FEAT-V7 (CRC/counters), FIX-V3 (FSM batería)
 * Estado: Implementado
 */
#define ENABLE_FEAT_V8_TESTING                1  // 0=deshabilitado, 1=habilitado

/**
 * FEAT-V9: BLE Configuration Mode
 * Sistema: Configuración/BLE
 * Archivo: AppController.cpp, src/data_buffer/BLEModule.cpp
 * Descripción: Modo de configuración vía Bluetooth Low Energy en primer arranque.
 *              Consume ~60 segundos en boot inicial.
 *              Deshabilitar para pruebas rápidas o producción sin reconfiguración.
 * Estado: Implementado (puede deshabilitarse temporalmente)
 */
#define ENABLE_FEAT_V9_BLE_CONFIG             0  // 0=deshabilitado, 1=habilitado

// ============================================================
// FEAT-V4: PARÁMETROS DE REINICIO PERIÓDICO
// ============================================================

/** 
 * @brief Modo de prueba para reinicio periódico
 * 0 = Producción (usa FEAT_V4_RESTART_HOURS en horas)
 * 1 = Stress test (usa FEAT_V4_RESTART_MINUTES en minutos)
 * NOTA: Durante DEBUG_EMI_DIAGNOSTIC, usar 0 para completar ciclos
 */
#define FEAT_V4_STRESS_TEST_MODE              0   // ← 0 para diagnóstico EMI

/** @brief Horas entre reinicios preventivos (producción) */
#define FEAT_V4_RESTART_HOURS                 24

/** @brief Minutos entre reinicios (solo para stress test) */
#define FEAT_V4_RESTART_MINUTES               5

/** @brief Threshold calculado en microsegundos */
#if FEAT_V4_STRESS_TEST_MODE
    #define FEAT_V4_THRESHOLD_US  ((uint64_t)FEAT_V4_RESTART_MINUTES * 60ULL * 1000000ULL)
#else
    #define FEAT_V4_THRESHOLD_US  ((uint64_t)FEAT_V4_RESTART_HOURS * 3600ULL * 1000000ULL)
#endif

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
    #if FEAT_V4_STRESS_TEST_MODE
    Serial.print(FEAT_V4_RESTART_MINUTES);
    Serial.println(F("min STRESS)"));
    #else
    Serial.print(FEAT_V4_RESTART_HOURS);
    Serial.println(F("h)"));
    #endif
    #else
    Serial.println(F("  [ ] FEAT-V4: Periodic Restart"));
    #endif
    
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    Serial.println(F("  [X] FEAT-V7: Production Diagnostics"));
    #else
    Serial.println(F("  [ ] FEAT-V7: Production Diagnostics"));
    #endif
    
    #if ENABLE_FEAT_V8_TESTING
    Serial.println(F("  [X] FEAT-V8: Testing System (TEST_*)"));
    #else
    Serial.println(F("  [ ] FEAT-V8: Testing System"));
    #endif
    
    #if ENABLE_FEAT_V9_BLE_CONFIG
    Serial.println(F("  [X] FEAT-V9: BLE Config Mode"));
    #else
    Serial.println(F("  [ ] FEAT-V9: BLE Config Mode (DISABLED)"));
    #endif
    
    // DEBUG Flags (FEAT-V5)
    #if DEBUG_STRESS_TEST_ENABLED
    Serial.println(F(""));
    Serial.println(F("  ⚠️  MODO STRESS TEST ACTIVO ⚠️"));
    #if DEBUG_MOCK_GPS
    Serial.println(F("  [X] DEBUG: Mock GPS"));
    #endif
    #if DEBUG_MOCK_ICCID
    Serial.println(F("  [X] DEBUG: Mock ICCID"));
    #endif
    #if DEBUG_MOCK_LTE
    Serial.println(F("  [X] DEBUG: Mock LTE"));
    #endif
    #endif
    
    // DEBUG EMI Diagnostic
    #if DEBUG_EMI_DIAGNOSTIC_ENABLED
    Serial.println(F(""));
    Serial.println(F("  🔬 MODO DIAGNÓSTICO EMI ACTIVO 🔬"));
    Serial.print(F("  Ciclos por reporte: "));
    Serial.println(DEBUG_EMI_DIAGNOSTIC_CYCLES);
    #if DEBUG_EMI_LOG_RAW_HEX
    Serial.println(F("  [X] Log hex dump habilitado"));
    #endif
    Serial.println(F("  Comunicación REAL con modem"));
    #endif
    
    Serial.println(F("====================="));
}

#endif // FEATURE_FLAGS_H
