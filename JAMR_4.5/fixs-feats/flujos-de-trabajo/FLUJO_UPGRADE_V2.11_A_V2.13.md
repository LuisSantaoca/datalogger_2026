# FLUJO: Upgrade JAMR_4.5 v2.10.1 → v2.13.0

**Fecha:** 2026-02-16
**Estado:** Aprobado para ejecucion
**Autor:** Ingenieria de Firmware
**Premisas aplicables:** PREMISAS_DE_FIXS.md (10 premisas)
**Metodologia:** METODOLOGIA_DE_CAMBIOS.md

---

## 1. CONTEXTO

El datalogger JAMR_4.5 en produccion (v2.7.1 en campo, v2.10.1 en repo) presenta:

- **126 segundos desperdiciados** en 9 intentos PWRKEY cuando el modem esta zombie
- **Sin cache ICCID**: Enciende modem solo para leer ICCID cada ciclo (FR-07, FR-08 incumplidos)
- **Sin trim CR en buffer**: Tramas con `\r` residual pueden ser rechazadas (FR-26 incumplido)
- **Sin recovery entre ciclos**: No hay mecanismo de recuperacion tras fallos consecutivos

### 1.1 RESTRICCION DE HARDWARE: SIN AUTOSWITCH

**JAMR_4.5 NO tiene circuito de autoswitch (GPIO 2).** A diferencia de `jarm_4.5_autoswich`, esta placa no puede cortar VBAT al modem SIM7080G por hardware.

Esto implica:

| Aspecto | Con autoswitch | JAMR_4.5 (sin autoswitch) |
|---------|---------------|---------------------------|
| Corte VBAT modem | Cada ~24h via GPIO 2 | **No existe** |
| Recovery zombie tipo A (PSM/UART) | Autoswitch + FIX-V7 | FIX-V7 + FIX-V13 (`esp_restart`) |
| Recovery zombie tipo B (latch-up) | Autoswitch corta VBAT | **IRRECUPERABLE por software** |
| Peor caso sin intervencion | 24h maximo | **Indefinido** hasta power cycle fisico |
| Requisito NFR-06 | Satisfecho | **No aplicable** (sin hardware) |

**Que hace `esp_restart()`:**
- Reinicia el ESP32-S3 completamente (GPIOs, UART, perifericOs)
- Reinicializa FIX-V7 (nuevo boot = permite nuevo intento de recovery)
- Borra RTC_DATA_ATTR (fresh start)

**Que NO hace `esp_restart()`:**
- No corta VBAT al SIM7080G (el modem tiene su propio regulador MIC2288)
- No recupera un modem en latch-up electrico (zombie tipo B)

**Consecuencia para FIX-V13:** La recovery por `esp_restart()` solo es efectiva contra zombies tipo A. Si el modem esta en latch-up (tipo B), el ciclo sera:

```
try → fail x6 → esp_restart → backoff 3 → try → fail x6 → esp_restart → ...
```

**Pero los datos no se pierden** (PRINC-01): el device sigue leyendo sensores y acumulando tramas en buffer. Cuando alguien haga power cycle fisico, todas las tramas pendientes se envian.

**Capas de defensa disponibles en JAMR_4.5:**

```
Capa 1: FIX-V7 (dentro de powerOn)     → PWRKEY retries + PSM disable + reset 13s
Capa 2: FIX-V13 (entre ciclos)         → esp_restart() tras 6 fallos + backoff
Capa 3: FEAT-V4 (periodico)            → esp_restart() cada 24h (limpia estado ESP32)
Capa 4: Intervencion fisica            → Power cycle manual (unica opcion para tipo B)
```

Esta limitacion es inherente al hardware y no puede resolverse por firmware.

---

## 2. DECISIONES

### 2.1 Fixes APROBADOS (3)

| Fix | Nombre | Requisito que satisface | Prioridad |
|-----|--------|------------------------|-----------|
| FIX-V11 | ICCID NVS Cache | **FR-07, FR-08** (obligatorio, nunca implementado) | Alta |
| FIX-V12 | Buffer Trim CR | **FR-26** (obligatorio, nunca implementado) | Alta |
| FIX-V13 | Consecutive Fail Recovery | **NFR-12** (recovery automatica de zombie) | Alta |

### 2.2 Fix DESCARTADO

| Fix | Nombre | Razon |
|-----|--------|-------|
| FIX-V10 | Modem Gate por Ciclo | **Beneficio marginal con FIX-V11 activo.** GPS solo usa modem en primer ciclo (1/144 por dia). Con ICCID cacheado, el unico punto de modem en ciclos normales es SendLTE. No hay nada despues que saltar. Complejidad en 3 estados FSM no justificada para 0.7% de los ciclos. |

