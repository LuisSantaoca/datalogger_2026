#ifndef SLEEPMODULE_H
#define SLEEPMODULE_H

#include <Arduino.h>
#include <esp_sleep.h>
#include "esp_system.h"
#include "config_data_sleepwakeup.h"

/**
 * @file SLEEPModule.h
 * @brief Módulo para manejo de deep sleep y wakeup en ESP32-S3.
 */

/**
 * @brief Clase para configurar fuentes de wakeup y entrar a deep sleep.
 */
class SleepModule {
public:
  /**
   * @brief Inicializa el módulo.
   * @return true si inicializó correctamente.
   */
  bool begin();

  /**
   * @brief Limpia todas las fuentes de wakeup configuradas.
   */
  void clearWakeupSources();

  /**
   * @brief Habilita wakeup por temporizador.
   * Usa DEFAULT_SLEEP_TIME_US del archivo de configuración.
   * @return true si se configuró correctamente.
   */
  bool enableTimerWakeup();

  /**
   * @brief Entra a deep sleep inmediatamente.
   */
  void enterDeepSleep();

  /**
   * @brief Obtiene la causa de wakeup.
   * @return Causa de wakeup según esp_sleep_wakeup_cause_t.
   */
  esp_sleep_wakeup_cause_t getWakeupCause() const;

  /**
   * @brief Convierte la causa de wakeup a texto.
   * @param cause Causa de wakeup.
   * @return Cadena descriptiva.
   */
  const char* wakeupCauseToString(esp_sleep_wakeup_cause_t cause) const;
};

#endif
