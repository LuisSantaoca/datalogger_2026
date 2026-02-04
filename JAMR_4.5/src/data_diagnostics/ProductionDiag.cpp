/**
 * @file ProductionDiag.cpp
 * @brief Implementación del sistema de diagnóstico de producción
 * @version FEAT-V7
 * @date 2026-01-28
 */

#include "ProductionDiag.h"
#include <LittleFS.h>

// ============================================================
// VARIABLES GLOBALES (internas al módulo)
// ============================================================

static ProductionStats g_stats;
static CycleEMIStats g_cycleEMI;
static bool g_initialized = false;
static uint32_t g_lastKnownEpoch = 0;

// ============================================================
// IMPLEMENTACIÓN - INICIALIZACIÓN
// ============================================================

bool ProdDiag::init() {
    // Crear directorio si no existe
    if (!LittleFS.exists(PROD_DIAG_DIR)) {
        if (!LittleFS.mkdir(PROD_DIAG_DIR)) {
            Serial.println(F("[ERROR][DIAG] No se pudo crear /diag"));
            return false;
        }
        Serial.println(F("[INFO][DIAG] Directorio /diag creado"));
    }
    
    // Intentar cargar estadísticas existentes
    if (!loadStats()) {
        // Inicializar estructura nueva
        memset(&g_stats, 0, sizeof(g_stats));
        g_stats.magic = PROD_DIAG_MAGIC;
        g_stats.version = PROD_DIAG_VERSION;
        Serial.println(F("[INFO][DIAG] Estadísticas inicializadas (primera vez)"));
    }
    
    // Resetear contadores de ciclo
    g_stats.cyclesSinceBoot = 0;
    resetCycleEMI();
    
    g_initialized = true;
    Serial.println(F("[INFO][DIAG] Sistema de diagnóstico inicializado"));
    return true;
}

// ============================================================
// IMPLEMENTACIÓN - CONTADORES DE CICLO
// ============================================================

void ProdDiag::incrementCycle() {
    if (!g_initialized) return;
    g_stats.totalCycles++;
    g_stats.cyclesSinceBoot++;
}

void ProdDiag::recordLTESendOk() {
    if (!g_initialized) return;
    g_stats.lteSendOk++;
}

void ProdDiag::recordLTESendFail() {
    if (!g_initialized) return;
    g_stats.lteSendFail++;
    logEvent(EVT_LTE_FAIL, 0);
}

void ProdDiag::recordATTimeout() {
    if (!g_initialized) return;
    g_stats.atTimeouts++;
    // ============ [FIX-V8 START] Registrar evento en LOG ============
    logEvent(EVT_AT_TIMEOUT, g_stats.atTimeouts);
    // ============ [FIX-V8 END] ============
}

void ProdDiag::recordOperatorFallback() {
    if (!g_initialized) return;
    g_stats.operatorFallbacks++;
    logEvent(EVT_OP_FALLBACK, 0);
}

void ProdDiag::recordLowBatteryEnter(uint16_t vBatCents) {
    if (!g_initialized) return;
    g_stats.lowBatteryEvents++;
    logEvent(EVT_LOW_BAT_ENTER, vBatCents);
}

void ProdDiag::recordLowBatteryExit(uint16_t cyclesInRest) {
    if (!g_initialized) return;
    logEvent(EVT_LOW_BAT_EXIT, cyclesInRest);
}

void ProdDiag::incrementLowBatteryCycle() {
    if (!g_initialized) return;
    g_stats.lowBatteryCycles++;
}

void ProdDiag::recordPeriodicRestart(uint32_t cyclesBeforeRestart) {
    if (!g_initialized) return;
    g_stats.feat4Restarts++;
    logEvent(EVT_RESTART_24H, (uint16_t)(cyclesBeforeRestart & 0xFFFF));
    saveStats(g_lastKnownEpoch);  // Guardar antes del restart
}

