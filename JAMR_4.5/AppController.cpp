/**
 * @file AppController.cpp
 * @brief Controlador principal de la aplicación de adquisición de datos con ESP32-S3
 * 
 * Este archivo implementa una máquina de estados finitos (FSM) que coordina todos
 * los módulos del sistema: sensores, GPS, LTE, almacenamiento y gestión de energía.
 * 
 * @details
 * El sistema opera en dos modos principales:
 * - Modo BLE: Configuración inicial via Bluetooth Low Energy (solo al arranque)
 * - Modo Ciclo: Lectura de sensores, GPS, envío de datos y sleep
 * 
 * Características principales:
 * - Lectura de múltiples sensores (ADC, I2C, RS485 Modbus)
 * - Adquisición GPS optimizada (solo primer ciclo post-boot)
 * - Transmisión LTE con selección inteligente de operadora
 * - Buffer persistente en LittleFS para tolerancia a fallos
 * - Deep sleep con timer wakeup para ahorro de energía
 * - Almacenamiento persistente (NVS) de configuraciones críticas
 * 
 * @author Sistema Sensores RV03
 * @date 2026-01-15
 * @version 2.1.0
 */

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_task_wdt.h>  // FIX-V5: Watchdog de sistema
#include <Preferences.h>
#include "AppController.h"
#include "src/version_info.h"   // FEAT-V0: Sistema de control de versiones centralizado
#include "src/FeatureFlags.h"   // FEAT-V1: Sistema de feature flags
#include "src/CycleTiming.h"    // FEAT-V2: Sistema de timing de ciclos
#include "src/DebugConfig.h"

// ============ [FEAT-V3 START] Include Crash Diagnostics ============
#if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
#include "src/data_diagnostics/CrashDiagnostics.h"  // FEAT-V3
#endif
// ============ [FEAT-V3 END] ============

// ============ [FEAT-V7 START] Include Production Diagnostics ============
#if ENABLE_FEAT_V7_PRODUCTION_DIAG
#include "src/data_diagnostics/ProductionDiag.h"  // FEAT-V7
#endif
// ============ [FEAT-V7 END] ============

// ============ [FEAT-V8 START] Include Testing System ============
#if ENABLE_FEAT_V8_TESTING
#include "src/data_tests/TestModule.h"  // FEAT-V8
#endif
// ============ [FEAT-V8 END] ============

#include "src/data_buffer/BUFFERModule.h"
#include "src/data_buffer/BLEModule.h"
#include "src/data_buffer/config_data_buffer.h"

#include "src/data_format/FORMATModule.h"
#include "src/data_format/config_data_format.h"

#include "src/data_gps/GPSModule.h"
#include "src/data_gps/config_data_gps.h"

#include "src/data_lte/LTEModule.h"
#include "src/data_lte/config_data_lte.h"
#include "src/data_lte/config_operadoras.h"

#include "src/data_sensors/ADCSensorModule.h"
#include "src/data_sensors/I2CSensorModule.h"
#include "src/data_sensors/RS485Module.h"
#include "src/data_sensors/config_data_sensors.h"

// ============ [DEBUG-EMI] Declaración externa de funciones de diagnóstico ============
#if DEBUG_EMI_DIAGNOSTIC_ENABLED
extern void printEMIDiagnosticReport(Stream* serial);
extern void resetEMIDiagnosticStats();
#endif

#include "src/data_sleepwakeup/SLEEPModule.h"
#include "src/data_sleepwakeup/config_data_sleepwakeup.h"

#include "src/data_time/RTCModule.h"
#include "src/data_time/config_data_time.h"

// ============ [FIX-V3 START] Variables persistentes en RTC ============
#if ENABLE_FIX_V3_LOW_BATTERY_MODE
/** @brief Flag que indica si estamos en modo reposo por batería baja */
RTC_DATA_ATTR static bool g_restMode = false;

/** @brief Contador de ciclos estables para salida de reposo */
RTC_DATA_ATTR static uint8_t g_stableCycleCounter = 0;

/** @brief Última lectura de vBat filtrada (para debug) */
RTC_DATA_ATTR static float g_lastVBatFiltered = 0.0f;
#endif
// ============ [FIX-V3 END] ============

// ============ [FEAT-V4 START] Variables persistentes en RTC ============
#if ENABLE_FEAT_V4_PERIODIC_RESTART
/**
 * @brief Acumulador de microsegundos de sleep planificados
 * @details Sobrevive deep sleep. Se acumula con el sleep_time_us real de cada ciclo.
 *          NO usa ciclos fijos - inmune a cambios de TIEMPO_SLEEP_ACTIVO.
 */
RTC_DATA_ATTR static uint64_t g_accum_sleep_us = 0;

/**
 * @brief Razón del último reinicio ejecutado por FEAT-V4
 * @details Permite distinguir restart planificado de crash en boot.
 *          Valores: FEAT4_RESTART_NONE, FEAT4_RESTART_PERIODIC, FEAT4_RESTART_EXECUTED
 */
RTC_DATA_ATTR static uint8_t g_last_restart_reason_feat4 = FEAT4_RESTART_NONE;
#endif
// ============ [FEAT-V4 END] ============

// ============ [FEAT-V5 START] Variables para stress test ============
#if DEBUG_STRESS_TEST_ENABLED
/** @brief Contador de ciclos desde último restart (persiste en deep sleep) */
RTC_DATA_ATTR static uint32_t g_stress_cycle_count = 0;

/** @brief Contador total de reinicios FEAT-V4 */
RTC_DATA_ATTR static uint32_t g_stress_restart_count = 0;

/** @brief Heap libre al inicio del ciclo (para detectar leaks) */
static uint32_t g_stress_heap_start = 0;

/** @brief Timestamp millis() al inicio del ciclo (para calcular tiempo real) */
static uint32_t g_stress_cycle_start_ms = 0;
#endif
// ============ [FEAT-V5 END] ============

// ============ [DEBUG-EMI START] Variables para diagnóstico EMI ============
#if DEBUG_EMI_DIAGNOSTIC_ENABLED
/** @brief Contador de ciclos para diagnóstico EMI (persiste en deep sleep) */
RTC_DATA_ATTR static uint32_t g_emiDiagCycleCount = 0;
#endif
// ============ [DEBUG-EMI END] ============

/** @brief Número de muestras a descartar al inicio de cada lectura de sensor */
static const uint8_t DISCARD_SAMPLES = 5;

/** @brief Número de muestras a promediar después de descartar */
static const uint8_t KEEP_SAMPLES = 5;

/** @brief Configuración global de la aplicación (tiempo de sleep, etc.) */
static AppConfig g_cfg;

/** @brief Módulo de gestión de buffer persistente en LittleFS */
static BUFFERModule buffer;

/** @brief Módulo de configuración Bluetooth Low Energy (usa el buffer para configuración) */
static BLEModule ble(buffer);

/** @brief Módulo de formateo de tramas (texto y Base64) */
static FormatModule formatter;

/** @brief Módulo de sensor ADC para medición de voltaje de batería */
static ADCSensorModule adcSensor;

/** @brief Módulo de sensor I2C para temperatura y humedad */
static I2CSensorModule i2cSensor;

/** @brief Módulo de sensor RS485 con protocolo Modbus RTU */
static RS485Module rs485Sensor;

/** @brief Puerto serial compartido para comunicación con módulo SIM7080G (GPS y LTE) */
static HardwareSerial SerialLTE(1);

/** @brief Módulo GPS que usa el SIM7080G GNSS */
static GPSModule gps(SerialLTE, GPS_PWRKEY_PIN);

/** @brief Módulo LTE Cat-M/NB-IoT que usa el SIM7080G modem */
static LTEModule lte(SerialLTE);

/** @brief Módulo de gestión de deep sleep y wakeup */
static SleepModule sleepModule;

/** @brief Sistema de almacenamiento persistente NVS para configuraciones críticas */
static Preferences preferences;

/**
 * @enum AppState
 * @brief Estados de la máquina de estados finitos (FSM) de la aplicación
 * 
 * @details
 * Flujo normal de operación:
 * 1. Boot: Inicialización (estado transitorio)
 * 2. BleOnly: Modo configuración BLE (solo en arranque desde apagado)
 * 3. Cycle_ReadSensors: Lectura de sensores ADC, I2C y RS485
 * 4. Cycle_Gps: Adquisición GPS (solo primer ciclo post-boot)
 * 5. Cycle_GetICCID: Lectura de ICCID del SIM
 * 6. Cycle_BuildFrame: Construcción de trama de datos (Base64)
 * 7. Cycle_BufferWrite: Escritura persistente en buffer
 * 8. Cycle_SendLTE: Transmisión por red celular
 * 9. Cycle_CompactBuffer: Eliminación de tramas enviadas
 * 10. Cycle_Sleep: Deep sleep con timer wakeup
 * 11. Error: Estado de error (detiene el sistema)
 */
