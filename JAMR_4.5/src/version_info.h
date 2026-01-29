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

#define FW_VERSION_STRING   "v2.6.0"
#define FW_VERSION_DATE     "2026-01-28"
#define FW_VERSION_NAME     "production-diagnostics"

/*******************************************************************************
 * HISTORIAL DE VERSIONES (más reciente arriba)
 * 
 * Formato: VERSION | FECHA | NOMBRE | DESCRIPCIÓN
 *          Cambios: archivo(línea), archivo(línea)
 ******************************************************************************/

// v2.6.0  | 2026-01-28 | production-diagnostics | FEAT-V7: Diagnóstico de producción persistente
//         |            |                        | Contadores: ciclos, LTE OK/FAIL, EMI, crashes, batería baja
//         |            |                        | Log de eventos circular, comandos Serial: STATS, LOG, CLEAR
//         |            |                        | Cambios: ProductionDiag.h/cpp(nuevo), config_production_diag.h(nuevo)
//         |            |                        | AppController.cpp(L42-44,L460,L493,L517,L782,L892,L903,L959,L1061-1064,L1472-1476,L1575)
//         |            |                        | LTEModule.cpp(L21-23,L315,L332,L354), FeatureFlags.h(L203-215,L323-327)
// v2.5.0  | 2026-01-28 | periodic-restart    | FEAT-V4: Reinicio periódico preventivo cada 24h
//         |            |                     | Acumula µs reales de sleep, restart en punto seguro
//         |            |                     | Cambios: FeatureFlags.h(L145-175), AppController.cpp(L69-82,L903-937,L1276-1316)
// v2.4.0  | 2026-01-26 | modem-poweroff-sleep | FIX-V4: Apagado robusto de modem antes de deep sleep
//         |            |                      | Espera URC "NORMAL POWER DOWN" según datasheet SIM7080G
//         |            |                      | Cambios: FeatureFlags.h(L99-120), LTEModule.cpp(L90-140), AppController.cpp(L1207-1218)
// v2.3.0  | 2026-01-15 | low-battery-mode    | FIX-V3: Modo reposo por batería baja (RF-06, RF-09)
//         |            |                     | Cambios: FeatureFlags.h(L56-91), AppController.cpp(L62-74,L186-280,L908-928)
// v2.2.0  | 2026-01-13 | fallback-operadora  | FIX-V2: Fallback a escaneo cuando falla operadora guardada (RF-12)
//         |            |                     | Cambios: FeatureFlags.h(L44-58), AppController.cpp(L385-455)
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
// v2.5.0  | PENDIENTE  | gps-retry           | Feature: Reintentar GPS si falla

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
#define FW_VERSION_MINOR    6
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

