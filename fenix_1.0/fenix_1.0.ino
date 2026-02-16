/**
 * @file fenix_1.0.ino
 * @brief Punto de entrada principal — Fenix 1.0 (sin autoswitch)
 *
 * Variante de firmware para dataloggers SIN circuito autoswitch.
 * Basado en jarm_4.5_autoswich v2.3.0. Hereda todos los fixes (V1-V14).
 * Agrega deteccion de modem zombie con recovery por software.
 *
 * @author Fenix 1.0
 * @date 2026-02-15
 * @version 1.0.0
 */

#include "AppController.h"

/** @brief Contador de ciclos persistente en RTC memory (sobrevive deep sleep, libre sin reset) */
RTC_DATA_ATTR uint16_t ciclos = 0;

/** @brief Voltaje de batería del ciclo anterior (sobrevive deep sleep) */
RTC_DATA_ATTR float prevBatVoltage = 0.0f;

/** @brief Fallos consecutivos del modem (sobrevive deep sleep, para zombie detection) */
RTC_DATA_ATTR uint8_t modemConsecFails = 0;

/** @brief Tiempo de sleep para pruebas: 30 segundos */
const uint64_t SLEEP_30_SECONDS  = 30ULL * 1000000ULL;

/** @brief Tiempo de sleep para pruebas: 1 minuto */
const uint64_t SLEEP_1_MINUTE    = 1ULL * 60ULL * 1000000ULL;

/** @brief Tiempo de sleep para pruebas: 2 minutos */
const uint64_t SLEEP_2_MINUTES   = 2ULL * 60ULL * 1000000ULL;

/** @brief Tiempo de sleep operación normal: 5 minutos */
const uint64_t SLEEP_5_MINUTES   = 5ULL * 60ULL * 1000000ULL;

/** @brief Tiempo de sleep operación normal: 10 minutos (defecto) */
const uint64_t SLEEP_10_MINUTES  = 10ULL * 60ULL * 1000000ULL;

/** @brief Tiempo de sleep operación normal: 15 minutos */
const uint64_t SLEEP_15_MINUTES  = 15ULL * 60ULL * 1000000ULL;

/** @brief Tiempo de sleep operación normal: 30 minutos */
const uint64_t SLEEP_30_MINUTES  = 30ULL * 60ULL * 1000000ULL;

/** @brief Tiempo de sleep largo plazo: 1 hora */
const uint64_t SLEEP_1_HOUR      = 60ULL * 60ULL * 1000000ULL;

/**
 * @brief Tiempo de sleep activo configurado
 *
 * Cambia esta constante para seleccionar el intervalo de sleep deseado.
 * Valores disponibles: SLEEP_30_SECONDS, SLEEP_1_MINUTE, SLEEP_2_MINUTES,
 * SLEEP_5_MINUTES, SLEEP_10_MINUTES, SLEEP_15_MINUTES, SLEEP_30_MINUTES,
 * SLEEP_1_HOUR
 */
const uint64_t TIEMPO_SLEEP_ACTIVO = SLEEP_10_MINUTES;

/**
 * @brief Función de configuración inicial de Arduino
 *
 * Se ejecuta una sola vez al inicio. Sin autoswitch: el contador de ciclos
 * se incrementa libre (sin reset a 144) para diagnostico.
 */
void setup() {
  ciclos++;
  Serial.begin(115200);
  delay(200);

  // FENIX: Sin autoswitch (GPIO 2). Ciclo libre para diagnostico.

  AppConfig cfg;
  cfg.sleep_time_us = TIEMPO_SLEEP_ACTIVO;
  cfg.ciclos = ciclos;
  cfg.prevBatVoltage = prevBatVoltage;
  cfg.modemConsecFails = modemConsecFails;

  AppInit(cfg);
}

/**
 * @brief Loop principal de Arduino
 *
 * Se ejecuta continuamente. Delega toda la lógica a AppLoop que
 * gestiona la máquina de estados finitos.
 */
void loop() {
  AppLoop();
}