void ProdDiag::recordGPSFail(uint8_t retries) {
    if (!g_initialized) return;
    g_stats.gpsFails++;
    logEvent(EVT_GPS_FAIL, retries);
}

void ProdDiag::recordCrash(uint8_t checkpoint) {
    if (!g_initialized) return;
    g_stats.crashCount++;
    logEvent(EVT_CRASH, checkpoint);
}

// ============================================================
// IMPLEMENTACIÓN - EMI DETECTION
// ============================================================

void ProdDiag::resetCycleEMI() {
    memset(&g_cycleEMI, 0, sizeof(g_cycleEMI));
}

void ProdDiag::countEMI(const String& response) {
    if (!g_initialized) return;
    
    g_cycleEMI.atCommands++;
    g_stats.atCommandsTotal++;
    
    uint16_t invalid = 0;
    for (size_t i = 0; i < response.length(); i++) {
        if (isInvalidATChar((uint8_t)response[i])) {
            invalid++;
        }
    }
    
    if (invalid > 0) {
        g_cycleEMI.corrupted++;
        g_cycleEMI.invalidChars += invalid;
        g_stats.atCorrupted++;
        g_stats.invalidCharsTotal += invalid;
    }
}

void ProdDiag::evaluateCycleEMI() {
    if (!g_initialized || g_cycleEMI.atCommands == 0) return;
    
    uint8_t pct = (g_cycleEMI.corrupted * 100) / g_cycleEMI.atCommands;
    
    // Actualizar peor porcentaje
    if (pct > g_stats.worstCorruptPct) {
        g_stats.worstCorruptPct = pct;
    }
    
    // Registrar eventos EMI si superan umbral
    if (pct >= EMI_EVENT_THRESHOLD_PCT) {
        logEvent(EVT_EMI_DETECTED, pct * 10);  // x10 para decimal
        g_stats.emiEvents++;
    }
    if (pct >= EMI_SEVERE_THRESHOLD_PCT) {
        logEvent(EVT_EMI_SEVERE, pct * 10);
    }
}

// ============================================================
// IMPLEMENTACIÓN - PERSISTENCIA
// ============================================================

uint16_t ProdDiag::calculateCRC16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool ProdDiag::saveStats(uint32_t epoch) {
    if (!g_initialized) return false;
    
    // Actualizar timestamp si se proporciona
    if (epoch > 0) {
        g_stats.lastUpdateEpoch = epoch;
        g_lastKnownEpoch = epoch;
        
        // Guardar primer boot si no existe
        if (g_stats.firstBootEpoch == 0) {
            g_stats.firstBootEpoch = epoch;
        }
    }
    
    // Calcular CRC (excluyendo el campo crc16)
    size_t dataLen = sizeof(ProductionStats) - sizeof(uint16_t);
    g_stats.crc16 = calculateCRC16((uint8_t*)&g_stats, dataLen);
    
    // Escribir archivo
    File f = LittleFS.open(PROD_DIAG_STATS_FILE, "w");
    if (!f) {
        Serial.println(F("[ERROR][DIAG] No se pudo abrir stats.bin para escritura"));
        return false;
    }
    
    size_t written = f.write((uint8_t*)&g_stats, sizeof(ProductionStats));
    f.close();
    
    if (written != sizeof(ProductionStats)) {
        Serial.println(F("[ERROR][DIAG] Error escribiendo stats.bin"));
        return false;
    }
    
    return true;
}

