#ifndef I2CSENSORMODULE_H
#define I2CSENSORMODULE_H

#include <Arduino.h>
#include <AHT20.h>
#include "config_data_sensors.h"

/**
 * @brief Módulo I2C para sensor AHT20 en ESP32-S3.
 */
class I2CSensorModule {
 public:
  /**
   * @brief Inicializa el sensor AHT20.
   * @return true si la inicialización fue exitosa.
   */
  bool begin();

  /**
   * @brief Lee temperatura y humedad del sensor AHT20.
   * @return true si la lectura fue exitosa.
   */
  bool readSensor();

  /**
   * @brief Obtiene la temperatura leída.
   * @return Temperatura en grados Celsius.
   */
  float getTemperature() const;

  /**
   * @brief Obtiene la humedad leída.
   * @return Humedad relativa en porcentaje.
   */
  float getHumidity() const;

  /**
   * @brief Obtiene la temperatura como string sin punto decimal.
   * @return String en formato "2156" para 21.56°C.
   */
  String getTemperatureString() const;

  /**
   * @brief Obtiene la humedad como string sin punto decimal.
   * @return String en formato "5523" para 55.23%.
   */
  String getHumidityString() const;

  /**
   * @brief Verifica si el sensor está conectado.
   * @return true si el sensor responde.
   */
  bool isConnected();

 private:
  AHT20 aht20_;
  float temperature_;
  float humidity_;
};

#endif