**Analisis que sustenta la decision:**

```
Ciclos subsecuentes CON FIX-V11 (143 de 144 por dia):
  Sensors → GPS de NVS (no modem) → ICCID de cache (no modem) → SendLTE (modem)
                                                                  ↑ unico punto
  FIX-V10 no tiene nada que saltar despues de SendLTE → beneficio = 0
```

### 2.3 Fixes DEPRECADOS

| Fix | Estado actual | Razon de deprecacion |
|-----|--------------|---------------------|
| FIX-V5 (Watchdog) | `= 0` desde siempre | ESP-IDF v5.x provee TWDT por defecto |
| FIX-V9 (Hard Power Cycle) | `= 0` desde v2.10.1 | GPIO 13 no controla MIC2288 EN en hardware real |

Accion: Marcar como DEPRECADO en comentarios de FeatureFlags.h. No eliminar codigo (premisa P6).

### 2.4 Parametros MUERTOS a limpiar

| Parametro | En | Razon |
|-----------|-----|-------|
| `FIX_V4_URC_TIMEOUT_MS` | FeatureFlags.h | Ya dice "DEPRECADO, usar FIX_V6_..." |
| `FIX_V4_PWRKEY_WAIT_MS` | FeatureFlags.h | Ya dice "DEPRECADO, usar FIX_V6_..." |

Accion: Agregar comentario `// DEPRECADO` mas visible. No eliminar.

---

## 3. ORDEN DE EJECUCION

```
Fase 1: FIX-V12 (Buffer Trim CR)         → v2.11.0  ← Mas simple, cero interacciones
   ↓ merge a main tras testing
Fase 2: FIX-V11 (ICCID NVS Cache)        → v2.12.0  ← Independiente, satisface FR-07/08
   ↓ merge a main tras testing
Fase 3: FIX-V13 (Consecutive Fail Recovery) → v2.13.0  ← Mas complejo, se beneficia de V11
   ↓ merge a main tras testing
Fase 4: Cleanup (deprecaciones)           → v2.13.1  ← Cosmetic, sin cambio funcional
```

**Principio rector:** Cada fase es un fix aislado en su branch, con su testing, su merge, y su version. No se avanza a la siguiente fase hasta que la anterior este validada y mergeada.

---

## 4. FASE 1: FIX-V12 — Buffer Trim CR → v2.11.0

**Riesgo:** BAJO | **Complejidad:** BAJA | **Requisito:** FR-26

### Por que primero

- Cambio mas simple del lote (agregar `.trim()` en 5 ubicaciones)
- Cero interaccion con otros fixes (solo toca BUFFERModule.cpp)
- Cero riesgo de degradacion (`.trim()` en Base64 es no-op si no hay whitespace)
- Satisface requisito incumplido inmediatamente

### Branch

```bash
git checkout main
git checkout -b fix-v12/buffer-trim-cr
```

### Archivos a modificar (3)

| Archivo | Cambio |
|---------|--------|
| `src/FeatureFlags.h` | Agregar `ENABLE_FIX_V12_BUFFER_TRIM_CR` despues de FIX-V9 |
| `src/data_buffer/BUFFERModule.cpp` | Agregar `.trim()` en 5 ubicaciones |
| `src/version_info.h` | Actualizar a v2.11.0 |

### Cambio en FeatureFlags.h

```cpp
/**
 * FIX-V12: Buffer Trim CR
 * Sistema: Buffer / LittleFS
 * Archivo: src/data_buffer/BUFFERModule.cpp
 * Descripcion: Agrega .trim() despues de readStringUntil('\n') para
 *              eliminar \r residual de tramas leidas del buffer.
 * Requisito: FR-26 (eliminar caracteres de control residuales)
 * Documentacion: fixs-feats/fixs/FIX_V12_BUFFER_TRIM_CR.md
 * Estado: Implementado
 */
#define ENABLE_FIX_V12_BUFFER_TRIM_CR    1
```

Agregar a `printActiveFlags()`:
```cpp
#if ENABLE_FIX_V12_BUFFER_TRIM_CR
Serial.println(F("  [X] FIX-V12: Buffer Trim CR"));
#else
Serial.println(F("  [ ] FIX-V12: Buffer Trim CR"));
#endif
```

### Cambio en BUFFERModule.cpp

Patron a aplicar en las **5 ubicaciones** (lineas 53, 110, 137, 185, 233):

```cpp
// ANTES:
lines[count] = file.readStringUntil('\n');

// DESPUES:
lines[count] = file.readStringUntil('\n');
#if ENABLE_FIX_V12_BUFFER_TRIM_CR
lines[count].trim();  // FIX-V12: eliminar \r residual (FR-26)
#endif
```

