/**
 * @file config_crash_diagnostics.h
 * @brief Configuración del sistema de diagnóstico post-mortem
 * @version FEAT-V3
 * @date 2026-01-15
 * 
 * Este archivo contiene todos los parámetros configurables del sistema
 * de crash diagnostics. Separado para facilitar ajustes sin tocar lógica.
 */

#ifndef CONFIG_CRASH_DIAGNOSTICS_H
#define CONFIG_CRASH_DIAGNOSTICS_H

// ============================================================
// CONFIGURACIÓN GENERAL
// ============================================================

/** @brief Valor mágico para validar datos RTC (indica datos válidos) */
#define CRASH_DIAG_MAGIC            0xDEADBEEF

/** @brief Namespace para almacenamiento NVS */
#define CRASH_DIAG_NVS_NAMESPACE    "crashdiag"

// ============================================================
// CONFIGURACIÓN DE BUFFERS
// ============================================================

/** @brief Longitud máxima del comando AT en RTC memory */
#define CRASH_DIAG_AT_CMD_LEN       48

/** @brief Longitud máxima de la respuesta AT en RTC memory */
#define CRASH_DIAG_AT_RESP_LEN      48

/** @brief Tamaño del ring buffer de historial en RTC */
#define CRASH_DIAG_HISTORY_SIZE     16

// ============================================================
// CONFIGURACIÓN DE LITTLEFS LOG
// ============================================================

/** @brief Ruta del archivo de log de crashes */
#define CRASH_DIAG_LOG_PATH         "/crash_log.bin"

/** @brief Número máximo de entradas en el log (ring buffer) */
#define CRASH_DIAG_LOG_MAX_ENTRIES  32

/** @brief Longitud del comando AT en log (truncado para ahorrar espacio) */
#define CRASH_DIAG_LOG_AT_LEN       32

// ============================================================
// KEYS NVS (max 15 chars)
// ============================================================

#define NVS_KEY_BOOT_COUNT          "boot_cnt"
#define NVS_KEY_CRASH_COUNT         "crash_cnt"
#define NVS_KEY_CONSEC_CRASH        "consec_crsh"
#define NVS_KEY_LAST_CHECKPOINT     "last_cp"
#define NVS_KEY_LAST_REASON         "last_reason"
#define NVS_KEY_LAST_EPOCH          "last_epoch"
#define NVS_KEY_SUCCESS_EPOCH       "success_ep"
#define NVS_KEY_CYCLES_FAIL         "cycles_fail"

#endif // CONFIG_CRASH_DIAGNOSTICS_H
