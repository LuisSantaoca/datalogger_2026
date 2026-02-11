#include "LTEModule.h"
#include "../DebugConfig.h"     // FIX-V1
#include "../FeatureFlags.h"    // FIX-V1, FIX-V2
#include <string.h>
#include <stdlib.h>

LTEModule::LTEModule(HardwareSerial& serial) : _serial(serial), _debugEnabled(false), _debugSerial(nullptr), _copsErrorCallback(nullptr) {
}

void LTEModule::setDebug(bool enable, Stream* debugSerial) {
    _debugEnabled = enable;
    _debugSerial = debugSerial;
}

void LTEModule::setErrorCallback(COPSErrorCallback callback) {
    _copsErrorCallback = callback;
}

// ============ [FIX-V1 START] Debug con formato estandarizado ============
#if ENABLE_FIX_V1_LTE_STANDARD_LOGGING

void LTEModule::debugPrint(const char* msg) {
    DEBUG_INFO(LTE, msg);
}

void LTEModule::debugPrint(const char* msg, int value) {
    DEBUG_INFO(LTE, String(msg) + value);
}

void LTEModule::debugError(const char* msg) { DEBUG_ERROR(LTE, msg); }
void LTEModule::debugError(const String& msg) { DEBUG_ERROR(LTE, msg); }
void LTEModule::debugWarn(const char* msg) { DEBUG_WARN(LTE, msg); }
void LTEModule::debugWarn(const String& msg) { DEBUG_WARN(LTE, msg); }
void LTEModule::debugInfo(const char* msg) { DEBUG_INFO(LTE, msg); }
void LTEModule::debugInfo(const String& msg) { DEBUG_INFO(LTE, msg); }
void LTEModule::debugVerbose(const char* msg) { DEBUG_VERBOSE(LTE, msg); }
void LTEModule::debugVerbose(const String& msg) { DEBUG_VERBOSE(LTE, msg); }

#else

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

void LTEModule::debugError(const char* msg) { debugPrint(msg); }
void LTEModule::debugError(const String& msg) { debugPrint(msg.c_str()); }
void LTEModule::debugWarn(const char* msg) { debugPrint(msg); }
void LTEModule::debugWarn(const String& msg) { debugPrint(msg.c_str()); }
void LTEModule::debugInfo(const char* msg) { debugPrint(msg); }
void LTEModule::debugInfo(const String& msg) { debugPrint(msg.c_str()); }
void LTEModule::debugVerbose(const char* msg) { debugPrint(msg); }
void LTEModule::debugVerbose(const String& msg) { debugPrint(msg.c_str()); }

#endif
// ============ [FIX-V1 END] ============

void LTEModule::begin() {
    pinMode(LTE_PWRKEY_PIN, OUTPUT);
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? LOW : HIGH);
    
    _serial.begin(LTE_SIM_BAUD, SERIAL_8N1, LTE_PIN_RX, LTE_PIN_TX);
    delay(100);
}

bool LTEModule::powerOn() {
    debugPrint("Encendiendo SIM7080G...");
    
    if (isAlive()) {
        debugPrint("Modulo ya esta encendido");
     
        delay(2000);
        return true;
    }
    
    // ============ [FIX-V2 START] Hard power cycle tras N intentos PWRKEY ============
#if ENABLE_FIX_V2_HARD_POWER_CYCLE
    // Fase 1: Intentar PWRKEY normal (FIX_V2_PWRKEY_ATTEMPTS_BEFORE_HARD intentos)
    for (uint16_t attempt = 0; attempt < FIX_V2_PWRKEY_ATTEMPTS_BEFORE_HARD; attempt++) {
        debugVerbose(String("Intento PWRKEY ") + (attempt+1) + "/" + FIX_V2_PWRKEY_ATTEMPTS_BEFORE_HARD + " (pre-hard)");  // FIX-V2
        
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                debugPrint("SIM7080G encendido correctamente!");
                delay(1000);
                delay(2000);
                return true;
            }
            delay(500);
        }
    }

    // Fase 2: Hard power cycle
    debugWarn("PWRKEY normal fallo, ejecutando hard power cycle...");  // FIX-V2
    hardPowerCycle();

    // Fase 3: Un intento PWRKEY final post-hard-cycle
    debugVerbose("Intento PWRKEY final (post-hard)");  // FIX-V2
    togglePWRKEY();
    delay(LTE_PWRKEY_POST_DELAY_MS);
    
    {
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                debugPrint("SIM7080G encendido tras hard power cycle!");
                delay(1000);
                delay(2000);
                return true;
            }
            delay(500);
        }
    }
