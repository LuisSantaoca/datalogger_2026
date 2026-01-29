/**
 * @file ProductionDiag.h
 * @brief Sistema de diagnóstico de producción - Contadores y log de eventos
 * @version FEAT-V7
 * @date 2026-01-28
 * 
 * Sistema de diagnóstico ligero siempre activo para:
 * - Acumular estadísticas de operación (LTE, batería, EMI)
 * - Registrar eventos críticos con timestamp
 * - Revisar estado al conectar Serial en campo
 * - Detectar EMI/ruido sin overhead significativo
 * 
 * @see FEAT_V7_PRODUCTION_DIAGNOSTICS.md para documentación completa
 */

#ifndef PRODUCTION_DIAG_H
#define PRODUCTION_DIAG_H

#include <Arduino.h>
#include "config_production_diag.h"

// ============================================================
// ESTRUCTURA DE ESTADÍSTICAS
// ============================================================

/**
 * @struct ProductionStats
 * @brief Contadores persistentes en LittleFS
 * 
 * Esta estructura se guarda en /diag/stats.bin y se actualiza
 * al final de cada ciclo. Incluye CRC16 para detectar corrupción.
 */
struct ProductionStats {
    // Identificador y versión
    uint32_t magic;              ///< 0x44494147 "DIAG" - detectar corrupción
    uint8_t  version;            ///< Versión del struct (migración futura)
    
    // Contadores de ciclo
    uint32_t totalCycles;        ///< Ciclos desde primera instalación
    uint32_t cyclesSinceBoot;    ///< Ciclos desde último power-on
    
    // Contadores LTE
    uint32_t lteSendOk;          ///< Tramas enviadas OK
    uint32_t lteSendFail;        ///< Intentos fallidos
    uint16_t atTimeouts;         ///< Timeouts AT acumulados
    uint16_t operatorFallbacks;  ///< Fallbacks de operadora
    
    // Contadores batería
    uint16_t lowBatteryCycles;   ///< Ciclos en modo reposo
    uint16_t lowBatteryEvents;   ///< Veces que entró a reposo
    
    // Contadores sistema
    uint8_t  feat4Restarts;      ///< Reinicios periódicos 24h
    uint8_t  crashCount;         ///< Crashes detectados
    uint8_t  lastResetReason;    ///< esp_reset_reason_t
    
    // Contadores EMI
    uint32_t atCommandsTotal;    ///< Total comandos AT ejecutados
    uint32_t atCorrupted;        ///< Respuestas con bytes inválidos
    uint32_t invalidCharsTotal;  ///< Bytes 0xFF/0x00/fuera de rango
    uint8_t  worstCorruptPct;    ///< Peor % corrupción (0-100)
    uint8_t  emiEvents;          ///< Veces que corrupción > 1%
    
    // Contadores GPS
    uint16_t gpsFails;           ///< Fallos de GPS
    
    // Timestamp
    uint32_t lastUpdateEpoch;    ///< Último update (epoch)
    uint32_t firstBootEpoch;     ///< Primer boot registrado
    
    // Reservado para futuro
    uint8_t  reserved[8];
    
    // Checksum
    uint16_t crc16;              ///< Validación de integridad
};

// ============================================================
// CONTADORES TEMPORALES (RAM) PARA CICLO ACTUAL
// ============================================================

/**
 * @struct CycleEMIStats
 * @brief Contadores EMI del ciclo actual (no persistentes)
 */
struct CycleEMIStats {
    uint16_t atCommands;         ///< Comandos AT este ciclo
    uint16_t corrupted;          ///< Comandos con corrupción
    uint16_t invalidChars;       ///< Bytes inválidos este ciclo
};

// ============================================================
// NAMESPACE PARA FUNCIONES
// ============================================================

/**
 * @namespace ProdDiag
 * @brief Funciones del sistema de diagnóstico de producción
 */
namespace ProdDiag {

    // ---- Inicialización ----
    
    /**
     * @brief Inicializa el sistema de diagnóstico
     * @return true si la inicialización fue exitosa
     * 
     * Crea directorio /diag si no existe, carga o inicializa stats.bin.
     * Debe llamarse en AppInit() después de montar LittleFS.
     */
    bool init();
    
    // ---- Contadores de ciclo ----
    
    /**
     * @brief Incrementa contador de ciclos completados
     */
    void incrementCycle();
    
    /**
     * @brief Registra envío LTE exitoso
     */
    void recordLTESendOk();
    
