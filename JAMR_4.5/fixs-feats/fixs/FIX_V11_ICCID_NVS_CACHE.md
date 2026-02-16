# FIX-V11: ICCID NVS Cache con Fallback

---

## INFORMACION GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V11 |
| **Tipo** | Fix (Optimizacion de energia) |
| **Sistema** | AppController / NVS |
| **Archivo Principal** | `AppController.cpp` |
| **Estado** | Aprobado |
| **Fecha Identificacion** | 2026-02-16 |
| **Version Target** | v2.12.0 |
| **Branch** | `fix-v11/iccid-nvs-cache` |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Origen** | Portado de `jarm_4.5_autoswich` FIX-V14 |
| **Requisitos** | **FR-07** (almacenar ICCID en NVS), **FR-08** (recuperar de NVS si falla) |
| **Prioridad** | **ALTA** |
| **Flujo** | flujos-de-trabajo/FLUJO_UPGRADE_V2.11_A_V2.13.md — Fase 2 |

---

## CUMPLIMIENTO DE PREMISAS

| Premisa | Descripcion | Cumplimiento | Evidencia |
|---------|-------------|--------------|-----------|
| **P1** | Aislamiento total | Si | Branch dedicado |
| **P2** | Cambios minimos | Si | Solo Cycle_GetICCID en AppController.cpp |
| **P3** | Defaults seguros | Si | Si NVS falla, ICCID queda vacio (comportamiento original) |
| **P4** | Feature flags | Si | `ENABLE_FIX_V11_ICCID_NVS_CACHE` con valor 1/0 |
| **P5** | Logging exhaustivo | Si | `[FIX-V11]` en cada operacion cache |
| **P6** | No cambiar logica existente | Si | Codigo original preservado en `#else` |
| **P7** | Testing gradual | Si | Plan de 5 capas |
| **P8** | Metricas objetivas | Si | Tabla antes/despues |
| **P9** | Rollback plan | Si | Flag a 0 + recompilar (<5 min) |

---

## DIAGNOSTICO

### Problema Identificado

El firmware enciende el modem SIM7080G **solo para leer el ICCID** en cada ciclo:

```cpp
// Cycle_GetICCID actual (v2.10.1):
if (lte.powerOn()) {        // ← Enciende modem (~10s best case)
    g_iccid = lte.getICCID();
    lte.powerOff();          // ← Apaga modem (~5s)
}
// Total: ~15s con modem OK, ~43s con modem zombie
```

Luego, en Cycle_SendLTE, **vuelve a encender el modem** para transmitir.

El ICCID es el identificador unico de la tarjeta SIM. **No cambia nunca** a menos que se reemplace la SIM fisicamente. No hay razon para leerlo en cada ciclo.

### Evidencia

Log de produccion (v2.7.1):

```
[TIMING] iccid: 43211 ms   ← 43 segundos para leer un valor que no cambia
```

En `jarm_4.5_autoswich`, este problema ya se resolvio con FIX-V14 (ICCID NVS cache con fallback). JAMR_4.5 no tiene esta optimizacion.

### Causa Raiz

Diseno original no contemplo cachear el ICCID. Cada ciclo hace:
1. `lte.powerOn()` → consume 200mA, tarda 10-43s
2. `lte.getICCID()` → lee valor que no cambia
3. `lte.powerOff()` → consume mas tiempo
4. Luego `lte.powerOn()` otra vez para LTE → redundante

---

## EVALUACION

### Impacto Cuantificado

| Metrica | Actual (v2.10.1) | Esperado (FIX-V11) | Ahorro |
|---------|------------------|---------------------|--------|
| Tiempo ICCID (modem OK) | ~15s | ~1ms (NVS read) | **99.99%** |
| Tiempo ICCID (modem zombie) | ~43s | ~1ms (NVS read) | **99.99%** |
| PowerOn extras por ciclo | 1 (solo para ICCID) | 0 | **-100%** |
| Energia ICCID por ciclo | ~4.2 mAh | ~0.001 mAh | **99.98%** |
| Ciclos LTE por dia (10min) | 144 | 144 | Sin cambio |
| PowerOn totales por dia | 288 (144 ICCID + 144 LTE) | 144 (solo LTE) | **-50%** |

### Impacto por Area

| Aspecto | Descripcion |
|---------|-------------|
| Energia | Elimina 144 powerOn/powerOff diarios (~605 mAh/dia ahorro) |
| Desgaste modem | 50% menos ciclos de encendido/apagado |
| Tiempo ciclo | -15s por ciclo normal, -43s por ciclo zombie |
| Fiabilidad | Menos interacciones con modem = menos oportunidades de fallo |

---

## SOLUCION

### Principio

> El ICCID de la SIM no cambia. Leerlo una vez, cachearlo en NVS, y usarlo de ahi para siempre. Solo releer del modem si el cache esta vacio (primera vez o NVS borrado).

### Estrategia de dos niveles

```
Nivel 1: NVS cache (preferido)
  ↓ Si cache vacio
Nivel 2: Modem powerOn + getICCID (primer uso)
  ↓ Resultado
Guardar en NVS para futuros ciclos
```

### Feature Flag en FeatureFlags.h

