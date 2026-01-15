/**
 * @file CrashDiagnostics.h
 * @brief Sistema de diagnóstico post-mortem para análisis de crashes
 * @version FEAT-V3
 * @date 2026-01-15
 * 
 * Este módulo implementa un sistema de "caja negra" para diagnóstico
 * post-mortem de crashes en dispositivos IoT desplegados en campo.
 * 
 * Estrategia de persistencia dual:
 * - RTC Memory: Rápido (~1µs), sobrevive reset/WDT, NO brownout
 * - NVS: Lento (~1-5ms), sobrevive TODO incluyendo brownout
 * - LittleFS: Historial extendido (32 eventos)
 * 
 * @see FEAT_V3_CRASH_DIAGNOSTICS.md para documentación completa
 */

#ifndef CRASH_DIAGNOSTICS_H
#define CRASH_DIAGNOSTICS_H

#include <Arduino.h>
#include "config_crash_diagnostics.h"
#include "../FeatureFlags.h"

// ============================================================
// CHECKPOINTS DEL SISTEMA
// ============================================================

/**
 * @enum CrashCheckpoint
 * @brief Puntos de control para identificar ubicación de crashes
 * 
 * Los rangos están organizados por subsistema:
 * - 10-19: Boot y sistema
 * - 100-109: Modem power
 * - 110-119: AT commands
 * - 120-129: Network attach
 * - 130-139: PDP context
 * - 140-149: TCP operations
 * - 150-159: Power off
 * - 200: Sleep
 * - 250: Ciclo exitoso
 */
enum CrashCheckpoint : uint8_t {
    CP_NONE = 0,
    
    // Boot y sistema (10-19)
    CP_BOOT_START = 10,
    CP_BOOT_SERIAL_OK = 11,
    CP_BOOT_GPIO_OK = 12,
    CP_BOOT_LITTLEFS_OK = 13,
    
    // Módem power (100-109)
    CP_MODEM_POWER_ON_START = 100,
    CP_MODEM_POWER_ON_PWRKEY = 101,
    CP_MODEM_POWER_ON_WAIT = 102,
    CP_MODEM_POWER_ON_OK = 103,
    
    // Módem AT commands (110-119)
    CP_MODEM_AT_SEND = 110,
    CP_MODEM_AT_WAIT_RESPONSE = 111,
    CP_MODEM_AT_RESPONSE_OK = 112,
    CP_MODEM_AT_RESPONSE_ERROR = 113,
    CP_MODEM_AT_TIMEOUT = 114,
    
    // Network (120-129)
    CP_MODEM_NETWORK_ATTACH_START = 120,
    CP_MODEM_NETWORK_ATTACH_WAIT = 121,
    CP_MODEM_NETWORK_ATTACH_OK = 122,
    CP_MODEM_NETWORK_ATTACH_FAIL = 123,
    
    // PDP Context (130-139)
    CP_MODEM_PDP_ACTIVATE_START = 130,
    CP_MODEM_PDP_ACTIVATE_WAIT = 131,
    CP_MODEM_PDP_ACTIVATE_OK = 132,
    CP_MODEM_PDP_ACTIVATE_FAIL = 133,
    
    // TCP (140-149)
    CP_MODEM_TCP_CONNECT_START = 140,
    CP_MODEM_TCP_CONNECT_WAIT = 141,
    CP_MODEM_TCP_CONNECT_OK = 142,
    CP_MODEM_TCP_CONNECT_FAIL = 143,
    CP_MODEM_TCP_SEND_START = 144,
    CP_MODEM_TCP_SEND_WAIT = 145,
    CP_MODEM_TCP_SEND_OK = 146,
    CP_MODEM_TCP_CLOSE = 147,
    
    // Power off (150-159)
    CP_MODEM_POWER_OFF_START = 150,
    CP_MODEM_POWER_OFF_OK = 151,
    
    // Sleep (200)
    CP_SLEEP_ENTER = 200,
    
    // Ciclo exitoso (250)
    CP_CYCLE_SUCCESS = 250,
};

// ============================================================
// ESTRUCTURAS DE DATOS
// ============================================================

/**
 * @struct CrashContext
 * @brief Contexto de crash almacenado en RTC memory
 * 
 * Esta estructura sobrevive a reset y WDT, pero NO a brownout.
 * Se usa para capturar el estado exacto al momento del crash.
 */
struct CrashContext {
    // Checkpoint actual
    uint8_t checkpoint;            ///< Enum del último punto alcanzado
    uint32_t timestamp_ms;         ///< millis() del checkpoint
    
    // Contexto del módem
    char last_at_command[CRASH_DIAG_AT_CMD_LEN];   ///< Último comando AT enviado
    char last_at_response[CRASH_DIAG_AT_RESP_LEN]; ///< Última respuesta recibida
    uint8_t modem_state;           ///< Estado del módem (0-255)
    uint8_t lte_attempts;          ///< Intentos de conexión LTE
    int8_t rssi;                   ///< Señal al momento del crash
    
    // Historial circular
    uint8_t history[CRASH_DIAG_HISTORY_SIZE];  ///< Últimos N checkpoints (ring buffer)
    uint8_t history_idx;           ///< Índice del ring buffer
    
    // Validación
    uint32_t magic;                ///< 0xDEADBEEF = datos válidos
};

/**
 * @struct CrashLogEntry
 * @brief Entrada en el log de LittleFS
 * 
 * Formato compacto para minimizar uso de flash.
 */
struct CrashLogEntry {
    uint32_t epoch;                ///< Timestamp del evento
    uint8_t checkpoint;            ///< Checkpoint alcanzado
    uint8_t event_type;            ///< 0=checkpoint, 1=crash, 2=success
    uint8_t reset_reason;          ///< Solo si event_type=1
    int8_t rssi;                   ///< Señal al momento
    char at_command[CRASH_DIAG_LOG_AT_LEN]; ///< Comando AT (truncado)
};