**Nota:** Cada ubicacion tiene variable diferente (`lines[count]`, `line`, `allLines[totalLines]`). Adaptar el nombre de variable en cada caso.

### Testing Fase 1

| Capa | Criterio | Tiempo |
|------|----------|--------|
| 1. Compilacion | 0 errores, 0 warnings | 2 min |
| 2. Verificacion | Hex dump de trama enviada: sin 0x0D | 5 min |
| 3. Hardware 1 ciclo | Trama enviada y aceptada por servidor | 20 min |
| 4. Hardware 24h | 0 tramas rechazadas | 24h |

### Criterio de avance

- [ ] Compila con flag = 1
- [ ] Compila con flag = 0
- [ ] Tramas limpias (sin \r)
- [ ] Servidor acepta tramas
- [ ] 0 reinicios inesperados en 24h
- [ ] Merge a main

### Commit

```bash
git commit -m "fix(FIX-V12): agregar .trim() en lecturas de buffer LittleFS

- Agrega ENABLE_FIX_V12_BUFFER_TRIM_CR en FeatureFlags.h
- .trim() en 5 ubicaciones de readStringUntil() en BUFFERModule.cpp
- Satisface FR-26: eliminar caracteres de control residuales
- Rollback: flag a 0 + recompilar

Refs: fixs-feats/fixs/FIX_V12_BUFFER_TRIM_CR.md"
```

---

## 5. FASE 2: FIX-V11 — ICCID NVS Cache → v2.12.0

**Riesgo:** BAJO | **Complejidad:** MEDIA | **Requisitos:** FR-07, FR-08

### Por que segundo

- Independiente de FIX-V12
- Satisface 2 requisitos de alta prioridad nunca implementados
- Simplifica el flujo para FIX-V13 (unico punto de modem pasa a ser SendLTE)

### Branch

```bash
git checkout main
git checkout -b fix-v11/iccid-nvs-cache
```

### Archivos a modificar (3)

| Archivo | Cambio |
|---------|--------|
| `src/FeatureFlags.h` | Agregar `ENABLE_FIX_V11_ICCID_NVS_CACHE` |
| `AppController.cpp` | Reescribir Cycle_GetICCID con logica de cache |
| `src/version_info.h` | Actualizar a v2.12.0 |

### Cambio en FeatureFlags.h

```cpp
/**
 * FIX-V11: ICCID NVS Cache con fallback
 * Sistema: AppController / NVS
 * Archivo: AppController.cpp
 * Descripcion: Cache ICCID en NVS "sensores/iccid" tras lectura exitosa.
 *              Si cache existe, no enciende modem para ICCID.
 *              Si cache vacio (primer boot), lee del modem y cachea.
 * Requisitos: FR-07 (almacenar ICCID en NVS), FR-08 (recuperar de NVS si falla)
 * Ahorro: Elimina powerOn+powerOff de ICCID en ciclos subsecuentes (~15-43s).
 * Documentacion: fixs-feats/fixs/FIX_V11_ICCID_NVS_CACHE.md
 * Estado: Implementado
 */
#define ENABLE_FIX_V11_ICCID_NVS_CACHE    1
```

### Cambio en AppController.cpp — Cycle_GetICCID

Principio: **preservar codigo original en #else** (premisa P6).

