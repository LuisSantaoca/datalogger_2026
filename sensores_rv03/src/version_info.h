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

#define FW_VERSION_STRING   "v2.1.0"
#define FW_VERSION_DATE     "2026-01-07"
#define FW_VERSION_NAME     "cycle-timing"

/*******************************************************************************
 * HISTORIAL DE VERSIONES (más reciente arriba)
 * 
 * Formato: VERSION | FECHA | NOMBRE | DESCRIPCIÓN
 *          Cambios: archivo(línea), archivo(línea)
 ******************************************************************************/

// v2.1.0  | 2026-01-07 | cycle-timing        | FEAT-V2: Sistema de timing de ciclo FSM
//         |            |                     | Cambios: CycleTiming.h(nuevo), FeatureFlags.h(L65), AppController.cpp(L32,L166,L620-820)
// v2.0.2  | 2026-01-07 | pdp-fix             | FIX-V1: Skip reset en configureOperator cuando hay operadora guardada
//         |            |                     | Cambios: LTEModule.h(L84), LTEModule.cpp(L326), AppController.cpp(L378)
// v2.0.1  | 2026-01-07 | feature-flags       | FEAT-V1: Sistema de feature flags para compilación condicional
//         |            |                     | Cambios: FeatureFlags.h(nuevo), AppController.cpp(L31,L510)
// v2.0.0  | 2025-12-18 | release-inicial     | Versión inicial con arquitectura FSM
//         | 2026-01-07 | +version-control    | FEAT-V0: Sistema de control de versiones centralizado
//         |            |                     | Cambios: version_info.h(nuevo), AppController.cpp(L30,L506)
// --------|------------|---------------------|------------------------------------------
// PENDIENTES:
// v2.2.0  | PENDIENTE  | gps-retry           | Feature: Reintentar GPS si falla

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
#define FW_VERSION_PATCH    2

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

