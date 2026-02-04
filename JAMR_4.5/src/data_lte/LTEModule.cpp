/**
 * @file LTEModule.cpp
 * @brief Implementación del módulo de comunicación LTE/GSM
 * @version 2.0.3
 * @date 2026-01-15
 * 
 * @see LTEModule.h para documentación de API
 */

#include "LTEModule.h"
#include "../FeatureFlags.h"  // FEAT-V1: Feature flags
#include <string.h>
#include <stdlib.h>

// ============ [FEAT-V3 START] Include Crash Diagnostics ============
#if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
#include "../data_diagnostics/CrashDiagnostics.h"  // FEAT-V3
#endif
// ============ [FEAT-V3 END] ============

// ============ [FEAT-V7 START] Include Production Diagnostics ============
#if ENABLE_FEAT_V7_PRODUCTION_DIAG
#include "../data_diagnostics/ProductionDiag.h"  // FEAT-V7
#endif
// ============ [FEAT-V7 END] ============

// ============ [DEBUG-EMI START] Estadísticas de diagnóstico EMI ============
#if DEBUG_EMI_DIAGNOSTIC_ENABLED
/** @brief Estructura para estadísticas de diagnóstico EMI */
struct EMIDiagStats {
    uint32_t totalATCommands;      // Total comandos AT enviados
    uint32_t successfulResponses;  // Respuestas exitosas (OK)
    uint32_t errorResponses;       // Respuestas ERROR
    uint32_t timeouts;             // Sin respuesta (timeout)
    uint32_t corruptedResponses;   // Respuestas con bytes inválidos
    uint32_t invalidCharsDetected; // Total caracteres inválidos detectados
    uint32_t totalBytesReceived;   // Total bytes recibidos
    uint32_t maxResponseTime;      // Tiempo máximo de respuesta (ms)
    uint32_t minResponseTime;      // Tiempo mínimo de respuesta (ms)
    uint32_t sumResponseTime;      // Suma para calcular promedio
};

static EMIDiagStats g_emiStats = {0, 0, 0, 0, 0, 0, 0, 0, UINT32_MAX, 0};

/**
 * @brief Detecta si un byte es válido para respuesta AT
 * @param c Byte a validar
 * @return true si es válido (ASCII imprimible o control común)
 */
static bool isValidATChar(uint8_t c) {
    // Caracteres válidos: CR, LF, espacio-tilde (0x20-0x7E)
    return (c == 0x0D || c == 0x0A || (c >= 0x20 && c <= 0x7E));
}

/**
 * @brief Log hex dump de respuesta AT para diagnóstico EMI
 */
static void logRawHex(const String& response, Stream* debugSerial) {
    if (!debugSerial) return;
    
    debugSerial->print(F("[EMI-RAW] "));
    debugSerial->print(response.length());
    debugSerial->print(F(" bytes: "));
    
    uint8_t invalidCount = 0;
    for (size_t i = 0; i < response.length() && i < 64; i++) {
        uint8_t c = (uint8_t)response[i];
        debugSerial->printf("%02X ", c);
        if (!isValidATChar(c)) {
            invalidCount++;
        }
    }
    
    if (response.length() > 64) {
        debugSerial->print(F("..."));
    }
    
    if (invalidCount > 0) {
        debugSerial->printf(" [!%d INVALID]", invalidCount);
        g_emiStats.invalidCharsDetected += invalidCount;
        g_emiStats.corruptedResponses++;
    }
    debugSerial->println();
}

/**
 * @brief Imprime reporte de estadísticas EMI
 */
void printEMIDiagnosticReport(Stream* serial) {
    if (!serial) return;
    
    serial->println();
    serial->println(F("╔════════════════════════════════════════════════════════════╗"));
    serial->println(F("║           REPORTE DIAGNÓSTICO EMI                          ║"));
    serial->println(F("╠════════════════════════════════════════════════════════════╣"));
    serial->printf(   "║  Comandos AT enviados:        %6lu                       ║\n", g_emiStats.totalATCommands);
    serial->printf(   "║  Respuestas exitosas (OK):    %6lu (%5.1f%%)              ║\n", 
                      g_emiStats.successfulResponses,
                      g_emiStats.totalATCommands > 0 ? (float)g_emiStats.successfulResponses / g_emiStats.totalATCommands * 100.0f : 0.0f);
    serial->printf(   "║  Respuestas ERROR:            %6lu (%5.1f%%)              ║\n",
                      g_emiStats.errorResponses,
                      g_emiStats.totalATCommands > 0 ? (float)g_emiStats.errorResponses / g_emiStats.totalATCommands * 100.0f : 0.0f);
    serial->printf(   "║  Timeouts (sin respuesta):    %6lu (%5.1f%%)              ║\n",
                      g_emiStats.timeouts,
                      g_emiStats.totalATCommands > 0 ? (float)g_emiStats.timeouts / g_emiStats.totalATCommands * 100.0f : 0.0f);
    serial->println(F("╠════════════════════════════════════════════════════════════╣"));
    serial->printf(   "║  Respuestas CORRUPTAS:        %6lu (%5.1f%%)  ", 
                      g_emiStats.corruptedResponses,
                      g_emiStats.totalATCommands > 0 ? (float)g_emiStats.corruptedResponses / g_emiStats.totalATCommands * 100.0f : 0.0f);
    if (g_emiStats.corruptedResponses > 0) {
        serial->println(F("⚠️ EMI?  ║"));
    } else {
        serial->println(F("✅ OK    ║"));
    }
    serial->printf(   "║  Caracteres inválidos total:  %6lu                       ║\n", g_emiStats.invalidCharsDetected);
    serial->printf(   "║  Bytes recibidos total:       %6lu                       ║\n", g_emiStats.totalBytesReceived);
    serial->println(F("╠════════════════════════════════════════════════════════════╣"));
    serial->printf(   "║  Tiempo respuesta MIN:        %6lu ms                    ║\n", 
                      g_emiStats.minResponseTime == UINT32_MAX ? 0 : g_emiStats.minResponseTime);
    serial->printf(   "║  Tiempo respuesta MAX:        %6lu ms                    ║\n", g_emiStats.maxResponseTime);
    serial->printf(   "║  Tiempo respuesta PROMEDIO:   %6lu ms                    ║\n",
                      g_emiStats.successfulResponses > 0 ? g_emiStats.sumResponseTime / g_emiStats.successfulResponses : 0);
    serial->println(F("╠════════════════════════════════════════════════════════════╣"));
    
    // Evaluación
    float corruptPct = g_emiStats.totalATCommands > 0 ? 
                       (float)g_emiStats.corruptedResponses / g_emiStats.totalATCommands * 100.0f : 0.0f;
    float timeoutPct = g_emiStats.totalATCommands > 0 ? 
                       (float)g_emiStats.timeouts / g_emiStats.totalATCommands * 100.0f : 0.0f;
    
    if (corruptPct > 5.0f || timeoutPct > 10.0f) {
        serial->println(F("║  DIAGNÓSTICO: ⚠️  PROBLEMAS DE EMI/INTEGRIDAD DETECTADOS  ║"));
        serial->println(F("║  RECOMENDACIÓN: Revisar PCB, blindaje, desacoplo         ║"));
    } else if (corruptPct > 0.0f || timeoutPct > 5.0f) {
        serial->println(F("║  DIAGNÓSTICO: ⚡ Ruido menor detectado                    ║"));
        serial->println(F("║  RECOMENDACIÓN: Monitorear en campo                      ║"));
    } else {
        serial->println(F("║  DIAGNÓSTICO: ✅ Comunicación limpia                      ║"));
        serial->println(F("║  RECOMENDACIÓN: OK para producción                       ║"));
    }
    serial->println(F("╚════════════════════════════════════════════════════════════╝"));
    serial->println();
}