```cpp
case AppState::Cycle_GetICCID: {
  TIMING_START(g_timing, iccid);

  #if DEBUG_MOCK_ICCID
  // (mock existente sin cambios)
  {
    unsigned long mockStart = millis();
    g_iccid = "89520000000000000000";
    Serial.printf("[MOCK][ICCID] %s (%lums)\n", g_iccid.c_str(), millis() - mockStart);
  }
  #else

  #if ENABLE_FIX_V11_ICCID_NVS_CACHE
  // ============ [FIX-V11 START] ICCID NVS Cache (FR-07, FR-08) ============
  bool gotCachedIccid = false;

  // 1. Intentar leer de NVS cache primero
  preferences.begin("sensores", true);  // read-only
  if (preferences.isKey("iccid")) {
    g_iccid = preferences.getString("iccid", "");
    if (g_iccid.length() > 0) {
      gotCachedIccid = true;
      Serial.print("[FIX-V11] ICCID de NVS cache: ");
      Serial.println(g_iccid);
    }
  }
  preferences.end();

  // 2. Solo encender modem si no hay cache
  if (!gotCachedIccid) {
    Serial.println("[FIX-V11] Sin cache ICCID, intentando modem...");
    if (lte.powerOn()) {
      g_iccid = lte.getICCID();
      #if ENABLE_FIX_V8_ICCID_FAIL_LOGGING && ENABLE_FEAT_V7_PRODUCTION_DIAG
      if (g_iccid.length() == 0) {
        Serial.println(F("[WARN][APP] ICCID vacio - fallo AT (timeout o SIM)"));
        ProdDiag::recordATTimeout();
      }
      #endif
      lte.powerOff();

      // 3. Cachear si se obtuvo
      if (g_iccid.length() > 0) {
        preferences.begin("sensores", false);  // read-write
        preferences.putString("iccid", g_iccid);
        preferences.end();
        Serial.println("[FIX-V11] ICCID leido del modem y cacheado en NVS");
      }
    } else {
      g_iccid = "";
      #if ENABLE_FIX_V8_ICCID_FAIL_LOGGING && ENABLE_FEAT_V7_PRODUCTION_DIAG
      Serial.println(F("[WARN][APP] powerOn fallo - fallo AT durante ICCID"));
      ProdDiag::recordATTimeout();
      #endif
    }
  }
  // ============ [FIX-V11 END] ============

  #else
  // Codigo original (v2.10.1): powerOn + getICCID + powerOff cada ciclo
  if (lte.powerOn()) {
    g_iccid = lte.getICCID();
    #if ENABLE_FIX_V8_ICCID_FAIL_LOGGING && ENABLE_FEAT_V7_PRODUCTION_DIAG
    if (g_iccid.length() == 0) {
      Serial.println(F("[WARN][APP] ICCID vacio - fallo AT (timeout o SIM)"));
      ProdDiag::recordATTimeout();
    }
    #endif
    lte.powerOff();
  } else {
    g_iccid = "";
    #if ENABLE_FIX_V8_ICCID_FAIL_LOGGING && ENABLE_FEAT_V7_PRODUCTION_DIAG
    Serial.println(F("[WARN][APP] powerOn fallo - fallo AT durante ICCID"));
    ProdDiag::recordATTimeout();
    #endif
  }
  #endif // ENABLE_FIX_V11_ICCID_NVS_CACHE

  #endif // DEBUG_MOCK_ICCID

  TIMING_END(g_timing, iccid);
  g_state = AppState::Cycle_BuildFrame;
  break;
}
```

### NVS Storage

| Namespace | Key | Tipo | Persistencia |
|-----------|-----|------|-------------|
| `sensores` | `iccid` | String (19-22 chars) | Sobrevive deep sleep, power cycle, esp_restart() |

### Testing Fase 2

| Capa | Criterio | Tiempo |
|------|----------|--------|
| 1. Compilacion | 0 errores con flag=1 y flag=0 | 4 min |
| 2. Primer boot | Log: "[FIX-V11] ICCID leido del modem y cacheado en NVS" | 5 min |
| 3. Ciclo subsecuente | Log: "[FIX-V11] ICCID de NVS cache: 8952..." | 20 min |
| 4. Timing ICCID | CYCLE TIMING iccid: < 10 ms (cache) vs ~43s (original) | 5 min |
| 5. Hardware 24h | ICCID correcto en todas las tramas enviadas | 24h |

### Criterio de avance

- [ ] Compila con flag = 1 y flag = 0
- [ ] Primer boot: ICCID leido del modem y cacheado
- [ ] Ciclos subsecuentes: ICCID de cache (< 10ms)
- [ ] Trama incluye ICCID correcto
- [ ] 0 reinicios inesperados en 24h
- [ ] Merge a main

### Commit

```bash
git commit -m "fix(FIX-V11): cache ICCID en NVS con fallback a modem

- Agrega ENABLE_FIX_V11_ICCID_NVS_CACHE en FeatureFlags.h
- Reescribe Cycle_GetICCID: NVS primero, modem solo si cache vacio
- Preserva codigo original en #else (rollback inmediato)
- Satisface FR-07 (persistir ICCID) y FR-08 (recuperar de NVS si falla)
- Elimina powerOn extra solo para ICCID (~15-43s ahorro por ciclo)

Refs: fixs-feats/fixs/FIX_V11_ICCID_NVS_CACHE.md"
```

---

## 6. FASE 3: FIX-V13 — Consecutive Fail Recovery → v2.13.0

**Riesgo:** MEDIO | **Complejidad:** ALTA | **Requisito:** NFR-12

### Por que tercero

- El mas complejo (RTC variables, NVS namespace, esp_restart)
- Se beneficia de FIX-V11 ya presente:
  - Con V11, el unico punto de modem en ciclos normales es SendLTE
  - El tracking de fallos es limpio: `sendBufferOverLTE` retorna bool
- Requiere testing mas exhaustivo (simular 6 fallos → restart → backoff)

### Branch

