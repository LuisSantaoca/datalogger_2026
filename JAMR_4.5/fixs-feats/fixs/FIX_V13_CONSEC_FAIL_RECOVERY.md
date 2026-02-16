# FIX-V13: Consecutive Fail Recovery

---

## INFORMACION GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V13 |
| **Tipo** | Fix (Recovery de sistema) |
| **Sistema** | AppController / FSM / NVS |
| **Archivo Principal** | `AppController.cpp`, `JAMR_4.5.ino` |
| **Estado** | Aprobado |
| **Fecha Identificacion** | 2026-02-16 |
| **Version Target** | v2.13.0 |
| **Branch** | `fix-v13/consec-fail-recovery` |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Origen** | Portado de `fenix_1.0` (zombie recovery por software) |
| **Requisitos** | **NFR-12B** (recovery inter-ciclo), **FR-38B** (reset recovery state en sw restart) |
| **Prioridad** | **ALTA** |
| **Flujo** | flujos-de-trabajo/FLUJO_UPGRADE_V2.11_A_V2.13.md — Fase 3 |
| **Nota** | FIX-V10 (Modem Gate) descartado. FIX-V13 opera independiente con `g_skipModemThisCycle`. |

---

## CUMPLIMIENTO DE PREMISAS

| Premisa | Descripcion | Cumplimiento | Evidencia |
|---------|-------------|--------------|-----------|
| **P1** | Aislamiento total | Si | Branch dedicado |
| **P2** | Cambios minimos | Si | .ino + AppController.h + AppController.cpp + FeatureFlags.h |
| **P3** | Defaults seguros | Si | Si flag=0, sin tracking ni restart |
| **P4** | Feature flags | Si | `ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY` con valor 1/0 |
| **P5** | Logging exhaustivo | Si | `[FIX-V13]` en cada evento de tracking |
| **P6** | No cambiar logica existente | Si | Solo agrega tracking y recovery |
| **P7** | Testing gradual | Si | Plan de 5 capas |
| **P8** | Metricas objetivas | Si | Tabla antes/despues |
| **P9** | Rollback plan | Si | Flag a 0 + recompilar (<5 min) |

---

## DIAGNOSTICO

### Problema Identificado

Cuando el modem SIM7080G entra en estado zombie persistente:

1. **Cada ciclo** intenta encender el modem, falla, y va a sleep
2. **No hay memoria** de que el ciclo anterior tambien fallo
3. **No hay escalamiento**: el firmware repite exactamente lo mismo cada 10 minutos
4. **No hay recovery**: sin autoswitch, no hay corte fisico de VBAT
5. **Datos se acumulan** en buffer sin que nadie los envie

El resultado es un datalogger que:
- Desperdicia bateria intentando encender un modem muerto
- Nunca intenta una estrategia diferente (como esp_restart)
- Eventualmente llena el buffer sin poder enviar

### Evidencia

En `fenix_1.0`, este problema ya se resolvio con un sistema de tracking de fallos consecutivos:
- Contador `modemConsecFails` persiste en RTC memory entre ciclos
- Tras 6 fallos consecutivos → `esp_restart()` limpia estado GPIO/UART
- Tras restart → 3 ciclos de backoff (solo sensores, sin modem)
- Si el modem se recupera → reset contador a 0

### Causa Raiz

El firmware no tiene estado entre ciclos respecto a la salud del modem. Cada ciclo trata el modem como si fuera la primera vez. Sin tracking, no hay posibilidad de escalar la respuesta.

---

## EVALUACION

### Impacto Cuantificado

| Metrica | Actual (v2.10.1) | Esperado (FIX-V13) |
|---------|------------------|---------------------|
| Ciclos con modem zombie antes de recovery | Infinito (no hay) | 6 + restart |
| Energia por ciclo durante zombie | ~18-42 mAh | 6-8 mAh (backoff) |
| Datos perdidos por zombie | Depende del tamano del buffer | Mismos (buffer sigue) |
| Probabilidad de recovery | 0% (solo manual) | ~60-70% (tipo A) |

### Impacto por Area

| Aspecto | Descripcion |
|---------|-------------|
| Recovery | esp_restart() limpia GPIO/UART, permite reintentar FIX-V7 |
| Energia | Backoff de 3 ciclos ahorra ~50 mAh (vs intentar modem) |
| Buffer | Datos siguen siendo recopilados durante backoff |
| Diagnostico | Contador de fallos visible en boot banner |

