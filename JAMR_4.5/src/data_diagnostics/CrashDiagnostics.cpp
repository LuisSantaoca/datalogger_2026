/**
 * @file CrashDiagnostics.cpp
 * @brief Implementación del sistema de diagnóstico post-mortem
 * @version FEAT-V3
 * @date 2026-01-15
 * 
 * @see CrashDiagnostics.h para documentación de API
 * @see FEAT_V3_CRASH_DIAGNOSTICS.md para documentación completa
 */

#include "CrashDiagnostics.h"

#if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS

#include <Preferences.h>
#include <LittleFS.h>
#include <esp_system.h>

// ============================================================
// VARIABLES RTC (sobreviven reset, NO brownout)
// ============================================================

RTC_DATA_ATTR static CrashContext g_crash_ctx;

// ============================================================
// VARIABLES LOCALES
// ============================================================

static Preferences s_prefs;
static bool s_initialized = false;
static bool s_had_crash = false;
static uint8_t s_last_reset_reason = 0;

// Contadores cacheados de NVS
static uint16_t s_boot_count = 0;
static uint16_t s_crash_count = 0;
static uint8_t s_consecutive_crashes = 0;
static uint8_t s_cycles_since_success = 0;

// ============================================================
// FUNCIONES AUXILIARES INTERNAS
// ============================================================

/**
 * @brief Truncar string preservando prefijo
 */