/** @brief Reset estadísticas EMI */
void resetEMIDiagnosticStats() {
    memset(&g_emiStats, 0, sizeof(g_emiStats));
    g_emiStats.minResponseTime = UINT32_MAX;
}
#endif
// ============ [DEBUG-EMI END] ============

LTEModule::LTEModule(HardwareSerial& serial) : _serial(serial), _debugEnabled(false), _debugSerial(nullptr) {
}

void LTEModule::setDebug(bool enable, Stream* debugSerial) {
    _debugEnabled = enable;
    _debugSerial = debugSerial;
}

void LTEModule::debugPrint(const char* msg) {
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println(msg);
    }
}

void LTEModule::debugPrint(const char* msg, int value) {
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print(msg);
        _debugSerial->println(value);
    }
}

void LTEModule::begin() {
    pinMode(LTE_PWRKEY_PIN, OUTPUT);
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? LOW : HIGH);
    
    _serial.begin(LTE_SIM_BAUD, SERIAL_8N1, LTE_PIN_RX, LTE_PIN_TX);
    delay(100);
}

bool LTEModule::powerOn() {
#if ENABLE_FIX_V7_ZOMBIE_MITIGATION
    // ============ [FIX-V7 START] Mitigación estado zombie (v1.1) ============
    CRASH_CHECKPOINT(CP_MODEM_POWER_ON_START);
    debugPrint("[LTE] Encendiendo SIM7080G (FIX-V7)...");
    
    // Backoff: evitar loops de recuperación que gastan batería
    static RTC_DATA_ATTR uint8_t s_recoveryAttempts = 0;
    
    // 0. Verificar si ya está encendido (idempotencia)
    if (isAlive()) {
        debugPrint("[LTE] Modem ya esta encendido");
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);
        return true;
    }
    
    // 1. Intentos normales de PWRKEY
    //    isAlive() ya tiene 3 reintentos de AT internos (~4s cada llamada)
    for (uint8_t attempt = 0; attempt < LTE_POWER_ON_ATTEMPTS; attempt++) {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("[LTE] Intento PWRKEY ");
            _debugSerial->print(attempt + 1);
            _debugSerial->print("/");
            _debugSerial->println(LTE_POWER_ON_ATTEMPTS);
        }
        
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_PWRKEY);
        togglePWRKEY();
        delay(FIX_V6_UART_READY_DELAY_MS);  // ~2.5s según datasheet
        
        // isAlive() = 3 reintentos AT × 1.3s = ~4s
        if (isAlive()) {
            if (_debugEnabled && _debugSerial) {
                _debugSerial->print("[LTE] Modem respondio en intento PWRKEY ");
                _debugSerial->println(attempt + 1);
            }
            
            #if FIX_V7_DISABLE_PSM
            // Deshabilitar PSM para prevenir futuros "primer AT perdido"
            debugPrint("[LTE] Deshabilitando PSM...");
            _serial.println("AT+CPSMS=0");
            delay(500);
            
            #if FIX_V7_VERIFY_PSM
            // Verificar que PSM quedó deshabilitado
            clearBuffer();
            _serial.println("AT+CPSMS?");
            delay(500);
            String resp = "";
            while (_serial.available()) resp += (char)_serial.read();
            if (resp.indexOf("+CPSMS: 0") != -1) {
                debugPrint("[LTE] PSM deshabilitado OK");
            } else {
                if (_debugEnabled && _debugSerial) {
                    _debugSerial->print("[LTE] WARN: PSM verify failed: ");
                    _debugSerial->println(resp);
                }
                #if ENABLE_FEAT_V7_PRODUCTION_DIAG
                ProdDiag::logEvent(EVT_PSM_FAIL, 0);
                #endif
            }
            #endif
            #endif
            
            CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);
            return true;
        }
    }
    
    // 2. ÚLTIMO RECURSO: Reset forzado por PWRKEY >12.6s
    //    Solo si no hemos agotado el backoff
    if (s_recoveryAttempts >= FIX_V7_MAX_RECOVERY_PER_BOOT) {
        debugPrint("[LTE] WARN: Backoff activo, omitiendo reset forzado");
        debugPrint("[LTE] ERROR: Modem no responde");
        return false;
    }
    
    debugPrint("[LTE] WARN: Intentos PWRKEY agotados, reset forzado 12.6s...");
    s_recoveryAttempts++;
    
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::logEvent(EVT_ZOMBIE_ATTEMPT, s_recoveryAttempts);
    #endif
    
    // Reset forzado: PWRKEY LOW por >12.6s (reinicia firmware del modem)
    // NOTA: NO corta alimentación, no resuelve latch-up
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? HIGH : LOW);
    delay(FIX_V6_PWRKEY_RESET_TIME_MS);  // 13000ms
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? LOW : HIGH);
    
    delay(FIX_V6_UART_READY_DELAY_MS);
    
    // Verificar recuperación
    if (isAlive()) {
        debugPrint("[LTE] Recuperado despues de reset forzado");
        
        #if FIX_V7_DISABLE_PSM
        _serial.println("AT+CPSMS=0");
        delay(200);
        #endif
        
        #if ENABLE_FEAT_V7_PRODUCTION_DIAG
        ProdDiag::logEvent(EVT_ZOMBIE_OK, 0);
        #endif
        
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);
        return true;
    }
    
    // 3. Estado zombie irrecuperable (tipo B) - FIX-V7 no puede resolver