#else
    for (uint16_t attempt = 0; attempt < LTE_POWER_ON_ATTEMPTS; attempt++) {
        debugVerbose(String("Intento de encendido ") + (attempt+1) + "/" + LTE_POWER_ON_ATTEMPTS);  // FIX-V1
        
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                debugPrint("SIM7080G encendido correctamente!");
                delay(1000);

                delay(2000);
                return true;
            }
            delay(500);
        }
    }
#endif
    // ============ [FIX-V2 END] ============
    
    debugError("No se pudo encender el modulo");  // FIX-V1
    return false;
}

bool LTEModule::powerOff() {
    debugPrint("Apagando SIM7080G...");
    
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
    
    debugWarn("Apagado por software fallo, usando PWRKEY...");  // FIX-V1
    togglePWRKEY();
    delay(3000);

    if (!isAlive()) {
        debugPrint("SIM7080G apagado correctamente");
        return true;
    }
    
    debugError("No se pudo apagar el modulo");  // FIX-V1
    return false;
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
    return waitForOK(timeout);
}

void LTEModule::togglePWRKEY() {
    delay(LTE_PWRKEY_PRE_DELAY_MS);

    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? HIGH : LOW);
    delay(LTE_PWRKEY_PULSE_MS);
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? LOW : HIGH);
    
    debugVerbose("PWRKEY toggled");  // FIX-V1
}

bool LTEModule::waitForOK(uint32_t timeout) {
    String response = "";
    uint32_t startTime = millis();
    
    while (millis() - startTime < timeout) {
        while (_serial.available()) {
            char c = _serial.read();
            response += c;
            
            if (response.indexOf("OK") != -1) {
                return true;
            }
            
            if (response.indexOf("ERROR") != -1) {
                return false;
            }
        }
        delay(10);
    }
    
    return false;
}

void LTEModule::clearBuffer() {
    while (_serial.available()) {
        _serial.read();
    }
}

bool LTEModule::resetModem() {
    debugWarn("Reiniciando funcionalidad del modem...");  // FIX-V1
    
    bool cfunSuccess = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            debugVerbose(String("Reintento CFUN ") + (attempt+1) + "/3");  // FIX-V1
            delay(2000);
        }
        
        if (sendATCommand("AT+CFUN=1,1", 15000)) {
            cfunSuccess = true;
            break;
        }
    }
    
    if (!cfunSuccess) {
        debugError("CFUN fallo tras 3 intentos");  // FIX-V1
        return false;
    }
    
    delay(3000);

    debugVerbose("Esperando SIM READY...");  // FIX-V1
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
        debugWarn("SIM no esta lista");  // FIX-V1
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
    
    debugVerbose(String("Respuesta completa: [") + response + "]");  // FIX-V1
    
    if (response.length() == 0) {
        debugError("Sin respuesta al comando AT+CCID");  // FIX-V1
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
            debugInfo(String("ICCID: ") + iccid);  // FIX-V1
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
                debugInfo(String("ICCID extraido: ") + iccid);  // FIX-V1
                return iccid;
            }
            foundDigit = false;
        }
    }
    
    debugError("No se pudo extraer el ICCID");  // FIX-V1
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
    
    // FIX-V1: Log ICCID bytes
    String hexStr = "ICCID bytes (" + String(len) + "): ";
    for (uint8_t i = 0; i < len; i++) {
        if (buffer[i] < 16) hexStr += "0";
        hexStr += String(buffer[i], HEX) + " ";
    }
    debugVerbose(hexStr);
    
    return len;
}

