/**
 * @file AppController.h
 * @brief Interfaz pública del controlador principal — Fenix 1.0
 *
 * Define la estructura de configuración y las funciones públicas para
 * inicializar y ejecutar el sistema de adquisición de datos.
 * Variante sin autoswitch con deteccion de modem zombie por software.
 *
 * @author Fenix 1.0
 * @date 2026-02-15
 * @version 1.0.0
 */

#pragma once
#include <Arduino.h>

// FEAT-V2: Versión de firmware para identificación en logs
#define FW_VERSION "1.0.0"

/** @brief Fallos consecutivos del modem antes de esp_restart() */
#define MODEM_FAIL_THRESHOLD_RESTART  6

/** @brief Ciclos a saltar modem tras esp_restart() (backoff) */
#define MODEM_SKIP_AFTER_RESTART      3

/**
 * @struct AppConfig
 * @brief Estructura de configuración de la aplicación
 *
 * Contiene parámetros configurables por el usuario que afectan
 * el comportamiento del sistema, principalmente el tiempo de sleep.
 */
struct AppConfig {
  /** @brief Tiempo de deep sleep en microsegundos (defecto: 10 minutos) */
  uint64_t sleep_time_us = 10ULL * 60ULL * 1000000ULL;
  /** @brief Contador de ciclos actual (desde .ino, RTC_DATA_ATTR, libre) */
  uint16_t ciclos = 0;
  /** @brief Voltaje de batería del ciclo anterior (desde .ino, RTC_DATA_ATTR) */
  float prevBatVoltage = 0.0f;
  /** @brief Fallos consecutivos del modem (desde .ino, RTC_DATA_ATTR) */
  uint8_t modemConsecFails = 0;
};

/**
 * @brief Inicializa el sistema completo y configura estado inicial
 *
 * Esta función debe ser llamada una sola vez desde setup() de Arduino.
 * Inicializa todos los módulos, detecta la causa del wakeup y establece
 * el estado inicial de la máquina de estados.
 *
 * @param cfg Estructura con parámetros de configuración
 *
 * @note No debe llamarse más de una vez por ciclo de ejecución
 * @see AppLoop()
 */
void AppInit(const AppConfig& cfg);

/**
 * @brief Ejecuta un paso de la máquina de estados finitos
 *
 * Esta función debe ser llamada continuamente desde loop() de Arduino.
 * Ejecuta la lógica del estado actual y gestiona transiciones entre estados.
 *
 * @note Función no bloqueante excepto en estados terminales (Error, Sleep)
 * @see AppInit()
 */
void AppLoop();
