# FIX-V14: Battery Modem Gate

---

## INFORMACION GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V14 |
| **Tipo** | Fix (Proteccion preventiva de hardware) |
| **Sistema** | Energia / Modem / AppController |
| **Archivo Principal** | `AppController.cpp` |
| **Estado** | Propuesto |
| **Fecha Identificacion** | 2026-02-16 |
| **Version Target** | v2.14.0 |
| **Depende de** | FEAT-V1 (FeatureFlags.h), FIX-V13 (`g_skipModemThisCycle`) |
| **Prioridad** | **ALTA** |

---

## CUMPLIMIENTO DE PREMISAS

| Premisa | Descripcion | Cumplimiento | Evidencia |
|---------|-------------|--------------|-----------|
| **P1** | Aislamiento total | Si | Branch dedicado |
| **P2** | Cambios minimos | Si | Solo FeatureFlags.h + AppController.cpp (~15 lineas) |
| **P3** | Defaults seguros | Si | Si flag=0, comportamiento original |
| **P4** | Feature flags | Si | `ENABLE_FIX_V14_BATTERY_MODEM_GATE` con valor 1/0 |
| **P5** | Logging exhaustivo | Si | `[FIX-V14]` en cada decision |
| **P6** | No cambiar logica existente | Si | Reutiliza `g_skipModemThisCycle` de FIX-V13 |
| **P7** | Testing gradual | Si | Verificable con fuente variable |
| **P8** | Metricas objetivas | Si | Tabla antes/despues |
| **P9** | Rollback plan | Si | Flag a 0 + recompilar (<5 min) |

---

## DIAGNOSTICO

### Problema Identificado

Cuando la bateria esta entre 3.20V y ~3.80V, el firmware opera en modo "normal" e intenta encender el modem SIM7080G. El modem consume picos de 1.5-2A durante transmision LTE. Bajo esta carga, la resistencia interna de la bateria causa una caida de voltaje (voltage sag) que puede llevar el voltaje real en VBAT del modem por debajo de su minimo de operacion (~3.3V), causando:

1. **Modem no responde** a comandos AT (timeout 40s)
2. **Modem entra en estado zombie** (no responde pero no se apaga)
3. **Ciclo desperdicia 80-126s** en timeouts de GPS + ICCID + LTE
4. **Fallos consecutivos** que eventualmente disparan FIX-V13 (esp_restart)
5. **esp_restart no resuelve nada** porque la causa raiz es el voltaje, no el software

### Hipotesis Fisica

```
V_modem_real = V_bateria_reposo - (I_pico x R_interna)

Ejemplo con bateria degradada (R_int = 300 mOhm):
- V_reposo = 3.75V
- I_pico = 1.5A (TX LTE)
- V_sag = 1.5A x 0.300 Ohm = 0.45V
- V_modem_real = 3.75V - 0.45V = 3.30V  <-- limite de operacion

Ejemplo con bateria nueva (R_int = 100 mOhm):
- V_reposo = 3.75V
- V_sag = 1.5A x 0.100 Ohm = 0.15V
- V_modem_real = 3.75V - 0.15V = 3.60V  <-- OK
```

La resistencia interna varia de ~100 mOhm (nueva) a ~500 mOhm (degradada). El umbral practico depende del estado de la bateria, pero **3.70V es un valor conservador** que protege incluso baterias con degradacion moderada.

### Evidencia

- **Logs de produccion**: Modem falla repetidamente en dispositivos con baterias degradadas
- **FIX-V13 se dispara**: 6 fallos consecutivos → esp_restart → 6 fallos mas → ciclo sin fin
- **FIX-V3 no actua**: Su umbral es 3.20V, demasiado bajo para prevenir este problema

### Causa Raiz

El firmware trata el modem como disponible en todo el rango 3.20V-4.20V. No hay evaluacion de "viabilidad energetica" del modem antes de intentar encenderlo. FIX-V3 protege contra dano a la bateria (3.20V), pero no protege contra fallos del modem por voltage sag (3.70V).

---

## EVALUACION

### Impacto Cuantificado

| Metrica | Sin FIX-V14 | Con FIX-V14 |
|---------|-------------|-------------|
| Ciclo con vBat=3.65V | 126-151s (timeouts) | ~23s (solo sensores) |
| Fallos consecutivos modem | 6+ (dispara FIX-V13) | 0 (modem no se intenta) |
| Energia por ciclo en zona 3.20-3.70V | ~18-42 mAh | ~2 mAh |
| Datos perdidos | Ninguno (buffer) | Ninguno (buffer) |
| Recuperacion de bateria | Lenta (modem drena) | Rapida (solo sensores) |