bool LTEModule::configureOperator(Operadora operadora) {
    
     resetModem();

    if (operadora >= NUM_OPERADORAS) {
        debugError("Operadora invalida");  // FIX-V1
        return false;
    }

    const OperadoraConfig& config = OPERADORAS[operadora];
    
    debugInfo(String("Configurando operadora: ") + config.nombre);  // FIX-V1

    debugVerbose(String("Enviando: ") + config.cnmp);  // FIX-V1
    if (!sendATCommand(config.cnmp, 2000)) {
        debugError("CNMP fallo");  // FIX-V1
        return false;
    }
    delay(200);

    debugVerbose(String("Enviando: ") + config.cmnb);  // FIX-V1
    if (!sendATCommand(config.cmnb, 2000)) {
        debugError("CMNB fallo");  // FIX-V1
        return false;
    }
    delay(200);

    String bandcfgCmd = "AT+CBANDCFG=\"CAT-M\"," + String(config.bandas);
    
    debugVerbose(String("Enviando: ") + bandcfgCmd);  // FIX-V1
    
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
            debugVerbose("BANDCFG OK");  // FIX-V1
            break;
        }
        if (response.indexOf("ERROR") != -1) {
            debugError(String("BANDCFG fallo. Respuesta: ") + response);  // FIX-V1
            return false;
        }
        delay(10);
    }
    
    if (response.indexOf("OK") == -1) {
        debugError(String("BANDCFG timeout. Respuesta: ") + response);  // FIX-V1
        return false;
    }
    delay(200);

    debugVerbose("Verificando redes disponibles...");  // FIX-V1
    
    String copsCmd = "AT+COPS=1,2,\"" + String(config.mcc_mnc) + "\"";
    
    debugVerbose(String("Enviando: ") + copsCmd);  // FIX-V1
    
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
    
    debugVerbose(String("Respuesta COPS: ") + copsResponse);  // FIX-V1
    
    if (!copsSuccess) {
        debugWarn("COPS manual fallo, intentando modo automatico...");  // FIX-V1
        
        // Invocar callback para borrar configuración de operadora en NVS
        if (_copsErrorCallback != nullptr) {
            debugWarn("Invocando callback de error COPS para limpiar NVS...");  // FIX-V1
            _copsErrorCallback();
        }
        
      return false;
    } else {
        debugInfo("COPS manual exitoso");  // FIX-V1
    }
    delay(1000);

    String cgdcont = "AT+CGDCONT=1,\"IP\",\"" + String(config.apn) + "\"";
    if (!sendATCommand(cgdcont.c_str(), 2000)) {
        debugError("CGDCONT fallo");  // FIX-V1
        return false;
    }

    debugInfo(String("Operadora configurada: ") + config.nombre);  // FIX-V1

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
    
    debugVerbose(String("Respuesta CGATT=1: ") + response);  // FIX-V1
    
    if (!success) {
        debugError("CGATT fallo");  // FIX-V1
        return false;
    }
    
    delay(1000);
    debugPrint("Attach exitoso");
    return true;
}

bool LTEModule::activatePDP() {
    debugPrint("Activando PDP context...");
    
    if (!sendATCommand("AT+CNACT=0,1", 10000)) {
        debugError("CNACT fallo");  // FIX-V1
        return false;
    }
    
    delay(500);
    debugPrint("PDP activado");
    return true;
}

