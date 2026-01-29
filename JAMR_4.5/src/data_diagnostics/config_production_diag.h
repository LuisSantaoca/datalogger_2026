/**
 * @file config_production_diag.h
 * @brief Configuración del sistema de diagnóstico de producción
 * @version FEAT-V7
 * @date 2026-01-28
 */

#ifndef CONFIG_PRODUCTION_DIAG_H
#define CONFIG_PRODUCTION_DIAG_H

// ============================================================
// RUTAS DE ARCHIVOS
// ============================================================

/** @brief Directorio de diagnósticos */
#define PROD_DIAG_DIR           "/diag"

/** @brief Archivo de estadísticas binarias */
#define PROD_DIAG_STATS_FILE    "/diag/stats.bin"

/** @brief Archivo de log de eventos (texto) */
#define PROD_DIAG_EVENTS_FILE   "/diag/events.txt"

// ============================================================
// LÍMITES DE ALMACENAMIENTO
// ============================================================

/** @brief Máximo de líneas en events.txt antes de rotar */
#define PROD_DIAG_MAX_EVENTS    20

/** @brief Tamaño máximo de events.txt en bytes (~2KB) */
#define PROD_DIAG_MAX_EVENTS_SIZE  2048

/** @brief Magic number para validar stats.bin */
#define PROD_DIAG_MAGIC         0x44494147  // "DIAG"

/** @brief Versión del struct ProductionStats */
#define PROD_DIAG_VERSION       1

// ============================================================
// UMBRALES EMI
// ============================================================

/** @brief % corrupción total para verdictO "MONITOR" */
#define EMI_THRESHOLD_MONITOR_PCT   0.1f

/** @brief % corrupción total para veredicto "PROBLEMA" */
#define EMI_THRESHOLD_PROBLEM_PCT   1.0f

/** @brief % corrupción por ciclo para veredicto "MONITOR" */
#define EMI_THRESHOLD_WORST_MONITOR 1

/** @brief % corrupción por ciclo para veredicto "PROBLEMA" */
#define EMI_THRESHOLD_WORST_PROBLEM 5

/** @brief Umbral de % por ciclo para registrar evento EMI_DETECTED */
#define EMI_EVENT_THRESHOLD_PCT     1

/** @brief Umbral de % por ciclo para registrar evento EMI_SEVERE */
#define EMI_SEVERE_THRESHOLD_PCT    5

// ============================================================
// CÓDIGOS DE EVENTOS
// ============================================================

/** @brief Boot del sistema */
#define EVT_BOOT            'B'

/** @brief Fallo de envío LTE */
#define EVT_LTE_FAIL        'L'

/** @brief Fallback de operadora */
#define EVT_OP_FALLBACK     'F'

/** @brief Entrada a modo batería baja */
#define EVT_LOW_BAT_ENTER   'E'

/** @brief Salida de modo batería baja */
#define EVT_LOW_BAT_EXIT    'X'

/** @brief Reinicio periódico 24h */
#define EVT_RESTART_24H     'R'

/** @brief Fallo GPS */
#define EVT_GPS_FAIL        'G'

/** @brief EMI detectado (>1%) */
#define EVT_EMI_DETECTED    'I'

/** @brief EMI severo (>5%) */
#define EVT_EMI_SEVERE      'S'

/** @brief Crash detectado */
#define EVT_CRASH           'C'

/** @brief Timeout AT */
#define EVT_AT_TIMEOUT      'T'

#endif // CONFIG_PRODUCTION_DIAG_H