enum class AppState : uint8_t {
  Boot = 0,              ///< Inicialización del sistema
  BleOnly,               ///< Modo configuración BLE exclusivo
  Cycle_ReadSensors,     ///< Lectura de todos los sensores
  Cycle_Gps,             ///< Adquisición de coordenadas GPS
  Cycle_GetICCID,        ///< Obtención del ICCID de la tarjeta SIM
  Cycle_BuildFrame,      ///< Construcción de trama de datos
  Cycle_BufferWrite,     ///< Escritura en buffer persistente
  Cycle_SendLTE,         ///< Transmisión de datos por LTE
  Cycle_CompactBuffer,   ///< Compactación del buffer
  Cycle_Sleep,           ///< Entrada a deep sleep
  Error                  ///< Estado de error fatal
};

/** @brief Estado actual de la máquina de estados */
static AppState g_state = AppState::Boot;

/** @brief Indica si el sistema ha sido inicializado correctamente */
static bool g_initialized = false;

/** @brief Causa del último wakeup (timer, pin externo, etc.) */
static esp_sleep_wakeup_cause_t g_wakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;

/** @brief Flag para ejecutar GPS solo en el primer ciclo después del boot */
static bool g_firstCycleAfterBoot = false;

/** @brief ICCID de la tarjeta SIM (identificador único) */
static String g_iccid;

/** @brief Timestamp Unix epoch en formato string */
static String g_epoch;

/** @brief Timestamp Unix epoch numérico para diagnósticos (FEAT-V7) */
static uint32_t g_lastEpoch = 0;

/** @brief Latitud en formato string con 6 decimales */
static char g_lat[COORD_LEN + 1];

/** @brief Longitud en formato string con 6 decimales */
static char g_lng[COORD_LEN + 1];

/** @brief Altitud en metros en formato string */
static char g_alt[ALT_LEN + 1];

/** @brief Array de valores de sensores en formato string (7 variables) */
static String g_varStr[VAR_COUNT];

/** @brief Buffer para trama completa codificada en Base64 */
static char g_frame[FRAME_BASE64_MAX_LEN];

// ============ Variables para CYCLE SUMMARY ============
/** @brief Operadora usada en último ciclo LTE */
static Operadora g_lastOperadoraUsed = Operadora::MOVISTAR;

/** @brief Score de señal de la operadora usada */
static int g_lastOperatorScore = -999;

/** @brief Valor CSQ de señal (0-31, 99=desconocido) */
static int g_lastCSQ = 99;

/** @brief Flag indicando si el ciclo LTE fue exitoso */
static bool g_lteCycleSuccess = false;

#if ENABLE_FEAT_V2_CYCLE_TIMING
/** @brief Estructura global para timing de ciclo (FEAT-V2) */
static CycleTiming g_timing;
#endif

// ============ CYCLE SUMMARY: Resumen de datos del ciclo ============
/**
 * @brief Imprime resumen de datos adquiridos en el ciclo
 * 
 * Muestra valores de sensores, batería, GPS, operadora y señal
 * para diagnóstico rápido en campo.
 */
static void printCycleSummary() {
    Serial.println(F(""));
    Serial.println(F("\xE2\x95\x94\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x97"));  // ╔══════════════════════════════════════╗
    Serial.println(F("\xE2\x95\x91         CYCLE DATA SUMMARY            \xE2\x95\x91"));  // ║         CYCLE DATA SUMMARY            ║
    Serial.println(F("\xE2\x95\xA0\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\xA3"));  // ╠══════════════════════════════════════╣
    
    // Sensores RS485 (Modbus)
    Serial.print(F("\xE2\x95\x91  RS485[0]:       "));  // ║  RS485[0]:       
    Serial.print(g_varStr[0]);
    for (int i = g_varStr[0].length(); i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));  // ║
    
    Serial.print(F("\xE2\x95\x91  RS485[1]:       "));
    Serial.print(g_varStr[1]);
    for (int i = g_varStr[1].length(); i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));
    
    Serial.print(F("\xE2\x95\x91  RS485[2]:       "));
    Serial.print(g_varStr[2]);
    for (int i = g_varStr[2].length(); i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));
    
    Serial.print(F("\xE2\x95\x91  RS485[3]:       "));
    Serial.print(g_varStr[3]);
    for (int i = g_varStr[3].length(); i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));
    
    // Sensores I2C (Temp/Hum) - Valores almacenados como x100
    float tempVal = g_varStr[4].toFloat() / 100.0f;
    Serial.print(F("\xE2\x95\x91  Temp (I2C):     "));
    Serial.print(tempVal, 2);
    Serial.print(F(" C"));
    int tempLen = String(tempVal, 2).length() + 2;
    for (int i = tempLen; i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));
    
    float humVal = g_varStr[5].toFloat() / 100.0f;
    Serial.print(F("\xE2\x95\x91  Hum (I2C):      "));
    Serial.print(humVal, 2);
    Serial.print(F(" %"));
    int humLen = String(humVal, 2).length() + 2;
    for (int i = humLen; i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));
    
    // Batería - Valor almacenado como x100
    float batVal = g_varStr[6].toFloat() / 100.0f;
    Serial.print(F("\xE2\x95\x91  Battery (ADC):  "));
    Serial.print(batVal, 2);
    Serial.print(F(" V"));
    int batLen = String(batVal, 2).length() + 2;
    for (int i = batLen; i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));
    
    Serial.println(F("\xE2\x95\xA0\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\xA3"));  // ╠══════════════════════════════════════╣
    
    // GPS
    Serial.print(F("\xE2\x95\x91  GPS Lat:        "));
    Serial.print(g_lat);
    for (int i = strlen(g_lat); i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));
    
    Serial.print(F("\xE2\x95\x91  GPS Lng:        "));
    Serial.print(g_lng);
    for (int i = strlen(g_lng); i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));
    
    Serial.print(F("\xE2\x95\x91  GPS Alt:        "));
    Serial.print(g_alt);
    Serial.print(F(" m"));
    for (int i = strlen(g_alt) + 2; i < 15; i++) Serial.print(' ');
    Serial.println(F("\xE2\x95\x91"));
    
    Serial.println(F("\xE2\x95\xA0\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\xA3"));  // ╠══════════════════════════════════════╣
    
    // LTE Info
    Serial.print(F("\xE2\x95\x91  ICCID:          "));
    if (g_iccid.length() > 0) {
        Serial.print(g_iccid);
        for (int i = g_iccid.length(); i < 20; i++) Serial.print(' ');
    } else {
        Serial.print(F("(not read)          "));
    }
    Serial.println(F("\xE2\x95\x91"));
    
    Serial.print(F("\xE2\x95\x91  Operator:       "));
    if (g_lteCycleSuccess) {
        Serial.print(OPERADORAS[g_lastOperadoraUsed].nombre);
        for (int i = strlen(OPERADORAS[g_lastOperadoraUsed].nombre); i < 15; i++) Serial.print(' ');
    } else {
        Serial.print(F("(no TX)        "));
    }
    Serial.println(F("\xE2\x95\x91"));
    
    Serial.print(F("\xE2\x95\x91  Signal (CSQ):   "));
    if (g_lteCycleSuccess && g_lastCSQ != 99 && g_lastCSQ != 0) {
        // CSQ a dBm: dBm = -113 + (CSQ * 2)
        int dbm = -113 + (g_lastCSQ * 2);
        Serial.print(g_lastCSQ);
        Serial.print(F(" ("));
        Serial.print(dbm);
        Serial.print(F(" dBm)"));
        for (int i = 0; i < 3; i++) Serial.print(' ');
    } else {
        Serial.print(F("N/A            "));
    }
    Serial.println(F("\xE2\x95\x91"));
    
#if ENABLE_FIX_V3_LOW_BATTERY_MODE
    Serial.print(F("\xE2\x95\x91  Rest Mode:      "));
    if (g_restMode) {
        Serial.print(F("ACTIVE (LTE blocked)"));
    } else {
        Serial.print(F("OFF            "));
    }
    Serial.println(F("\xE2\x95\x91"));
#endif
    
    Serial.println(F("\xE2\x95\x9A\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x9D"));  // ╚══════════════════════════════════════╝
    Serial.println(F(""));
}

/**
 * @brief Rellena un buffer con caracteres '0' y agrega terminador nulo
 * 
 * Utilizada para inicializar coordenadas GPS cuando no hay datos válidos.
 * 
 * @param dst Puntero al buffer de destino
 * @param width Número de caracteres '0' a escribir
 * 
 * @note El buffer debe tener al menos width+1 bytes de capacidad
 */
static void fillZeros(char* dst, size_t width) {
  for (size_t i = 0; i < width; i++) dst[i] = '0';
  dst[width] = '\0';
}

// ============ [FIX-V3 START] Funciones de control de batería ============
#if ENABLE_FIX_V3_LOW_BATTERY_MODE
/**
 * @brief Lee voltaje de batería con filtrado (promedio de N muestras)
 * 
 * Descarta primera lectura para eliminar ruido del multiplexor ADC.
 * Promedia N lecturas separadas por delay configurable.
 * Usa ADCSensorModule para obtener voltaje calibrado real.
 * 
 * @return Voltaje filtrado en voltios
 */
static float readVBatFiltered() {
    float sum = 0.0f;
    
    // Descartar primera lectura (ruido de multiplexor)
    adcSensor.readSensor();
    delay(FIX_V3_VBAT_FILTER_DELAY_MS);
    
    // Tomar N muestras y promediar
    for (int i = 0; i < FIX_V3_VBAT_FILTER_SAMPLES; i++) {
        adcSensor.readSensor();
        sum += adcSensor.getValue();  // Ya calibrado en voltios reales
        delay(FIX_V3_VBAT_FILTER_DELAY_MS);
    }
    
    return sum / FIX_V3_VBAT_FILTER_SAMPLES;
}

