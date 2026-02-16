/**
 * @file TestModule.cpp
 * @brief Implementación de tests minimalistas
 * @version FEAT-V8
 * @date 2026-01-28
 */

#include "TestModule.h"
#include "../FeatureFlags.h"

#if ENABLE_FIX_V11_ICCID_NVS_CACHE
#include <Preferences.h>
#endif

#if ENABLE_FEAT_V7_PRODUCTION_DIAG
#include "../data_diagnostics/ProductionDiag.h"
#endif

// ============================================================
// UTILIDADES DE TEST
// ============================================================

/**
 * @brief Macro para verificar condiciones de test
 */
#define TEST_ASSERT(condition, name, serial) \
    do { \
        if (condition) { \
            serial->printf("  ✓ %s\n", name); \
            passCount++; \
        } else { \
            serial->printf("  ✗ %s\n", name); \
            failCount++; \
        } \
    } while(0)

/**
 * @brief Imprime resumen de tests
 */
static void printSummary(Stream* serial, uint16_t pass, uint16_t fail) {
    serial->println();
    if (fail == 0) {
        serial->printf("[TEST] ✅ ALL PASS: %d/%d\n", pass, pass + fail);
    } else {
        serial->printf("[TEST] ❌ FAILURES: %d pass, %d fail\n", pass, fail);
    }
    serial->println();
}

// ============================================================
// PROCESAMIENTO DE COMANDOS
// ============================================================

bool TestModule::processCommand(Stream* serial) {
    if (!serial || !serial->available()) {
        return false;
    }

    String cmd = serial->readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd.length() == 0) {
        return false;
    }

    serial->println();
    serial->printf("[TEST] Comando recibido: %s\n", cmd.c_str());
    serial->println();

    if (cmd == "TEST_CRC") {
        testCRC16(serial);
        return true;
    }
    else if (cmd == "TEST_BAT") {
        testBatteryFSM(serial);
        return true;
    }
    else if (cmd == "TEST_COUNT") {
        testCounters(serial);
        return true;
    }
    else if (cmd == "TEST_PARSE") {
        testParsing(serial);
        return true;
    }
    else if (cmd == "TEST_HELP" || cmd == "HELP") {
        printHelp(serial);
        return true;
    }
    #if ENABLE_FIX_V11_ICCID_NVS_CACHE
    else if (cmd == "CLEAR_ICCID") {
        Preferences prefs;
        prefs.begin("sensores", false);
        prefs.remove("iccid");
        prefs.end();
        serial->println("[FIX-V11] Cache ICCID borrado de NVS. Proximo ciclo leera del modem.");
        return true;
    }
    #endif
    else if (cmd.startsWith("TEST_")) {
        serial->printf("[TEST] ❌ Comando desconocido: %s\n", cmd.c_str());
        serial->println("[TEST] Escribe TEST_HELP para ver comandos disponibles\n");
        return true;
    }

    return false;
}

void TestModule::printHelp(Stream* serial) {
    serial->println("╔════════════════════════════════════════════════════════════╗");
    serial->println("║           SISTEMA DE TESTS MINIMALISTA (FEAT-V8)          ║");
    serial->println("╠════════════════════════════════════════════════════════════╣");
    serial->println("║  TEST_CRC    - Validación CRC16 con vectores conocidos    ║");
    serial->println("║  TEST_BAT    - FSM de batería (transiciones)              ║");
    serial->println("║  TEST_COUNT  - Contadores ProductionDiag (incrementos)    ║");
    serial->println("║  TEST_PARSE  - Parsing de respuestas AT del módem         ║");
    serial->println("║  TEST_HELP   - Muestra esta ayuda                         ║");
    #if ENABLE_FIX_V11_ICCID_NVS_CACHE
    serial->println("╠════════════════════════════════════════════════════════════╣");
    serial->println("║  CLEAR_ICCID - Borrar cache ICCID de NVS (FIX-V11)       ║");
    #endif
    serial->println("╚════════════════════════════════════════════════════════════╝");
    serial->println();
}

// ============================================================
// TEST 1: CRC16
// ============================================================

