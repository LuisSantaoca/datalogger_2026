/**
 * @file BLEModule.cpp
 * @brief Implementación del módulo BLE para extracción de datos del buffer
 */

#include "BLEModule.h"
#include "config_data_buffer.h"
#include "../DebugConfig.h"

// Variable estática para acceso desde callbacks
static BLEModule* bleModuleInstance = nullptr;

BLEModule::BLEModule(BUFFERModule& bufferModule) 
    : buffer(bufferModule),
      pServer(nullptr),
      pCharRead(nullptr),
      pCharControl(nullptr),
      pCharStatus(nullptr),
      deviceConnected(false),
      oldDeviceConnected(false),
      bleActive(false),
      startTime(0),
      lastConnectionTime(0) {
    bleModuleInstance = this;
}

bool BLEModule::begin(const char* deviceName) {
    // Crear dispositivo BLE
    BLEDevice::init(deviceName);
    
    // Crear servidor BLE
    pServer = BLEDevice::createServer();
    ServerCallbacks* serverCallbacks = new ServerCallbacks(this);
    serverCallbacks->connectedPtr = &deviceConnected;
    pServer->setCallbacks(serverCallbacks);
    
    // Crear servicio BLE
    BLEService* pService = pServer->createService(SERVICE_UUID);
    
    // Característica de lectura (Read/Notify)
    pCharRead = pService->createCharacteristic(
        CHAR_UUID_READ,
        BLECharacteristic::PROPERTY_READ | 
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharRead->addDescriptor(new BLE2902());
    
    // Característica de control (Write)
    pCharControl = pService->createCharacteristic(
        CHAR_UUID_CONTROL,
        BLECharacteristic::PROPERTY_WRITE
    );
    pCharControl->setCallbacks(new CharacteristicCallbacks(this));
    
    // Característica de estado (Read/Notify)
    pCharStatus = pService->createCharacteristic(
        CHAR_UUID_STATUS,
        BLECharacteristic::PROPERTY_READ | 
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pCharStatus->addDescriptor(new BLE2902());
    
    // Iniciar servicio
    pService->start();
    
    // Iniciar advertising
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();
    
    bleActive = true;
    startTime = millis();
    lastConnectionTime = millis();
    
    DEBUG_INFO(BLE, "BLE inicializado. Esperando conexión...");
    return true;
}

void BLEModule::update() {
    if (!bleActive) {
        return;
    }
    
    // Verificar timeout de 1 minuto sin conexión
    if (!deviceConnected && !oldDeviceConnected) {
        if (millis() - startTime > 60000) {  // 60 segundos
            DEBUG_WARN(BLE, "Timeout: 1 minuto sin conexión. Apagando BLE...");
            end();
            return;
        }
    }
    
    // Manejar reconexión
    if (!deviceConnected && oldDeviceConnected) {
        DEBUG_INFO(BLE, "Cliente BLE desconectado. Apagando BLE...");
        end();
        return;
    }
    
    // Manejar nueva conexión
    if (deviceConnected && !oldDeviceConnected) {
        DEBUG_INFO(BLE, "Cliente BLE conectado");
        oldDeviceConnected = deviceConnected;
        lastConnectionTime = millis();
        // Enviar estado inicial
        sendBufferStatus();
    }
}

void BLEModule::end() {
    if (!bleActive) {
        return;
    }
    
    bleActive = false;
    
    if (pServer) {
        pServer->getAdvertising()->stop();
    }
    
    BLEDevice::deinit(true);
    
    DEBUG_INFO(BLE, "BLE apagado. Continuando programa...");
}

bool BLEModule::isActive() {
    return bleActive;
}

bool BLEModule::isConnected() {
    return deviceConnected && bleActive;
}

int BLEModule::sendBufferData(int maxLines) {
    if (!deviceConnected) {
        return 0;
    }
    
    String lines[MAX_LINES_TO_READ];
    int lineCount = 0;
    
    // Leer líneas no procesadas del buffer
    if (!buffer.readUnprocessedLines(lines, maxLines, lineCount)) {
        pCharRead->setValue("ERROR: No se pudo leer el buffer");
        pCharRead->notify();
        return 0;
    }
    
    if (lineCount == 0) {
        pCharRead->setValue("INFO: Buffer vacío");
        pCharRead->notify();
        return 0;
    }
    
    // Enviar cada línea
    for (int i = 0; i < lineCount; i++) {
        String data = String(i) + ":" + lines[i];
        pCharRead->setValue(data.c_str());
        pCharRead->notify();
        delay(50); // Pequeño delay para asegurar la transmisión
    }
    
    Serial.print("Enviadas ");
    Serial.print(lineCount);
    Serial.println(" líneas vía BLE");
    
    return lineCount;
}

void BLEModule::sendBufferStatus() {
    if (!deviceConnected) {
        return;
    }
    
    String status = "STATUS|";
    status += "SIZE:" + String(buffer.getFileSize()) + "|";
    status += "EXISTS:" + String(buffer.fileExists() ? "YES" : "NO");
    
    pCharStatus->setValue(status.c_str());
    pCharStatus->notify();
    
    Serial.println("Estado del buffer enviado vía BLE");
}

void BLEModule::processCommand(const String& command) {
    Serial.print("Comando BLE recibido: ");
    Serial.println(command);
    
    if (command == "READ_ALL") {
        // Leer y enviar todas las líneas no procesadas
        sendBufferData(MAX_LINES_TO_READ);
        
    } else if (command == "STATUS") {
        // Enviar estado del buffer
        sendBufferStatus();
        
    } else if (command == "CLEAR") {
        // Limpiar el buffer
        if (buffer.clearFile()) {
            pCharControl->setValue("OK: Buffer limpiado");
            Serial.println("Buffer limpiado vía comando BLE");
        } else {
            pCharControl->setValue("ERROR: No se pudo limpiar el buffer");
        }
        sendBufferStatus();
        
    } else if (command.startsWith("READ:")) {
        // Leer un número específico de líneas
        int numLines = command.substring(5).toInt();
        if (numLines > 0 && numLines <= MAX_LINES_TO_READ) {
            sendBufferData(numLines);
        } else {
            pCharControl->setValue("ERROR: Número de líneas inválido");
        }
        
    } else if (command == "REMOVE_PROCESSED") {
        // Eliminar líneas procesadas
        if (buffer.removeProcessedLines()) {
            pCharControl->setValue("OK: Líneas procesadas eliminadas");
            Serial.println("Líneas procesadas eliminadas vía comando BLE");
        } else {
            pCharControl->setValue("ERROR: No se pudo eliminar líneas");
        }
        sendBufferStatus();
        
    } else {
        pCharControl->setValue("ERROR: Comando desconocido");
        Serial.println("Comando BLE desconocido");
    }
}

// =============================================================================
// Implementación de ServerCallbacks
// =============================================================================

void ServerCallbacks::onConnect(BLEServer* pServer) {
    *connectedPtr = true;
}

void ServerCallbacks::onDisconnect(BLEServer* pServer) {
    *connectedPtr = false;
}

// =============================================================================
// Implementación de CharacteristicCallbacks
// =============================================================================

void CharacteristicCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
    String command = pCharacteristic->getValue().c_str();
    command.trim();
    
    if (command.length() > 0) {
        module->processCommand(command);
    }
}