bool ProdDiag::loadStats() {
    if (!LittleFS.exists(PROD_DIAG_STATS_FILE)) {
        return false;
    }
    
    File f = LittleFS.open(PROD_DIAG_STATS_FILE, "r");
    if (!f) {
        return false;
    }
    
    ProductionStats temp;
    size_t readBytes = f.read((uint8_t*)&temp, sizeof(ProductionStats));
    f.close();
    
    if (readBytes != sizeof(ProductionStats)) {
        Serial.println(F("[WARN][DIAG] stats.bin tamaño incorrecto"));
        return false;
    }
    
    // Validar magic
    if (temp.magic != PROD_DIAG_MAGIC) {
        Serial.println(F("[WARN][DIAG] stats.bin magic inválido"));
        return false;
    }
    
    // Validar CRC
    size_t dataLen = sizeof(ProductionStats) - sizeof(uint16_t);
    uint16_t calcCRC = calculateCRC16((uint8_t*)&temp, dataLen);
    if (calcCRC != temp.crc16) {
        Serial.println(F("[WARN][DIAG] stats.bin CRC inválido"));
        return false;
    }
    
    // Copiar datos válidos
    memcpy(&g_stats, &temp, sizeof(ProductionStats));
    g_lastKnownEpoch = g_stats.lastUpdateEpoch;
    
    Serial.print(F("[INFO][DIAG] Stats cargadas. Ciclos totales: "));
    Serial.println(g_stats.totalCycles);
    
    return true;
}

// ============ [FEAT-V9 START] Función para actualizar epoch ============
void ProdDiag::setCurrentEpoch(uint32_t epoch) {
    if (epoch > 0) {
        g_lastKnownEpoch = epoch;
    }
}
// ============ [FEAT-V9 END] ============

// ============================================================
// IMPLEMENTACIÓN - LOG DE EVENTOS
// ============================================================

void ProdDiag::logEvent(char eventCode, uint16_t data, uint32_t epoch) {
    if (!g_initialized) return;
    
    uint32_t ts = (epoch > 0) ? epoch : g_lastKnownEpoch;
    
    // Verificar tamaño del archivo antes de escribir
    if (LittleFS.exists(PROD_DIAG_EVENTS_FILE)) {
        File check = LittleFS.open(PROD_DIAG_EVENTS_FILE, "r");
        if (check) {
            size_t currentSize = check.size();
            check.close();
            
            // Si está cerca del límite, rotar (borrar más antiguas)
            if (currentSize > PROD_DIAG_MAX_EVENTS_SIZE - 50) {
                // Leer contenido actual
                File f = LittleFS.open(PROD_DIAG_EVENTS_FILE, "r");
                if (f) {
                    String content = f.readString();
                    f.close();
                    
                    // Encontrar mitad del archivo y cortar desde ahí
                    int midPoint = content.length() / 2;
                    int newlinePos = content.indexOf('\n', midPoint);
                    if (newlinePos > 0) {
                        String trimmed = content.substring(newlinePos + 1);
                        
                        // Reescribir archivo
                        File fw = LittleFS.open(PROD_DIAG_EVENTS_FILE, "w");
                        if (fw) {
                            fw.print(trimmed);
                            fw.close();
                        }
                    }
                }
            }
        }
    }
    
    // Append evento
    File f = LittleFS.open(PROD_DIAG_EVENTS_FILE, "a");
    if (!f) {
        Serial.println(F("[ERROR][DIAG] No se pudo abrir events.txt"));
        return;
    }
    
    // Formato: epoch,código,dato
    f.print(ts);
    f.print(',');
    f.print(eventCode);
    f.print(',');
    f.println(data);
    f.close();
}

// ============================================================
// IMPLEMENTACIÓN - COMANDOS SERIAL
// ============================================================

bool ProdDiag::handleCommand(const String& cmd) {
    String upperCmd = cmd;
    upperCmd.toUpperCase();
    upperCmd.trim();
    
    if (upperCmd == "STATS" || upperCmd == "DIAG") {
        printStats();
        return true;
    } else if (upperCmd == "LOG" || upperCmd == "EVENTS") {
        printEventLog();
        return true;
    } else if (upperCmd == "CLEAR" || upperCmd == "RESET") {
        clearAll();
        return true;
    }
    return false;
}