bool LTEModule::deactivatePDP() {
    debugPrint("Desactivando PDP context...");
    
    if (!sendATCommand("AT+CNACT=0,0", 5000)) {
        debugError("Deactivacion CNACT fallo");  // FIX-V1
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
            debugVerbose(String("Reintento CGATT=0 ") + (attempt+1) + "/3");  // FIX-V1
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
        
        debugVerbose(String("Respuesta CGATT=0: ") + response);  // FIX-V1
        
        if (detachSuccess) {
            debugPrint("Detach exitoso");
            return true;
        }
    }
    
    debugWarn("Detach CGATT fallo tras 3 intentos");  // FIX-V1
    return true;
}

String LTEModule::getNetworkInfo() {
    debugVerbose("Obteniendo info de red...");  // FIX-V1
    
    String response = sendATCommandWithResponse("AT+CPSI?", 3000);
    
    debugVerbose(String("Info de red:\n") + response);  // FIX-V1
    
    return response;
}

bool LTEModule::testOperator(Operadora operadora) {


    if (operadora >= NUM_OPERADORAS) {
        debugError("Operadora invalida");  // FIX-V1
        return false;
    }

    const OperadoraConfig& config = OPERADORAS[operadora];
    
    debugInfo(String("======== Prueba completa: ") + config.nombre + " ========");  // FIX-V1
    

   

    if (!configureOperator(operadora)) {
        debugError("Error en configuracion");  // FIX-V1
        return false;
    }

    delay(1000);

    if (!activatePDP()) {
        debugError("Error en activacion PDP");  // FIX-V1
        return false;
    }

    String networkInfo = getNetworkInfo();
    _signalQualities[operadora] = parseSignalQuality(networkInfo);
    _signalQualities[operadora].operadora = operadora;
    delay(2000);

    deactivatePDP();
    delay(2000);

 

    debugInfo(String("Prueba de ") + config.nombre + " completada");  // FIX-V1

    return true;
}

String LTEModule::getCurrentOperator() {
    debugVerbose("Consultando operadora actual...");  // FIX-V1
    
    String response = sendATCommandWithResponse("AT+COPS?", 3000);
    debugVerbose(String("Operadora actual:\n") + response);  // FIX-V1
    return response;
}

String LTEModule::getCurrentBands() {
    debugVerbose("Consultando bandas configuradas...");  // FIX-V1
    
    String response = sendATCommandWithResponse("AT+CBANDCFG?", 3000);
    debugVerbose(String("Bandas configuradas:\n") + response);  // FIX-V1
    return response;
}

String LTEModule::scanNetworks() {
    debugPrint("Escaneando redes disponibles...");
    
    if (!resetModem()) {
        debugError("No se pudo reiniciar el modem");  // FIX-V1
        return "";
    }
    
    debugInfo("Esto puede tardar hasta 3 minutos...");  // FIX-V1
    
    String response = sendATCommandWithResponse("AT+COPS=?", 180000);
    
    debugVerbose(String("Redes disponibles:\n") + response);  // FIX-V1
    
    resetModem();
    
    return response;
}

String LTEModule::getSMSService() {
    debugVerbose("Consultando servicio SMS...");  // FIX-V1
    
    String response = sendATCommandWithResponse("AT+CSMS?", 3000);
    debugVerbose(String("Servicio SMS:\n") + response);  // FIX-V1
    return response;
}