#if ENABLE_FIX_V9_HARD_POWER_CYCLE
    // ============ [FIX-V9 START] Hard power cycle como último recurso ============
    debugPrint("[LTE] FIX-V9: Intentando HARD POWER CYCLE via IO13...");
    
    if (hardPowerCycle()) {
        // Después del hard cycle, ejecutar secuencia PWRKEY completa
        debugPrint("[LTE] FIX-V9: VBAT restaurado, ejecutando PWRKEY...");
        togglePWRKEY();
        delay(FIX_V6_UART_READY_DELAY_MS);
        
        if (isAlive()) {
            debugPrint("[LTE] SUCCESS: Modem recuperado con HARD POWER CYCLE!");
            
            #if FIX_V7_DISABLE_PSM
            _serial.println("AT+CPSMS=0");
            delay(200);
            #endif
            
            #if ENABLE_FEAT_V7_PRODUCTION_DIAG
            ProdDiag::logEvent(EVT_HARD_CYCLE_OK, 0);
            #endif
            
            CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);
            return true;
        }
    }
    
    debugPrint("[LTE] ERROR: Modem no responde ni con HARD POWER CYCLE");
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::logEvent(EVT_MODEM_ZOMBIE, 0);
    #endif
    // ============ [FIX-V9 END] ============
#else
    // FIX-V9 deshabilitado: comportamiento original FIX-V7
    debugPrint("[LTE] ERROR: Modem zombie - requiere power cycle fisico");
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::logEvent(EVT_MODEM_ZOMBIE, 0);
    #endif
#endif
    
    return false;
    // ============ [FIX-V7 END] ============
    
#else
    // Código original preservado (pre FIX-V7)
    CRASH_CHECKPOINT(CP_MODEM_POWER_ON_START);  // FEAT-V3
    debugPrint("Encendiendo SIM7080G...");
    
    if (isAlive()) {
        debugPrint("Modulo ya esta encendido");
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);  // FEAT-V3
        delay(2000);
        return true;
    }
    
    for (uint16_t attempt = 0; attempt < LTE_POWER_ON_ATTEMPTS; attempt++) {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Intento de encendido ");
            _debugSerial->print(attempt + 1);
            _debugSerial->print(" de ");
            _debugSerial->println(LTE_POWER_ON_ATTEMPTS);
        }
        
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_PWRKEY);  // FEAT-V3
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_WAIT);  // FEAT-V3
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                debugPrint("SIM7080G encendido correctamente!");
                CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);  // FEAT-V3
                delay(1000);
                return true;
            }
            delay(500);
        }
    }
    
    debugPrint("Error: No se pudo encender el modulo");
    return false;
#endif
}

// ============ [FIX-V9 START] Hard power cycle via IO13 ============
#if ENABLE_FIX_V9_HARD_POWER_CYCLE

/**
 * @brief Contador de hard cycles para protección anti-loop
 * @note RTC_DATA_ATTR persiste en deep sleep
 */
RTC_DATA_ATTR static uint8_t s_hardCycleCount = 0;

bool LTEModule::hardPowerCycle() {
    // Protección anti-loop: limitar ciclos por boot
    if (s_hardCycleCount >= FIX_V9_MAX_HARD_CYCLES_PER_BOOT) {
        debugPrint("[LTE] HARD-CYCLE: Limite alcanzado, omitiendo");
        return false;
    }
    s_hardCycleCount++;
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print(F("[LTE] HARD-CYCLE: Iniciando ("));
        _debugSerial->print(s_hardCycleCount);
        _debugSerial->print(F("/"));
        _debugSerial->print(FIX_V9_MAX_HARD_CYCLES_PER_BOOT);
        _debugSerial->println(F(")"));
    }
    
    // Configurar pin como salida
    pinMode(MODEM_EN_PIN, OUTPUT);
    
    // PASO 1: Cortar alimentación VBAT
    debugPrint("[LTE] HARD-CYCLE: IO13=LOW, cortando VBAT...");
    digitalWrite(MODEM_EN_PIN, LOW);
    
    // PASO 2: Esperar descarga completa de capacitores
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print(F("[LTE] HARD-CYCLE: Esperando "));
        _debugSerial->print(FIX_V9_POWEROFF_DELAY_MS);
        _debugSerial->println(F("ms para descarga..."));
    }
    delay(FIX_V9_POWEROFF_DELAY_MS);
    
    // PASO 3: Restaurar alimentación VBAT
    debugPrint("[LTE] HARD-CYCLE: IO13=HIGH, restaurando VBAT...");
    digitalWrite(MODEM_EN_PIN, HIGH);
    
    // PASO 4: Esperar estabilización del boost converter
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print(F("[LTE] HARD-CYCLE: Esperando "));
        _debugSerial->print(FIX_V9_STABILIZATION_MS);
        _debugSerial->println(F("ms para estabilizacion..."));
    }
    delay(FIX_V9_STABILIZATION_MS);
    
    debugPrint("[LTE] HARD-CYCLE: Ciclo completado, listo para PWRKEY");
    
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::logEvent(EVT_HARD_CYCLE, s_hardCycleCount);
    #endif
    
    return true;
}

#endif // ENABLE_FIX_V9_HARD_POWER_CYCLE
// ============ [FIX-V9 END] ============

