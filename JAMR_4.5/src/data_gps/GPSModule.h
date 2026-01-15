#ifndef GPS_MODULE_H
#define GPS_MODULE_H

#include <Arduino.h>
#include "config_data_gps.h"
#include "../DebugConfig.h"

// Macros de Debug antiguas - COMENTADAS (ahora usa DebugConfig.h)
/*
#if DEBUG_ENABLED
  #define DEBUG_ERROR(...)   if(DEBUG_LEVEL >= DEBUG_LEVEL_ERROR)   { Serial.print("[ERROR] ");   Serial.println(__VA_ARGS__); }
  #define DEBUG_WARN(...)    if(DEBUG_LEVEL >= DEBUG_LEVEL_WARNING) { Serial.print("[WARN]  ");   Serial.println(__VA_ARGS__); }
  #define DEBUG_INFO(...)    if(DEBUG_LEVEL >= DEBUG_LEVEL_INFO)    { Serial.print("[INFO]  ");   Serial.println(__VA_ARGS__); }
  #define DEBUG_DEBUG(...)   if(DEBUG_LEVEL >= DEBUG_LEVEL_DEBUG)   { Serial.print("[DEBUG] ");   Serial.println(__VA_ARGS__); }
  #define DEBUG_VERBOSE(...) if(DEBUG_LEVEL >= DEBUG_LEVEL_VERBOSE) { Serial.print("[VERBOSE] "); Serial.println(__VA_ARGS__); }
#else
  #define DEBUG_ERROR(...)
  #define DEBUG_WARN(...)
  #define DEBUG_INFO(...)
  #define DEBUG_DEBUG(...)
  #define DEBUG_VERBOSE(...)
#endif
*/

/**
 * @file GPSModule.h
 * @brief Encendido por PWRKEY, control AT y lectura GNSS en SIM7080G.
 */

struct GpsFix {
  bool hasFix;
  float latitude;
  float longitude;
  float altitude;
};

class GPSModule {
 public:
  /**
   * @brief Constructor.
   * @param serial Stream para comandos AT.
   * @param pwrKeyPin Pin conectado a PWRKEY.
   */
  GPSModule(Stream& serial, uint8_t pwrKeyPin);

  /**
   * @brief Enciende el módulo usando PWRKEY y valida AT.
   * @param attempts Número de intentos de secuencia PWRKEY.
   * @return true si el módulo responde AT OK.
   */
  bool powerOn(uint16_t attempts = GPS_POWER_ON_ATTEMPTS);

  /**
   * @brief Apaga GNSS y manda apagado del módulo por AT.
   * @return true si se envió el comando de apagado.
   */
  bool powerOff();

  /**
   * @brief Obtiene lat/lon/alt con reintentos y apaga el módulo al conseguir fix.
   * @param[out] fix Resultado GNSS.
   * @param retries Número de reintentos.
   * @return true si se obtuvo fix válido.
   */
  bool getCoordinatesAndShutdown(GpsFix& fix, uint16_t retries = GPS_GNSS_MAX_RETRIES);

  /**
   * @brief Extrae latitud como string.
   * @param[out] latStr String de latitud.
   * @param retries Número de reintentos.
   * @return true si se obtuvo fix válido.
   */
  bool getLatitudeAsString(String& latStr, uint16_t retries = GPS_GNSS_MAX_RETRIES);

  /**
   * @brief Extrae longitud como string.
   * @param[out] lonStr String de longitud.
   * @param retries Número de reintentos.
   * @return true si se obtuvo fix válido.
   */
  bool getLongitudeAsString(String& lonStr, uint16_t retries = GPS_GNSS_MAX_RETRIES);

  /**
   * @brief Extrae altitud como string.
   * @param[out] altStr String de altitud.
   * @param retries Número de reintentos.
   * @return true si se obtuvo fix válido.
   */
  bool getAltitudeAsString(String& altStr, uint16_t retries = GPS_GNSS_MAX_RETRIES);

 private:
  Stream& serial_;
  uint8_t pwrKeyPin_;

  void applyPwrKeySequence();
  void setPwrKeyIdle();
  void flushInput();

  bool waitAtReady(uint32_t timeoutMs);

  bool sendCommand(const char* command);
  bool waitForOk(uint32_t timeoutMs);
  bool readLine(char* buffer, size_t bufferSize, uint32_t timeoutMs);

  bool gnssPowerOn();
  bool gnssPowerOff();
  bool requestCgnsinf(GpsFix& fix);
  bool parseCgnsinf(const char* line, GpsFix& fix);
  bool isValidNumber(const char* s) const;
};

#endif
