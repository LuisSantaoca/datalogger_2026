#ifndef RS485MODULE_H
#define RS485MODULE_H

#include <Arduino.h>
#include <ModbusMaster.h>
#include "config_data_sensors.h"

/**
 * @brief Módulo RS485 Modbus RTU para ESP32-S3.
 */
class RS485Module {
 public:
  /**
   * @brief Inicializa RS485 y energiza el sensor.
   * @return true si la inicialización fue exitosa.
   */
  bool begin();

  /**
   * @brief Ejecuta lectura Modbus 40001–40004.
   * @return true si la lectura fue correcta.
   */
  bool readSensor();

  /**
   * @brief Obtiene un valor leído del sensor.
   * @param index Índice del registro (0–3).
   * @return Valor del registro o 0 si es inválido.
   */
  uint16_t getRegister(uint8_t index) const;

  /**
   * @brief Obtiene un valor leído del sensor como string.
   * @param index Índice del registro (0–3).
   * @return String con el valor del registro.
   */
  String getRegisterString(uint8_t index) const;

  /**
   * @brief Escribe un valor en un registro Modbus.
   * @param address Dirección del registro (ej: 20 para 40021).
   * @param value Valor a escribir.
   * @return true si la escritura fue exitosa.
   */
  bool writeRegister(uint16_t address, uint16_t value);

  /**
   * @brief Habilita la alimentación del sensor.
   */
  void enablePow();

  /**
   * @brief Deshabilita la alimentación del sensor.
   */
  void disablePow();

 private:
  HardwareSerial* serialPort_;
  ModbusMaster node_;
  uint16_t registers_[MODBUS_REGISTER_COUNT];
};

#endif