static void safeCopy(char* dst, const char* src, size_t maxLen) {
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    size_t len = strlen(src);
    if (len >= maxLen) {
        len = maxLen - 1;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/**
 * @brief Agregar checkpoint al historial circular
 */
static void addToHistory(uint8_t cp) {
    g_crash_ctx.history[g_crash_ctx.history_idx] = cp;
    g_crash_ctx.history_idx = (g_crash_ctx.history_idx + 1) % CRASH_DIAG_HISTORY_SIZE;
}

/**
 * @brief Escribir entrada al log de LittleFS
 */
static void writeLogEntry(uint8_t eventType, uint32_t epoch) {
    if (!LittleFS.begin(false)) {
        Serial.println(F("[WARN][CRASH] LittleFS no disponible para log"));
        return;
    }
    
    // Leer índice actual
    File idxFile = LittleFS.open("/crash_idx.bin", "r");
    uint8_t currentIdx = 0;
    if (idxFile) {
        idxFile.read(&currentIdx, 1);
        idxFile.close();
    }
    
    // Preparar entrada
    CrashLogEntry entry;
    entry.epoch = epoch;
    entry.checkpoint = g_crash_ctx.checkpoint;
    entry.event_type = eventType;
    entry.reset_reason = s_last_reset_reason;
    entry.rssi = g_crash_ctx.rssi;
    safeCopy(entry.at_command, g_crash_ctx.last_at_command, CRASH_DIAG_LOG_AT_LEN);
    
    // Escribir entrada
    File logFile = LittleFS.open(CRASH_DIAG_LOG_PATH, "r+");
    if (!logFile) {
        // Crear archivo nuevo
        logFile = LittleFS.open(CRASH_DIAG_LOG_PATH, "w");
        if (!logFile) {
            Serial.println(F("[ERROR][CRASH] No se pudo crear log"));
            return;
        }
        // Inicializar con ceros
        uint8_t zeros[sizeof(CrashLogEntry)] = {0};
        for (int i = 0; i < CRASH_DIAG_LOG_MAX_ENTRIES; i++) {
            logFile.write(zeros, sizeof(CrashLogEntry));
        }
        logFile.close();
        logFile = LittleFS.open(CRASH_DIAG_LOG_PATH, "r+");
    }
    
    // Posicionar y escribir
    size_t pos = currentIdx * sizeof(CrashLogEntry);
    logFile.seek(pos);
    logFile.write((uint8_t*)&entry, sizeof(CrashLogEntry));
    logFile.close();
    
    // Actualizar índice
    currentIdx = (currentIdx + 1) % CRASH_DIAG_LOG_MAX_ENTRIES;
    idxFile = LittleFS.open("/crash_idx.bin", "w");
    if (idxFile) {
        idxFile.write(&currentIdx, 1);
        idxFile.close();
    }
}

// ============================================================
// IMPLEMENTACIÓN API
// ============================================================

namespace CrashDiag {

bool init() {
    if (s_initialized) return true;
    
    // Obtener reset reason
    s_last_reset_reason = (uint8_t)esp_reset_reason();
    
    // Cargar datos de NVS
    if (!s_prefs.begin(CRASH_DIAG_NVS_NAMESPACE, false)) {
        Serial.println(F("[ERROR][CRASH] No se pudo abrir NVS"));
        return false;
    }
    
    s_boot_count = s_prefs.getUShort(NVS_KEY_BOOT_COUNT, 0);
    s_crash_count = s_prefs.getUShort(NVS_KEY_CRASH_COUNT, 0);
    s_consecutive_crashes = s_prefs.getUChar(NVS_KEY_CONSEC_CRASH, 0);
    s_cycles_since_success = s_prefs.getUChar(NVS_KEY_CYCLES_FAIL, 0);
    
    // Incrementar boot count
    s_boot_count++;
    s_prefs.putUShort(NVS_KEY_BOOT_COUNT, s_boot_count);
    
    // Analizar tipo de reset
    if (isCrashReason(s_last_reset_reason)) {
        s_had_crash = true;
        s_crash_count++;
        s_consecutive_crashes++;
        s_cycles_since_success++;
        
        s_prefs.putUShort(NVS_KEY_CRASH_COUNT, s_crash_count);
        s_prefs.putUChar(NVS_KEY_CONSEC_CRASH, s_consecutive_crashes);
        s_prefs.putUChar(NVS_KEY_CYCLES_FAIL, s_cycles_since_success);
        
        // Guardar contexto del crash si RTC válido
        if (g_crash_ctx.magic == CRASH_DIAG_MAGIC) {
            s_prefs.putUChar(NVS_KEY_LAST_CHECKPOINT, g_crash_ctx.checkpoint);
            s_prefs.putUChar(NVS_KEY_LAST_REASON, s_last_reset_reason);
            // Escribir al log de LittleFS
            writeLogEntry(1, millis()); // 1 = crash
        }
        
    } else if (s_last_reset_reason == ESP_RST_POWERON) {
        // Power-on: resetear contadores volátiles
        s_consecutive_crashes = 0;
        s_cycles_since_success = 0;
        s_prefs.putUChar(NVS_KEY_CONSEC_CRASH, 0);
        s_prefs.putUChar(NVS_KEY_CYCLES_FAIL, 0);
    }
    // Deep sleep y SW reset: solo incrementar boot_count (ya hecho arriba)
    
    s_prefs.end();
    
    // Resetear contexto RTC para nuevo ciclo
    memset(&g_crash_ctx, 0, sizeof(g_crash_ctx));
    g_crash_ctx.magic = CRASH_DIAG_MAGIC;
    
    s_initialized = true;
    return true;
}

void setCheckpoint(CrashCheckpoint cp) {
    g_crash_ctx.checkpoint = (uint8_t)cp;
    g_crash_ctx.timestamp_ms = millis();
    addToHistory((uint8_t)cp);
}

void logATCommand(const char* cmd) {
    safeCopy(g_crash_ctx.last_at_command, cmd, CRASH_DIAG_AT_CMD_LEN);
}

void logATResponse(const char* resp) {
    safeCopy(g_crash_ctx.last_at_response, resp, CRASH_DIAG_AT_RESP_LEN);
}

void setRSSI(int8_t rssi) {
    g_crash_ctx.rssi = rssi;
}

void syncToNVS() {
    if (!s_prefs.begin(CRASH_DIAG_NVS_NAMESPACE, false)) {
        return;
    }
    s_prefs.putUChar(NVS_KEY_LAST_CHECKPOINT, g_crash_ctx.checkpoint);
    s_prefs.end();
}

bool hadCrash() {
    return s_had_crash;
}

void analyzeLastCrash() {
    // Ya se analiza en init(), este método es para compatibilidad
}

void printReport() {
    Serial.println(F(""));
    Serial.println(F("====================================="));
    Serial.println(F("🔍 CRASH DIAGNOSTICS REPORT"));
    Serial.println(F("====================================="));
    
    // Reset reason
    Serial.print(F("Reset Reason: "));
    Serial.print(resetReasonToString(s_last_reset_reason));
    Serial.print(F(" ("));
    Serial.print(s_last_reset_reason);
    Serial.println(F(")"));
    
    // Contadores
    Serial.print(F("Boot Count: "));
    Serial.println(s_boot_count);
    
    Serial.print(F("Crash Count: "));
    Serial.print(s_crash_count);
    Serial.print(F(" (consecutive: "));
    Serial.print(s_consecutive_crashes);
    Serial.println(F(")"));
    
    Serial.print(F("Cycles Since Last Success: "));
    Serial.println(s_cycles_since_success);
    
    // Contexto RTC
    Serial.println(F(""));
    if (g_crash_ctx.magic == CRASH_DIAG_MAGIC || s_had_crash) {
        // Cargar checkpoint de NVS si está disponible
        s_prefs.begin(CRASH_DIAG_NVS_NAMESPACE, true);
        uint8_t lastCp = s_prefs.getUChar(NVS_KEY_LAST_CHECKPOINT, 0);
        s_prefs.end();
        
        Serial.print(F("Last Checkpoint: "));
        Serial.print(checkpointToString((CrashCheckpoint)lastCp));
        Serial.print(F(" ("));
        Serial.print(lastCp);
        Serial.println(F(")"));
        
        Serial.print(F("Last AT Command: "));
        Serial.println(g_crash_ctx.last_at_command[0] ? g_crash_ctx.last_at_command : "(none)");
        
        Serial.print(F("Last AT Response: "));
        Serial.println(g_crash_ctx.last_at_response[0] ? g_crash_ctx.last_at_response : "(none)");
        
        Serial.print(F("RSSI: "));
        Serial.print(g_crash_ctx.rssi);
        Serial.println(F(" dBm"));
        
        // Historial
        Serial.println(F(""));
        Serial.println(F("Checkpoint History (newest first):"));
        int idx = g_crash_ctx.history_idx;
        for (int i = 0; i < CRASH_DIAG_HISTORY_SIZE; i++) {
            idx = (idx - 1 + CRASH_DIAG_HISTORY_SIZE) % CRASH_DIAG_HISTORY_SIZE;
            uint8_t cp = g_crash_ctx.history[idx];
            if (cp != 0) {
                Serial.print(F("  ["));
                Serial.print(i);
                Serial.print(F("] "));
                Serial.print(checkpointToString((CrashCheckpoint)cp));
                Serial.print(F(" ("));
                Serial.print(cp);
                Serial.println(F(")"));
            }
        }
    } else {
        Serial.println(F("RTC Context: [LOST - magic invalid]"));
        Serial.println(F("Using NVS fallback data only"));
    }
    
    Serial.println(F("====================================="));
    Serial.println(F(""));
}

void printHistory() {
    Serial.println(F(""));
    Serial.println(F("=== CRASH LOG HISTORY (LittleFS) ==="));
    
    if (!LittleFS.begin(false)) {
        Serial.println(F("[ERROR] LittleFS no disponible"));
        return;
    }
    
    File logFile = LittleFS.open(CRASH_DIAG_LOG_PATH, "r");
    if (!logFile) {
        Serial.println(F("(No hay historial)"));
        return;
    }
    
    CrashLogEntry entry;
    int count = 0;
    
    while (logFile.read((uint8_t*)&entry, sizeof(CrashLogEntry)) == sizeof(CrashLogEntry)) {
        if (entry.epoch == 0) continue;  // Entrada vacía
        
        Serial.print(F("["));
        Serial.print(count++);
        Serial.print(F("] Type: "));
        
        switch (entry.event_type) {
            case 0: Serial.print(F("CHECKPOINT")); break;
            case 1: Serial.print(F("CRASH")); break;
            case 2: Serial.print(F("SUCCESS")); break;
            default: Serial.print(F("UNKNOWN")); break;
        }
        
        Serial.print(F(" | CP: "));
        Serial.print(checkpointToString((CrashCheckpoint)entry.checkpoint));
        
        if (entry.event_type == 1) {
            Serial.print(F(" | Reason: "));
            Serial.print(resetReasonToString(entry.reset_reason));
        }
        
        Serial.print(F(" | RSSI: "));
        Serial.print(entry.rssi);
        Serial.println(F(" dBm"));
        
        if (entry.at_command[0]) {
            Serial.print(F("     AT: "));
            Serial.println(entry.at_command);
        }
    }
    
    logFile.close();
    Serial.println(F("===================================="));
}

void clearHistory() {
    if (!LittleFS.begin(false)) {
        return;
    }
    LittleFS.remove(CRASH_DIAG_LOG_PATH);
    LittleFS.remove("/crash_idx.bin");
    Serial.println(F("[INFO][CRASH] Historial borrado"));
}

void markCycleSuccess() {
    // Resetear contadores de fallo
    s_consecutive_crashes = 0;
    s_cycles_since_success = 0;
    
    if (!s_prefs.begin(CRASH_DIAG_NVS_NAMESPACE, false)) {
        return;
    }
    s_prefs.putUChar(NVS_KEY_CONSEC_CRASH, 0);
    s_prefs.putUChar(NVS_KEY_CYCLES_FAIL, 0);
    s_prefs.putULong(NVS_KEY_SUCCESS_EPOCH, millis());
    s_prefs.end();
    
    // Log success
    writeLogEntry(2, millis()); // 2 = success
    
    // Marcar checkpoint
    setCheckpoint(CP_CYCLE_SUCCESS);
}

uint16_t getBootCount() { return s_boot_count; }
uint16_t getCrashCount() { return s_crash_count; }
uint8_t getConsecutiveCrashes() { return s_consecutive_crashes; }
uint8_t getLastCheckpoint() { return g_crash_ctx.checkpoint; }
uint8_t getLastResetReason() { return s_last_reset_reason; }

bool isCrashReason(uint8_t reason) {
    switch (reason) {
        case ESP_RST_TASK_WDT:
        case ESP_RST_INT_WDT:
        case ESP_RST_PANIC:
        case ESP_RST_BROWNOUT:
        case ESP_RST_UNKNOWN:
            return true;
        default:
            return false;
    }
}

const char* checkpointToString(CrashCheckpoint cp) {
    switch (cp) {
        case CP_NONE: return "NONE";
        case CP_BOOT_START: return "BOOT_START";
        case CP_BOOT_SERIAL_OK: return "BOOT_SERIAL_OK";
        case CP_BOOT_GPIO_OK: return "BOOT_GPIO_OK";
        case CP_BOOT_LITTLEFS_OK: return "BOOT_LITTLEFS_OK";
        case CP_MODEM_POWER_ON_START: return "MODEM_POWER_ON_START";
        case CP_MODEM_POWER_ON_PWRKEY: return "MODEM_POWER_ON_PWRKEY";
        case CP_MODEM_POWER_ON_WAIT: return "MODEM_POWER_ON_WAIT";
        case CP_MODEM_POWER_ON_OK: return "MODEM_POWER_ON_OK";
        case CP_MODEM_AT_SEND: return "MODEM_AT_SEND";
        case CP_MODEM_AT_WAIT_RESPONSE: return "MODEM_AT_WAIT_RESPONSE";
        case CP_MODEM_AT_RESPONSE_OK: return "MODEM_AT_RESPONSE_OK";
        case CP_MODEM_AT_RESPONSE_ERROR: return "MODEM_AT_RESPONSE_ERROR";
        case CP_MODEM_AT_TIMEOUT: return "MODEM_AT_TIMEOUT";
        case CP_MODEM_NETWORK_ATTACH_START: return "NETWORK_ATTACH_START";
        case CP_MODEM_NETWORK_ATTACH_WAIT: return "NETWORK_ATTACH_WAIT";
        case CP_MODEM_NETWORK_ATTACH_OK: return "NETWORK_ATTACH_OK";
        case CP_MODEM_NETWORK_ATTACH_FAIL: return "NETWORK_ATTACH_FAIL";
        case CP_MODEM_PDP_ACTIVATE_START: return "PDP_ACTIVATE_START";
        case CP_MODEM_PDP_ACTIVATE_WAIT: return "PDP_ACTIVATE_WAIT";
        case CP_MODEM_PDP_ACTIVATE_OK: return "PDP_ACTIVATE_OK";
        case CP_MODEM_PDP_ACTIVATE_FAIL: return "PDP_ACTIVATE_FAIL";
        case CP_MODEM_TCP_CONNECT_START: return "TCP_CONNECT_START";
        case CP_MODEM_TCP_CONNECT_WAIT: return "TCP_CONNECT_WAIT";
        case CP_MODEM_TCP_CONNECT_OK: return "TCP_CONNECT_OK";
        case CP_MODEM_TCP_CONNECT_FAIL: return "TCP_CONNECT_FAIL";
        case CP_MODEM_TCP_SEND_START: return "TCP_SEND_START";
        case CP_MODEM_TCP_SEND_WAIT: return "TCP_SEND_WAIT";
        case CP_MODEM_TCP_SEND_OK: return "TCP_SEND_OK";
        case CP_MODEM_TCP_CLOSE: return "TCP_CLOSE";
        case CP_MODEM_POWER_OFF_START: return "MODEM_POWER_OFF_START";
        case CP_MODEM_POWER_OFF_OK: return "MODEM_POWER_OFF_OK";
        case CP_SLEEP_ENTER: return "SLEEP_ENTER";
        case CP_CYCLE_SUCCESS: return "CYCLE_SUCCESS";
        default: return "UNKNOWN";
    }
}

const char* resetReasonToString(uint8_t reason) {
    switch (reason) {
        case ESP_RST_UNKNOWN: return "UNKNOWN";
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXTERNAL";
        case ESP_RST_SW: return "SOFTWARE";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNDEFINED";
    }
}

} // namespace CrashDiag

#endif // ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