---

## MODELO DE ESTADOS

```
                    ┌─────────────────────┐
                    │  OPERACION NORMAL   │
                    │  modemConsecFails=0  │
                    └──────────┬──────────┘
                               │ LTE falla
                               ▼
                    ┌─────────────────────┐
                    │  DEGRADADO          │
                    │  modemConsecFails++  │
                    │  (1..5)             │
                    └──────────┬──────────┘
                               │ modemConsecFails >= 6
                               ▼
                    ┌─────────────────────┐
                    │  RESTART            │
                    │  Guardar backoff NVS│
                    │  esp_restart()      │
                    └──────────┬──────────┘
                               │ Boot post-restart
                               ▼
                    ┌─────────────────────┐
                    │  BACKOFF            │
                    │  modemSkipCycles=3  │
                    │  Solo sensores      │
                    └──────────┬──────────┘
                               │ modemSkipCycles=0
                               ▼
                    ┌─────────────────────┐
                    │  REINTENTO          │
                    │  Intentar modem     │
                    └──────────┬──────────┘
                        OK /       \ FAIL
                          /           \
            ┌────────────┐     ┌──────────────┐
            │  NORMAL    │     │  DEGRADADO   │
            │  fails=0   │     │  fails++     │
            └────────────┘     └──────────────┘
```

---

## SOLUCION

### Feature Flag en FeatureFlags.h

```cpp
/**
 * FIX-V13: Consecutive Fail Recovery
 * Sistema: AppController / FSM / NVS
 * Archivo: AppController.cpp, JAMR_4.5.ino
 * Descripcion: Tracking de fallos de modem entre ciclos:
 *   - modemConsecFails en RTC_DATA_ATTR (sobrevive deep sleep)
 *   - Tras N fallos → esp_restart() (limpia GPIO/UART)
 *   - Post-restart → backoff M ciclos (solo sensores)
 *   - Estado de backoff en NVS (sobrevive esp_restart)
 * Origen: Portado de fenix_1.0 (zombie recovery)
 * Documentacion: fixs-feats/fixs/FIX_V13_CONSEC_FAIL_RECOVERY.md
 * Estado: Propuesto
 */
#define ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY    1

/** @brief Fallos consecutivos antes de esp_restart() */
#define FIX_V13_FAIL_THRESHOLD                 6

/** @brief Ciclos sin modem post-restart (backoff) */
#define FIX_V13_BACKOFF_CYCLES                 3
```

### Variables persistentes

| Variable | Ubicacion | Persistencia | Proposito |
|----------|-----------|--------------|-----------|
| `modemConsecFails` | JAMR_4.5.ino (RTC_DATA_ATTR) | Deep sleep | Contador de fallos consecutivos |
| `modemSkipCycles` | AppController.cpp (RTC_DATA_ATTR) | Deep sleep | Contador de backoff |
| `modem_rcv/postRst` | NVS (bool) | esp_restart() | Flag de post-restart |
| `modem_rcv/skipCyc` | NVS (uint8) | esp_restart() | Ciclos de backoff iniciales |

**Razon de usar NVS para backoff:** RTC_DATA_ATTR **sobrevive** `esp_restart()` en ESP32-S3 (solo se borra con power-on reset). El backoff necesita sobrevivir el restart, y NVS es la forma mas segura.

**Razon de usar RTC para modemConsecFails:** Debe sobrevivir deep sleep. Tambien sobrevive `esp_restart()`, por lo que se resetea **explicitamente** en el boot post-restart (cuando `postRst == true`).

### Codigo: JAMR_4.5.ino (variables RTC)

```cpp
#include "AppController.h"

// FIX-V13: Variables persistentes en RTC memory
RTC_DATA_ATTR uint16_t ciclos = 0;
RTC_DATA_ATTR uint8_t modemConsecFails = 0;

void setup() {
    ciclos++;

    AppConfig cfg;
    cfg.sleep_time_us = TIEMPO_SLEEP_ACTIVO;
    cfg.ciclos = ciclos;
    cfg.modemConsecFails = modemConsecFails;
    AppInit(cfg);
}
```