/**
 * @brief Evalúa si debe entrar/salir de modo reposo
 * 
 * Implementa histéresis con umbrales:
 * - Entrada: vBat <= 3.20V (inmediato)
 * - Salida: vBat >= 3.80V Y estable (3 ciclos consecutivos)
 * 
 * @param vBatFiltered Voltaje filtrado actual
 * @return true si puede operar normalmente (enviar LTE), false si debe estar en reposo
 */
static bool evaluateBatteryState(float vBatFiltered) {
    g_lastVBatFiltered = vBatFiltered;
    
    if (!g_restMode) {
        // === MODO NORMAL: Evaluar entrada a reposo ===
        if (vBatFiltered <= FIX_V3_UTS_LOW_ENTER) {
            g_restMode = true;
            g_stableCycleCounter = 0;
            Serial.println(F(""));
            Serial.println(F("╔════════════════════════════════════════════════════╗"));
            Serial.println(F("║  [FIX-V3] BATERIA BAJA - ENTRANDO A MODO REPOSO    ║"));
            Serial.print(F("║  vBat_filtrada: "));
            Serial.print(vBatFiltered, 2);
            Serial.print(F("V <= "));
            Serial.print(FIX_V3_UTS_LOW_ENTER, 2);
            Serial.println(F("V                  ║"));
            Serial.println(F("║  Radio/LTE BLOQUEADO hasta recuperacion            ║"));
            Serial.println(F("╚════════════════════════════════════════════════════╝"));
            
            // ============ [FEAT-V7 START] Registrar entrada a low battery ============
            #if ENABLE_FEAT_V7_PRODUCTION_DIAG
            ProdDiag::recordLowBatteryEnter((uint16_t)(vBatFiltered * 100));  // Centésimas
            #endif
            // ============ [FEAT-V7 END] ============
            
            return false;
        }
        return true;  // Operar normalmente
        
    } else {
        // === MODO REPOSO: Evaluar salida ===
        if (vBatFiltered >= FIX_V3_UTS_LOW_EXIT) {
            g_stableCycleCounter++;
            Serial.print(F("[FIX-V3] vBat: "));
            Serial.print(vBatFiltered, 2);
            Serial.print(F("V >= "));
            Serial.print(FIX_V3_UTS_LOW_EXIT, 2);
            Serial.print(F("V | Estabilidad: "));
            Serial.print(g_stableCycleCounter);
            Serial.print(F("/"));
            Serial.println(FIX_V3_STABLE_CYCLES_REQUIRED);
            
            if (g_stableCycleCounter >= FIX_V3_STABLE_CYCLES_REQUIRED) {
                g_restMode = false;
                g_stableCycleCounter = 0;
                Serial.println(F(""));
                Serial.println(F("╔════════════════════════════════════════════════════╗"));
                Serial.println(F("║  [FIX-V3] BATERIA RECUPERADA - SALIENDO DE REPOSO ║"));
                Serial.println(F("║  Condicion de estabilidad cumplida                ║"));
                Serial.println(F("║  Reanudando operacion normal                       ║"));
                Serial.println(F("╚════════════════════════════════════════════════════╝"));
                
                // ============ [FEAT-V7 START] Registrar salida de low battery ============
                #if ENABLE_FEAT_V7_PRODUCTION_DIAG
                ProdDiag::recordLowBatteryExit(ProdDiag::getStats().lowBatteryCycles);
                #endif
                // ============ [FEAT-V7 END] ============
                
                return true;
            }
        } else {
            // Voltaje cayó: reiniciar contador de estabilidad
            if (g_stableCycleCounter > 0) {
                Serial.print(F("[FIX-V3] vBat: "));
                Serial.print(vBatFiltered, 2);
                Serial.print(F("V < "));
                Serial.print(FIX_V3_UTS_LOW_EXIT, 2);
                Serial.println(F("V | Estabilidad REINICIADA"));
            }
            g_stableCycleCounter = 0;
        }
        
        Serial.print(F("[FIX-V3] Modo REPOSO activo. vBat: "));
        Serial.print(vBatFiltered, 2);
        Serial.println(F("V. Esperando recuperacion..."));
        
        // ============ [FEAT-V7 START] Incrementar ciclos en modo reposo ============
        #if ENABLE_FEAT_V7_PRODUCTION_DIAG
        ProdDiag::incrementLowBatteryCycle();
        #endif
        // ============ [FEAT-V7 END] ============
        
        return false;
    }
}
#endif
// ============ [FIX-V3 END] ============

/**
 * @brief Formatea una coordenada GPS (latitud o longitud) con 6 decimales
 * 
 * Convierte un valor float a string con precisión de 6 decimales, lo cual
 * proporciona aproximadamente 0.1 metros de precisión en el ecuador.
 * 
 * @param out Buffer de salida para el string resultante
 * @param outSz Tamaño del buffer de salida
 * @param v Valor de coordenada a formatear
 * 
 * @note Formato de salida: "±DD.DDDDDD" (ej: "-99.123456")
 */
static void formatCoord(char* out, size_t outSz, float v) {
  snprintf(out, outSz, "%.6f", (double)v);
}

/**
 * @brief Formatea la altitud GPS como entero (metros sobre nivel del mar)
 * 
 * Redondea la altitud al metro más cercano y la convierte a string.
 * 
 * @param out Buffer de salida para el string resultante
 * @param outSz Tamaño del buffer de salida
 * @param altMeters Altitud en metros (valor float)
 * 
 * @note La altitud se redondea para reducir tamaño de trama sin pérdida significativa de precisión
 */
static void formatAlt(char* out, size_t outSz, float altMeters) {
  long a = lroundf(altMeters);
  snprintf(out, outSz, "%ld", a);
}

/**
 * @brief Lee el sensor ADC descartando muestras iniciales y promediando
 * 
 * Implementa una estrategia de muestreo robusto:
 * 1. Descarta las primeras DISCARD_SAMPLES lecturas (5)
 * 2. Promedia las siguientes KEEP_SAMPLES lecturas (5)
 * 3. Redondea el resultado a entero
 * 
 * @param[out] outValueStr String de salida con el valor promediado
 * 
 * @return true si se obtuvieron lecturas válidas, false si todas fallaron
 * 
 * @note El descarte de muestras iniciales elimina efectos transitorios del multiplexor ADC
 */
static bool readADC_DiscardAndAverage(String& outValueStr) {
  double sum = 0.0;
  uint8_t got = 0;

  for (uint8_t i = 0; i < (DISCARD_SAMPLES + KEEP_SAMPLES); i++) {
    if (!adcSensor.readSensor()) continue;
    if (i >= DISCARD_SAMPLES) {
      sum += adcSensor.getValue();
      got++;
    }
  }

  if (got == 0) return false;

  int avgInt = (int)lround(sum / got);
  outValueStr = String(avgInt);
  return true;
}

/**
 * @brief Lee el sensor I2C (temperatura y humedad) con descarte y promediado
 * 
 * Implementa la misma estrategia de muestreo robusto que readADC_DiscardAndAverage:
 * 1. Descarta las primeras DISCARD_SAMPLES lecturas (5)
 * 2. Promedia las siguientes KEEP_SAMPLES lecturas (5)
 * 3. Multiplica por 100 para almacenar con 2 decimales como entero
 * 
 * @param[out] outTempStr String con temperatura promediada (en centésimas de grado)
 * @param[out] outHumStr String con humedad promediada (en centésimas de porcentaje)
 * 
 * @return true si se obtuvieron lecturas válidas, false si todas fallaron
 * 
 * @note Los valores se almacenan multiplicados por 100 (ej: 25.67°C → "2567")
 */
static bool readI2C_DiscardAndAverage(String& outTempStr, String& outHumStr) {
  double sumT = 0.0, sumH = 0.0;
  uint8_t got = 0;

  for (uint8_t i = 0; i < (DISCARD_SAMPLES + KEEP_SAMPLES); i++) {
    if (!i2cSensor.readSensor()) continue;
    if (i >= DISCARD_SAMPLES) {
      sumT += i2cSensor.getTemperature();
      sumH += i2cSensor.getHumidity();
      got++;
    }
  }

  if (got == 0) return false;

  int t100 = (int)lround((sumT / got) * 100.0);
  int h100 = (int)lround((sumH / got) * 100.0);
  outTempStr = String(t100);
  outHumStr  = String(h100);
  return true;
}

/**
 * @brief Lee el sensor RS485 Modbus descartando lecturas iniciales
 * 
 * Implementa estrategia de descarte pero toma solo la última lectura válida
 * (no promedia, ya que Modbus puede tener valores discretos):
 * 1. Descarta las primeras DISCARD_SAMPLES lecturas (5)
 * 2. Toma la última lectura válida de las siguientes KEEP_SAMPLES (5)
 * 
 * @param[out] regsOut Array de 4 strings con los valores de los 4 registros Modbus
 * 
 * @return true si se obtuvo al menos una lectura válida, false si todas fallaron
 * 
 * @note Lee 4 registros consecutivos via Modbus RTU (función 0x03 - Read Holding Registers)
 */