```bash
git checkout main
git checkout -b fix-v13/consec-fail-recovery
```

### Archivos a modificar (6)

| Archivo | Cambio |
|---------|--------|
| `JAMR_4.5.ino` | RTC variables + pasar en AppConfig |
| `AppController.h` | Extender AppConfig con modemConsecFails |
| `AppController.cpp` | Recovery en AppInit + tracking en Cycle_SendLTE + cambiar `(void)` a `bool` |
| `src/FeatureFlags.h` | Agregar flag + parametros threshold/backoff |
| `src/data_lte/LTEModule.cpp` | Reset `s_recoveryAttempts` en sw restart (interaccion FIX-V7) |
| `src/version_info.h` | Actualizar a v2.13.0 |

### Cambio en FeatureFlags.h

```cpp
/**
 * FIX-V13: Consecutive Fail Recovery
 * Sistema: AppController / NVS / RTC
 * Archivo: JAMR_4.5.ino, AppController.h, AppController.cpp
 * Descripcion: Tracking de fallos LTE entre ciclos (RTC_DATA_ATTR).
 *              Tras N fallos consecutivos: esp_restart() como recovery.
 *              Post-restart: backoff M ciclos sin modem (solo sensores).
 *              Reset contador a 0 si TX exitoso.
 * Requisito: NFR-12 (recovery automatica de estados zombie)
 * Documentacion: fixs-feats/fixs/FIX_V13_CONSEC_FAIL_RECOVERY.md
 * Estado: Implementado
 */
#define ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY   1
#define FIX_V13_FAIL_THRESHOLD                6   // fallos antes de esp_restart()
#define FIX_V13_BACKOFF_CYCLES                3   // ciclos sin modem post-restart
```

### Cambio en JAMR_4.5.ino

```cpp
#include "AppController.h"

// FIX-V13: Variables persistentes en RTC memory (sobreviven deep sleep)
#if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
RTC_DATA_ATTR uint8_t modemConsecFails = 0;
#endif

void setup() {
  AppConfig cfg;
  cfg.sleep_time_us = TIEMPO_SLEEP_ACTIVO;

  #if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
  cfg.modemConsecFails = modemConsecFails;
  #endif

  AppInit(cfg);
}
```

### Cambio en AppController.h

```cpp
struct AppConfig {
  uint64_t sleep_time_us = 10ULL * 60ULL * 1000000ULL;
  #if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
  uint8_t modemConsecFails = 0;
  #endif
};
```

### Cambio en AppController.cpp — AppInit() (recovery logic)

Agregar despues de `printActiveFlags()`:

```cpp
// ============ [FIX-V13 START] Recovery por fallos consecutivos ============
#if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
{
  static RTC_DATA_ATTR uint8_t modemSkipCycles = 0;

  // 1. Detectar si venimos de un restart de recovery
  preferences.begin("modem_rcv", false);
  bool postRestart = preferences.getBool("postRst", false);
  if (postRestart) {
    modemSkipCycles = preferences.getUChar("skipCyc", FIX_V13_BACKOFF_CYCLES);
    preferences.putBool("postRst", false);  // Limpiar flag
    preferences.end();
    Serial.print("[FIX-V13] Post-restart recovery: backoff ");
    Serial.print(modemSkipCycles);
    Serial.println(" ciclos");
  } else {
    preferences.end();
  }

  // 2. Evaluar estado
  if (modemSkipCycles > 0) {
    modemSkipCycles--;
    g_skipModemThisCycle = true;  // Variable que Cycle_GetICCID y Cycle_SendLTE consultan
    Serial.print("[FIX-V13] Backoff activo, skip modem. Restantes: ");
    Serial.println(modemSkipCycles);
  } else if (g_cfg.modemConsecFails >= FIX_V13_FAIL_THRESHOLD) {
    Serial.print("[FIX-V13] Threshold alcanzado (");
    Serial.print(g_cfg.modemConsecFails);
    Serial.println(" fallos). Reiniciando...");
    preferences.begin("modem_rcv", false);
    preferences.putBool("postRst", true);
    preferences.putUChar("skipCyc", FIX_V13_BACKOFF_CYCLES);
    preferences.end();
    // Reset contador para proximo intento
    extern uint8_t modemConsecFails;
    modemConsecFails = 0;
    delay(100);
    esp_restart();
    // No retorna
  }

  Serial.print("[FIX-V13] modemConsecFails=");
  Serial.print(g_cfg.modemConsecFails);
  Serial.print(" skipCycles=");
  Serial.println(modemSkipCycles);
}
#endif
// ============ [FIX-V13 END] ============
```

### Cambio en AppController.cpp — Variable global

