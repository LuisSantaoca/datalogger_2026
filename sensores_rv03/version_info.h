/**
 * @file version_info.h
 * @brief Información centralizada de versión del firmware
 * 
 * INSTRUCCIONES:
 * 1. Modificar SOLO la línea FW_VERSION_STRING con la nueva versión
 * 2. Comentar la versión anterior y agregarla al historial
 * 3. Actualizar FW_VERSION_DATE con la fecha del cambio
 * 
 * @author Sistema Sensores RV03
 */

#pragma once

/*******************************************************************************
 * VERSIÓN ACTIVA - MODIFICAR SOLO ESTA SECCIÓN
 ******************************************************************************/

#define FW_VERSION_STRING   "v2.0.0"
#define FW_VERSION_DATE     "2025-12-18"
#define FW_VERSION_NAME     "release-inicial"

/*******************************************************************************
 * HISTORIAL DE VERSIONES (más reciente arriba)
 ******************************************************************************/

// v2.0.0  | 2025-12-18 | release-inicial     | Versión inicial con arquitectura FSM
// --------|------------|---------------------|------------------------------------------
// PRÓXIMAS VERSIONES:
// v2.0.1  | PENDIENTE  | pdp-fix             | Fix: Reducir eventos PDP redundantes
// v2.1.0  | PENDIENTE  | gps-retry           | Feature: Reintentar GPS si falla

/*******************************************************************************
 * MACROS DERIVADAS (NO MODIFICAR)
 ******************************************************************************/

// Para imprimir en Serial
#define FW_FULL_VERSION     FW_VERSION_STRING " (" FW_VERSION_NAME ")"
#define FW_BUILD_DATE       __DATE__
#define FW_BUILD_TIME       __TIME__

// Para comparaciones programáticas (extraer componentes)
// Ejemplo: v2.0.1 -> MAJOR=2, MINOR=0, PATCH=1
// Nota: Estos se deben actualizar manualmente si se necesitan
#define FW_VERSION_MAJOR    2
#define FW_VERSION_MINOR    0
#define FW_VERSION_PATCH    0

/*******************************************************************************
 * FUNCIÓN DE IMPRESIÓN
 ******************************************************************************/

/**
 * @brief Imprime información completa de versión al Serial
 */
inline void printFirmwareVersion() {
    Serial.println(F("========================================"));
    Serial.print(F("[INFO] Firmware: "));
    Serial.println(F(FW_FULL_VERSION));
    Serial.print(F("[INFO] Fecha version: "));
    Serial.println(F(FW_VERSION_DATE));
    Serial.print(F("[INFO] Compilado: "));
    Serial.print(F(FW_BUILD_DATE));
    Serial.print(F(" "));
    Serial.println(F(FW_BUILD_TIME));
    Serial.println(F("========================================"));
}