bool LTEModule::sendSMS(const char* phoneNumber, const char* message) {
    debugPrint("Enviando SMS...");
    
    debugInfo(String("Numero: ") + phoneNumber + " | Mensaje: " + message);  // FIX-V1
    
    if (!sendATCommand("AT+CMGF=1", 2000)) {
        debugError("CMGF fallo (modo texto)");  // FIX-V1
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
        debugError(String("No se recibio prompt '>'. Respuesta: ") + response);  // FIX-V1
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
    
    debugVerbose(String("Respuesta envio SMS: ") + response);  // FIX-V1
    
    if (success) {
        debugPrint("SMS enviado exitosamente");
        return true;
    } else {
        debugError("Fallo al enviar SMS");  // FIX-V1
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
        
        // FIX-V1: Consolidated signal quality log
        debugVerbose(String("Calidad de senal - RSSI: ") + sq.rssi + " dBm, RSRP: " + sq.rsrp + " dBm, RSRQ: " + sq.rsrq + " dB, SINR: " + sq.sinr + " dB, Score: " + sq.score);
    }
    
    return sq;
}

Operadora LTEModule::getBestOperator() {
    Operadora best = TELCEL;
    int bestScore = -9999;
    
    debugInfo("\n=== Resumen de calidad de senal ===");  // FIX-V1
    
    for (int i = 0; i < NUM_OPERADORAS; i++) {
        if (_signalQualities[i].valid) {
            // FIX-V1: Consolidated per-operator log
            debugInfo(String(OPERADORAS[i].nombre) + ": RSRP=" + _signalQualities[i].rsrp + " dBm, RSRQ=" + _signalQualities[i].rsrq + " dB, SINR=" + _signalQualities[i].sinr + " dB, Score=" + _signalQualities[i].score);
            
            if (_signalQualities[i].score > bestScore) {
                bestScore = _signalQualities[i].score;
                best = (Operadora)i;
            }
        }
    }
    
    // FIX-V1: Consolidated best operator result log
    debugInfo(String("\nMejor operadora: ") + OPERADORAS[best].nombre + " (Score: " + bestScore + ")\n===================================");
    
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
    debugPrint("Abriendo conexion TCP...");
    
    debugVerbose("Cerrando conexion previa si existe...");  // FIX-V1
    sendATCommand("AT+CACLOSE=0", 2000);
    delay(1000);
    
    String caOpenCmd = "AT+CAOPEN=0,0,\"TCP\",\"" + String(DB_SERVER_IP) + "\"," + String(TCP_PORT);
    
    bool caSuccess = false;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0) {
            debugVerbose(String("Reintento CAOPEN ") + (attempt + 1) + " de 3");  // FIX-V1
            sendATCommand("AT+CACLOSE=0", 2000);
            delay(2000);
        }
        
        debugVerbose(String("Enviando: ") + caOpenCmd);  // FIX-V1
        
        clearBuffer();
        _serial.println(caOpenCmd);
        
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
        
        debugVerbose(String("Respuesta CAOPEN: ") + response);  // FIX-V1
        
        if (caSuccess) {
            break;
        }
    }
    
    if (caSuccess) {
        debugPrint("Conexion TCP abierta exitosamente");
        return true;
    } else {
        debugError("Fallo al abrir conexion TCP tras 3 intentos");  // FIX-V1
        
        // Invocar callback para borrar configuración de operadora en NVS
        if (_copsErrorCallback != nullptr) {
            debugWarn("Invocando callback de error TCP para limpiar NVS...");  // FIX-V1
            _copsErrorCallback();
        }
        
        return false;
    }
}

bool LTEModule::closeTCPConnection() {
    debugVerbose("Cerrando conexion TCP...");  // FIX-V1
    
    if (!sendATCommand("AT+CACLOSE=0", 10000)) {
        debugError("CACLOSE fallo");  // FIX-V1
        return false;
    }
    
    delay(500);
    debugPrint("Conexion TCP cerrada");
    return true;
}

bool LTEModule::isTCPConnected() {
    String response = sendATCommandWithResponse("AT+CASTATE?", 3000);
    debugVerbose(String("Estado TCP: ") + response);  // FIX-V1
    
    return (response.indexOf("+CASTATE: 0,1") != -1);
}

bool LTEModule::sendTCPData(const char* data) {
    return sendTCPData((const uint8_t*)data, strlen(data));
}

bool LTEModule::sendTCPData(String data) {
    return sendTCPData((const uint8_t*)data.c_str(), data.length());
}