void ProdDiag::printStats() {
    Serial.println(F(""));
    Serial.println(F("╔══════════════════════════════════════╗"));
    Serial.println(F("║  DIAGNÓSTICO PRODUCCIÓN              ║"));
    Serial.println(F("╠══════════════════════════════════════╣"));
    
    // Ciclos
    Serial.print(F("║  Ciclos totales: "));
    Serial.println(g_stats.totalCycles);
    Serial.print(F("║  Desde boot: "));
    Serial.print(g_stats.cyclesSinceBoot);
    float days = (float)g_stats.cyclesSinceBoot * 10.0f / 1440.0f;  // asumiendo 10min/ciclo
    Serial.print(F(" (~"));
    Serial.print(days, 1);
    Serial.println(F(" días)"));
    
    // Reset reason
    Serial.print(F("║  Last reset: "));
    switch(g_stats.lastResetReason) {
        case 1: Serial.println(F("POWERON")); break;
        case 3: Serial.println(F("SOFTWARE")); break;
        case 4: Serial.println(F("PANIC")); break;
        case 5: Serial.println(F("INT_WDT")); break;
        case 6: Serial.println(F("TASK_WDT")); break;
        case 7: Serial.println(F("WDT")); break;
        case 8: Serial.println(F("DEEPSLEEP")); break;
        case 9: Serial.println(F("BROWNOUT")); break;
        default: Serial.println(g_stats.lastResetReason); break;
    }
    
    Serial.println(F("╠══════════════════════════════════════╣"));
    Serial.println(F("║  LTE:"));
    
    // LTE stats
    uint32_t totalSends = g_stats.lteSendOk + g_stats.lteSendFail;
    float successRate = (totalSends > 0) ? (g_stats.lteSendOk * 100.0f / totalSends) : 0;
    Serial.print(F("║    Enviadas OK: "));
    Serial.print(g_stats.lteSendOk);
    Serial.print(F(" ("));
    Serial.print(successRate, 1);
    Serial.println(F("%)"));
    Serial.print(F("║    Fallidas: "));
    Serial.println(g_stats.lteSendFail);
    Serial.print(F("║    AT Timeouts: "));
    Serial.println(g_stats.atTimeouts);
    Serial.print(F("║    Op. Fallbacks: "));
    Serial.println(g_stats.operatorFallbacks);
    
    Serial.println(F("╠══════════════════════════════════════╣"));
    Serial.println(F("║  BATERÍA:"));
    Serial.print(F("║    Ciclos en reposo: "));
    Serial.println(g_stats.lowBatteryCycles);
    Serial.print(F("║    Eventos low-bat: "));
    Serial.println(g_stats.lowBatteryEvents);
    
    Serial.println(F("╠══════════════════════════════════════╣"));
    Serial.println(F("║  EMI STATUS:"));
    Serial.print(F("║    AT Commands: "));
    Serial.println(g_stats.atCommandsTotal);
    
    float corruptPct = (g_stats.atCommandsTotal > 0) ? 
        (g_stats.atCorrupted * 100.0f / g_stats.atCommandsTotal) : 0;
    Serial.print(F("║    Corrupted: "));
    Serial.print(g_stats.atCorrupted);
    Serial.print(F(" ("));
    Serial.print(corruptPct, 3);
    Serial.println(F("%)"));
    
    Serial.print(F("║    Invalid chars: "));
    Serial.println(g_stats.invalidCharsTotal);
    Serial.print(F("║    Worst cycle: "));
    Serial.print(g_stats.worstCorruptPct);
    Serial.println(F("%"));
    Serial.print(F("║    EMI Events: "));
    Serial.println(g_stats.emiEvents);
    Serial.print(F("║    Verdict: "));
    Serial.println(getEMIVerdict());
    
    Serial.println(F("╠══════════════════════════════════════╣"));
    Serial.println(F("║  SISTEMA:"));
    Serial.print(F("║    Restarts 24h: "));
    Serial.println(g_stats.feat4Restarts);
    Serial.print(F("║    Crashes: "));
    Serial.println(g_stats.crashCount);
    Serial.print(F("║    GPS Fails: "));
    Serial.println(g_stats.gpsFails);
    
    Serial.println(F("╚══════════════════════════════════════╝"));
    Serial.println(F(""));
}