    /**
     * @brief Registra fallo de envío LTE
     */
    void recordLTESendFail();
    
    /**
     * @brief Registra timeout de comando AT
     */
    void recordATTimeout();
    
    /**
     * @brief Registra fallback de operadora
     */
    void recordOperatorFallback();
    
    /**
     * @brief Registra entrada a modo batería baja
     * @param vBat Voltaje de batería (centésimas: 318 = 3.18V)
     */
    void recordLowBatteryEnter(uint16_t vBatCents);
    
    /**
     * @brief Registra salida de modo batería baja
     * @param cyclesInRest Ciclos que estuvo en reposo
     */
    void recordLowBatteryExit(uint16_t cyclesInRest);
    
    /**
     * @brief Incrementa contador de ciclos en modo batería baja
     */
    void incrementLowBatteryCycle();
    
    /**
     * @brief Registra reinicio periódico 24h
     * @param cyclesBeforeRestart Ciclos acumulados antes del restart
     */
    void recordPeriodicRestart(uint32_t cyclesBeforeRestart);
    
    /**
     * @brief Registra fallo de GPS
     * @param retries Número de reintentos
     */
    void recordGPSFail(uint8_t retries);
    
    /**
     * @brief Registra crash detectado
     * @param checkpoint Checkpoint donde ocurrió
     */
    void recordCrash(uint8_t checkpoint);
    
    // ---- EMI Detection ----
    
    /**
     * @brief Resetea contadores EMI del ciclo actual
     */
    void resetCycleEMI();
    
    /**
     * @brief Cuenta caracteres inválidos en respuesta AT
     * @param response Respuesta del modem
     * 
     * Llamar después de cada comando AT para contabilizar EMI.
     */
    void countEMI(const String& response);
    
    /**
     * @brief Evalúa EMI del ciclo y registra eventos si necesario
     * 
     * Llamar al final del ciclo LTE, antes de guardar stats.
     */
    void evaluateCycleEMI();
    
    // ---- Persistencia ----
    
    /**
     * @brief Guarda estadísticas en LittleFS
     * @param epoch Timestamp actual (0 = no actualizar timestamp)
     * @return true si se guardó correctamente
     */
    bool saveStats(uint32_t epoch = 0);
    
    /**
     * @brief Carga estadísticas desde LittleFS
     * @return true si se cargó correctamente
     */
    bool loadStats();
    
    // ---- Log de eventos ----
    
    /**
     * @brief Registra un evento en el log
     * @param eventCode Código del evento (EVT_*)
     * @param data Dato asociado
     * @param epoch Timestamp (0 = usar último conocido)
     */
    void logEvent(char eventCode, uint16_t data, uint32_t epoch = 0);
    
    // ---- Comandos Serial ----
    
    /**
     * @brief Procesa comando de diagnóstico recibido por Serial
     * @param cmd Comando recibido (STATS, LOG, CLEAR)
     * @return true si el comando fue reconocido
     */
    bool handleCommand(const String& cmd);
    
    /**
     * @brief Imprime estadísticas al Serial
     */
    void printStats();
    
    /**
     * @brief Imprime log de eventos al Serial
     */
    void printEventLog();
    
    /**
     * @brief Limpia todos los datos de diagnóstico
     */
    void clearAll();
    
    // ---- Utilidades ----
    
    /**
     * @brief Obtiene veredicto EMI basado en estadísticas
     * @return String con veredicto ("PCB OK", "MONITOR", "PROBLEMA EMI")
     */
    const char* getEMIVerdict();
    
    /**
     * @brief Verifica si un carácter es inválido para respuesta AT
     * @param c Carácter a verificar
     * @return true si es inválido (0xFF, 0x00, >0x7E excepto CR/LF)
     */
    inline bool isInvalidATChar(uint8_t c) {
        return (c == 0xFF || c == 0x00 || (c > 0x7E && c != 0x0D && c != 0x0A));
    }
    
    /**
     * @brief Calcula CRC16 de los datos
     * @param data Puntero a los datos
     * @param len Longitud de los datos
     * @return CRC16
     */
    uint16_t calculateCRC16(const uint8_t* data, size_t len);
    
    /**
     * @brief Obtiene acceso a las estadísticas (solo lectura)
     * @return Referencia constante a ProductionStats
     */
    const ProductionStats& getStats();
    
    /**
     * @brief Actualiza razón del último reset
     * @param reason esp_reset_reason_t
     */
    void setResetReason(uint8_t reason);

} // namespace ProdDiag

#endif // PRODUCTION_DIAG_H