### Codigo: AppController.h (AppConfig extendido)

```cpp
struct AppConfig {
    uint64_t sleep_time_us = 10ULL * 60ULL * 1000000ULL;
    uint16_t ciclos = 0;           // Contador de ciclos (diagnostico)
    uint8_t modemConsecFails = 0;  // FIX-V13: fallos consecutivos
};
```

### Codigo: AppInit() — Recovery logic

```cpp
// En AppInit(), despues de ProdDiag::init():

#if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
{
    extern uint8_t modemConsecFails;
    static RTC_DATA_ATTR uint8_t modemSkipCycles = 0;

    // 1. Detectar post-restart: leer flag de NVS
    preferences.begin("modem_rcv", false);
    bool postRestart = preferences.getBool("postRst", false);
    if (postRestart) {
        modemSkipCycles = preferences.getUChar("skipCyc", FIX_V13_BACKOFF_CYCLES);
        preferences.putBool("postRst", false);  // Limpiar flag
        preferences.end();

        // FR-38B: Reset explícito de estados RTC que sobreviven esp_restart()
        modemConsecFails = 0;
        lte.resetRecoveryState();  // Reset s_recoveryAttempts de FIX-V7

        Serial.print("[FIX-V13] Post-restart detectado. Backoff: ");
        Serial.print(modemSkipCycles);
        Serial.println(" ciclos");
        Serial.println("[FIX-V13][FR-38B] modemConsecFails y s_recoveryAttempts reseteados");
    } else {
        preferences.end();
    }

    // 2. Evaluar estado actual
    if (modemSkipCycles > 0) {
        // ESTADO: BACKOFF — solo sensores, sin modem
        modemSkipCycles--;
        g_skipModemThisCycle = true;
        Serial.print("[FIX-V13] Backoff activo. Skip modem. Restantes: ");
        Serial.println(modemSkipCycles);

    } else if (g_cfg.modemConsecFails >= FIX_V13_FAIL_THRESHOLD) {
        // ESTADO: THRESHOLD — guardar backoff y reiniciar
        Serial.println("[FIX-V13] ========================================");
        Serial.print("[FIX-V13] THRESHOLD ALCANZADO: ");
        Serial.print(g_cfg.modemConsecFails);
        Serial.print(" fallos >= ");
        Serial.println(FIX_V13_FAIL_THRESHOLD);
        Serial.println("[FIX-V13] Guardando backoff en NVS y reiniciando...");
        Serial.println("[FIX-V13] ========================================");

        preferences.begin("modem_rcv", false);
        preferences.putBool("postRst", true);
        preferences.putUChar("skipCyc", FIX_V13_BACKOFF_CYCLES);
        preferences.end();

        // NOTA: NO resetear modemConsecFails aqui. Se resetea en boot post-restart
        // (cuando postRst == true) para garantizar consistencia. RTC_DATA_ATTR
        // sobrevive esp_restart() en ESP32-S3.
        Serial.flush();
        delay(100);
        esp_restart();
        // No retorna
    }

    // Log estado
    Serial.print("[FIX-V13] modemConsecFails=");
    Serial.print(g_cfg.modemConsecFails);
    Serial.print(" skipCycles=");
    Serial.println(modemSkipCycles);
}
#endif
```

### Codigo: Cycle_SendLTE — Tracking de fallos

**Cambio prerequisito:** En v2.10.1, la llamada es `(void)sendBufferOverLTE_AndMarkProcessed();` (retorno descartado). FIX-V13 requiere capturar el resultado: `bool lteOk = sendBufferOverLTE_AndMarkProcessed();`. Ver FLUJO seccion 6 (Fase 3) para el codigo completo de Cycle_SendLTE.

```cpp
// Dentro de Cycle_SendLTE, despues de sendBufferOverLTE_AndMarkProcessed():

#if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
extern uint8_t modemConsecFails;
if (lteOk) {
    modemConsecFails = 0;  // Reset: modem funciona
    Serial.println("[FIX-V13] Modem OK. Fails reseteado a 0.");
} else {
    modemConsecFails++;
    Serial.print("[FIX-V13] LTE fallo. Fails consecutivos: ");
    Serial.println(modemConsecFails);
}
#endif
```

### Codigo: LTEModule — FR-38B reset de s_recoveryAttempts