```cpp
#if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
/** @brief Si true, skip operaciones de modem este ciclo (backoff FIX-V13) */
static bool g_skipModemThisCycle = false;
#endif
```

### Cambio en AppController.cpp — Cycle_GetICCID (backoff guard)

Con FIX-V11 activo, ICCID usa cache y no toca modem. Pero si cache esta vacio Y estamos en backoff:

```cpp
// Dentro del bloque FIX-V11, antes de intentar modem:
if (!gotCachedIccid) {
  #if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
  if (g_skipModemThisCycle) {
    Serial.println("[FIX-V13] Backoff activo: skip modem para ICCID");
    g_iccid = "";  // Trama degradada pero datos en buffer
  } else {
  #endif
    // ... codigo de powerOn + getICCID + powerOff ...
  #if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
  }
  #endif
}
```

### Cambio en AppController.cpp — Cycle_SendLTE (tracking)

```cpp
case AppState::Cycle_SendLTE: {
  TIMING_START(g_timing, sendLte);

  #if DEBUG_MOCK_LTE
  // (mock existente sin cambios)
  #else

  #if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
  if (g_skipModemThisCycle) {
    Serial.println("[FIX-V13] Backoff activo: skip LTE. Datos en buffer.");
  } else {
  #endif
    Serial.println("[DEBUG][APP] Iniciando envio por LTE...");
    bool lteOk = sendBufferOverLTE_AndMarkProcessed();
    Serial.println("[DEBUG][APP] Envio completado, pasando a CompactBuffer");

    // ============ [FIX-V13 START] Tracking de fallos ============
    #if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
    {
      extern uint8_t modemConsecFails;
      if (lteOk) {
        if (modemConsecFails > 0) {
          Serial.print("[FIX-V13] TX exitoso, reset contador (era ");
          Serial.print(modemConsecFails);
          Serial.println(")");
        }
        modemConsecFails = 0;
      } else {
        modemConsecFails++;
        Serial.print("[FIX-V13] TX fallo. Fails consecutivos: ");
        Serial.print(modemConsecFails);
        Serial.print("/");
        Serial.println(FIX_V13_FAIL_THRESHOLD);
      }
    }
    #endif
    // ============ [FIX-V13 END] ============
  #if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
  }
  #endif

  #endif // DEBUG_MOCK_LTE

  TIMING_END(g_timing, sendLte);
  g_state = AppState::Cycle_CompactBuffer;
  break;
}
```

### Cambio en LTEModule.cpp — Reset s_recoveryAttempts en sw restart

**BUG CORREGIDO:** `s_recoveryAttempts` es `static RTC_DATA_ATTR` (linea 189). Persiste en `esp_restart()` porque RTC memory sobrevive software reset. Sin esta correccion, el `esp_restart()` de FIX-V13 no da oportunidad real de recovery: FIX-V7 entra en backoff inmediato porque el contador ya esta en 1.

Agregar despues de `static RTC_DATA_ATTR uint8_t s_recoveryAttempts = 0;` (linea 189):

```cpp
    #if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY
    // FIX-V13: Tras esp_restart(), permitir nuevo intento de recovery
    if (esp_reset_reason() == ESP_RST_SW) {
        s_recoveryAttempts = 0;
    }
    #endif
```

Esto permite que tras `esp_restart()` (FIX-V13 o FEAT-V4), FIX-V7 tenga un intento limpio de recovery con reset forzado 12.6s.

### Interaccion con FIX-V7 (Zombie Mitigation)

| Aspecto | FIX-V7 | FIX-V13 |
|---------|--------|---------|
| Alcance | Dentro de `powerOn()` | Entre ciclos |
| Tracking | `s_recoveryAttempts` por boot | `modemConsecFails` en RTC |
| Accion | Limita retries dentro de powerOn | esp_restart() + backoff |
| Reset | Power-on real, o sw restart (con fix) | TX exitoso |

Son complementarios: FIX-V7 intenta recovery DENTRO de un ciclo. Si falla, FIX-V13 acumula el fallo. Tras 6 ciclos fallidos, FIX-V13 hace `esp_restart()` que resetea `s_recoveryAttempts` → FIX-V7 tiene nuevo intento completo con reset forzado 12.6s.

### Nota sobre falsos positivos en tracking

`sendBufferOverLTE_AndMarkProcessed()` retorna `false` tanto por modem muerto como por fallo de red (sin cobertura, servidor caido). FIX-V13 cuenta ambos como fallo.

**Decision:** Aceptar falsos positivos. Razon:
- Costo de `esp_restart()` innecesario: ~10s de delay, datos preservados
- Costo de no detectar zombie real: datos sin enviar indefinidamente
- En un device sin autoswitch, ser conservador es preferible