static bool readRS485_DiscardAndTakeLast(String regsOut[4]) {
  bool okLast = false;

  for (uint8_t i = 0; i < (DISCARD_SAMPLES + KEEP_SAMPLES); i++) {
    bool ok = rs485Sensor.readSensor();
    if (i >= DISCARD_SAMPLES && ok) {
      okLast = true;
      regsOut[0] = rs485Sensor.getRegisterString(0);
      regsOut[1] = rs485Sensor.getRegisterString(1);
      regsOut[2] = rs485Sensor.getRegisterString(2);
      regsOut[3] = rs485Sensor.getRegisterString(3);
    }
  }

  return okLast;
}

/**
 * @brief Envía todas las tramas del buffer por LTE y las marca como procesadas
 * 
 * Implementa un sistema completo de transmisión con selección inteligente de operadora:
 * 
 * 1. **Recuperación de operadora persistente:**
 *    - Lee operadora exitosa anterior desde NVS ("lastOperator")
 *    - Si no existe, escanea todas las operadoras disponibles
 * 
 * 2. **Selección de operadora:**
 *    - Si hay operadora guardada: la usa directamente (optimización)
 *    - Si no: testea todas y selecciona la mejor según score de señal
 *    - Score = (4×SINR) + 2×(RSRP+120) + (RSRQ+20)
 * 
 * 3. **Conexión LTE:**
 *    - Enciende modem SIM7080G
 *    - Configura operadora seleccionada
 *    - Attach a red, activa PDP context
 *    - Abre conexión TCP al servidor
 * 
 * 4. **Transmisión de tramas:**
 *    - Lee todas las líneas del buffer (máx MAX_LINES_TO_READ)
 *    - Salta líneas ya marcadas como procesadas
 *    - Envía cada línea por TCP
 *    - Marca como procesada si envío exitoso
 *    - Detiene envío al primer fallo (preserva datos)
 * 
 * 5. **Gestión de operadora persistente:**
 *    - Si hubo éxito: guarda operadora en NVS para próximos ciclos
 *    - Si falló todo: borra operadora guardada para forzar escaneo
 * 
 * @return true si se envió al menos una trama exitosamente, false si no se pudo enviar ninguna
 * 
 * @note Las tramas que no se enviaron permanecen en el buffer para el próximo ciclo
 * @note El sistema optimiza reconectándose a la última operadora exitosa
 * @see BUFFERModule::markLineAsProcessed()
 * @see LTEModule::testOperator()
 * @see LTEModule::getBestOperator()
 */
static bool sendBufferOverLTE_AndMarkProcessed() {
  if (!lte.powerOn()) return false;

  Operadora operadoraAUsar;
  bool tieneOperadoraGuardada = false;

  preferences.begin("sensores", false);
  if (preferences.isKey("lastOperator")) {
    operadoraAUsar = (Operadora)preferences.getUChar("lastOperator", 0);
    tieneOperadoraGuardada = true;
    Serial.print("[INFO][APP] Usando operadora guardada: ");
    Serial.println(OPERADORAS[operadoraAUsar].nombre);
  }
  preferences.end();

  if (!tieneOperadoraGuardada) {
    Serial.println("[INFO][APP] No hay operadora guardada. Probando todas...");
    for (uint8_t i = 0; i < NUM_OPERADORAS; i++) {
      lte.testOperator((Operadora)i);
    }
    operadoraAUsar = lte.getBestOperator();
    Serial.print("[INFO][APP] Mejor operadora seleccionada: ");
    Serial.println(OPERADORAS[operadoraAUsar].nombre);
  }

#if ENABLE_FIX_V1_SKIP_RESET_PDP
  // ============ [FIX-V1 START] Si tiene operadora guardada, skip reset ============
  bool configOk = lte.configureOperator(operadoraAUsar, tieneOperadoraGuardada);
  // ============ [FIX-V1 END] ============
#else
  bool configOk = lte.configureOperator(operadoraAUsar);
#endif

#if ENABLE_FIX_V2_FALLBACK_OPERADORA
  // ============ [FIX-V2 START] Fallback a escaneo si falla operadora guardada ============
  // Fecha: 13 Ene 2026
  // Autor: Luis Ocaranza
  // Requisito: RF-12
  // Premisas: P2 (mínimo), P3 (defaults), P4 (flag), P5 (logs), P6 (aditivo)
  if (!configOk && tieneOperadoraGuardada) {
    Serial.println("[WARN][APP] Operadora guardada falló. Evaluando fallback...");
    
    // --- PROTECCIÓN ANTI-BUCLE: Verificar si debemos saltar escaneo ---
    preferences.begin("sensores", false);
    uint8_t skipCycles = preferences.getUChar("skipScanCycles", 0);
    if (skipCycles > 0) {
      preferences.putUChar("skipScanCycles", skipCycles - 1);
      preferences.end();
      Serial.print("[WARN][APP] Saltando escaneo. Ciclos restantes: ");
      Serial.println(skipCycles - 1);
      lte.powerOff();
      return false;
    }
    preferences.end();
    // --- FIN PROTECCIÓN ANTI-BUCLE ---
    
    Serial.println("[INFO][APP] Iniciando escaneo de todas las operadoras...");
    
    // Borrar operadora de NVS inmediatamente
    preferences.begin("sensores", false);
    preferences.remove("lastOperator");
    preferences.end();
    Serial.println("[INFO][APP] Operadora eliminada de NVS");
    
    // Escanear todas las operadoras
    for (uint8_t i = 0; i < NUM_OPERADORAS; i++) {
      lte.testOperator((Operadora)i);
    }
    
    // Seleccionar la mejor
    operadoraAUsar = lte.getBestOperator();
    int bestScore = lte.getOperatorScore(operadoraAUsar);
    tieneOperadoraGuardada = false;  // Ya no tiene guardada
    
    Serial.print("[INFO][APP] Nueva operadora seleccionada: ");
    Serial.print(OPERADORAS[operadoraAUsar].nombre);
    Serial.print(" (Score: ");
    Serial.print(bestScore);
    Serial.println(")");
    
    // ============ [FEAT-V7 START] Registrar fallback de operadora ============
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::recordOperatorFallback();
    #endif
    // ============ [FEAT-V7 END] ============
    
    // --- VALIDACIÓN DE SCORE: Verificar que hay señal válida ---
    if (bestScore <= -999) {
      Serial.println("[ERROR][APP] Ninguna operadora con señal válida.");
      Serial.print("[INFO][APP] Activando protección: saltando próximos ");
      Serial.print(FIX_V2_SKIP_CYCLES_ON_FAIL);
      Serial.println(" ciclos de escaneo.");
      
      preferences.begin("sensores", false);
      preferences.putUChar("skipScanCycles", FIX_V2_SKIP_CYCLES_ON_FAIL);
      preferences.end();
      
      lte.powerOff();
      return false;
    }
    // --- FIN VALIDACIÓN ---
    
    // Intentar configurar con la nueva operadora
    configOk = lte.configureOperator(operadoraAUsar);
  }
  // ============ [FIX-V2 END] ============
#endif

  if (!configOk) { lte.powerOff(); return false; }
  
  // Guardar info para CYCLE SUMMARY
  g_lastOperadoraUsed = operadoraAUsar;
  g_lastOperatorScore = lte.getOperatorScore(operadoraAUsar);
  
  if (!lte.attachNetwork())                     { lte.powerOff(); return false; }
  if (!lte.activatePDP())                       { lte.powerOff(); return false; }
  
  // Obtener CSQ para CYCLE SUMMARY
  g_lastCSQ = lte.getCSQ();
  
  if (!lte.openTCPConnection())                 { lte.deactivatePDP(); lte.powerOff(); return false; }

  String allLines[MAX_LINES_TO_READ];
  int total = 0;
  if (!buffer.readLines(allLines, MAX_LINES_TO_READ, total)) {
    lte.closeTCPConnection();
    lte.deactivatePDP();
    lte.powerOff();
    Serial.println("[ERROR][APP] No se pudieron leer líneas del buffer");
    return false;
  }

  Serial.print("[INFO][APP] Líneas en buffer: ");
  Serial.println(total);

  bool anySent = false;
  int sentCount = 0;

  for (int i = 0; i < total; i++) {
    if (allLines[i].startsWith(PROCESSED_MARKER)) {
      Serial.print("[DEBUG][APP] Línea ");
      Serial.print(i+1);
      Serial.println(" ya procesada, saltando...");
      continue;
    }

    Serial.print("[INFO][APP] Enviando línea ");
    Serial.print(i+1);
    Serial.print("/");
    Serial.println(total);
    
    bool sentOk = lte.sendTCPData(allLines[i]);
    if (sentOk) {
      buffer.markLineAsProcessed(i);
      anySent = true;
      sentCount++;
      Serial.print("[INFO][APP] Línea ");
      Serial.print(i+1);
      Serial.println(" enviada y marcada como procesada");
      delay(50);
    } else {
      Serial.print("[WARN][APP] Fallo al enviar línea ");
      Serial.print(i+1);
      Serial.println(". Deteniendo envío. Línea permanece en buffer.");
      break;
    }
  }

  lte.closeTCPConnection();
  lte.deactivatePDP();
  lte.detachNetwork();
  lte.powerOff();

  Serial.print("[INFO][APP] Resumen: ");
  Serial.print(sentCount);
  Serial.print(" de ");
  Serial.print(total);
  Serial.println(" tramas enviadas");

  preferences.begin("sensores", false);
  if (anySent) {
    preferences.putUChar("lastOperator", (uint8_t)operadoraAUsar);
    Serial.print("[INFO][APP] Operadora guardada para futuros envios: ");
    Serial.println(OPERADORAS[operadoraAUsar].nombre);
    CRASH_MARK_SUCCESS();  // FEAT-V3: Marcar ciclo exitoso
    g_lteCycleSuccess = true;  // Marcar ciclo LTE exitoso para CYCLE SUMMARY
    
    // ============ [FEAT-V7 START] Registrar envío exitoso ============
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::recordLTESendOk();
    #endif
    // ============ [FEAT-V7 END] ============
  } else {
    if (tieneOperadoraGuardada) {
      preferences.remove("lastOperator");
      Serial.println("[WARN][APP] Envio fallido. Operadora eliminada. Próximo ciclo escaneará todas.");
    }
    // ============ [FEAT-V7 START] Registrar envío fallido ============
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::recordLTESendFail();
    #endif
    // ============ [FEAT-V7 END] ============
  }
  preferences.end();

  return anySent;
}