bool LTEModule::sendTCPData(const uint8_t* data, size_t length) {
    debugPrint("Enviando datos por TCP...");
    
    if (length == 0) {
        debugError("No hay datos para enviar");  // FIX-V1
        return false;
    }
    
    if (length > 1460) {
        debugWarn("Datos mayores a 1460 bytes, truncando");  // FIX-V1
        length = 1460;
    }
    
    String casendCmd = "AT+CASEND=0," + String(length);
    
    // FIX-V1: Consolidated TCP send details
    {
        String preview = "";
        for (size_t i = 0; i < length && i < 50; i++) { preview += (char)data[i]; }
        if (length > 50) preview += "...";
        debugVerbose(String("Enviando: ") + casendCmd + " | Datos (" + length + " bytes): " + preview);
    }
    
    clearBuffer();
    _serial.println(casendCmd);
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
        debugError(String("No se recibio prompt '>'. Respuesta: ") + response);  // FIX-V1
        debugError("No se recibio prompt '>' para envio TCP");  // FIX-V1
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
    
    debugVerbose(String("Respuesta CASEND: ") + response);  // FIX-V1
    
    if (success) {
        debugPrint("Datos enviados exitosamente por TCP");
        return true;
    } else {
        debugError("Fallo al enviar datos por TCP");  // FIX-V1
        return false;
    }
}

// ============ [FIX-V2 START] Hard Power Cycle via MIC2288 EN pin ============
#if ENABLE_FIX_V2_HARD_POWER_CYCLE
/**
 * @brief Ciclo de apagado/encendido por hardware usando pin MODEM_EN_PIN (MIC2288 EN).
 * 
 * PASO 1: Configurar MODEM_EN_PIN como OUTPUT y poner LOW → corta alimentación al módem.
 * PASO 2: Esperar FIX_V2_POWEROFF_DELAY_MS para descarga de capacitores.
 * PASO 3: Poner HIGH → restaura alimentación.
 * PASO 4: Esperar FIX_V2_STABILIZATION_MS para estabilización.
 * PASO 5: Restaurar MODEM_EN_PIN como INPUT (evita conflicto con ADC batería).
 * 
 * @return true si el ciclo se completó (no garantiza que el módem responda AT).
 */
bool LTEModule::hardPowerCycle() {
    debugWarn("=== HARD POWER CYCLE: Cortando alimentacion del modem ===");  // FIX-V2

    // PASO 1: Cortar alimentación
    pinMode(MODEM_EN_PIN, OUTPUT);
    digitalWrite(MODEM_EN_PIN, LOW);
    debugInfo(String("MODEM_EN_PIN(") + MODEM_EN_PIN + ") -> LOW (alimentacion cortada)");  // FIX-V2

    // PASO 2: Esperar descarga de capacitores
    debugInfo(String("Esperando ") + FIX_V2_POWEROFF_DELAY_MS + "ms para descarga...");  // FIX-V2
    delay(FIX_V2_POWEROFF_DELAY_MS);

    // PASO 3: Restaurar alimentación
    digitalWrite(MODEM_EN_PIN, HIGH);
    debugInfo(String("MODEM_EN_PIN(") + MODEM_EN_PIN + ") -> HIGH (alimentacion restaurada)");  // FIX-V2

    // PASO 4: Esperar estabilización
    debugInfo(String("Esperando ") + FIX_V2_STABILIZATION_MS + "ms para estabilizacion...");  // FIX-V2
    delay(FIX_V2_STABILIZATION_MS);

    // PASO 5: Restaurar pin como INPUT para no interferir con lectura ADC de batería
    pinMode(MODEM_EN_PIN, INPUT);
    debugInfo(String("MODEM_EN_PIN(") + MODEM_EN_PIN + ") restaurado a INPUT (compartido con ADC bateria)");  // FIX-V2

    debugWarn("=== HARD POWER CYCLE completado ===");  // FIX-V2
    return true;
}
#endif
// ============ [FIX-V2 END] ============