bool LTEModule::powerOff() {
    debugPrint("Apagando SIM7080G...");
    
#if ENABLE_FIX_V6_MODEM_POWER_SEQUENCE
    // ============ [FIX-V6 START] Apagado robusto según datasheet SIMCOM ============
    
    // 0. Verificar si ya está apagado (evita falsos "zombie")
    //    NOTA: Si el modem está en estado zombie, isAlive() retorna false
    //    pero el modem sigue alimentado (LED parpadea). No podemos distinguir.
    if (!isAlive()) {
        debugPrint("[LTE] Modem no responde - asumiendo apagado o zombie");
        debugPrint("[LTE] WARN: Si LED sigue parpadeando, es zombie tipo B (requiere power cycle)");
        return true;
    }
    
    // 1. Vaciar buffer UART (puede tener URCs pendientes)
    while (_serial.available()) _serial.read();
    
    // 2. Intentar apagado graceful con AT+CPOWD=1
    debugPrint("[LTE] Enviando AT+CPOWD=1");
    _serial.println(LTE_POWER_OFF_COMMAND);
    
    // 3. CRÍTICO: Esperar URC "NORMAL POWER DOWN" (datasheet: ~1.8s típico)
    unsigned long start = millis();
    while (millis() - start < FIX_V6_URC_WAIT_TIMEOUT_MS) {
        if (_serial.available()) {
            String response = _serial.readStringUntil('\n');
            response.trim();  // Limpiar \r\n y espacios
            if (response.indexOf("NORMAL POWER DOWN") != -1) {
                debugPrint("[LTE] URC recibido - apagado confirmado");
                delay(500);
                return true;
            }
        }
        delay(100);
    }
    
    // 4. Si no recibió URC, intentar PWRKEY extendido
    debugPrint("[LTE] WARN: URC no recibido, intentando PWRKEY");
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? HIGH : LOW);
    delay(FIX_V6_PWRKEY_OFF_TIME_MS);
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? LOW : HIGH);
    delay(FIX_V6_TOFF_TON_BUFFER_MS);  // Esperar settling
    
    if (!isAlive()) {
        debugPrint("[LTE] Apagado por PWRKEY exitoso");
        return true;
    }
    
    // 5. ÚLTIMO RECURSO: Reset forzado (>12.6s según datasheet)
    debugPrint("[LTE] WARN: Forzando reset por PWRKEY >12.6s");
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? HIGH : LOW);
    delay(FIX_V6_PWRKEY_RESET_TIME_MS);
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? LOW : HIGH);
    delay(FIX_V6_TOFF_TON_BUFFER_MS);
    
    bool success = !isAlive();
    if (!success) {
        debugPrint("[LTE] ERROR: Modem no responde a reset - posible zombie");
        #if ENABLE_FEAT_V7_PRODUCTION_DIAG
        ProdDiag::logEvent(EVT_MODEM_ZOMBIE, 0);  // FIX-V6: Registrar evento zombie
        #endif
    }
    return success;
    
    // ============ [FIX-V6 END] ============
#else
    // Código original preservado
    if (!isAlive()) {
        debugPrint("Modulo ya esta apagado");
        return true;
    }
    
    _serial.println(LTE_POWER_OFF_COMMAND);
    delay(2000);

    if (!isAlive()) {
        debugPrint("SIM7080G apagado correctamente");
        return true;
    }
    
    debugPrint("Apagado por software fallo, usando PWRKEY...");
    togglePWRKEY();
    delay(3000);

    if (!isAlive()) {
        debugPrint("SIM7080G apagado correctamente");
        return true;
    }
    
    debugPrint("Error: No se pudo apagar el modulo");
    return false;
#endif
}

bool LTEModule::isAlive() {
    clearBuffer();
    
    for (int i = 0; i < 3; i++) {
        _serial.println("AT");
        if (waitForOK(1000)) {
            return true;
        }
        delay(300);
    }
    return false;
}

bool LTEModule::sendATCommand(const char* cmd, uint32_t timeout) {
    clearBuffer();
    _serial.println(cmd);
    
    #if DEBUG_EMI_DIAGNOSTIC_ENABLED
    g_emiStats.totalATCommands++;
    if (_debugEnabled && _debugSerial) {
        _debugSerial->printf("[EMI-CMD] %s\n", cmd);
    }
    #endif
    
    return waitForOK(timeout);
}

void LTEModule::togglePWRKEY() {
    delay(LTE_PWRKEY_PRE_DELAY_MS);

    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? HIGH : LOW);
    delay(LTE_PWRKEY_PULSE_MS);
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? LOW : HIGH);
    
    debugPrint("PWRKEY toggled");
}

bool LTEModule::waitForOK(uint32_t timeout) {
    String response = "";
    uint32_t startTime = millis();
    
    while (millis() - startTime < timeout) {
        while (_serial.available()) {
            char c = _serial.read();
            response += c;
            
            #if DEBUG_EMI_DIAGNOSTIC_ENABLED
            g_emiStats.totalBytesReceived++;
            #endif
            
            if (response.indexOf("OK") != -1) {
                #if DEBUG_EMI_DIAGNOSTIC_ENABLED
                uint32_t respTime = millis() - startTime;
                g_emiStats.successfulResponses++;
                g_emiStats.sumResponseTime += respTime;
                if (respTime < g_emiStats.minResponseTime) g_emiStats.minResponseTime = respTime;
                if (respTime > g_emiStats.maxResponseTime) g_emiStats.maxResponseTime = respTime;
                
                #if DEBUG_EMI_LOG_RAW_HEX
                logRawHex(response, _debugSerial);
                #endif
                #endif
                
                // ============ [FEAT-V7 START] Contar EMI para diagnóstico producción ============
                #if ENABLE_FEAT_V7_PRODUCTION_DIAG
                ProdDiag::countEMI(response);
                #endif
                // ============ [FEAT-V7 END] ============
                
                return true;
            }
            
            if (response.indexOf("ERROR") != -1) {
                #if DEBUG_EMI_DIAGNOSTIC_ENABLED
                g_emiStats.errorResponses++;
                #if DEBUG_EMI_LOG_RAW_HEX
                logRawHex(response, _debugSerial);
                #endif
                #endif
                
                // ============ [FEAT-V7 START] Contar EMI para diagnóstico producción ============
                #if ENABLE_FEAT_V7_PRODUCTION_DIAG
                ProdDiag::countEMI(response);
                #endif
                // ============ [FEAT-V7 END] ============
                
                return false;
            }
        }
        delay(10);
    }
    
    #if DEBUG_EMI_DIAGNOSTIC_ENABLED
    g_emiStats.timeouts++;
    if (_debugEnabled && _debugSerial) {
        _debugSerial->printf("[EMI-TIMEOUT] No response in %lu ms\n", timeout);
        if (response.length() > 0) {
            logRawHex(response, _debugSerial);
        }
    }
    #endif
    
    // ============ [FEAT-V7 START] Registrar timeout AT ============
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::recordATTimeout();
    #endif
    // ============ [FEAT-V7 END] ============
    
    return false;
}

