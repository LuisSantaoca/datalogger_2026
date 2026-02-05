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

#define FW_VERSION_STRING   "v2.10.1"
#define FW_VERSION_DATE     "2026-02-05"
#define FW_VERSION_NAME     "revert-fix-v9"

/*******************************************************************************
 * HISTORIAL DE VERSIONES (más reciente arriba)
 * 
 * Formato: VERSION | FECHA | NOMBRE | DESCRIPCIÓN
 *          Cambios: archivo(línea), archivo(línea)
 ******************************************************************************/

// v2.10.1 | 2026-02-05 | revert-fix-v9           | REVERSIÓN FIX-V9: Deshabilitado por validación hardware
//         |            |                         | Razón: Test demostró GPIO 13 NO controla MIC2288 EN en PCB real
//         |            |                         | Evidencia: Modem responde ATOK con IO13=LOW (34.509s en test_modem.ino TEST 3)
//         |            |                         | Schematic.json: NO existe conexión GPIO 13 → MIC2288 pin 4 (EN)
//         |            |                         | Comportamiento restaurado: FIX-V7 LITE mode con softReset
//         |            |                         | Cambios: FeatureFlags.h(L282) - ENABLE_FIX_V9_HARD_POWER_CYCLE = 0
//         |            |                         | Docs: datalogger-review/HALLAZGO_CRITICO_GPIO13_2026-02-05.md
//
// v2.10.0 | 2026-02-05 | hard-power-cycle        | FIX-V9: Hard power cycle del modem vía IO13
//         |            |                         | Solución DEFINITIVA al estado zombie tipo B:
//         |            |                         | - IO13 controla MIC2288 EN vía divisor R23/R24
//         |            |                         | - IO13 LOW → VBAT colapsa a ~0V (descarga caps)
//         |            |                         | - IO13 HIGH → VBAT = 3.8V (boost restaurado)
//         |            |                         | - Secuencia: 5s OFF + 1s estabilización + PWRKEY
//         |            |                         | - Protección anti-loop: máx 2 ciclos por boot
//         |            |                         | Cambios: FeatureFlags.h(L265-295), LTEModule.cpp(L343-400,L291-330)
//         |            |                         |          LTEModule.h(L253-262), config_data_lte.h(L27-36)
//         |            |                         | Docs: fixs-feats/fixs/FIX_V9_HARD_POWER_CYCLE.md
//         |            |                         |       REPORTE_TECNICO_MODEM_ZOMBIE_2026-01-31.md (Sec 9.7)
// v2.9.0  | 2026-02-03 | zombie-mitigation       | FIX-V7: Mitigación estado zombie del modem SIM7080G
//         |            |                         | Estrategia por capas en powerOn():
//         |            |                         | - Intentos PWRKEY (3x) con isAlive() entre cada uno
//         |            |                         | - Deshabilitar PSM (AT+CPSMS=0) tras primera respuesta
//         |            |                         | - Verificar PSM (AT+CPSMS?) + log si falla
//         |            |                         | - Reset forzado 12.6s como último recurso
//         |            |                         | - Backoff: máximo 1 ciclo completo por boot
//         |            |                         | Limitación: NO soluciona zombies tipo B (latch-up)
//         |            |                         | Cambios: FeatureFlags.h(L220-250), LTEModule.cpp(L182-290)
//         |            |                         |          config_production_diag.h(L100-108)
//         |            |                         | Docs: fixs-feats/fixs/FIX_V7_ZOMBIE_MITIGATION.md
// v2.8.0  | 2026-01-29 | modem-power-sequence    | FIX-V6: Secuencia robusta de power on/off del modem
//         |            |                         | Implementa especificaciones datasheet SIMCOM SIM7080G:
//         |            |                         | - Espera URC "NORMAL POWER DOWN" en powerOff (timeout 10s)
//         |            |                         | - PWRKEY extendido 1.5s (>1s encender, >1.2s apagar)
//         |            |                         | - Reset forzado >12.6s como último recurso
//         |            |                         | - Manejo PSM: AT+CPSMS=0 después de wake
//         |            |                         | - Buffer flush antes de AT commands
//         |            |                         | - Múltiples reintentos AT post-wake (primer cmd perdido)
//         |            |                         | Resuelve: modem zombie en dispositivos 6948, 6963
//         |            |                         | Cambios: FeatureFlags.h(L145-185), LTEModule.cpp(L185-310)
//         |            |                         | Docs: fixs-feats/fixs/FIX_V6_MODEM_POWER_SEQUENCE.md
//         |            |                         |       calidad/INVESTIGACION_MODEM_ZOMBIE.md
// v2.7.1  | 2026-01-29 | serial-diag-commands    | FEAT-V9: Comandos Serial STATS y LOG para diagnóstico
//         |            |                         | Conecta ProdDiag::printStats() y printEventLog() al Serial
//         |            |                         | Scope V3/V7 independiente, setTimeout(50ms) anti-bloqueo
//         |            |                         | Comandos: STATS, LOG (adicionales a DIAG, HISTORY, CLEAR)
//         |            |                         | Bugfix: DEEPSLEEP (cases 5,8) en setResetReason()
//         |            |                         | Bugfix: epoch=0 → añadido setCurrentEpoch() en Cycle_BuildFrame
//         |            |                         | Cambios: AppController.cpp(L1165-1198,L1397-1403)
//         |            |                         |          ProductionDiag.cpp(L262-268,L524-528), ProductionDiag.h(L243-251)
// v2.7.0  | 2026-01-28 | minimal-testing         | FEAT-V8: Sistema de tests minimalista via serial
//         |            |                         | 4 tests críticos: CRC16, FSM batería, contadores, parsing AT
//         |            |                         | Comandos: TEST_CRC, TEST_BAT, TEST_COUNT, TEST_PARSE, TEST_HELP
//         |            |                         | Zero overhead cuando flag = 0
//         |            |                         | Cambios: TestModule.h/cpp(nuevo), config_tests.h(nuevo)
//         |            |                         | FeatureFlags.h(L233-247,L362-366), AppController.cpp(L47-50,L1176-1179)
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
#define FW_VERSION_MINOR    7
#define FW_VERSION_PATCH    1

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

