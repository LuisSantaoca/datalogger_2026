/**
 * @file config_data_buffer.h
 * @brief Configuración del módulo de buffer de datos
 * @version 1.0.0
 * @date 2026-01-15
 * 
 * Parámetros de rutas de archivos y marcadores de procesamiento.
 */

#ifndef CONFIG_DATA_BUFFER_H
#define CONFIG_DATA_BUFFER_H

// =============================================================================
// CONFIGURACIÓN DE ARCHIVOS
// =============================================================================

/**
 * Ruta del archivo donde se almacena el buffer de datos.
 * Este archivo guarda información persistente en el sistema de archivos.
 */
#define BUFFER_FILE_PATH "/buffer.txt"

/**
 * Número máximo de líneas que se pueden leer del archivo buffer.
 * Controla el uso de memoria y el tiempo de procesamiento al leer datos.
 */
#define MAX_LINES_TO_READ 50

/**
 * Marcador que se agrega al inicio de las líneas ya procesadas.
 * Permite identificar qué líneas han sido procesadas por el sistema.
 */
#define PROCESSED_MARKER "[P]"

// =============================================================================
// CONFIGURACIÓN DE COMUNICACIÓN SERIAL
// =============================================================================

/**
 * Velocidad de comunicación serial en baudios.
 * Determina la velocidad de transmisión de datos a través del puerto serial.
 */
#define SERIAL_BAUD_RATE 115200

#endif
