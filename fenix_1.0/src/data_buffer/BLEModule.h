/**
 * @file BLEModule.h
 * @brief Módulo BLE para extraer datos del buffer de forma inalámbrica
 */

#ifndef BLEMODULE_H
#define BLEMODULE_H

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "BUFFERModule.h"

// UUIDs para el servicio y características BLE
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHAR_UUID_READ      "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // Leer datos del buffer
#define CHAR_UUID_CONTROL   "beb5483e-36e1-4688-b7f5-ea07361b26a9"  // Controlar operaciones
#define CHAR_UUID_STATUS    "beb5483e-36e1-4688-b7f5-ea07361b26aa"  // Estado del buffer

class BLEModule {
  public:
    /**
     * Constructor de la clase BLEModule.
     * @param bufferModule Referencia al módulo de buffer para acceso a datos.
     */
    BLEModule(BUFFERModule& bufferModule);
    
    /**
     * Inicializa el servicio BLE con el nombre del dispositivo.
     * @param deviceName Nombre del dispositivo BLE visible para conexión.
     * @return true si la inicialización fue exitosa, false en caso contrario.
     */
    bool begin(const char* deviceName);
    
    /**
     * Actualiza el estado del módulo BLE.
     * Debe llamarse periódicamente en el loop principal.
     */
    void update();
    
    /**
     * Finaliza y apaga el módulo BLE.
     */
    void end();
    
    /**
     * Verifica si el módulo BLE está activo.
     * @return true si BLE está activo, false si ha sido apagado.
     */
    bool isActive();
    
    /**
     * Obtiene el estado de conexión del BLE.
     * @return true si hay un cliente conectado, false en caso contrario.
     */
    bool isConnected();
    
    /**
     * Envía líneas del buffer al cliente BLE conectado.
     * @param maxLines Número máximo de líneas a enviar.
     * @return Número de líneas enviadas.
     */
    int sendBufferData(int maxLines);
    
    /**
     * Envía el estado actual del buffer (tamaño, líneas, etc).
     */
    void sendBufferStatus();
    
  private:
    friend class CharacteristicCallbacks;
    
    BUFFERModule& buffer;
    BLEServer* pServer;
    BLECharacteristic* pCharRead;
    BLECharacteristic* pCharControl;
    BLECharacteristic* pCharStatus;
    bool deviceConnected;
    bool oldDeviceConnected;
    bool bleActive;
    unsigned long startTime;
    unsigned long lastConnectionTime;
    
    /**
     * Procesa comandos recibidos vía característica de control.
     * @param command Comando recibido del cliente BLE.
     */
    void processCommand(const String& command);
};

/**
 * Clase para manejar callbacks de conexión/desconexión del servidor BLE.
 */
class ServerCallbacks: public BLEServerCallbacks {
  public:
    ServerCallbacks(BLEModule* bleModule) : module(bleModule) {}
    
    void onConnect(BLEServer* pServer);
    void onDisconnect(BLEServer* pServer);
    
  private:
    BLEModule* module;
    friend class BLEModule;
    bool* connectedPtr;
};

/**
 * Clase para manejar callbacks de escritura en características BLE.
 */
class CharacteristicCallbacks: public BLECharacteristicCallbacks {
  public:
    CharacteristicCallbacks(BLEModule* bleModule) : module(bleModule) {}
    
    void onWrite(BLECharacteristic* pCharacteristic);
    
  private:
    BLEModule* module;
};

#endif