`s_recoveryAttempts` es `static RTC_DATA_ATTR` dentro de `powerOn()` (LTEModule.cpp:189). Sobrevive `esp_restart()`. Sin resetearlo, FIX-V7 ve `s_recoveryAttempts >= FIX_V7_MAX_RECOVERY_PER_BOOT` y salta toda la mitigacion zombie post-restart.

**Solucion:** Agregar metodo publico en LTEModule:

```cpp
// En LTEModule.h:
void resetRecoveryState();

// En LTEModule.cpp:
void LTEModule::resetRecoveryState() {
    // FR-38B: Reset counter para que FIX-V7 tenga un intento limpio post-restart
    // s_recoveryAttempts es static RTC_DATA_ATTR dentro de powerOn(),
    // no es accesible directamente. Solución: variable de control.
    static RTC_DATA_ATTR bool s_forceResetRecovery = false;
    s_forceResetRecovery = true;
}
```

Y en `powerOn()`, al inicio:

```cpp
static RTC_DATA_ATTR uint8_t s_recoveryAttempts = 0;
static RTC_DATA_ATTR bool s_forceResetRecovery = false;
if (s_forceResetRecovery) {
    s_recoveryAttempts = 0;
    s_forceResetRecovery = false;
    Serial.println("[FIX-V13][FR-38B] s_recoveryAttempts reseteado por restart");
}
```

**Alternativa mas simple:** Mover `s_recoveryAttempts` a miembro de clase (no static local) y resetearlo directamente. El ingeniero debe evaluar cual es mas limpia dado el scope de `powerOn()`.

---

## VALOR DE esp_restart()

### Que hace esp_restart()

| Efecto | Beneficio |
|--------|-----------|
| Reinicia todos los GPIOs a estado por defecto | Limpia posibles estados erroneos en PWRKEY |
| Reinicializa UART | Limpia posible corrupcion serial |
| Re-ejecuta AppInit() completo | Reinicializa todos los modulos |
| **NO** resetea RTC_DATA_ATTR automaticamente | modemConsecFails y s_recoveryAttempts **sobreviven** esp_restart(). Se resetean explicitamente en boot post-restart (FR-38B) |

### Que NO hace esp_restart()

| Limitacion | Impacto |
|------------|---------|
| NO corta VBAT del SIM7080G | No resuelve latch-up tipo B |
| NO borra NVS | Backoff persiste (correcto) |
| NO resetea deep sleep counter | FEAT-V4 sigue contando |

### Cuando es efectivo

- **Zombie tipo A** (PSM, UART): Alta probabilidad de recovery (~70%)
- **Zombie tipo B** (latch-up): No efectivo. Datos se bufferean hasta intervencion manual.

---

## INTERACCION CON OTROS FIXES

| Fix | Interaccion |
|-----|-------------|
| FIX-V7 (Zombie Mitigation) | `s_recoveryAttempts` sobrevive esp_restart() (RTC_DATA_ATTR). Se resetea explicitamente via `lte.resetRecoveryState()` en boot post-restart (FR-38B) |
| FIX-V11 (ICCID Cache) | Durante backoff, ICCID se lee del NVS cache |
| FEAT-V4 (Periodic Restart) | esp_restart() de FIX-V13 es diferente al de FEAT-V4 (different NVS flags) |
| FEAT-V7 (Production Diag) | Los contadores de ProdDiag persisten en LittleFS (no se pierden con restart) |

---

## INTERACCION CON FEAT-V4 (Periodic Restart)

**Pregunta critica:** El esp_restart() de FIX-V13, interfiere con FEAT-V4?

**Respuesta:** No, son independientes:

| Aspecto | FEAT-V4 | FIX-V13 |
|---------|---------|---------|
| Trigger | Acumulador de tiempo >= 24h | modemConsecFails >= 6 |
| NVS flag | `g_last_restart_reason_feat4` (RTC) | `modem_rcv/postRst` (NVS) |
| Post-restart | Reset acumulador a 0 | Backoff 3 ciclos |
| Proposito | Prevenir degradacion memoria | Recuperar modem zombie |

FEAT-V4 detecta su propio restart por `g_last_restart_reason_feat4 == FEAT4_RESTART_EXECUTED`.
FIX-V13 detecta su propio restart por `modem_rcv/postRst == true` en NVS.