### Impacto por Area

| Aspecto | Descripcion |
|---------|-------------|
| Energia | Elimina pico de 1.5-2A cuando no hay voltaje suficiente |
| Datos | Sensores siguen leyendo, buffer acumula, envio cuando bateria recupere |
| Recovery | Evita cascada de fallos que disparan FIX-V13 innecesariamente |
| Hardware | Protege SIM7080G de operar fuera de rango de voltaje |

---

## RELACION CON FIX-V3

```
Voltaje    FIX-V14              FIX-V3              Estado
──────────────────────────────────────────────────────────
4.20V      -                    -                   Normal completo
  |
3.90V      EXIT (modem ON)      -                   Normal completo
  |        ↕ Histeresis 200mV
3.70V      ENTER (modem OFF)    -                   Sensores + buffer (sin modem)
  |
3.50V      Activo               -                   Sensores + buffer (sin modem)
  |
3.20V      Activo               ENTER (reposo)      Sensores + buffer (sin modem ni LTE)
  |
3.00V      Activo               Activo              Solo buffer (proteccion maxima)
```

**Diferencia real:**
- **FIX-V3**: Protege la bateria de dano (3.20V). Evalua en `Cycle_StoreBuffer` (despues de BuildFrame, antes de SendLTE). Bloquea solo LTE. GPS se ejecuta normalmente incluso en modo reposo FIX-V3 (no hay check de `g_restMode` en `Cycle_Gps`).
- **FIX-V14**: Protege el modem de voltage sag (3.70V). Evalua en `Cycle_ReadSensors` (antes de GPS/ICCID/LTE). Bloquea GPS + ICCID + LTE via `g_skipModemThisCycle`.

La diferencia funcional entre ambos es el **rango de voltaje**, no el bloqueo de GPS. En ambos casos los sensores siguen leyendo y el buffer acumula. FIX-V14 ahorra adicionalmente los ~40s de timeout de GPS al evitar `gps.powerOn()`.

**Compatibilidad:** Son complementarios. FIX-V14 actua primero (3.70V). Si la bateria sigue bajando, FIX-V3 entra a 3.20V con su propia logica de estabilidad. No hay conflicto porque FIX-V14 usa `g_skipModemThisCycle` (por ciclo, se resetea en cada wakeup) y FIX-V3 usa `g_restMode` (persistente RTC).

---

## MODELO DE ESTADOS

```
                    ┌─────────────────────┐
                    │  MODEM HABILITADO   │
                    │  g_modemGatedByBat  │
                    │     = false         │
                    └──────────┬──────────┘
                               │ vBat <= 3.70V
                               ▼
                    ┌─────────────────────┐
                    │  MODEM BLOQUEADO    │
                    │  g_modemGatedByBat  │
                    │     = true          │
                    │                     │
                    │  Sensores: SI       │
                    │  Buffer: SI         │
                    │  GPS: NO            │
                    │  ICCID: cache NVS   │
                    │  LTE: NO            │
                    └──────────┬──────────┘
                               │ vBat >= 3.90V
                               ▼
                    ┌─────────────────────┐
                    │  MODEM HABILITADO   │
                    │  g_modemGatedByBat  │
                    │     = false         │
                    │                     │
                    │  Envia buffer       │
                    │  acumulado          │
                    └─────────────────────┘
```

**Histeresis de 200mV** previene oscilacion en la frontera. Si la bateria esta a 3.72V, no se enciende y apaga el modem ciclo a ciclo — se mantiene apagado hasta que suba a 3.90V.

**RTC_DATA_ATTR** en `g_modemGatedByBat` asegura que el estado persiste entre ciclos de deep sleep.

---

## SOLUCION

### Feature Flag en FeatureFlags.h

