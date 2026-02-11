/**
 * @file AppController.h
 * @brief Interfaz pública del controlador principal de la aplicación
 * 
 * Define la estructura de configuración y las funciones públicas para
 * inicializar y ejecutar el sistema de adquisición de datos.
 * 
 * @author Sistema Sensores RV03
 * @date 2025-12-18
 * @version 2.0
 */

#pragma once
#include <Arduino.h>

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