void LTEModule::clearBuffer() {
    while (_serial.available()) {
        _serial.read();
    }
}

bool LTEModule::resetModem() {
    debugPrint("Reiniciando funcionalidad del modem...");
    
    bool cfunSuccess = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            if (_debugEnabled && _debugSerial) {
                _debugSerial->print("Reintento CFUN ");
                _debugSerial->print(attempt + 1);
                _debugSerial->println(" de 3");
            }
            delay(2000);
        }
        
        if (sendATCommand("AT+CFUN=1,1", 15000)) {
            cfunSuccess = true;
            break;
        }
    }
    
    if (!cfunSuccess) {
        debugPrint("Error: CFUN fallo tras 3 intentos");
        return false;
    }
    
    delay(3000);

    debugPrint("Esperando SIM READY...");
    bool simReady = false;
    for (int i = 0; i < 10; i++) {
        String cpinResponse = sendATCommandWithResponse("AT+CPIN?", 5000);
        if (cpinResponse.indexOf("+CPIN: READY") != -1) {
            debugPrint("SIM READY");
            simReady = true;
            break;
        }
        delay(1000);
    }
    
    if (!simReady) {
        debugPrint("Advertencia: SIM no esta lista");
        return false;
    }
    delay(5000);
    return true;
}

String LTEModule::sendATCommandWithResponse(const char* cmd, uint32_t timeout) {
    clearBuffer();
    _serial.println(cmd);
    
    String response = "";
    uint32_t startTime = millis();
    
    while (millis() - startTime < timeout) {
        while (_serial.available()) {
            char c = _serial.read();
            response += c;
        }
        
        if (response.indexOf("OK") != -1 || response.indexOf("ERROR") != -1) {
            return response;
        }
        
        delay(10);
    }
    
    return response;
}

String LTEModule::getICCID() {
    debugPrint("Obteniendo ICCID...");
    
    String response = sendATCommandWithResponse("AT+CCID", 3000);
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Respuesta completa: [");
        _debugSerial->print(response);
        _debugSerial->println("]");
    }
    
    if (response.length() == 0) {
        debugPrint("Error: Sin respuesta al comando AT+CCID");
        return "";
    }
    
    int ccidIndex = response.indexOf("+CCID:");
    if (ccidIndex != -1) {
        int startIdx = ccidIndex + 7;
        
        while (startIdx < response.length() && response.charAt(startIdx) == ' ') {
            startIdx++;
        }
        
        int endIdx = startIdx;
        while (endIdx < response.length() && 
               response.charAt(endIdx) != '\r' && 
               response.charAt(endIdx) != '\n') {
            endIdx++;
        }
        
        String iccid = response.substring(startIdx, endIdx);
        iccid.trim();
        
        if (iccid.length() > 0) {
            if (_debugEnabled && _debugSerial) {
                _debugSerial->print("ICCID: ");
                _debugSerial->println(iccid);
            }
            return iccid;
        }
    }
    
    int startIdx = 0;
    bool foundDigit = false;
    for (int i = 0; i < response.length(); i++) {
        if (isDigit(response.charAt(i)) && !foundDigit) {
            startIdx = i;
            foundDigit = true;
        }
        if (foundDigit && (response.charAt(i) == '\r' || response.charAt(i) == '\n')) {
            String iccid = response.substring(startIdx, i);
            iccid.trim();
            if (iccid.length() >= 19 && iccid.length() <= 22) {
                if (_debugEnabled && _debugSerial) {
                    _debugSerial->print("ICCID extraido: ");
                    _debugSerial->println(iccid);
                }
                return iccid;
            }
            foundDigit = false;
        }
    }
    
    debugPrint("Error: No se pudo extraer el ICCID");
    return "";
}

uint8_t LTEModule::getICCIDBytes(uint8_t* buffer, uint8_t maxLen) {
    String iccid = getICCID();
    
    if (iccid.length() == 0) {
        return 0;
    }
    
    uint8_t len = iccid.length();
    if (len > maxLen) {
        len = maxLen;
    }
    
    for (uint8_t i = 0; i < len; i++) {
        buffer[i] = (uint8_t)iccid.charAt(i);
    }
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("ICCID bytes (");
        _debugSerial->print(len);
        _debugSerial->print("): ");
        for (uint8_t i = 0; i < len; i++) {
            if (buffer[i] < 16) _debugSerial->print("0");
            _debugSerial->print(buffer[i], HEX);
            _debugSerial->print(" ");
        }
        _debugSerial->println();
    }
    
    return len;
}