```cpp
/**
 * FIX-V14: Battery Modem Gate
 * Sistema: Energia / Modem / AppController
 * Archivo: AppController.cpp
 * Descripcion: Deshabilita operaciones de modem (GPS, ICCID, LTE) cuando
 *   la bateria esta por debajo del umbral de operacion segura del modem.
 *   Sensores siguen leyendo y datos se acumulan en buffer.
 *   Modem se reactiva cuando bateria supera umbral de salida.
 *   Usa g_skipModemThisCycle de FIX-V13 como mecanismo de skip.
 * Histeresis: 200mV (3.70V off, 3.90V on)
 * Depende: FIX-V13 (g_skipModemThisCycle)
 * Documentacion: fixs-feats/fixs/FIX_V14_BATTERY_MODEM_GATE.md
 * Estado: Propuesto
 */
#define ENABLE_FIX_V14_BATTERY_MODEM_GATE     1

/** @brief Voltaje por debajo del cual se desactiva el modem (voltios) */
#define FIX_V14_VBAT_MODEM_OFF                3.70f

/** @brief Voltaje por encima del cual se reactiva el modem (voltios) */
#define FIX_V14_VBAT_MODEM_ON                 3.90f
```

### Variable persistente en AppController.cpp

```cpp
#if ENABLE_FIX_V14_BATTERY_MODEM_GATE
/** @brief Estado persistente: modem bloqueado por bateria baja */
RTC_DATA_ATTR static bool g_modemGatedByBat = false;
#endif
```

### Codigo: Cycle_ReadSensors — Evaluar vBat despues de leer sensores

Despues de leer el ADC (vBat ya disponible), y ANTES de la transicion a Cycle_Gps/Cycle_GetICCID:

```cpp
case AppState::Cycle_ReadSensors: {
    TIMING_START(g_timing, sensors);
    String vBat;
    if (!readADC_DiscardAndAverage(vBat)) vBat = "0";

    // ... (lecturas I2C, RS485 sin cambios) ...

    g_varStr[6] = vBat;
    TIMING_END(g_timing, sensors);

    // ============ [FIX-V14 START] Battery Modem Gate ============
    #if ENABLE_FIX_V14_BATTERY_MODEM_GATE
    {
        float vBatFiltered = readVBatFiltered();  // Reutiliza funcion de FIX-V3

        if (vBatFiltered <= FIX_V14_VBAT_MODEM_OFF) {
            g_modemGatedByBat = true;
        } else if (vBatFiltered >= FIX_V14_VBAT_MODEM_ON) {
            g_modemGatedByBat = false;
        }
        // Si esta entre OFF y ON: mantener estado anterior (histeresis)

        if (g_modemGatedByBat) {
            g_skipModemThisCycle = true;
            Serial.print(F("[FIX-V14] vBat="));
            Serial.print(vBatFiltered, 2);
            Serial.println(F("V: modem gate por bateria baja"));
        }
    }
    #endif
    // ============ [FIX-V14 END] ============

    if (g_firstCycleAfterBoot) {
        // ... (transicion a Cycle_Gps) ...
    } else {
        // ... (transicion a Cycle_GetICCID) ...
    }
    break;
}
```

### Dependencia de FIX-V3

`readVBatFiltered()` ya existe y esta implementada en FIX-V3. FIX-V14 la reutiliza. Si FIX-V3 esta deshabilitado (flag=0), la funcion no existiria. Opciones:

1. **Recomendado**: Extraer `readVBatFiltered()` fuera del guard de FIX-V3 para que sea disponible independientemente
2. **Alternativa**: Hacer FIX-V14 dependiente de FIX-V3 (ambos flags = 1)
3. **Alternativa**: Duplicar la funcion dentro del guard de FIX-V14

La opcion 1 es la mas limpia: `readVBatFiltered()` es util para cualquier logica de bateria.

### Dependencia de FIX-V13

`g_skipModemThisCycle` es una variable `static bool` declarada en AppController.cpp bajo el guard `ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY`. FIX-V14 reutiliza esta variable.

Si FIX-V13 esta deshabilitado, `g_skipModemThisCycle` no existe. Opciones:

1. **Recomendado**: Mover la declaracion de `g_skipModemThisCycle` fuera del guard de FIX-V13, protegida por un guard combinado:
```cpp
#if ENABLE_FIX_V13_CONSEC_FAIL_RECOVERY || ENABLE_FIX_V14_BATTERY_MODEM_GATE
static bool g_skipModemThisCycle = false;
#endif
```
2. **Alternativa**: Hacer FIX-V14 dependiente de FIX-V13 (ambos flags = 1)

La opcion 1 permite usar cada fix independientemente.

### Guard de compilacion

Si el ingeniero habilita FIX-V14=1 sin las dependencias, la compilacion debe fallar con mensaje claro:

```cpp
// En FeatureFlags.h, despues de las definiciones de FIX-V14:
#if ENABLE_FIX_V14_BATTERY_MODEM_GATE && !ENABLE_FIX_V3_LOW_BATTERY_MODE
  #error "FIX-V14 requiere readVBatFiltered(). Habilitar FIX-V3 o extraer la funcion."
#endif
```

Este guard se puede eliminar si se extrae `readVBatFiltered()` fuera del guard de FIX-V3.

### Semantica de g_skipModemThisCycle

`g_skipModemThisCycle` es **write-only-true** durante un ciclo. Nunca se pone a `false` mid-cycle. Se resetea a `false` implicitamente en cada wakeup (nuevo boot tras deep sleep reinicializa variables `static` no-RTC).

Multiples escritores pueden coexistir sin conflicto (OR implicito):
- FIX-V13: lo pone `true` en AppInit() si hay backoff activo
- FIX-V14: lo pone `true` en Cycle_ReadSensors si vBat esta bajo

Si ambos lo ponen `true`, el efecto es el mismo: skip modem. No hay escenario donde uno quiera liberarlo y el otro no, porque la decision se toma una vez por ciclo al inicio.

---

## OPTIMIZACION RECOMENDADA: g_vBatFilteredThisCycle

`readVBatFiltered()` toma ~550ms (10 muestras x 50ms). Actualmente se llamaria dos veces por ciclo:
1. FIX-V14 en `Cycle_ReadSensors`
2. FIX-V3 en `Cycle_StoreBuffer` (linea 1580)

**Total: ~1.1s de ADC sampling por ciclo.** No es grave pero es ineficiente.

**Optimizacion recomendada:** Leer una vez en `Cycle_ReadSensors`, guardar en variable de ciclo, reutilizar en FIX-V3:

```cpp
static float g_vBatFilteredThisCycle = 0.0f;

// En Cycle_ReadSensors (una sola lectura):
g_vBatFilteredThisCycle = readVBatFiltered();

// FIX-V14 usa g_vBatFilteredThisCycle directamente
// FIX-V3 en Cycle_StoreBuffer usa g_vBatFilteredThisCycle en vez de llamar readVBatFiltered()
```

Esta optimizacion es deseable pero fuera del scope minimo de FIX-V14. El ingeniero puede implementarla junto con FIX-V14 o como mejora posterior.

---

## INTERACCION CON OTROS FIXES

| Fix | Interaccion |
|-----|-------------|
| **FIX-V3** (Low Battery 3.20V) | FIX-V14 actua antes (3.70V). Son complementarios. FIX-V3 bloquea LTE, FIX-V14 bloquea GPS+ICCID+LTE |
| **FIX-V11** (ICCID Cache) | Durante gate, ICCID se lee del NVS cache (sin modem) |
| **FIX-V13** (Consec Fail Recovery) | FIX-V14 previene fallos que disparan FIX-V13. Comparten `g_skipModemThisCycle` |
| **FIX-V7** (Zombie Mitigation) | FIX-V14 evita encender modem cuando el voltaje causaria fallo, reduciendo zombies por voltage sag |
| **FEAT-V4** (Periodic Restart) | Sin interaccion directa |
| **FEAT-V7** (Production Diag) | Se podria agregar `ProdDiag::recordBatteryModemGate()` (futuro, fuera de scope) |

### Cascada de protecciones

```
vBat > 3.90V:  Operacion normal completa
vBat 3.70-3.90V:  Depende de estado anterior (histeresis FIX-V14)
vBat < 3.70V:  FIX-V14 bloquea modem, sensores siguen
vBat < 3.20V:  FIX-V3 entra en reposo (sensores + buffer, sin LTE)
vBat < 3.00V:  Brownout protection del ESP32 (hardware)
```

---

## VERIFICACION

### Output Esperado — Ciclo con vBat=3.65V

```
[INFO] Firmware: v2.14.0 (battery-modem-gate)
...
[INFO][APP] === CYCLE DATA SUMMARY ===
[FIX-V14] vBat=3.65V: modem gate por bateria baja
[INFO][APP] Ciclo subsecuente: recuperando coordenadas GPS de NVS
[FIX-V11] ICCID de NVS cache: 89520...
[INFO][APP] Trama guardada en buffer (sin envio LTE)
...
[INFO][APP] Deep sleep 10 minutos
```