El acumulador de FEAT-V4 (`g_accum_sleep_us`) es RTC_DATA_ATTR, por lo que se resetea a 0 con esp_restart(). Esto significa que un restart de FIX-V13 tambien reinicia el timer de 24h de FEAT-V4. Esto es aceptable: si el modem estaba zombie, un restart completo es beneficioso.

---

## ARCHIVOS A MODIFICAR

| Archivo | Cambio | Lineas aprox |
|---------|--------|--------------|
| `JAMR_4.5.ino` | Agregar RTC variables (ciclos, modemConsecFails) + AppConfig | +10 |
| `AppController.h` | Extender AppConfig con ciclos + modemConsecFails | +5 |
| `AppController.cpp` | Recovery logic en AppInit() + tracking en Cycle_SendLTE + banner | +50 |
| `src/FeatureFlags.h` | Agregar flag + parametros | +12 |
| `src/data_lte/LTEModule.cpp` | Agregar `resetRecoveryState()` + check `s_forceResetRecovery` en `powerOn()` (FR-38B) | +10 |
| `src/data_lte/LTEModule.h` | Declarar `resetRecoveryState()` | +1 |
| `src/version_info.h` | Actualizar version | +3 |

---

## CRITERIOS DE ACEPTACION

| # | Criterio | Verificacion |
|---|----------|--------------|
| 1 | Boot banner muestra "modemConsecFails=N" | Log serial |
| 2 | LTE exitoso resetea fails a 0 | Log: "[FIX-V13] Modem OK. Fails reseteado a 0" |
| 3 | LTE fallido incrementa fails | Log: "[FIX-V13] LTE fallo. Fails consecutivos: N" |
| 4 | Tras 6 fallos, esp_restart() se ejecuta | Log: "[FIX-V13] THRESHOLD ALCANZADO" |
| 5 | Post-restart muestra backoff activo | Log: "[FIX-V13] Post-restart detectado. Backoff: 3" |
| 6 | Durante backoff, modem no se usa | Log: "[FIX-V13] Backoff activo. Skip modem" |
| 7 | Backoff decrementa hasta 0, luego modem se reintenta | Log serial |
| 8 | FEAT-V4 no se ve afectado (detecta su propio restart) | Log de FEAT-V4 |
| 9 | Flag a 0 restaura comportamiento original | Compilar y probar |
| 10 | Sensores siguen leyendo durante backoff | CYCLE DATA SUMMARY |

---

## ROLLBACK

```cpp
// En FeatureFlags.h cambiar:
#define ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY    0  // Cambiar a 0
// Recompilar y flashear
// NOTA: Tambien quitar variables RTC del .ino y campos de AppConfig
```

**Tiempo de rollback:** <10 minutos (requiere revertir .ino y .h)

---

## LIMITACIONES

| Limitacion | Razon | Mitigacion |
|------------|-------|------------|
| esp_restart() no corta VBAT | No hay control hardware | Solo efectivo para zombie tipo A |
| Backoff de 3 ciclos sin modem | Datos se acumulan sin enviar | Buffer LittleFS los retiene |
| esp_restart() resetea FEAT-V4 timer | RTC_DATA_ATTR se pierde | Aceptable: restart completo es beneficioso |
| Si zombie persiste post-restart | 6+3 ciclos mas antes del siguiente restart | Buffer sigue reteniendo datos |
| modemConsecFails max 255 (uint8_t) | Overflow teorico tras 255 fallos sin restart | Threshold de 6 hace esto imposible |

---

## HISTORIAL

| Fecha | Accion | Version |
|-------|--------|---------|
| 2026-02-16 | Documentacion inicial. Portado de fenix_1.0 zombie recovery | 1.0 |
| 2026-02-16 | Requisitos: NFR-12→NFR-12B+FR-38B. Eliminar ref FIX-V10. Agregar LTEModule.cpp. Documentar (void)→bool. | 1.1 |
| 2026-02-16 | FR-38B: agregar codigo concreto para resetRecoveryState(). Corregir tabla esp_restart() (RTC_DATA_ATTR NO se resetea). Mover reset modemConsecFails al boot post-restart. Agregar LTEModule.h a archivos. | 1.2 |

---

*Fin del documento*