/**
 * @brief Inicializa la aplicación y todos sus módulos
 * 
 * Secuencia de inicialización:
 * 
 * 1. **Configuración básica:**
 *    - Guarda configuración de usuario (tiempo de sleep)
 *    - Inicia comunicación serial para debug (115200 baud)
 * 
 * 2. **Gestión de energía:**
 *    - Inicia módulo de sleep
 *    - Detecta causa del wakeup (timer, pin, o boot desde apagado)
 * 
 * 3. **Sistema de tiempo:**
 *    - Inicializa RTC DS1307 para timestamps
 * 
 * 4. **Almacenamiento:**
 *    - Monta sistema LittleFS para buffer persistente
 *    - Si falla: entra en estado de Error
 * 
 * 5. **Sensores:**
 *    - Inicializa ADC (voltaje batería)
 *    - Inicializa I2C (temperatura/humedad)
 *    - Inicializa RS485 Modbus RTU
 * 
 * 6. **Comunicaciones:**
 *    - Configura módulo LTE (SIM7080G)
 *    - Habilita debug del modem
 * 
 * 7. **Selección de modo:**
 *    - **Si wakeup == UNDEFINED (boot desde apagado):**
 *      * Inicia BLE para configuración
 *      * Estado inicial: BleOnly
 *      * Marca próximo ciclo para lectura GPS
 *    - **Si wakeup == TIMER (wakeup desde sleep):**
 *      * Salta BLE
 *      * Estado inicial: Cycle_ReadSensors
 *      * NO lee GPS (usa coordenadas guardadas)
 * 
 * @param cfg Estructura de configuración con parámetros de usuario
 * 
 * @note BLE solo se activa en el primer boot, no en wakeups subsecuentes
 * @note GPS solo se lee en el primer ciclo post-boot para ahorrar energía
 */
void AppInit(const AppConfig& cfg) {
  g_cfg = cfg;

  Serial.begin(115200);
  delay(200);

  // ============ [FEAT-V3 START] Crash Diagnostics Init ============
  #if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
  CRASH_CHECKPOINT(CP_BOOT_START);
  CrashDiag::init();
  
  // Imprimir reporte si hubo crash
  if (CrashDiag::hadCrash()) {
    Serial.println(F("\n⚠️ CRASH DETECTADO EN CICLO ANTERIOR"));
    CrashDiag::printReport();
    Serial.println(F("Escribe 'HISTORY' para ver historial completo\n"));
    
    // ============ [FEAT-V7 START] Registrar crash en ProductionDiag ============
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::recordCrash(CrashDiag::getLastCheckpoint());
    #endif
    // ============ [FEAT-V7 END] ============
  }
  #endif
  // ============ [FEAT-V3 END] ============

  // ============ [FEAT-V0 START] Imprimir versión al iniciar ============
  printFirmwareVersion();
  // ============ [FEAT-V0 END] ============

  // ============ [FEAT-V1 START] Imprimir feature flags activos ============
  printActiveFlags();
  // ============ [FEAT-V1 END] ============

  // ============ [FIX-V5 START] Inicializar Watchdog de Sistema ============
  #if ENABLE_FIX_V5_WATCHDOG
  // ESP-IDF v5.x requiere estructura de configuración
  const esp_task_wdt_config_t wdt_config = {
    .timeout_ms = FIX_V5_WATCHDOG_TIMEOUT_S * 1000,  // Convertir a ms
    .idle_core_mask = 0,                             // No monitorear idle tasks
    .trigger_panic = true                            // Reset en timeout
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);  // Agregar tarea actual (loopTask)
  Serial.print(F("[FIX-V5] Watchdog iniciado ("));
  Serial.print(FIX_V5_WATCHDOG_TIMEOUT_S);
  Serial.println(F("s)"));
  #endif
  // ============ [FIX-V5 END] ============

  // ============ [FEAT-V4 START] Validación anti boot-loop ============
  #if ENABLE_FEAT_V4_PERIODIC_RESTART
  {
    esp_reset_reason_t reset_reason = esp_reset_reason();
    
    // [FEAT-V5] Log de contadores stress test al inicio (verificar persistencia RTC)
    #if DEBUG_STRESS_TEST_ENABLED
    Serial.printf("[STRESS] Boot - restart_count=%lu, cycle_count=%lu (RTC persist check)\n",
                  g_stress_restart_count, g_stress_cycle_count);
    #endif
    
    // Caso 1: Boot después de restart planificado FEAT-V4
    if (g_last_restart_reason_feat4 == FEAT4_RESTART_EXECUTED) {
      Serial.println(F("[FEAT-V4] Boot post-restart periódico detectado."));
      Serial.printf("[FEAT-V4] Tiempo acumulado antes de restart: %llu us\n", g_accum_sleep_us);
      
      // Reset completo del acumulador y flag
      g_accum_sleep_us = 0;
      g_last_restart_reason_feat4 = FEAT4_RESTART_NONE;
    }
    // Caso 2: Power-on (power cycle completo) - resetear acumulador y contadores stress
    else if (reset_reason == ESP_RST_POWERON) {
      Serial.println(F("[FEAT-V4] Power-on detectado. Reseteando acumulador."));
      g_accum_sleep_us = 0;
      g_last_restart_reason_feat4 = FEAT4_RESTART_NONE;
      #if DEBUG_STRESS_TEST_ENABLED
      g_stress_restart_count = 0;
      g_stress_cycle_count = 0;
      Serial.println(F("[STRESS] Power-on: contadores reseteados a 0"));
      #endif
    }
    // Caso 3: Wakeup normal de deep sleep - acumulador persiste (no hacer nada)
    
    // Log estado actual del acumulador
    float pct = (FEAT_V4_THRESHOLD_US > 0) ? 
                ((float)g_accum_sleep_us / FEAT_V4_THRESHOLD_US * 100.0f) : 0.0f;
    Serial.printf("[FEAT-V4] Acumulador: %llu / %llu us (%.1f%%)\n",
        g_accum_sleep_us, 
        (uint64_t)FEAT_V4_THRESHOLD_US,
        pct);
  }
  #endif
  // ============ [FEAT-V4 END] ============

  sleepModule.begin();
  g_wakeupCause = sleepModule.getWakeupCause();

  (void)initializeRTC();

  if (!buffer.begin()) {
    g_state = AppState::Error;
    g_initialized = true;
    return;
  }
  CRASH_CHECKPOINT(CP_BOOT_LITTLEFS_OK);  // FEAT-V3

  // ============ [FEAT-V7 START] Inicializar Production Diagnostics ============
  #if ENABLE_FEAT_V7_PRODUCTION_DIAG
  ProdDiag::init();
  ProdDiag::setResetReason((uint8_t)esp_reset_reason());
  #endif
  // ============ [FEAT-V7 END] ============

  // ============ [FIX-V9 START] Imprimir info adicional de boot ============
  {
    esp_reset_reason_t rst = esp_reset_reason();
    Serial.print(F("[INFO] Reset reason: "));
    switch(rst) {
      case ESP_RST_POWERON:  Serial.println(F("POWERON")); break;
      case ESP_RST_SW:       Serial.println(F("SOFTWARE")); break;
      case ESP_RST_PANIC:    Serial.println(F("PANIC/CRASH")); break;
      case ESP_RST_INT_WDT:  Serial.println(F("INT_WATCHDOG")); break;
      case ESP_RST_TASK_WDT: Serial.println(F("TASK_WATCHDOG")); break;
      case ESP_RST_WDT:      Serial.println(F("WATCHDOG")); break;
      case ESP_RST_DEEPSLEEP: Serial.println(F("DEEPSLEEP")); break;
      case ESP_RST_BROWNOUT: Serial.println(F("BROWNOUT")); break;
      default:               Serial.printf("UNKNOWN(%d)\n", (int)rst); break;
    }
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    const ProdDiagStats& st = ProdDiag::getStats();
    Serial.printf("[INFO] Ciclos: %lu total, %lu desde boot\n", st.totalCycles, st.cyclesSinceBoot);
    #endif
  }
  Serial.println(F("====================="));
  // ============ [FIX-V9 END] ============

  (void)adcSensor.begin();
  (void)i2cSensor.begin();
  (void)rs485Sensor.begin();

  lte.begin();
  lte.setDebug(true, &Serial);

  if (g_wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    #if ENABLE_FEAT_V9_BLE_CONFIG
    ble.begin("RV03");
    g_state = AppState::BleOnly;
    #else
    // BLE deshabilitado - saltar directamente a ciclo de sensores
    Serial.println(F("[INFO][APP] BLE DESHABILITADO - Iniciando ciclo directo"));
    g_state = AppState::Cycle_ReadSensors;
    #endif
    g_firstCycleAfterBoot = true;
  } else {
    g_state = AppState::Cycle_ReadSensors;
    g_firstCycleAfterBoot = false;
  }

  g_initialized = true;
}