### NVS Storage

| Namespace | Key | Tipo | Uso |
|-----------|-----|------|-----|
| `modem_rcv` | `postRst` | bool | Flag: venimos de restart de recovery |
| `modem_rcv` | `skipCyc` | uint8 | Ciclos de backoff pendientes |

### Testing Fase 3

| Capa | Criterio | Tiempo |
|------|----------|--------|
| 1. Compilacion | 0 errores con flag=1 y flag=0 | 4 min |
| 2. TX exitoso | modemConsecFails se resetea a 0 | 20 min |
| 3. Simular fallo | Desconectar antena, verificar contador incrementa | 1h |
| 4. Threshold | Tras 6 fallos: log "[FIX-V13] Threshold alcanzado", esp_restart() | 2h |
| 5. Post-restart | Log: "[FIX-V13] Post-restart recovery: backoff 3 ciclos" | 30 min |
| 6. Fin backoff | Ciclo 4 post-restart: intenta modem normalmente | 1h |
| 7. Estabilidad 24h | Sin loops, sin degradacion | 24h |

### Criterio de avance

- [ ] Compila con flag = 1 y flag = 0
- [ ] TX exitoso resetea contador
- [ ] Contador incrementa en fallo
- [ ] esp_restart() se ejecuta en threshold
- [ ] Backoff funciona post-restart
- [ ] Sin boot loops (verificar con FEAT-V3 crash diagnostics)
- [ ] 0 reinicios inesperados en 24h (aparte del restart de recovery)
- [ ] Merge a main

### Commit

```bash
git commit -m "fix(FIX-V13): recovery por fallos consecutivos de modem

- Agrega ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY en FeatureFlags.h
- RTC_DATA_ATTR modemConsecFails: tracking entre deep sleeps
- Tras 6 fallos consecutivos: esp_restart() como recovery
- Post-restart: backoff 3 ciclos (solo sensores, datos en buffer)
- TX exitoso resetea contador a 0
- Preserva codigo original en #else
- Satisface NFR-12 (recovery automatica de estados zombie)

Refs: fixs-feats/fixs/FIX_V13_CONSEC_FAIL_RECOVERY.md"
```

---

## 7. FASE 4: Cleanup → v2.13.1

**Riesgo:** NULO | **Complejidad:** BAJA | Solo comentarios, sin cambio funcional.

### Branch

```bash
git checkout main
git checkout -b cleanup/deprecate-v5-v9
```

### Cambios en FeatureFlags.h

1. FIX-V5: Agregar `* Estado: DEPRECADO - ESP-IDF v5.x provee TWDT`
2. FIX-V9: Agregar `* Estado: DEPRECADO - Hardware no soporta (ver HALLAZGO_CRITICO_GPIO13)`
3. FIX-V4 params: Agregar `// DEPRECADO - parametros migrados a FIX-V6` mas visible

### Commit

```bash
git commit -m "docs: marcar FIX-V5 y FIX-V9 como deprecados en FeatureFlags.h

- FIX-V5: ESP-IDF v5.x ya provee TWDT
- FIX-V9: GPIO 13 no controla MIC2288 EN en hardware real
- FIX-V4 params: nota de deprecacion mas visible"
```

---

## 8. MAPA DE INTERACCIONES POST-IMPLEMENTACION

```
                    ┌──────────────────────────────────┐
                    │         CICLO FSM NORMAL         │
                    └──────────────────────────────────┘

Boot/Wakeup
    │
    ▼
┌─────────────┐
│ Cycle_Read  │  FIX-V3: Si vBat < 3.20V → skip a Sleep
│  Sensors    │  (sensores siempre leen, datos al buffer)
└─────┬───────┘
      │
      ▼
┌─────────────┐  Primer ciclo: gps.powerOn() via SIM7080G
│ Cycle_Gps   │  Ciclos subsecuentes: NVS cache (sin modem)
└─────┬───────┘
      │
      ▼
┌──────────────┐  FIX-V11: NVS cache primero (1ms)
│Cycle_GetICCID│  Solo modem si cache vacio (primer boot)
│              │  FIX-V13: Si backoff → skip modem
└─────┬────────┘
      │
      ▼
┌─────────────┐  Build + Buffer Write
│ Build/Write │  FIX-V12: tramas limpias sin \r
└─────┬───────┘
      │
      ▼
┌─────────────┐  FIX-V3: Bloqueado si vBat < 3.20V
│Cycle_SendLTE│  FIX-V13: Si backoff → skip, datos en buffer
│             │  FIX-V13: Si TX falla → modemConsecFails++
│             │  FIX-V13: Si TX OK → modemConsecFails = 0
│             │  (internamente: FIX-V7 zombie mitigation en powerOn)
└─────┬───────┘
      │
      ▼
┌─────────────┐  FIX-V4: lte.powerOff() antes de sleep
│ Cycle_Sleep │  FIX-V13: Si fails >= 6 → esp_restart() en proximo boot
│             │  FEAT-V4: Si acumulado >= 24h → esp_restart()
└─────────────┘
```