void ProdDiag::printEventLog() {
    Serial.println(F(""));
    Serial.println(F("=== LOG DE EVENTOS ==="));
    
    if (!LittleFS.exists(PROD_DIAG_EVENTS_FILE)) {
        Serial.println(F("(sin eventos)"));
        return;
    }
    
    File f = LittleFS.open(PROD_DIAG_EVENTS_FILE, "r");
    if (!f) {
        Serial.println(F("(error leyendo)"));
        return;
    }
    
    Serial.println(F("Formato: epoch,código,dato"));
    Serial.println(F("Códigos: B=Boot L=LTE_Fail F=Fallback E=LowBat_Enter X=LowBat_Exit"));
    Serial.println(F("         R=Restart24h G=GPS_Fail I=EMI S=EMI_Severe C=Crash T=AT_Timeout"));
    Serial.println(F("---"));
    
    while (f.available()) {
        String line = f.readStringUntil('\n');
        Serial.println(line);
    }
    f.close();
    
    Serial.println(F("=== FIN LOG ==="));
    Serial.println(F(""));
}

void ProdDiag::clearAll() {
    Serial.println(F("[INFO][DIAG] Limpiando datos de diagnóstico..."));
    
    // Borrar archivos
    if (LittleFS.exists(PROD_DIAG_STATS_FILE)) {
        LittleFS.remove(PROD_DIAG_STATS_FILE);
    }
    if (LittleFS.exists(PROD_DIAG_EVENTS_FILE)) {
        LittleFS.remove(PROD_DIAG_EVENTS_FILE);
    }
    
    // Reinicializar estructura
    memset(&g_stats, 0, sizeof(g_stats));
    g_stats.magic = PROD_DIAG_MAGIC;
    g_stats.version = PROD_DIAG_VERSION;
    
    // Guardar estructura vacía
    saveStats(g_lastKnownEpoch);
    
    Serial.println(F("[INFO][DIAG] Datos limpiados"));
}

// ============================================================
// IMPLEMENTACIÓN - UTILIDADES
// ============================================================

const char* ProdDiag::getEMIVerdict() {
    if (g_stats.atCommandsTotal == 0) {
        return "SIN DATOS";
    }
    
    float corruptPct = (g_stats.atCorrupted * 100.0f) / g_stats.atCommandsTotal;
    
    if (corruptPct < EMI_THRESHOLD_MONITOR_PCT && 
        g_stats.worstCorruptPct < EMI_THRESHOLD_WORST_MONITOR) {
        return "PCB OK";
    } else if (corruptPct < EMI_THRESHOLD_PROBLEM_PCT && 
               g_stats.worstCorruptPct < EMI_THRESHOLD_WORST_PROBLEM) {
        return "MONITOR";
    } else {
        return "PROBLEMA EMI";
    }
}

const ProductionStats& ProdDiag::getStats() {
    return g_stats;
}

void ProdDiag::setResetReason(uint8_t reason) {
    if (!g_initialized) return;
    g_stats.lastResetReason = reason;
    
    // Registrar evento de boot
    char bootChar = 'U';  // Unknown
    switch(reason) {
        case 1: bootChar = 'P'; break;  // Poweron
        case 3: bootChar = 'S'; break;  // Software
        case 5: bootChar = 'D'; break;  // Deepsleep (FIX: faltaba)
        case 6: bootChar = 'W'; break;  // Task WDT
        case 7: bootChar = 'W'; break;  // WDT
        case 8: bootChar = 'D'; break;  // Deepsleep (alternativo)
        case 9: bootChar = 'B'; break;  // Brownout
    }
    logEvent(EVT_BOOT, (uint16_t)bootChar);
}