// ============================================================
// MACROS PARA USO CON FEATURE FLAG
// ============================================================

#if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
    #define CRASH_CHECKPOINT(cp) CrashDiag::setCheckpoint(cp)
    #define CRASH_LOG_AT(cmd) CrashDiag::logATCommand(cmd)
    #define CRASH_LOG_RESPONSE(resp) CrashDiag::logATResponse(resp)
    #define CRASH_ANALYZE() CrashDiag::analyzeLastCrash()
    #define CRASH_PRINT_REPORT() CrashDiag::printReport()
    #define CRASH_SYNC_NVS() CrashDiag::syncToNVS()
    #define CRASH_MARK_SUCCESS() CrashDiag::markCycleSuccess()
    #define CRASH_SET_RSSI(val) CrashDiag::setRSSI(val)
#else
    #define CRASH_CHECKPOINT(cp)
    #define CRASH_LOG_AT(cmd)
    #define CRASH_LOG_RESPONSE(resp)
    #define CRASH_ANALYZE()
    #define CRASH_PRINT_REPORT()
    #define CRASH_SYNC_NVS()
    #define CRASH_MARK_SUCCESS()
    #define CRASH_SET_RSSI(val)
#endif

// ============================================================
// NAMESPACE API
// ============================================================

namespace CrashDiag {
    
    // ---- Inicialización ----
    
    /**
     * @brief Inicializar sistema de diagnóstico
     * 
     * Debe llamarse al inicio de setup(). Realiza:
     * - Incrementar boot_count
     * - Analizar reset_reason
     * - Si crash: incrementar crash_count, guardar contexto
     * 
     * @return true si inicialización exitosa
     */
    bool init();
    
    // ---- Checkpoints (muy rápido, solo RTC) ----
    
    /**
     * @brief Marcar un checkpoint
     * 
     * Actualiza RTC memory con el checkpoint actual.
     * Tiempo de ejecución: ~1µs
     * 
     * @param cp Checkpoint a marcar
     */
    void setCheckpoint(CrashCheckpoint cp);
    
    /**
     * @brief Guardar último comando AT enviado
     * 
     * Se trunca a CRASH_DIAG_AT_CMD_LEN caracteres (prefijo).
     * Llamar ANTES de enviar el comando.
     * 
     * @param cmd Comando AT (ej: "AT+CAOPEN=0,0,\"TCP\"...")
     */
    void logATCommand(const char* cmd);
    
    /**
     * @brief Guardar última respuesta AT recibida
     * 
     * Se trunca a CRASH_DIAG_AT_RESP_LEN caracteres (prefijo).
     * 
     * @param resp Respuesta recibida del módem
     */
    void logATResponse(const char* resp);
    
    /**
     * @brief Guardar valor de RSSI actual
     * 
     * @param rssi Valor de señal en dBm
     */
    void setRSSI(int8_t rssi);
    
    // ---- Persistencia NVS ----
    
    /**
     * @brief Sincronizar RTC a NVS
     * 
     * Llamar ANTES de operaciones de alto riesgo:
     * - Antes de AT+CAOPEN (TCP connect)
     * - Antes de AT+CASEND (TCP send)
     * - Antes de entrar a deep sleep
     * 
     * Tiempo de ejecución: ~1-5ms
     */
    void syncToNVS();
    
    // ---- Análisis post-mortem ----
    
    /**
     * @brief Verificar si hubo crash en ciclo anterior
     * 
     * @return true si el último reset fue anormal
     */
    bool hadCrash();
    
    /**
     * @brief Analizar causa del último reset
     * 
     * Actualiza contadores según tipo de reset.
     * Se llama automáticamente en init().
     */
    void analyzeLastCrash();
    
    /**
     * @brief Imprimir reporte completo a Serial
     * 
     * Formato legible para humanos con toda la información
     * de diagnóstico disponible.
     */
    void printReport();
    
    /**
     * @brief Imprimir historial extendido de LittleFS
     * 
     * Muestra los últimos CRASH_DIAG_LOG_MAX_ENTRIES eventos.
     */
    void printHistory();
    
    /**
     * @brief Limpiar historial de LittleFS
     * 
     * Usar después de analizar, antes de redesplegar.
     */
    void clearHistory();
    
    // ---- Ciclo exitoso ----
    
    /**
     * @brief Marcar ciclo como exitoso
     * 
     * Resetea consecutive_crashes y cycles_since_success.
     * Llamar después de TCP send exitoso.
     */
    void markCycleSuccess();
    
    // ---- Getters para debug ----
    
    uint16_t getBootCount();
    uint16_t getCrashCount();
    uint8_t getConsecutiveCrashes();
    uint8_t getLastCheckpoint();
    uint8_t getLastResetReason();
    
    /**
     * @brief Obtener nombre legible del checkpoint
     * 
     * @param cp Checkpoint a convertir
     * @return String con nombre (ej: "TCP_CONNECT_START")
     */
    const char* checkpointToString(CrashCheckpoint cp);
    
    /**
     * @brief Obtener nombre legible del reset reason
     * 
     * @param reason Código ESP_RST_xxx
     * @return String con nombre (ej: "TASK_WDT")
     */
    const char* resetReasonToString(uint8_t reason);
    
    /**
     * @brief Verificar si un reset reason es crash
     * 
     * @param reason Código ESP_RST_xxx
     * @return true si es crash (WDT, PANIC, BROWNOUT, UNKNOWN)
     */
    bool isCrashReason(uint8_t reason);
}

#endif // CRASH_DIAGNOSTICS_H