void TestModule::testCRC16(Stream* serial) {
    serial->println("[TEST] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    serial->println("[TEST] TEST 1: CRC16 VALIDATION");
    serial->println("[TEST] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    serial->println();

    uint16_t passCount = 0;
    uint16_t failCount = 0;

#if ENABLE_FEAT_V7_PRODUCTION_DIAG
    // Test 1: Vector conocido de CRC16-CCITT (0xFFFF init)
    {
        uint8_t data1[] = {0x01, 0x02, 0x03, 0x04};
        uint16_t crc1 = ProdDiag::calculateCRC16(data1, 4);
        serial->printf("[TEST] Vector 1: {0x01,0x02,0x03,0x04} -> CRC=0x%04X\n", crc1);
        // Nota: CRC16-CCITT con init 0xFFFF para {0x01,0x02,0x03,0x04} = 0x89C3
        TEST_ASSERT(crc1 == 0x89C3, "CRC16 vector conocido", serial);
    }

    // Test 2: Dato vacío debe dar CRC = 0xFFFF
    {
        uint8_t data2[] = {};
        uint16_t crc2 = ProdDiag::calculateCRC16(data2, 0);
        serial->printf("[TEST] Vector 2: (vacío) -> CRC=0x%04X\n", crc2);
        TEST_ASSERT(crc2 == 0xFFFF, "CRC16 dato vacío = 0xFFFF", serial);
    }

    // Test 3: Dos cálculos consecutivos deben dar mismo resultado (idempotencia)
    {
        uint8_t data3[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
        uint16_t crc3a = ProdDiag::calculateCRC16(data3, 5);
        uint16_t crc3b = ProdDiag::calculateCRC16(data3, 5);
        serial->printf("[TEST] Vector 3: {0xAA,...,0xEE} -> CRC=0x%04X (x2)\n", crc3a);
        TEST_ASSERT(crc3a == crc3b, "CRC16 idempotente", serial);
    }

    // Test 4: Datos diferentes deben dar CRC diferentes
    {
        uint8_t data4a[] = {0x10, 0x20, 0x30};
        uint8_t data4b[] = {0x10, 0x20, 0x31};  // Último byte diferente
        uint16_t crc4a = ProdDiag::calculateCRC16(data4a, 3);
        uint16_t crc4b = ProdDiag::calculateCRC16(data4b, 3);
        serial->printf("[TEST] Vector 4a: {0x10,0x20,0x30} -> CRC=0x%04X\n", crc4a);
        serial->printf("[TEST] Vector 4b: {0x10,0x20,0x31} -> CRC=0x%04X\n", crc4b);
        TEST_ASSERT(crc4a != crc4b, "CRC16 detecta cambios", serial);
    }

    printSummary(serial, passCount, failCount);
#else
    serial->println("[TEST] ⚠️  FEAT-V7 deshabilitado - test CRC16 no disponible");
    serial->println();
#endif
}

// ============================================================
// TEST 2: BATTERY FSM
// ============================================================

void TestModule::testBatteryFSM(Stream* serial) {
    serial->println("[TEST] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    serial->println("[TEST] TEST 2: BATTERY FSM (State Machine)");
    serial->println("[TEST] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    serial->println();

    uint16_t passCount = 0;
    uint16_t failCount = 0;

#if ENABLE_FIX_V3_LOW_BATTERY_MODE
    serial->println("[TEST] Simulando transiciones de FSM de batería");
    serial->println();

    // Test 1: Voltaje normal (3.8V) debe estar en modo activo
    {
        float vBat1 = 3.8f;
        bool isLow1 = (vBat1 <= FIX_V3_UTS_LOW_ENTER);
        serial->printf("[TEST] Estado 1: vBat=%.2fV ", vBat1);
        TEST_ASSERT(!isLow1, "Modo ACTIVO (vBat > 3.20V)", serial);
    }

    // Test 2: Voltaje bajo (3.10V) debe entrar en modo reposo
    {
        float vBat2 = 3.10f;
        bool isLow2 = (vBat2 <= FIX_V3_UTS_LOW_ENTER);
        serial->printf("[TEST] Estado 2: vBat=%.2fV ", vBat2);
        TEST_ASSERT(isLow2, "Modo REPOSO (vBat <= 3.20V)", serial);
    }

    // Test 3: Histéresis - salida requiere voltaje mayor
    {
        float vBat3 = 3.50f;  // Entre 3.20 (entrada) y 3.80 (salida)
        bool canExit = (vBat3 >= FIX_V3_UTS_LOW_EXIT);
        serial->printf("[TEST] Estado 3: vBat=%.2fV ", vBat3);
        TEST_ASSERT(!canExit, "Histéresis (vBat < 3.80V)", serial);
    }

    // Test 4: Voltaje recuperado (3.85V) permite salir
    {
        float vBat4 = 3.85f;
        bool canExit4 = (vBat4 >= FIX_V3_UTS_LOW_EXIT);
        serial->printf("[TEST] Estado 4: vBat=%.2fV ", vBat4);
        TEST_ASSERT(canExit4, "Salida OK (vBat >= 3.80V)", serial);
    }

    // Test 5: Umbrales configurados correctamente
    {
        float entryV = FIX_V3_UTS_LOW_ENTER;
        float exitV = FIX_V3_UTS_LOW_EXIT;
        serial->printf("[TEST] Estado 5: Entry=%.2fV Exit=%.2fV ", entryV, exitV);
        TEST_ASSERT(exitV > entryV, "Histéresis configurada", serial);
    }

    printSummary(serial, passCount, failCount);
#else
    serial->println("[TEST] ⚠️  FIX-V3 deshabilitado - test FSM no disponible");
    serial->println();
#endif
}

// ============================================================
// TEST 3: COUNTERS
// ============================================================

void TestModule::testCounters(Stream* serial) {
    serial->println("[TEST] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    serial->println("[TEST] TEST 3: PRODUCTION DIAG COUNTERS");
    serial->println("[TEST] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    serial->println();

    uint16_t passCount = 0;
    uint16_t failCount = 0;

#if ENABLE_FEAT_V7_PRODUCTION_DIAG
    serial->println("[TEST] Leyendo contadores actuales de ProductionDiag");
    serial->println();

    const ProductionStats& stats = ProdDiag::getStats();
    
    // Test 1: Magic number debe ser válido
    {
        serial->printf("[TEST] Magic: 0x%08X ", stats.magic);
        TEST_ASSERT(stats.magic == 0x44494147, "Magic = 0x44494147", serial);
    }

    // Test 2: Versión debe ser 1
    {
        serial->printf("[TEST] Version: %d ", stats.version);
        TEST_ASSERT(stats.version == 1, "Version = 1", serial);
    }

    // Test 3: Total de ciclos debe ser positivo
    {
        serial->printf("[TEST] Total cycles: %lu ", stats.totalCycles);
        TEST_ASSERT(stats.totalCycles > 0, "Tiene ciclos registrados", serial);
    }

    // Test 4: CRC debe ser válido
    {
        size_t dataLen = sizeof(ProductionStats) - sizeof(uint16_t);
        uint16_t calcCRC = ProdDiag::calculateCRC16((uint8_t*)&stats, dataLen);
        serial->printf("[TEST] CRC: stored=0x%04X calc=0x%04X ", stats.crc16, calcCRC);
        TEST_ASSERT(stats.crc16 == calcCRC, "CRC válido", serial);
    }

    // Test 5: Relación totalCycles >= lteSendOk + lteSendFail
    {
        uint32_t lteTotal = stats.lteSendOk + stats.lteSendFail;
        serial->printf("[TEST] Cycles=%lu LTE=%lu ", stats.totalCycles, lteTotal);
        TEST_ASSERT(stats.totalCycles >= lteTotal, "Ciclos >= intentos LTE", serial);
    }

    // Test 6: Mostrar contadores actuales (informativo)
    serial->println();
    serial->println("[TEST] Contadores actuales:");
    serial->printf("[TEST]   - LTE OK:   %lu\n", stats.lteSendOk);
    serial->printf("[TEST]   - LTE FAIL: %lu\n", stats.lteSendFail);
    serial->printf("[TEST]   - AT Timeouts: %u\n", stats.atTimeouts);
    serial->printf("[TEST]   - EMI Events: %u\n", stats.emiEvents);
    serial->printf("[TEST]   - GPS Fails: %u\n", stats.gpsFails);
    serial->println();

    printSummary(serial, passCount, failCount);
#else
    serial->println("[TEST] ⚠️  FEAT-V7 deshabilitado - test contadores no disponible");
    serial->println();
#endif
}

// ============================================================
// TEST 4: PARSING
// ============================================================

/**
 * @brief Simula extracción de valor de respuesta AT
 * @param response Respuesta AT (ej: "+CPIN: READY")
 * @param prefix Prefijo a buscar (ej: "+CPIN: ")
 * @return Valor extraído o cadena vacía
 */
static String extractValue(const String& response, const char* prefix) {
    int idx = response.indexOf(prefix);
    if (idx == -1) return "";
    
    idx += strlen(prefix);
    int endIdx = response.indexOf('\r', idx);
    if (endIdx == -1) endIdx = response.length();
    
    return response.substring(idx, endIdx);
}

/**
 * @brief Simula parsing de señal (ej: "+CSQ: 18,99")
 */
static bool parseCSQ(const String& response, int& rssi, int& ber) {
    int idx = response.indexOf("+CSQ: ");
    if (idx == -1) return false;
    
    idx += 6;  // Saltar "+CSQ: "
    int commaIdx = response.indexOf(',', idx);
    if (commaIdx == -1) return false;
    
    rssi = response.substring(idx, commaIdx).toInt();
    ber = response.substring(commaIdx + 1).toInt();
    
    return true;
}

void TestModule::testParsing(Stream* serial) {
    serial->println("[TEST] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    serial->println("[TEST] TEST 4: AT RESPONSE PARSING");
    serial->println("[TEST] ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    serial->println();

    uint16_t passCount = 0;
    uint16_t failCount = 0;

    // Test 1: Extracción de ICCID
    {
        String response1 = "+CCID: 8952071800005284632\r\nOK\r\n";
        String iccid = extractValue(response1, "+CCID: ");
        serial->printf("[TEST] Parse ICCID: '%s' ", iccid.c_str());
        TEST_ASSERT(iccid == "8952071800005284632", "ICCID extraído OK", serial);
    }

    // Test 2: Extracción de CPIN
    {
        String response2 = "+CPIN: READY\r\nOK\r\n";
        String cpin = extractValue(response2, "+CPIN: ");
        serial->printf("[TEST] Parse CPIN: '%s' ", cpin.c_str());
        TEST_ASSERT(cpin == "READY", "CPIN extraído OK", serial);
    }

    // Test 3: Parsing de CSQ (RSSI, BER)
    {
        String response3 = "+CSQ: 18,99\r\nOK\r\n";
        int rssi = 0, ber = 0;
        bool ok3 = parseCSQ(response3, rssi, ber);
        serial->printf("[TEST] Parse CSQ: rssi=%d ber=%d ", rssi, ber);
        TEST_ASSERT(ok3 && rssi == 18 && ber == 99, "CSQ parseado OK", serial);
    }

    // Test 4: Respuesta inválida (sin prefijo)
    {
        String response4 = "ERROR\r\n";
        String value4 = extractValue(response4, "+CCID: ");
        serial->printf("[TEST] Parse ERROR: '%s' ", value4.c_str());
        TEST_ASSERT(value4.length() == 0, "Respuesta ERROR manejada", serial);
    }

    // Test 5: Parsing de operadora (AT+COPS?)
    {
        String response5 = "+COPS: 0,0,\"TELCEL\",7\r\nOK\r\n";
        int idx = response5.indexOf("\"");
        int idx2 = response5.indexOf("\"", idx + 1);
        String operadora = (idx != -1 && idx2 != -1) ? 
                          response5.substring(idx + 1, idx2) : "";
        serial->printf("[TEST] Parse COPS: '%s' ", operadora.c_str());
        TEST_ASSERT(operadora == "TELCEL", "Operadora extraída OK", serial);
    }

    // Test 6: Parsing con bytes inválidos (simula EMI)
    {
        String response6 = "+CCID: 89520\xFF\xFF8000052846\r\nOK\r\n";
        bool hasInvalid = false;
        for (size_t i = 0; i < response6.length(); i++) {
            uint8_t c = (uint8_t)response6[i];
            if (c == 0xFF || c == 0x00) {
                hasInvalid = true;
                break;
            }
        }
        serial->printf("[TEST] Detect EMI: 0xFF found=%s ", hasInvalid ? "yes" : "no");
        TEST_ASSERT(hasInvalid, "Detección EMI funciona", serial);
    }

    printSummary(serial, passCount, failCount);
}