#if ENABLE_FIX_V1_SKIP_RESET_PDP
bool LTEModule::configureOperator(Operadora operadora, bool skipReset) {
    // ============ [FIX-V1 START] Skip reset cuando hay operadora guardada ============
    if (!skipReset) {
        resetModem();
    } else {
        debugPrint("Saltando reset del modem (skipReset=true)");
    }
    // ============ [FIX-V1 END] ============
#else
bool LTEModule::configureOperator(Operadora operadora) {
    resetModem();
#endif
    if (operadora >= NUM_OPERADORAS) {
        debugPrint("Error: Operadora invalida");
        return false;
    }

    const OperadoraConfig& config = OPERADORAS[operadora];
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Configurando operadora: ");
        _debugSerial->println(config.nombre);
    }

    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Enviando: ");
        _debugSerial->println(config.cnmp);
    }
    if (!sendATCommand(config.cnmp, 2000)) {
        debugPrint("Error: CNMP fallo");
        return false;
    }
    delay(200);

    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Enviando: ");
        _debugSerial->println(config.cmnb);
    }
    if (!sendATCommand(config.cmnb, 2000)) {
        debugPrint("Error: CMNB fallo");
        return false;
    }
    delay(200);

    String bandcfgCmd = "AT+CBANDCFG=\"CAT-M\"," + String(config.bandas);
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Enviando: ");
        _debugSerial->println(bandcfgCmd);
    }
    
    clearBuffer();
    _serial.println(bandcfgCmd);
    delay(100);
    
    String response = "";
    uint32_t startTime = millis();
    while (millis() - startTime < 5000) {
        while (_serial.available()) {
            char c = _serial.read();
            response += c;
        }
        if (response.indexOf("OK") != -1) {
            if (_debugEnabled && _debugSerial) {
                _debugSerial->println("BANDCFG OK");
            }
            break;
        }
        if (response.indexOf("ERROR") != -1) {
            if (_debugEnabled && _debugSerial) {
                _debugSerial->print("BANDCFG ERROR. Respuesta: ");
                _debugSerial->println(response);
            }
            debugPrint("Error: BANDCFG fallo");
            return false;
        }
        delay(10);
    }
    
    if (response.indexOf("OK") == -1) {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("BANDCFG TIMEOUT. Respuesta: ");
            _debugSerial->println(response);
        }
        debugPrint("Error: BANDCFG fallo");
        return false;
    }
    delay(200);

    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("Verificando redes disponibles...");
    }
    
    String copsCmd = "AT+COPS=1,2,\"" + String(config.mcc_mnc) + "\"";
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Enviando: ");
        _debugSerial->println(copsCmd);
    }
    
    clearBuffer();
    _serial.println(copsCmd);
    
    String copsResponse = "";
    uint32_t copsStartTime = millis();
    bool copsSuccess = false;
    
    while (millis() - copsStartTime < 120000) {
        while (_serial.available()) {
            char c = _serial.read();
            copsResponse += c;
        }
        
        if (copsResponse.indexOf("OK") != -1) {
            copsSuccess = true;
            break;
        }
        
        if (copsResponse.indexOf("ERROR") != -1) {
            break;
        }
        
        delay(10);
    }
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Respuesta COPS: ");
        _debugSerial->println(copsResponse);
    }
    
    if (!copsSuccess) {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->println("COPS manual fallo, intentando modo automatico...");
        }
      return false;
    } else {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->println("COPS manual exitoso");
        }
    }
    delay(1000);

    String cgdcont = "AT+CGDCONT=1,\"IP\",\"" + String(config.apn) + "\"";
    if (!sendATCommand(cgdcont.c_str(), 2000)) {
        debugPrint("Error: CGDCONT fallo");
        return false;
    }

    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Operadora configurada: ");
        _debugSerial->println(config.nombre);
    }

    return true;
}

bool LTEModule::attachNetwork() {
    debugPrint("Attach a red...");
    
    clearBuffer();
    _serial.println("AT+CGATT=1");
    
    String response = "";
    uint32_t startTime = millis();
    bool success = false;
    
    while (millis() - startTime < 75000) {
        while (_serial.available()) {
            char c = _serial.read();
            response += c;
        }
        
        if (response.indexOf("OK") != -1) {
            success = true;
            break;
        }
        
        if (response.indexOf("ERROR") != -1) {
            break;
        }
        
        delay(10);
    }
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Respuesta CGATT=1: ");
        _debugSerial->println(response);
    }
    
    if (!success) {
        debugPrint("Error: CGATT fallo");
        return false;
    }
    
    delay(1000);
    debugPrint("Attach exitoso");
    return true;
}

bool LTEModule::activatePDP() {
    debugPrint("Activando PDP context...");
    
    if (!sendATCommand("AT+CNACT=0,1", 10000)) {
        debugPrint("Error: CNACT fallo");
        return false;
    }
    
    delay(500);
    debugPrint("PDP activado");
    return true;
}

bool LTEModule::deactivatePDP() {
    debugPrint("Desactivando PDP context...");
    
    if (!sendATCommand("AT+CNACT=0,0", 5000)) {
        debugPrint("Error: Deactivacion CNACT fallo");
        return false;
    }
    
    debugPrint("PDP desactivado");
    return true;
}

bool LTEModule::detachNetwork() {
    debugPrint("Detach de red...");
    
    bool detachSuccess = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            if (_debugEnabled && _debugSerial) {
                _debugSerial->print("Reintento CGATT=0 ");
                _debugSerial->print(attempt + 1);
                _debugSerial->println(" de 3");
            }
            delay(2000);
        }
        
        clearBuffer();
        _serial.println("AT+CGATT=0");
        
        String response = "";
        uint32_t startTime = millis();
        
        while (millis() - startTime < 15000) {
            while (_serial.available()) {
                char c = _serial.read();
                response += c;
            }
            
            if (response.indexOf("OK") != -1) {
                detachSuccess = true;
                break;
            }
            
            if (response.indexOf("ERROR") != -1) {
                break;
            }
            
            delay(10);
        }
        
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Respuesta CGATT=0: ");
            _debugSerial->println(response);
        }
        
        if (detachSuccess) {
            debugPrint("Detach exitoso");
            return true;
        }
    }
    
    debugPrint("Advertencia: Detach CGATT fallo tras 3 intentos");
    return true;
}

String LTEModule::getNetworkInfo() {
    debugPrint("Obteniendo info de red...");
    
    String response = sendATCommandWithResponse("AT+CPSI?", 3000);
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("Info de red:");
        _debugSerial->println(response);
    }
    
    return response;
}

bool LTEModule::testOperator(Operadora operadora) {
    if (operadora >= NUM_OPERADORAS) {
        debugPrint("Error: Operadora invalida");
        return false;
    }

    const OperadoraConfig& config = OPERADORAS[operadora];
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("========================================");
        _debugSerial->print("Prueba completa: ");
        _debugSerial->println(config.nombre);
        _debugSerial->println("========================================");
    }
    

   

    if (!configureOperator(operadora)) {
        debugPrint("Error en configuracion");
        return false;
    }

    //getCurrentOperator();
    //getCurrentBands();
    delay(1000);

    if (!activatePDP()) {
        debugPrint("Error en activacion PDP");
        return false;
    }

    String networkInfo = getNetworkInfo();
    _signalQualities[operadora] = parseSignalQuality(networkInfo);
    _signalQualities[operadora].operadora = operadora;
    delay(2000);

    deactivatePDP();
    delay(2000);

    resetModem();

    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Prueba de ");
        _debugSerial->print(config.nombre);
        _debugSerial->println(" completada");
        _debugSerial->println("========================================");
    }

    return true;
}

String LTEModule::getCurrentOperator() {
    debugPrint("Consultando operadora actual...");
    
    String response = sendATCommandWithResponse("AT+COPS?", 3000);
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("Operadora actual:");
        _debugSerial->println(response);
    }
    
    return response;
}