### Output Esperado — Recuperacion a 3.92V

```
[INFO][APP] === CYCLE DATA SUMMARY ===
// FIX-V14 NO imprime nada (vBat >= 3.90V, gate liberado)
[INFO][APP] Primer ciclo: leyendo GPS
...
[INFO][APP] Envio LTE exitoso
[FIX-V13] Modem OK. Fails reseteado a 0.
```

### Criterios de Aceptacion

| # | Criterio | Verificacion |
|---|----------|--------------|
| 1 | vBat <= 3.70V: modem no se enciende | Log: "[FIX-V14] modem gate por bateria baja" |
| 2 | Sensores siguen leyendo | CYCLE DATA SUMMARY muestra valores de sensores |
| 3 | Datos se guardan en buffer | Buffer crece cada ciclo |
| 4 | vBat >= 3.90V: modem se reactiva | LTE exitoso, buffer se vacia |
| 5 | Histeresis funciona: vBat=3.75V no reactiva | Estado se mantiene (gate activo) |
| 6 | ICCID usa cache NVS durante gate | Log: "[FIX-V11] ICCID de NVS cache" |
| 7 | FIX-V13 no incrementa fails durante gate (modem no se intenta) | modemConsecFails conserva valor previo al gate, no crece |
| 8 | Flag a 0 restaura comportamiento original | Compilar y probar |
| 9 | FIX-V3 sigue funcionando independientemente | Bajar a 3.20V, reposo funciona |

---

## ARCHIVOS A MODIFICAR

| Archivo | Cambio | Lineas aprox |
|---------|--------|--------------|
| `src/FeatureFlags.h` | Flag + umbrales + printActiveFlags() | +12 |
| `AppController.cpp` | Variable RTC + evaluacion en Cycle_ReadSensors | +15 |
| `AppController.cpp` | Mover `g_skipModemThisCycle` a guard combinado | ~2 (modificar) |
| `AppController.cpp` | Mover `readVBatFiltered()` fuera de guard FIX-V3 | ~2 (modificar) |
| `src/version_info.h` | v2.14.0 | +3 |

**Total: ~30 lineas nuevas, ~4 lineas modificadas en 3 archivos**

---

## ROLLBACK

```cpp
// En FeatureFlags.h cambiar:
#define ENABLE_FIX_V14_BATTERY_MODEM_GATE    0  // Cambiar a 0
// Recompilar y flashear
```

**Tiempo de rollback:** <5 minutos

---

## LIMITACIONES

| Limitacion | Razon | Mitigacion |
|------------|-------|------------|
| Umbral 3.70V es teorico | Depende de R_interna de cada bateria | 200mV histeresis da margen |
| No mide R_interna real | Requeriria medicion bajo carga | Umbral conservador compensa |
| readVBatFiltered() toma ~550ms | 10 muestras x 50ms delay | Se llama 2 veces (FIX-V14 + FIX-V3). Ver optimizacion g_vBatFilteredThisCycle |
| GPS no se lee durante gate | Modem compartido GPS/LTE | Coordenadas de NVS (ultimas conocidas) |
| Buffer puede llenarse | Si bateria tarda dias en recuperar | Compact buffer libera espacio |

---

## NOTA SOBRE EL UMBRAL

El valor de 3.70V es conservador para una bateria con R_interna ~300 mOhm (degradacion moderada). En campo, el umbral optimo depende de:

- Estado de degradacion de la bateria (ciclos de carga)
- Temperatura ambiente (R_interna aumenta con frio)
- Capacidad del panel solar (velocidad de recuperacion)

Se recomienda **correlacionar logs de fallos de modem con voltaje de bateria** para ajustar el umbral en una version futura. Los parametros `FIX_V14_VBAT_MODEM_OFF` y `FIX_V14_VBAT_MODEM_ON` son configurables en FeatureFlags.h sin cambiar logica.

---

## HISTORIAL

| Fecha | Accion | Version |
|-------|--------|---------|
| 2026-02-16 | Documentacion inicial. Basado en analisis de voltage sag con bateria degradada. | 1.0 |
| 2026-02-16 | Correccion criterio #7 (modemConsecFails no se resetea, se conserva). Agregar #error guard. Documentar semantica write-only-true de g_skipModemThisCycle. Optimizacion g_vBatFilteredThisCycle. Correccion: FIX-V3 tampoco bloquea GPS. | 1.1 |

---

*Fin del documento*
