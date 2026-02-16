# FIX-V10: Modem Gate por Ciclo — DESCARTADO

---

## INFORMACION GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V10 |
| **Tipo** | Fix (Optimizacion de ciclo) |
| **Sistema** | AppController / FSM |
| **Archivo Principal** | `AppController.cpp` |
| **Estado** | **DESCARTADO** |
| **Fecha Identificacion** | 2026-02-16 |
| **Fecha Descarte** | 2026-02-16 |
| **Version Target** | ~~v3.0.0~~ N/A |
| **Branch** | N/A |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Prioridad** | ~~ALTA~~ N/A |

---

## RAZON DE DESCARTE

Con FIX-V11 (ICCID NVS Cache) activo, el beneficio de FIX-V10 se reduce a **0.7% de los ciclos** (1 de 144 por dia):

- GPS solo usa modem en el primer ciclo despues de boot (1/144)
- ICCID usa cache NVS en ciclos subsecuentes (no toca modem)
- El unico punto de modem en ciclos normales es SendLTE
- No hay nada despues de SendLTE que saltar

**Complejidad (3 estados FSM modificados) no justificada para beneficio marginal.**

Analisis:
```
Ciclos subsecuentes CON FIX-V11 (143 de 144 por dia):
  Sensors -> GPS de NVS (no modem) -> ICCID de cache (no modem) -> SendLTE (modem)
                                                                    unico punto
  FIX-V10 no tiene nada que saltar despues de SendLTE -> beneficio = 0
```

**Referencia:** flujos-de-trabajo/FLUJO_UPGRADE_V2.11_A_V2.13.md, seccion 2.2

---

## CUMPLIMIENTO DE PREMISAS

| Premisa | Descripcion | Cumplimiento | Evidencia |
|---------|-------------|--------------|-----------|
| **P1** | Aislamiento total | Si | Branch dedicado |
| **P2** | Cambios minimos | Si | Solo AppController.cpp + FeatureFlags.h |
| **P3** | Defaults seguros | Si | Flag deshabilitado = comportamiento original |
| **P4** | Feature flags | Si | `ENABLE_FIX_V10_MODEM_GATE` con valor 1/0 |
| **P5** | Logging exhaustivo | Si | `[FIX-V10]` en cada decision |
| **P6** | No cambiar logica existente | Si | Solo agrega `if` antes de powerOn |
| **P7** | Testing gradual | Si | Plan de 5 capas |
| **P8** | Metricas objetivas | Si | Tabla antes/despues |
| **P9** | Rollback plan | Si | Flag a 0 + recompilar (<5 min) |

---

## DIAGNOSTICO

### Problema Identificado

Cuando el modem SIM7080G esta en estado zombie, el firmware intenta encenderlo **3 veces por separado** en un solo ciclo:

1. **Cycle_Gps**: `gps.powerOn()` falla tras 3 intentos PWRKEY (~40s)
2. **Cycle_GetICCID**: `lte.powerOn()` falla tras 3 intentos PWRKEY (~43s)
3. **Cycle_SendLTE**: `lte.powerOn()` falla tras 3 intentos PWRKEY (~43s)

**Total desperdiciado: ~126 segundos** en timeouts inútiles cuando el modem claramente no va a responder.

### Evidencia

Log de produccion (v2.7.1, datalogger en campo, 2026-02-16):

```
[WARN][GPS] Timeout esperando AT OK
[WARN][GPS] Intento 1 fallido, reintentando...
[WARN][GPS] Timeout esperando AT OK
[WARN][GPS] Intento 2 fallido, reintentando...
[WARN][GPS] Timeout esperando AT OK
[WARN][GPS] Intento 3 fallido, reintentando...
[ERROR][GPS] Fallo al encender el modulo despues de todos los intentos
[TIMING] gps: 40210 ms

Encendiendo SIM7080G...
Intento de encendido 1 de 3
PWRKEY toggled
Intento de encendido 2 de 3
PWRKEY toggled
Intento de encendido 3 de 3
PWRKEY toggled
Error: No se pudo encender el modulo
[TIMING] iccid: 43211 ms

Encendiendo SIM7080G...
Intento de encendido 1 de 3
PWRKEY toggled
Intento de encendido 2 de 3
PWRKEY toggled
Intento de encendido 3 de 3
PWRKEY toggled
Error: No se pudo encender el modulo
[TIMING] sendLte: 43200 ms
```

**Timing Summary:**
```
GPS:           40210 ms   ← modem muerto
ICCID:         43211 ms   ← modem muerto (repetido)
Send LTE:      43200 ms   ← modem muerto (repetido)
CYCLE TOTAL:  150982 ms   ← 126s desperdiciados
```

### Causa Raiz

