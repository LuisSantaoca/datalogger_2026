/**
 * @file sensores_rv03.ino
 * @brief Punto de entrada principal de la aplicación Arduino
 * 
 * Archivo sketch principal que configura el tiempo de sleep y
 * delega toda la lógica al AppController.
 * 
 * @author Sistema Sensores RV03
 * @date 2025-12-18
 * @version 2.0
 */

#include "AppController.h"

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
 * Se ejecuta una sola vez al inicio. Configura el tiempo de sleep
 * y llama a AppInit para inicializar todos los módulos del sistema.
 */
void setup() {

   // Incrementar contador de ciclos y reiniciar cada 24 horas (144 ciclos * 10 min)
  ciclos++;
  if (ciclos >= 144) {  // 10 min * 144 ≈ 24h
    ciclos = 0;
    esp_restart();
  }

  AppConfig cfg;
  cfg.sleep_time_us = TIEMPO_SLEEP_ACTIVO;

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