```cpp
/**
 * FIX-V11: ICCID NVS Cache con fallback
 * Sistema: AppController / NVS
 * Archivo: AppController.cpp
 * Descripcion: Cache ICCID en NVS "sensores/iccid" tras primera lectura.
 *              Si modem falla, usa cache. Elimina powerOn extra solo para ICCID.
 * Ahorro: ~15-43s por ciclo (elimina powerOn+powerOff de ICCID).
 * Origen: Portado de jarm_4.5_autoswich FIX-V14
 * Documentacion: fixs-feats/fixs/FIX_V11_ICCID_NVS_CACHE.md
 * Estado: Propuesto
 */
#define ENABLE_FIX_V11_ICCID_NVS_CACHE    1
```

### Codigo: Cycle_GetICCID reescrito

Referencia: ver FLUJO_UPGRADE_V2.11_A_V2.13.md seccion 5 (Fase 2) para el codigo completo.

```cpp
case AppState::Cycle_GetICCID: {
    TIMING_START(g_timing, iccid);

    #if DEBUG_MOCK_ICCID
    // Mock existente sin cambios
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

| Namespace | Key | Tipo | Valor | Persistencia |
|-----------|-----|------|-------|--------------|
| `sensores` | `iccid` | String | "8952020..." (19-22 chars) | Sobrevive deep sleep, power cycle, esp_restart() |

---

## ESCENARIOS

### Escenario 1: Primer boot (NVS vacio)

```
1. NVS cache: vacio
2. lte.powerOn() → OK
3. lte.getICCID() → "89520200..."
4. Cache en NVS → OK
5. lte.powerOff()
Resultado: ICCID leido del modem, cacheado para futuros ciclos
```

### Escenario 2: Ciclo normal (cache existe)

```
1. NVS cache: "89520200..."
2. ICCID = cache (1ms)
3. No se enciende modem
Resultado: 15-43 segundos ahorrados
```

### Escenario 3: Modem zombie (cache existe)

```
1. NVS cache: "89520200..."
2. ICCID = cache (1ms)
3. No se intenta powerOn
Resultado: Sin impacto del zombie en ICCID
```

### Escenario 4: Modem zombie (sin cache)

```
1. NVS cache: vacio
2. lte.powerOn() → FAIL
3. ICCID = "" (vacio)
4. Trama se construye con ICCID vacio
Resultado: Trama degradada pero datos se conservan en buffer
```

### Escenario 5: SIM reemplazada

```
1. NVS cache: ICCID antiguo
2. ICCID se usa del cache (incorrecto temporalmente)
3. Cuando modem funciona normalmente, un ciclo futuro leera el nuevo ICCID
NOTA: Requiere borrar NVS manualmente o esperar primer boot limpio
```

---

## INTERACCION CON OTROS FIXES

| Fix | Interaccion |
|-----|-------------|
| FIX-V8 (ICCID Fail Logging) | Solo se registra fallo cuando se intenta powerOn (no cuando usa cache). |
| FIX-V13 (Consec Fail) | Reducir powerOns reduce oportunidades de fallo consecutivo. |
| FEAT-V7 (Production Diag) | AT timeouts solo se registran cuando se intenta powerOn. |

---

## ARCHIVOS A MODIFICAR

| Archivo | Cambio | Lineas aprox |
|---------|--------|--------------|
| `src/FeatureFlags.h` | Agregar flag `ENABLE_FIX_V11_ICCID_NVS_CACHE` | +10 |
| `AppController.cpp` | Reescribir Cycle_GetICCID con logica de cache | +30 |
| `src/version_info.h` | Actualizar version | +3 |

---

## CRITERIOS DE ACEPTACION

| # | Criterio | Verificacion |
|---|----------|--------------|
| 1 | Primera lectura: ICCID leido del modem y cacheado en NVS | Log: "[FIX-V11] ICCID leido y cacheado en NVS" |
| 2 | Ciclos subsecuentes: ICCID leido del cache, sin powerOn | Log: "[FIX-V11] ICCID de NVS cache: 8952..." |
| 3 | ICCID timing < 10ms cuando usa cache | CYCLE TIMING: iccid: 0-5 ms |
| 4 | Sin cache + modem OK: lee del modem y cachea | Log serial |
| 5 | Sin cache + modem zombie: ICCID vacio, trama degradada | Log serial |
| 6 | Flag a 0 restaura comportamiento original (powerOn cada ciclo) | Compilar y probar |
| 7 | Trama incluye ICCID correcto del cache | Verificar trama normal |

---

## ROLLBACK

```cpp
// En FeatureFlags.h cambiar:
#define ENABLE_FIX_V11_ICCID_NVS_CACHE    0  // Cambiar a 0
// Recompilar y flashear
```

**Tiempo de rollback:** <5 minutos

---

## LIMITACIONES

| Limitacion | Razon | Mitigacion |
|------------|-------|------------|
| Si SIM se reemplaza, cache tiene ICCID viejo | NVS persiste hasta borrado explicito | Borrar NVS manualmente o esperar power cycle limpio |
| Si NVS se corrompe, ICCID se pierde | Raro, NVS tiene wear leveling | Relectura automatica del modem |
| Primera lectura sigue requiriendo powerOn | Inevitable: hay que leer al menos una vez | Solo pasa una vez (primer boot) |

---

## HISTORIAL

| Fecha | Accion | Version |
|-------|--------|---------|
| 2026-02-16 | Documentacion inicial. Portado de jarm_4.5_autoswich FIX-V14 | 1.0 |
| 2026-02-16 | Eliminar refs FIX-V10 (descartado). Alinear codigo con FLUJO. Preservar original en #else. | 1.1 |

---

*Fin del documento*