---

## 9. IMPACTO ESPERADO POR ESCENARIO

### Ciclo normal (modem OK, ciclo subsecuente)

| Fase | v2.10.1 | v2.13.0 | Cambio |
|------|---------|---------|--------|
| Sensors | 22s | 22s | = |
| GPS | 0s (NVS) | 0s (NVS) | = |
| ICCID | **43s** (powerOn+Off) | **<10ms** (NVS cache) | **-43s** |
| Build+Buffer | 0s | 0s | = |
| LTE | 43s | 43s | = |
| **TOTAL** | **~108s** | **~65s** | **-43s** |

### Ciclo con modem zombie (ciclo subsecuente)

| Fase | v2.10.1 | v2.13.0 | Cambio |
|------|---------|---------|--------|
| Sensors | 22s | 22s | = |
| GPS | 0s (NVS) | 0s (NVS) | = |
| ICCID | **43s** (powerOn fail) | **<10ms** (NVS cache) | **-43s** |
| Build+Buffer | 0s | 0s | = |
| LTE | **43s** (powerOn fail) | **43s** (powerOn fail) | = |
| **TOTAL** | **~108s** | **~65s** | **-43s** |
| **+ FIX-V13 tras 6 fallos** | sigue igual forever | **esp_restart + 3 ciclos backoff** | **Recovery** |

### Ciclo en backoff (post-restart FIX-V13)

| Fase | Tiempo | Nota |
|------|--------|------|
| Sensors | 22s | Solo sensores |
| GPS | 0s | NVS cache |
| ICCID | <10ms | NVS cache |
| Build+Buffer | 0s | Datos guardados |
| LTE | <1ms | Skip (backoff) |
| **TOTAL** | **~23s** | Maxima conservacion bateria |

### Zombie tipo B persistente (SIN AUTOSWITCH — peor caso)

El modem esta en latch-up electrico. Ningun software lo recupera.

```
Ciclo 1-6:    Sensors OK → ICCID cache → LTE fail (43s) → modemConsecFails++
Ciclo 7:      esp_restart() (FIX-V13)
Ciclo 8-10:   Backoff (solo sensores, ~23s cada uno)
Ciclo 11-16:  LTE fail de nuevo (tipo B persiste) → modemConsecFails++
Ciclo 17:     esp_restart() (FIX-V13)
...           (ciclo se repite indefinidamente)
```

**Comportamiento:**
- Sensores: **100% operativos** — datos se acumulan en buffer (PRINC-01)
- Transmision: **0%** — modem irrecuperable sin power cycle fisico
- Bateria: Conservada por backoff (~23s vs ~65s en ciclos normales)
- Buffer: Acumula tramas, ~50 tramas max (FR-25), las mas antiguas se preservan

**Recovery:** Solo por intervencion fisica (desconectar bateria/panel 10s).
Tras power cycle: todas las tramas del buffer se envian en los primeros ciclos.

**Monitoreo remoto:** Si el servidor no recibe datos por >1h, alertar al operador.
Esto NO es un fix de firmware — es un procedimiento operativo externo.

---

## 10. TRAZABILIDAD A REQUISITOS

| Requisito | Descripcion | Fix que lo satisface | Estado |
|-----------|-------------|---------------------|--------|
| FR-07 | ICCID almacenado en NVS | **FIX-V11** | Pendiente (Fase 2) |
| FR-08 | Recuperar ICCID de NVS si falla | **FIX-V11** | Pendiente (Fase 2) |
| FR-26 | Eliminar \r residual de buffer | **FIX-V12** | Pendiente (Fase 1) |
| NFR-12 | Recovery automatica de zombie | **FIX-V13** + FIX-V7 | Pendiente (Fase 3) |

**Actualizar** REQUISITOS_FIRMWARE.md seccion 8 (Trazabilidad) despues de merge de cada fase.

---

## 11. ROLLBACK

| Nivel | Accion | Tiempo |
|-------|--------|--------|
| Por fix individual | Flag a 0 en FeatureFlags.h + recompilar | < 5 min |
| Version anterior | `git checkout v2.12.0` (o v2.11.0, v2.10.1) + recompilar | < 10 min |
| Factory | `esptool.py erase_flash` + flash v2.10.1 | < 20 min |

---

*Fin del documento*