/**
 * @brief Loop principal de la máquina de estados finitos (FSM)
 * 
 * Ejecuta la lógica de cada estado de la FSM. Debe ser llamada continuamente
 * desde loop() de Arduino.
 * 
 * **Estados y transiciones:**
 * 
 * - **BleOnly:**
 *   - Actualiza servicio BLE
 *   - Cuando BLE se desactiva → Cycle_ReadSensors
 * 
 * - **Cycle_ReadSensors:**
 *   - Lee ADC (batería), I2C (temp/hum), RS485 (4 registros)
 *   - Si es primer ciclo → Cycle_Gps
 *   - Si no → recupera GPS de NVS y va a Cycle_GetICCID
 * 
 * - **Cycle_Gps:**
 *   - Enciende GNSS, espera fix, guarda coordenadas en NVS
 *   - Si no hay fix → usa ceros
 *   - Siempre → Cycle_GetICCID
 * 
 * - **Cycle_GetICCID:**
 *   - Enciende LTE brevemente para leer ICCID
 *   - Apaga LTE
 *   - Siempre → Cycle_BuildFrame
 * 
 * - **Cycle_BuildFrame:**
 *   - Construye trama texto: $,iccid,epoch,lat,lng,alt,var1-7,#
 *   - Codifica a Base64
 *   - Muestra ambas versiones por serial
 *   - Si éxito → Cycle_BufferWrite
 *   - Si fallo → Error
 * 
 * - **Cycle_BufferWrite:**
 *   - Guarda trama Base64 en buffer LittleFS (persistente)
 *   - Siempre → Cycle_SendLTE (aunque falle guardado)
 * 
 * - **Cycle_SendLTE:**
 *   - Llama sendBufferOverLTE_AndMarkProcessed()
 *   - Siempre → Cycle_CompactBuffer
 * 
 * - **Cycle_CompactBuffer:**
 *   - Elimina del buffer solo líneas marcadas como procesadas
 *   - Tramas no enviadas permanecen para próximo ciclo
 *   - Siempre → Cycle_Sleep
 * 
 * - **Cycle_Sleep:**
 *   - Configura timer wakeup (según cfg.sleep_time_us)
 *   - Entra en deep sleep (no retorna)
 *   - Próximo wakeup → reinicia ESP32 → AppInit()
 * 
 * - **Boot / Error:**
 *   - Espera indefinidamente (delay 1s)
 * 
 * @note Esta función nunca debe ser bloqueante excepto en estados finales
 * @note El sistema usa deep sleep, por lo que cada wakeup reinicia completamente el ESP32
 */