GPS y LTE usan el **mismo modulo SIM7080G** (compartido por `SerialLTE`). Si `gps.powerOn()` no pudo encender el modulo, `lte.powerOn()` tampoco va a poder: es el mismo hardware. No hay razon para reintentar.

---

## EVALUACION

### Impacto Cuantificado

| Metrica | Actual (v2.10.1) | Esperado (FIX-V10) | Ahorro |
|---------|------------------|---------------------|--------|
| Tiempo ciclo con modem zombie | ~151s | ~63s | **88s** |
| Intentos PWRKEY por ciclo zombie | 9 (3+3+3) | 3 | **-6 intentos** |
| Bateria por ciclo zombie | ~42 mAh | ~18 mAh | **57%** |

### Impacto por Area

| Aspecto | Descripcion |
|---------|-------------|
| Energia | -57% consumo en ciclos con modem zombie |
| Tiempo | -88s por ciclo = mas ciclos en un dia con modem fallando |
| Desgaste | Menos intentos PWRKEY = menos estres mecanico en el pin |
| Buffer | Mas rapido al sleep = mas datos bufferizados por dia |

---

## SOLUCION

### Principio

> Si el modem no respondio a `powerOn()` en una fase del ciclo, no va a responder en las fases siguientes. Marcar como "no disponible" y saltar las fases restantes que dependen del modem.

### Feature Flag en FeatureFlags.h

```cpp
/**
 * FIX-V10: Modem Gate por ciclo
 * Sistema: AppController / FSM
 * Archivo: AppController.cpp
 * Descripcion: Si powerOn() falla en Cycle_Gps o Cycle_GetICCID,
 *              marca modem como no disponible y skip fases restantes.
 * Ahorro: ~88s por ciclo con modem zombie (de 151s a 63s).
 * Documentacion: fixs-feats/fixs/FIX_V10_MODEM_GATE.md
 * Estado: Propuesto
 */
#define ENABLE_FIX_V10_MODEM_GATE    1
```

### Variable de gate (AppController.cpp)

```cpp
#if ENABLE_FIX_V10_MODEM_GATE
/** @brief Gate: si false, skip todas las operaciones de modem este ciclo */
static bool g_modemAvailable = true;
#endif
```

**Ciclo de vida:**
- Se inicializa a `true` al inicio de cada ciclo
- Se pone a `false` si `gps.powerOn()` falla (Cycle_Gps)
- Se pone a `false` si `lte.powerOn()` falla (Cycle_GetICCID)
- Se consulta en Cycle_GetICCID y Cycle_SendLTE antes de intentar powerOn

### Puntos de aplicacion

| Estado FSM | Gate | Comportamiento si gate=false |
|------------|------|-------------------------------|
| Cycle_Gps | SETTER | Si `gps.powerOn()` falla → `g_modemAvailable = false` |
| Cycle_GetICCID | CHECK + SETTER | Si gate false → skip powerOn, usar NVS cache (FIX-V11) |
| Cycle_SendLTE | CHECK | Si gate false → skip LTE, datos permanecen en buffer |

### Codigo: Cycle_Gps (solo cambio relevante)

```cpp
// ANTES (original):
if (gps.powerOn()) {
    gotFix = gps.getCoordinatesAndShutdown(fix);
} else {
    Serial.println("[WARN][APP] No se obtuvo fix GPS, usando ceros");
}

// DESPUES (con FIX-V10):
if (gps.powerOn()) {
    gotFix = gps.getCoordinatesAndShutdown(fix);
} else {
    Serial.println("[WARN][APP] No se obtuvo fix GPS, usando ceros");
    #if ENABLE_FIX_V10_MODEM_GATE
    g_modemAvailable = false;
    Serial.println("[FIX-V10] Modem gate: marcado como no disponible");
    #endif
}
```

### Codigo: Cycle_GetICCID (envuelve logica existente)

```cpp
#if ENABLE_FIX_V10_MODEM_GATE
if (!g_modemAvailable) {
    Serial.println("[FIX-V10] Modem gate activo: skip powerOn para ICCID");
    // Fallback a NVS cache se maneja por FIX-V11
} else {
#endif
    // ... codigo original de powerOn + getICCID + powerOff ...
    // Si powerOn falla aqui tambien:
    #if ENABLE_FIX_V10_MODEM_GATE
    g_modemAvailable = false;
    #endif
#if ENABLE_FIX_V10_MODEM_GATE
}
#endif
```

### Codigo: Cycle_SendLTE (envuelve logica existente)

```cpp
#if ENABLE_FIX_V10_MODEM_GATE
if (!g_modemAvailable) {
    Serial.println("[FIX-V10] Modem gate activo: skip LTE. Datos en buffer.");
} else {
#endif
    // ... codigo original de sendBufferOverLTE_AndMarkProcessed() ...
#if ENABLE_FIX_V10_MODEM_GATE
}
#endif
```