int LTEModule::getCSQ() {
    String response = sendATCommandWithResponse("AT+CSQ", 3000);
    
    // Respuesta esperada: +CSQ: XX,YY donde XX es la calidad (0-31, 99=desconocido)
    int csqIndex = response.indexOf("+CSQ:");
    if (csqIndex == -1) {
        return 99;  // Error
    }
    
    int commaIndex = response.indexOf(',', csqIndex);
    if (commaIndex == -1) {
        return 99;
    }
    
    String csqStr = response.substring(csqIndex + 6, commaIndex);
    csqStr.trim();
    int csq = csqStr.toInt();
    
    return csq;
}

String LTEModule::getCurrentBands() {
    debugPrint("Consultando bandas configuradas...");
    
    String response = sendATCommandWithResponse("AT+CBANDCFG?", 3000);
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("Bandas configuradas:");
        _debugSerial->println(response);
    }
    
    return response;
}

String LTEModule::scanNetworks() {
    debugPrint("Escaneando redes disponibles...");
    
    if (!resetModem()) {
        debugPrint("Error: No se pudo reiniciar el modem");
        return "";
    }
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("Esto puede tardar hasta 3 minutos...");
    }
    
    String response = sendATCommandWithResponse("AT+COPS=?", 180000);
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("Redes disponibles:");
        _debugSerial->println(response);
    }
    
    resetModem();
    
    return response;
}

String LTEModule::getSMSService() {
    debugPrint("Consultando servicio SMS...");
    
    String response = sendATCommandWithResponse("AT+CSMS?", 3000);
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("Servicio SMS:");
        _debugSerial->println(response);
    }
    
    return response;
}

bool LTEModule::sendSMS(const char* phoneNumber, const char* message) {
    debugPrint("Enviando SMS...");
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Numero: ");
        _debugSerial->println(phoneNumber);
        _debugSerial->print("Mensaje: ");
        _debugSerial->println(message);
    }
    
    if (!sendATCommand("AT+CMGF=1", 2000)) {
        debugPrint("Error: CMGF fallo (modo texto)");
        return false;
    }
    delay(100);
    
    String cmgsCmd = "AT+CMGS=\"" + String(phoneNumber) + "\"";
    clearBuffer();
    _serial.println(cmgsCmd);
    delay(500);
    
    String response = "";
    uint32_t startTime = millis();
    bool promptReceived = false;
    
    while (millis() - startTime < 5000) {
        while (_serial.available()) {
            char c = _serial.read();
            response += c;
            if (c == '>') {
                promptReceived = true;
                break;
            }
        }
        if (promptReceived) break;
        delay(10);
    }
    
    if (!promptReceived) {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Error: No se recibio prompt. Respuesta: ");
            _debugSerial->println(response);
        }
        debugPrint("Error: No se recibio prompt '>'" );
        return false;
    }
    
    _serial.print(message);
    delay(100);
    _serial.write(0x1A);
    
    response = "";
    startTime = millis();
    bool success = false;
    
    while (millis() - startTime < 30000) {
        while (_serial.available()) {
            char c = _serial.read();
            response += c;
        }
        
        if (response.indexOf("+CMGS:") != -1) {
            success = true;
            break;
        }
        
        if (response.indexOf("ERROR") != -1) {
            break;
        }
        
        delay(10);
    }
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Respuesta envio SMS: ");
        _debugSerial->println(response);
    }
    
    if (success) {
        debugPrint("SMS enviado exitosamente");
        return true;
    } else {
        debugPrint("Error: Fallo al enviar SMS");
        return false;
    }
}

bool LTEModule::sendSMS(const char* message) {
    return sendSMS(SMS_PHONE_NUMBER, message);
}

SignalQuality LTEModule::parseSignalQuality(String cpsiResponse) {
    SignalQuality sq;
    sq.valid = false;
    sq.rsrp = -999;
    sq.rsrq = -999;
    sq.rssi = -999;
    sq.sinr = -999;
    sq.score = -999;
    
    int cpsiIndex = cpsiResponse.indexOf("+CPSI:");
    if (cpsiIndex == -1) {
        return sq;
    }
    
    int startIdx = cpsiIndex + 6;
    int commaCount = 0;
    String values[15];
    String currentValue = "";
    
    for (int i = startIdx; i < cpsiResponse.length(); i++) {
        char c = cpsiResponse.charAt(i);
        
        if (c == ',' || c == '\r' || c == '\n') {
            values[commaCount] = currentValue;
            currentValue = "";
            commaCount++;
            
            if (c == '\r' || c == '\n' || commaCount >= 15) {
                break;
            }
        } else {
            currentValue += c;
        }
    }
    
    if (commaCount >= 13) {
        sq.rsrq = values[10].toInt();
        sq.rsrp = values[11].toInt();
        sq.rssi = values[12].toInt();
        sq.sinr = values[13].toInt();
        sq.valid = true;
        
        sq.score = (4 * sq.sinr) + 2 * (sq.rsrp + 120) + (sq.rsrq + 20);
        
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Calidad de senal - RSSI: ");
            _debugSerial->print(sq.rssi);
            _debugSerial->print(" dBm, RSRP: ");
            _debugSerial->print(sq.rsrp);
            _debugSerial->print(" dBm, RSRQ: ");
            _debugSerial->print(sq.rsrq);
            _debugSerial->print(" dB, SINR: ");
            _debugSerial->print(sq.sinr);
            _debugSerial->print(" dB, Score: ");
            _debugSerial->println(sq.score);
        }
    }
    
    return sq;
}

Operadora LTEModule::getBestOperator() {
    Operadora best = TELCEL;
    int bestScore = -9999;
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println("\n=== Resumen de calidad de senal ===");
    }
    
    for (int i = 0; i < NUM_OPERADORAS; i++) {
        if (_signalQualities[i].valid) {
            if (_debugEnabled && _debugSerial) {
                _debugSerial->print(OPERADORAS[i].nombre);
                _debugSerial->print(": RSRP=");
                _debugSerial->print(_signalQualities[i].rsrp);
                _debugSerial->print(" dBm, RSRQ=");
                _debugSerial->print(_signalQualities[i].rsrq);
                _debugSerial->print(" dB, SINR=");
                _debugSerial->print(_signalQualities[i].sinr);
                _debugSerial->print(" dB, Score=");
                _debugSerial->println(_signalQualities[i].score);
            }
            
            if (_signalQualities[i].score > bestScore) {
                bestScore = _signalQualities[i].score;
                best = (Operadora)i;
            }
        }
    }
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("\nMejor operadora: ");
        _debugSerial->print(OPERADORAS[best].nombre);
        _debugSerial->print(" (Score: ");
        _debugSerial->print(bestScore);
        _debugSerial->println(")");
        _debugSerial->println("===================================\n");
    }
    
    return best;
}

