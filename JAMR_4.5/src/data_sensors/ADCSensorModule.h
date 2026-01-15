#ifndef ADCSENSORMODULE_H
#define ADCSENSORMODULE_H

#include <Arduino.h>
#include "config_data_sensors.h"

/**
 * @brief Módulo ADC para lectura de sensores analógicos en ESP32-S3.
 */
class ADCSensorModule {
 public:
  /**
   * @brief Inicializa el módulo ADC.
   * @return true si la inicialización fue exitosa.
   */
  bool begin();

  /**
   * @brief Lee el valor del sensor ADC.
   * @return true si la lectura fue exitosa.
   */
  bool readSensor();

  /**
   * @brief Obtiene el valor ADC en crudo (0-4095).
   * @return Valor ADC en crudo.
   */
  uint16_t getRawValue() const;

  /**
   * @brief Obtiene el voltaje leído.
   * @return Voltaje en voltios.
   */
  float getVoltage() const;

  /**
   * @brief Obtiene el valor convertido según el tipo de sensor.
   * @return Valor del sensor.
   */
  float getValue() const;

  /**
   * @brief Obtiene el valor convertido como string.
   * @return String con el valor del sensor.
   */
  String getValueString() const;

 private:
  uint16_t rawValue_;
  float voltage_;
  float value_;
};

#endif