void AppLoop() {
  if (!g_initialized) return;

  // ============ [FIX-V5 START] Feed del Watchdog ============
  #if ENABLE_FIX_V5_WATCHDOG
  esp_task_wdt_reset();  // Alimentar watchdog cada iteración del loop
  #endif
  // ============ [FIX-V5 END] ============

  // ============ [FEAT-V9 START] Comandos Serial Diagnóstico ============
  // Lector Serial INDEPENDIENTE de flags específicos
  if (Serial.available()) {
    Serial.setTimeout(50);  // Prevenir bloqueo por monitor serial abierto
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    // Comandos FEAT-V3: CrashDiagnostics
    #if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
    if (cmd == "DIAG") {
      CrashDiag::printReport();
    } else if (cmd == "HISTORY") {
      CrashDiag::printHistory();
    } else if (cmd == "CLEAR") {
      CrashDiag::clearHistory();
    }
    #endif
    
    // Comandos FEAT-V7: ProductionDiag
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    if (cmd == "STATS") {
      ProdDiag::printStats();
    } else if (cmd == "LOG") {
      ProdDiag::printEventLog();
    }
    #endif
  }
  // ============ [FEAT-V9 END] ============

  // ============ [FEAT-V8 START] Comandos de testing Serial ============
  #if ENABLE_FEAT_V8_TESTING
  TestModule::processCommand(&Serial);
  #endif
  // ============ [FEAT-V8 END] ============

  // ============ [STRESS TEST] Skip BLE wait en modo stress ============
  #if DEBUG_STRESS_TEST_ENABLED
  // En stress test: desactivar BLE inmediatamente para ciclos rápidos
  #if ENABLE_FEAT_V9_BLE_CONFIG
  if (ble.isActive()) {
    Serial.println(F("[STRESS] Skipping BLE wait - desactivando inmediatamente"));
    ble.end();
  }
  #else
  ble.update();
  #endif
  #endif

  switch (g_state) {
    #if ENABLE_FEAT_V9_BLE_CONFIG
    case AppState::BleOnly: {
      if (!ble.isActive()) {
        TIMING_RESET(g_timing);  // Inicia timing del ciclo cuando termina BLE
        g_lteCycleSuccess = false;  // Reset para CYCLE SUMMARY
        g_lastCSQ = 99;  // Reset CSQ
        
        // ============ [DEBUG-EMI] Log de inicio de ciclo diagnóstico EMI ============
        #if DEBUG_EMI_DIAGNOSTIC_ENABLED
        g_emiDiagCycleCount++;
        Serial.println();
        Serial.println(F("╔════════════════════════════════════════════════════════════╗"));
        Serial.printf(   "║  [EMI-DIAG] CICLO #%lu / %d                                  ║\n", 
                        g_emiDiagCycleCount, DEBUG_EMI_DIAGNOSTIC_CYCLES);
        Serial.printf(   "║  Heap libre: %lu bytes                                     ║\n", ESP.getFreeHeap());
        Serial.println(F("║  Modo: COMUNICACIÓN REAL (mocks desactivados)              ║"));
        Serial.println(F("╚════════════════════════════════════════════════════════════╝"));
        #endif
        
        // ============ [FEAT-V5] Log de inicio de ciclo stress test ============
        #if DEBUG_STRESS_TEST_ENABLED
        g_stress_cycle_count++;
        g_stress_heap_start = ESP.getFreeHeap();
        g_stress_cycle_start_ms = millis();  // Track tiempo real del ciclo
        Serial.println();
        Serial.println(F("\n[STRESS] ══════════════════════════════════════"));
        Serial.printf("[STRESS] CICLO #%lu (restart #%lu)\n", g_stress_cycle_count, g_stress_restart_count);
        Serial.printf("[STRESS] Heap libre: %lu bytes\n", g_stress_heap_start);
        #if ENABLE_FEAT_V4_PERIODIC_RESTART
        uint64_t threshold = FEAT_V4_THRESHOLD_US;
        uint8_t pct = (uint8_t)((g_accum_sleep_us * 100ULL) / threshold);
        uint32_t secsToRestart = (uint32_t)((threshold - g_accum_sleep_us) / 1000000ULL);
        Serial.printf("[STRESS] Tiempo acum: %llu / %llu us (%u%%)\n", g_accum_sleep_us, threshold, pct);
        Serial.printf("[STRESS] Proximo restart en: ~%lu seg\n", secsToRestart);
        #endif
        Serial.println(F("[STRESS] ══════════════════════════════════════"));
        #endif
        
        g_state = AppState::Cycle_ReadSensors;
      }
      break;
    }
    #endif

    case AppState::Cycle_ReadSensors: {
      TIMING_START(g_timing, sensors);
      String vBat;
      if (!readADC_DiscardAndAverage(vBat)) vBat = "0";

      String vTemp, vHum;
      if (!readI2C_DiscardAndAverage(vTemp, vHum)) { vTemp = "0"; vHum = "0"; }

      String regs[4] = {"0","0","0","0"};
      (void)readRS485_DiscardAndTakeLast(regs);

      
      g_varStr[0] = regs[0];
      g_varStr[1] = regs[1];
      g_varStr[2] = regs[2];
      g_varStr[3] = regs[3];
      g_varStr[4] = vTemp;
      g_varStr[5] = vHum;
      g_varStr[6] = vBat;
      TIMING_END(g_timing, sensors);

      if (g_firstCycleAfterBoot) {
        Serial.println("[INFO][APP] Primer ciclo: leyendo GPS");
        g_state = AppState::Cycle_Gps;
        g_firstCycleAfterBoot = false;
      } else {
        TIMING_START(g_timing, nvsGps);
        Serial.println("[INFO][APP] Ciclo subsecuente: recuperando coordenadas GPS de NVS");
        
        preferences.begin("sensores", true);
        if (preferences.isKey("gps_lat") && preferences.isKey("gps_lng") && preferences.isKey("gps_alt")) {
          float lat = preferences.getFloat("gps_lat", 0.0f);
          float lng = preferences.getFloat("gps_lng", 0.0f);
          float alt = preferences.getFloat("gps_alt", 0.0f);
          preferences.end();
          
          formatCoord(g_lat, sizeof(g_lat), lat);
          formatCoord(g_lng, sizeof(g_lng), lng);
          formatAlt(g_alt, sizeof(g_alt), alt);
          
          Serial.print("[INFO][APP] GPS recuperado - Lat: ");
          Serial.print(g_lat);
          Serial.print(", Lng: ");
          Serial.print(g_lng);
          Serial.print(", Alt: ");
          Serial.println(g_alt);
        } else {
          preferences.end();
          fillZeros(g_lat, COORD_LEN);
          fillZeros(g_lng, COORD_LEN);
          fillZeros(g_alt, ALT_LEN);
          Serial.println("[WARN][APP] No hay coordenadas GPS en NVS, usando ceros");
        }
        TIMING_END(g_timing, nvsGps);
        g_state = AppState::Cycle_GetICCID;
      }
      break;
    }

    case AppState::Cycle_Gps: {
      TIMING_START(g_timing, gps);
      fillZeros(g_lat, COORD_LEN);
      fillZeros(g_lng, COORD_LEN);
      fillZeros(g_alt, ALT_LEN);

      #if DEBUG_MOCK_GPS
      // [DEBUG][FEAT-V5] GPS simulado para stress test
      {
        unsigned long mockStart = millis();
        formatCoord(g_lat, sizeof(g_lat), 19.4326f);   // CDMX dummy lat
        formatCoord(g_lng, sizeof(g_lng), -99.1332f);  // CDMX dummy lng
        formatAlt(g_alt, sizeof(g_alt), 2240.0f);      // CDMX dummy alt
        Serial.printf("[MOCK][GPS] Coords: %s, %s, alt=%s (%lums)\n", 
                      g_lat, g_lng, g_alt, millis() - mockStart);
      }
      TIMING_END(g_timing, gps);
      g_state = AppState::Cycle_GetICCID;
      break;
      #endif

      GpsFix fix;
      bool gotFix = false;

      if (gps.powerOn()) {
        gotFix = gps.getCoordinatesAndShutdown(fix);
      }

      if (gotFix && fix.hasFix) {
        formatCoord(g_lat, sizeof(g_lat), fix.latitude);
        formatCoord(g_lng, sizeof(g_lng), fix.longitude);
        formatAlt(g_alt, sizeof(g_alt), fix.altitude);
        
        preferences.begin("sensores", false);
        preferences.putFloat("gps_lat", fix.latitude);
        preferences.putFloat("gps_lng", fix.longitude);
        preferences.putFloat("gps_alt", fix.altitude);
        preferences.end();
        
        Serial.println("[INFO][APP] Coordenadas GPS guardadas en NVS");
        Serial.print("[INFO][APP] Lat: ");
        Serial.print(g_lat);
        Serial.print(", Lng: ");
        Serial.print(g_lng);
        Serial.print(", Alt: ");
        Serial.println(g_alt);
      } else {
        Serial.println("[WARN][APP] No se obtuvo fix GPS, usando ceros");
      }
      TIMING_END(g_timing, gps);
      g_state = AppState::Cycle_GetICCID;
      break;
    }
    case AppState::Cycle_GetICCID: {
      TIMING_START(g_timing, iccid);
      
      #if DEBUG_MOCK_ICCID
      // [DEBUG][FEAT-V5] ICCID simulado para stress test
      {
        unsigned long mockStart = millis();
        g_iccid = "89520000000000000000";  // ICCID dummy
        Serial.printf("[MOCK][ICCID] %s (%lums)\n", g_iccid.c_str(), millis() - mockStart);
      }
      #else
      if (lte.powerOn()) {
        g_iccid = lte.getICCID();
        // ============ [FIX-V8 START] Logging fallo ICCID ============
        #if ENABLE_FIX_V8_ICCID_FAIL_LOGGING && ENABLE_FEAT_V7_PRODUCTION_DIAG
        if (g_iccid.length() == 0) {
          Serial.println(F("[WARN][APP] ICCID vacío - fallo AT (timeout o SIM)"));
          ProdDiag::recordATTimeout();
        }
        #endif
        // ============ [FIX-V8 END] ============
        lte.powerOff();
      } else {
        g_iccid = "";
        // ============ [FIX-V8 START] Logging fallo powerOn ============
        #if ENABLE_FIX_V8_ICCID_FAIL_LOGGING && ENABLE_FEAT_V7_PRODUCTION_DIAG
        Serial.println(F("[WARN][APP] powerOn falló - fallo AT durante ICCID"));
        ProdDiag::recordATTimeout();
        #endif
        // ============ [FIX-V8 END] ============
      }
      #endif
      
      TIMING_END(g_timing, iccid);
      g_state = AppState::Cycle_BuildFrame;
      break;
    }
    case AppState::Cycle_BuildFrame: {
      TIMING_START(g_timing, buildFrame);
      g_epoch = getEpochString();
      g_lastEpoch = (uint32_t)g_epoch.toInt();  // FEAT-V7: Guardar epoch numérico

      // ============ [FEAT-V9 START] Actualizar epoch en ProductionDiag ============
      #if ENABLE_FEAT_V7_PRODUCTION_DIAG
      ProdDiag::setCurrentEpoch(g_lastEpoch);
      #endif
      // ============ [FEAT-V9 END] ============

      formatter.reset();
      formatter.setIccid(g_iccid.c_str());
      formatter.setEpoch(g_epoch.c_str());
      formatter.setLat(g_lat);
      formatter.setLng(g_lng);
      formatter.setAlt(g_alt);

      for (uint8_t i = 0; i < VAR_COUNT; i++) {
        formatter.setVar(i, g_varStr[i].c_str());
      }

      char frameNormal[FRAME_MAX_LEN];
      if (!formatter.buildFrame(frameNormal, sizeof(frameNormal))) {
        g_state = AppState::Error;
        break;
      }
      Serial.println("[INFO][APP] === TRAMA NORMAL ===");
      Serial.println(frameNormal);

      Serial.println("[DEBUG][APP] Generando trama Base64...");
      if (!formatter.buildFrameBase64(g_frame, sizeof(g_frame))) {
        g_state = AppState::Error;
        break;
      }
      Serial.println("[INFO][APP] === TRAMA BASE64 ===");
      Serial.println(g_frame);
      Serial.println();
      TIMING_END(g_timing, buildFrame);
      g_state = AppState::Cycle_BufferWrite;
      break;
    }

    case AppState::Cycle_BufferWrite: {
      TIMING_START(g_timing, bufferWrite);
      Serial.println("[INFO][APP] Guardando trama en buffer (persistente)...");
      bool saved = buffer.appendLine(String(g_frame));
      if (saved) {
        Serial.println("[INFO][APP] Trama guardada exitosamente en buffer");
      } else {
        Serial.println("[ERROR][APP] Fallo al guardar trama en buffer");
      }
      TIMING_END(g_timing, bufferWrite);
      
#if ENABLE_FIX_V3_LOW_BATTERY_MODE
      // ============ [FIX-V3 START] Verificar batería antes de LTE ============
      float vBatFiltered = readVBatFiltered();
      
      if (!evaluateBatteryState(vBatFiltered)) {
        // Estamos en reposo - SALTAR LTE, ir directo a sleep
        Serial.println(F("[FIX-V3] Datos guardados. LTE bloqueado por bateria baja."));
        Serial.print(F("[FIX-V3] Buffer tiene tramas pendientes. TX cuando vBat >= "));
        Serial.print(FIX_V3_UTS_LOW_EXIT, 2);
        Serial.println(F("V estable."));
        g_state = AppState::Cycle_Sleep;  // Saltar LTE
        break;
      }
      // ============ [FIX-V3 END] ============
#endif
      
      Serial.println("[DEBUG][APP] Pasando a Cycle_SendLTE");
      g_state = AppState::Cycle_SendLTE;
      break;
    }

    case AppState::Cycle_SendLTE: {
      TIMING_START(g_timing, sendLte);
      
      #if DEBUG_MOCK_LTE
      // [DEBUG][FEAT-V5] LTE simulado para stress test - marca todo como enviado
      {
        unsigned long mockStart = millis();
        String lines[20];
        int count = 0;
        if (buffer.readUnprocessedLines(lines, 20, count)) {
          for (int i = 0; i < count; i++) {
            buffer.markLineAsProcessed(i);
          }
        }
        Serial.printf("[MOCK][LTE] %d tramas marcadas como enviadas (%lums)\n", count, millis() - mockStart);
      }
      #else
      Serial.println("[DEBUG][APP] Iniciando envio por LTE...");
      (void)sendBufferOverLTE_AndMarkProcessed();
      Serial.println("[DEBUG][APP] Envio completado, pasando a CompactBuffer");
      #endif
      
      TIMING_END(g_timing, sendLte);
      g_state = AppState::Cycle_CompactBuffer;
      break;
    }

    case AppState::Cycle_CompactBuffer: {
      TIMING_START(g_timing, compactBuffer);
      Serial.println("[INFO][APP] Eliminando solo tramas procesadas del buffer...");
      (void)buffer.removeProcessedLines();
      Serial.println("[INFO][APP] Buffer compactado. Tramas no enviadas permanecen para próximo ciclo.");
      TIMING_END(g_timing, compactBuffer);
      g_state = AppState::Cycle_Sleep;
      break;
    }

    case AppState::Cycle_Sleep: {
      TIMING_FINALIZE(g_timing);
      TIMING_PRINT_SUMMARY(g_timing);
      printCycleSummary();  // Resumen de datos del ciclo
      
      // ============ [FEAT-V7 START] Finalizar ciclo y guardar diagnósticos ============
      #if ENABLE_FEAT_V7_PRODUCTION_DIAG
      ProdDiag::incrementCycle();
      ProdDiag::evaluateCycleEMI();
      ProdDiag::saveStats(g_lastEpoch);  // Usar último epoch conocido
      ProdDiag::resetCycleEMI();  // Resetear para próximo ciclo
      #endif
      // ============ [FEAT-V7 END] ============
      
      // ============ [DEBUG-EMI] Reporte de diagnóstico EMI ============
      #if DEBUG_EMI_DIAGNOSTIC_ENABLED
      Serial.printf("\n[EMI-DIAG] Fin ciclo %lu / %d\n", g_emiDiagCycleCount, DEBUG_EMI_DIAGNOSTIC_CYCLES);
      
      if (g_emiDiagCycleCount >= DEBUG_EMI_DIAGNOSTIC_CYCLES) {
        // Generar reporte final
        printEMIDiagnosticReport(&Serial);
        
        // Resetear para siguiente ronda
        resetEMIDiagnosticStats();
        g_emiDiagCycleCount = 0;
        
        Serial.println(F("[EMI-DIAG] *** Estadísticas reseteadas. Iniciando nueva ronda ***"));
      }
      #endif
      
      // ============ [FEAT-V5] Log de fin de ciclo stress test ============
      #if DEBUG_STRESS_TEST_ENABLED
      {
        uint32_t heapNow = ESP.getFreeHeap();
        int32_t heapDelta = (int32_t)heapNow - (int32_t)g_stress_heap_start;
        Serial.println(F("\n[STRESS] ────────── FIN CICLO ──────────"));
        Serial.printf("[STRESS] Heap: %lu -> %lu (%+ld bytes)%s\n", 
                      g_stress_heap_start, heapNow, heapDelta,
                      (heapDelta < -500) ? " ⚠️ LEAK?" : "");
        Serial.printf("[STRESS] Ciclos totales: %lu\n", g_stress_cycle_count);
        Serial.println(F("[STRESS] ──────────────────────────────\n"));
      }
      #endif
      
      // ============ [FIX-V4 START] Apagar modem antes de deep sleep ============
      #if ENABLE_FIX_V4_MODEM_POWEROFF_SLEEP
      // Garantizar apagado limpio del modem según datasheet SIM7080G
      // Referencia: Hardware Design v1.05, Page 23, 27
      // "It is strongly recommended to turn off the module through PWRKEY 
      //  or AT command before disconnecting the module VBAT power."
      Serial.println(F("[FIX-V4] Asegurando apagado de modem antes de sleep..."));
      lte.powerOff();  // Ahora usa URC "NORMAL POWER DOWN" + PWRKEY fallback
      Serial.println(F("[FIX-V4] Secuencia de apagado completada."));
      #endif
      // ============ [FIX-V4 END] ============
      
      // ============ [FEAT-V4 START] Reinicio periódico preventivo ============
      #if ENABLE_FEAT_V4_PERIODIC_RESTART
      {
        // Acumular tiempo: en stress test usa tiempo REAL, en producción usa sleep planificado
        #if DEBUG_STRESS_TEST_ENABLED
        // En stress test: acumular SOLO tiempo real del ciclo (awake)
        // No sumamos sleep porque no hacemos deep sleep real en stress test
        uint32_t cycle_real_ms = millis() - g_stress_cycle_start_ms;
        uint64_t time_to_add = (uint64_t)cycle_real_ms * 1000ULL;  // ms -> us
        g_accum_sleep_us += time_to_add;
        Serial.printf("[STRESS] Tiempo ciclo real: %lu ms (%llu us agregados)\\n", 
                      cycle_real_ms, time_to_add);
        Serial.printf("[STRESS] Acumulador ahora: %llu / %llu us\\n", 
                      g_accum_sleep_us, (uint64_t)FEAT_V4_THRESHOLD_US);
        #else
        // En producción: acumular solo tiempo de sleep planificado
        g_accum_sleep_us += g_cfg.sleep_time_us;
        #endif
        
        // Log del progreso del acumulador
        float pct = (FEAT_V4_THRESHOLD_US > 0) ? 
                    ((float)g_accum_sleep_us / FEAT_V4_THRESHOLD_US * 100.0f) : 0.0f;
        Serial.printf("[FEAT-V4] Acumulador: %llu / %llu us (%.1f%%) -> ", 
            g_accum_sleep_us, (uint64_t)FEAT_V4_THRESHOLD_US, pct);
        
        // ¿Alcanzamos el threshold (24h por defecto)?
        if (g_accum_sleep_us >= FEAT_V4_THRESHOLD_US) {
          Serial.println(F("RESTART"));
          
          // [FEAT-V5] Incrementar contador de restarts
          #if DEBUG_STRESS_TEST_ENABLED
          uint32_t cycles_completed = g_stress_cycle_count;  // Guardar antes de reset
          g_stress_restart_count++;
          Serial.printf("[STRESS] *** RESTART #%lu alcanzado tras %lu ciclos ***\n", 
                        g_stress_restart_count, cycles_completed);
          g_stress_cycle_count = 0;  // Reset ciclos para nuevo período (después del log)
          #endif
          
          Serial.println(F(""));
          Serial.println(F("╔════════════════════════════════════════════════════╗"));
          Serial.println(F("║  [FEAT-V4] REINICIO PERIODICO PREVENTIVO           ║"));
          Serial.println(F("╠════════════════════════════════════════════════════╣"));
          Serial.printf(   "║  Tiempo acumulado: %llu us\n", g_accum_sleep_us);
          #if FEAT_V4_STRESS_TEST_MODE
          Serial.printf(   "║  Threshold: %llu us (%d minutos STRESS)\n", (uint64_t)FEAT_V4_THRESHOLD_US, FEAT_V4_RESTART_MINUTES);
          #else
          Serial.printf(   "║  Threshold: %llu us (%d horas)\n", (uint64_t)FEAT_V4_THRESHOLD_US, FEAT_V4_RESTART_HOURS);
          #endif
          Serial.printf(   "║  Reset reason anterior: %d\n", (int)esp_reset_reason());
          Serial.println(F("║  Motivo: PERIODIC_24H (planificado)                ║"));
          Serial.println(F("║  Ejecutando esp_restart() en punto seguro...       ║"));
          Serial.println(F("╚════════════════════════════════════════════════════╝"));
          
          // ============ [FEAT-V7 START] Registrar reinicio periódico ============
          #if ENABLE_FEAT_V7_PRODUCTION_DIAG
          ProdDiag::recordPeriodicRestart(ProdDiag::getStats().totalCycles);
          #endif
          // ============ [FEAT-V7 END] ============
          
          // Marcar que el restart fue intencional (anti boot-loop)
          g_last_restart_reason_feat4 = FEAT4_RESTART_EXECUTED;
          
          // Integración FEAT-V3: Checkpoint antes de restart
          CRASH_CHECKPOINT(CP_SLEEP_ENTER);  // Usar mismo checkpoint (es un "exit" limpio)
          CRASH_SYNC_NVS();
          
          Serial.flush();  // Garantizar que logs se envían
          delay(100);
          
          esp_restart();  // Reinicio limpio - NO llega a deep sleep
          // Nunca llega aquí
        } else {
          Serial.println(F("sleep"));
        }
      }
      #endif
      // ============ [FEAT-V4 END] ============
      
      // ============ [STRESS TEST] Skip deep sleep para ciclos rápidos ============
      #if DEBUG_STRESS_TEST_ENABLED
      Serial.println(F("[STRESS] Skipping deep sleep - volviendo a inicio inmediatamente"));
      delay(500);  // Pequeña pausa para no saturar logs
      #if ENABLE_FEAT_V9_BLE_CONFIG
      g_state = AppState::BleOnly;  // Volver al inicio con BLE
      #else
      g_state = AppState::Cycle_ReadSensors;  // Saltar BLE, directo a sensores
      #endif
      break;  // Salir del case sin hacer deep sleep
      #endif
      // ============ [STRESS TEST END] ============
      
      CRASH_CHECKPOINT(CP_SLEEP_ENTER);  // FEAT-V3
      CRASH_SYNC_NVS();  // FEAT-V3: Guardar estado antes de sleep
      sleepModule.clearWakeupSources();
      esp_sleep_enable_timer_wakeup(g_cfg.sleep_time_us);
      sleepModule.enterDeepSleep();
      break;
    }

    case AppState::Boot:
    case AppState::Error:
    default:
      delay(1000);
      break;
  }
}