int LTEModule::getOperatorScore(Operadora operadora) {
    if (operadora >= NUM_OPERADORAS) {
        return -999;
    }
    
    if (_signalQualities[operadora].valid) {
        return _signalQualities[operadora].score;
    }
    
    return -999;
}

bool LTEModule::openTCPConnection() {
    CRASH_CHECKPOINT(CP_MODEM_TCP_CONNECT_START);  // FEAT-V3
    debugPrint("Abriendo conexion TCP...");
    
    debugPrint("Cerrando conexion previa si existe...");
    sendATCommand("AT+CACLOSE=0", 2000);
    delay(1000);
    
    String caOpenCmd = "AT+CAOPEN=0,0,\"TCP\",\"" + String(DB_SERVER_IP) + "\"," + String(TCP_PORT);
    CRASH_LOG_AT(caOpenCmd.c_str());  // FEAT-V3
    CRASH_SYNC_NVS();  // FEAT-V3: Guardar antes de operación crítica
    
    bool caSuccess = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            if (_debugEnabled && _debugSerial) {
                _debugSerial->print("Reintento CAOPEN ");
                _debugSerial->print(attempt + 1);
                _debugSerial->println(" de 3");
            }
            sendATCommand("AT+CACLOSE=0", 2000);
            delay(2000);
        }
        
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Enviando: ");
            _debugSerial->println(caOpenCmd);
        }
        
        clearBuffer();
        _serial.println(caOpenCmd);
        
        CRASH_CHECKPOINT(CP_MODEM_TCP_CONNECT_WAIT);  // FEAT-V3
        String response = "";
        uint32_t startTime = millis();
        
        while (millis() - startTime < 75000) {
            while (_serial.available()) {
                char c = _serial.read();
                response += c;
            }
            
            if (response.indexOf("+CAOPEN: 0,0") != -1) {
                caSuccess = true;
                break;
            }
            
            if (response.indexOf("ERROR") != -1) {
                break;
            }
            
            
            
            delay(10);
        }
        
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Respuesta CAOPEN: ");
            _debugSerial->println(response);
        }
        
        if (caSuccess) {
            break;
        }
    }
    
    if (caSuccess) {
        CRASH_CHECKPOINT(CP_MODEM_TCP_CONNECT_OK);  // FEAT-V3
        debugPrint("Conexion TCP abierta exitosamente");
        return true;
    } else {
        CRASH_CHECKPOINT(CP_MODEM_TCP_CONNECT_FAIL);  // FEAT-V3
        debugPrint("Error: Fallo al abrir conexion TCP tras 3 intentos");
        return false;
    }
}

bool LTEModule::closeTCPConnection() {
    debugPrint("Cerrando conexion TCP...");
    
    if (!sendATCommand("AT+CACLOSE=0", 10000)) {
        debugPrint("Error: CACLOSE fallo");
        return false;
    }
    
    delay(500);
    debugPrint("Conexion TCP cerrada");
    return true;
}

bool LTEModule::isTCPConnected() {
    String response = sendATCommandWithResponse("AT+CASTATE?", 3000);
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Estado TCP: ");
        _debugSerial->println(response);
    }
    
    return (response.indexOf("+CASTATE: 0,1") != -1);
}

bool LTEModule::sendTCPData(const char* data) {
    return sendTCPData((const uint8_t*)data, strlen(data));
}

bool LTEModule::sendTCPData(String data) {
    return sendTCPData((const uint8_t*)data.c_str(), data.length());
}

bool LTEModule::sendTCPData(const uint8_t* data, size_t length) {
    CRASH_CHECKPOINT(CP_MODEM_TCP_SEND_START);  // FEAT-V3
    debugPrint("Enviando datos por TCP...");
    
    if (length == 0) {
        debugPrint("Error: No hay datos para enviar");
        return false;
    }
    
    if (length > 1460) {
        debugPrint("Advertencia: Datos mayores a 1460 bytes, truncando");
        length = 1460;
    }
    
    String casendCmd = "AT+CASEND=0," + String(length);
    CRASH_LOG_AT(casendCmd.c_str());  // FEAT-V3
    CRASH_SYNC_NVS();  // FEAT-V3: Guardar antes de operación crítica
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Enviando: ");
        _debugSerial->println(casendCmd);
        _debugSerial->print("Datos (");
        _debugSerial->print(length);
        _debugSerial->print(" bytes): ");
        for (size_t i = 0; i < length && i < 50; i++) {
            _debugSerial->print((char)data[i]);
        }
        if (length > 50) _debugSerial->print("...");
        _debugSerial->println();
    }
    
    clearBuffer();
    _serial.println(casendCmd);
    delay(500);
    
    CRASH_CHECKPOINT(CP_MODEM_TCP_SEND_WAIT);  // FEAT-V3
    String response = "";
    uint32_t startTime = millis();
    bool promptReceived = false;
    
    while (millis() - startTime < 5000) {
        while (_serial.available()) {
            char c = _serial.read();
            response += c;
            if (c == '>') {
                promptReceived = true;
                break;
            }
        }
        if (promptReceived) break;
        delay(10);
    }
    
    if (!promptReceived) {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Error: No se recibio prompt. Respuesta: ");
            _debugSerial->println(response);
        }
        debugPrint("Error: No se recibio prompt '>' para envio TCP");
        return false;
    }
    
    for (size_t i = 0; i < length; i++) {
        _serial.write(data[i]);
    }
    
    response = "";
    startTime = millis();
    bool success = false;
    
    while (millis() - startTime < 10000) {
        while (_serial.available()) {
            char c = _serial.read();
            response += c;
        }
        
        if (response.indexOf("OK") != -1) {
            success = true;
            break;
        }
        
        if (response.indexOf("ERROR") != -1) {
            break;
        }
        
        delay(10);
    }
    
    if (_debugEnabled && _debugSerial) {
        _debugSerial->print("Respuesta CASEND: ");
        _debugSerial->println(response);
    }
    
    if (success) {
        CRASH_CHECKPOINT(CP_MODEM_TCP_SEND_OK);  // FEAT-V3
        debugPrint("Datos enviados exitosamente por TCP");
        return true;
    } else {
        debugPrint("Error: Fallo al enviar datos por TCP");
        return false;
    }
}