---

## ANALISIS DE TIEMPOS

### Flujo con modem zombie SIN FIX-V10

```
Sensors:  22s   [OK]
GPS:      40s   [FAIL - modem muerto, 3 intentos PWRKEY]
ICCID:    43s   [FAIL - modem muerto, 3 intentos PWRKEY (repetido)]
Build:     0s   [OK]
Buffer:    0s   [OK]
LTE:      43s   [FAIL - modem muerto, 3 intentos PWRKEY (repetido)]
Compact:   0s   [OK]
TOTAL:   151s   (126s desperdiciados)
```

### Flujo con modem zombie CON FIX-V10

```
Sensors:  22s   [OK]
GPS:      40s   [FAIL - modem muerto, marca gate=false]
ICCID:     1ms  [SKIP - gate false, usa NVS cache]
Build:     0s   [OK]
Buffer:    0s   [OK]
LTE:       1ms  [SKIP - gate false, datos en buffer]
Compact:   0s   [OK]
TOTAL:    63s   (ahorro de 88s)
```

### Nota: Ciclos subsecuentes (no primer boot)

En ciclos subsecuentes, GPS no usa modem (lee NVS). El primer uso del modem es en Cycle_GetICCID. Si ICCID tiene cache (FIX-V11), el primer uso real es Cycle_SendLTE. En este caso, FIX-V10 no aporta ahorro adicional, pero FIX-V13 maneja el tracking entre ciclos.

---

## INTERACCION CON OTROS FIXES

| Fix | Interaccion con FIX-V10 |
|-----|-------------------------|
| FIX-V7 (Zombie Mitigation) | FIX-V7 actua DENTRO de `powerOn()`. FIX-V10 actua ENTRE estados FSM. Son complementarios. |
| FIX-V11 (ICCID Cache) | FIX-V10 skip ICCID powerOn. FIX-V11 provee el ICCID de cache. Trabajan juntos. |
| FIX-V13 (Consec Fail) | FIX-V10 reduce desperdicio en un ciclo. FIX-V13 detecta patron multi-ciclo. |
| FIX-V3 (Low Battery) | FIX-V3 bloquea LTE por bateria baja. FIX-V10 bloquea por modem muerto. Son independientes. |

---

## ARCHIVOS A MODIFICAR

| Archivo | Cambio | Lineas aprox |
|---------|--------|--------------|
| `src/FeatureFlags.h` | Agregar flag `ENABLE_FIX_V10_MODEM_GATE` | +10 |
| `AppController.cpp` | Variable `g_modemAvailable` + checks en 3 estados FSM | +25 |
| `src/version_info.h` | Actualizar version | +3 |

---

## CRITERIOS DE ACEPTACION

| # | Criterio | Verificacion |
|---|----------|--------------|
| 1 | Si GPS powerOn falla, log muestra "[FIX-V10] Modem gate: marcado como no disponible" | Log serial |
| 2 | Si gate=false, ICCID muestra "[FIX-V10] Modem gate activo: skip powerOn para ICCID" | Log serial |
| 3 | Si gate=false, LTE muestra "[FIX-V10] Modem gate activo: skip LTE" | Log serial |
| 4 | Datos permanecen en buffer cuando LTE es skipped | Revisar buffer.txt |
| 5 | Ciclo con modem zombie < 70s (vs 151s antes) | CYCLE TIMING SUMMARY |
| 6 | Flag a 0 restaura comportamiento original | Compilar y probar |
| 7 | Ciclo normal (modem OK) no se ve afectado | Log serial |

---

## ROLLBACK

```cpp
// En FeatureFlags.h cambiar:
#define ENABLE_FIX_V10_MODEM_GATE    0  // Cambiar a 0
// Recompilar y flashear
```

**Tiempo de rollback:** <5 minutos

---

## LIMITACIONES

| Limitacion | Razon | Mitigacion |
|------------|-------|------------|
| Solo protege dentro de un ciclo | El gate se resetea en cada boot/wakeup | FIX-V13 maneja tracking entre ciclos |
| GPS solo se lee en primer ciclo | En ciclos subsecuentes, modem no se usa para GPS | FIX-V11 maneja ICCID, FIX-V13 maneja LTE |
| No detecta modem que se cae mid-cycle | Gate solo se activa en powerOn failure | FIX-V7 maneja recovery dentro de powerOn |

---

## HISTORIAL

| Fecha | Accion | Version |
|-------|--------|---------|
| 2026-02-16 | Documentacion inicial basada en analisis de log de produccion | 1.0 |

---

*Fin del documento*
