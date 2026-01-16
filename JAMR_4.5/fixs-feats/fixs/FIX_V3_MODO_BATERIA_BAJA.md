# FIX-V3: Modo Reposo por Baja Batería

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V3 |
| **Tipo** | Fix (Control de energía en firmware) |
| **Sistema** | Energía / LTE / Buffer |
| **Archivo Principal** | `AppController.cpp` |
| **Estado** | 📋 Documentado |
| **Fecha Identificación** | 2026-01-15 |
| **Versión Target** | v2.3.0 |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Prioridad** | **Crítica** |
| **Requisitos** | RF-06, RF-09 |
| **Premisas** | P1✅ P2✅ P3✅ P4✅ P5✅ P6✅ |

---

## 🎯 OBJETIVO

> **Proteger la batería evitando que caiga a zona crítica y permitir recuperación hasta nivel seguro antes de reintentar operación completa.**

El sistema debe entrar en **modo reposo** cuando el voltaje de batería cae a niveles peligrosos, y solo debe salir cuando se alcance un nivel de recuperación **estable**.

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
Cuando la batería está baja, el dispositivo intenta transmitir por LTE. El modem consume picos de hasta 2A durante la transmisión TCP, causando brownout del ESP32. El dispositivo se reinicia, repite el intento, y entra en un **bucle de muerte** hasta agotar completamente la batería.

### Evidencia de Campo
- Panel solar conectado y funcionando correctamente
- Batería se descarga completamente a pesar del panel
- Capacidad de carga del panel es suficiente para operación normal
- **Causa:** Consumo pico de LTE > Capacidad instantánea del panel + batería baja

### Síntomas
1. Dispositivo se reinicia durante transmisión LTE
2. Batería se agota rápidamente (horas en lugar de días)
3. Datos no llegan al servidor a pesar de estar en buffer
4. Al recuperar dispositivo: batería completamente descargada

### Causa Raíz
1. Voltaje de batería se lee pero **solo para incluirlo en la trama**
2. No existe comparación contra umbral de seguridad
3. Transmisión LTE se intenta **siempre**, sin importar estado de batería
4. Pico de corriente de modem (2A) colapsa batería baja
5. Brownout → reinicio → bucle de muerte

---

## 📊 UMBRALES DE OPERACIÓN (CONTRATO)

### Valores del Contrato Actual

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| `UTS_LOW_ENTER` | **3.20 V** | Entrada a modo reposo |
| `UTS_LOW_EXIT` | **3.80 V** | Salida de modo reposo (si estable) |
| **Histéresis** | **0.60 V** | Diferencia EXIT - ENTER |

> ⚠️ **NOTA IMPORTANTE:** Los valores 3.20V / 3.80V son el **contrato actual** y son configurables por revisión de hardware. Sin embargo, el **comportamiento** (histéresis + estabilidad) es **OBLIGATORIO** en toda implementación.

### Diagrama de Umbrales

```
                    ┌─────────────────────────────────────┐
                    │         VOLTAJE DE BATERÍA          │
                    ├─────────────────────────────────────┤
    4.2V ──────────►│ ████████████████████ FULL          │
                    │                                     │
    3.80V ─────────►│ █████████████████ UTS_LOW_EXIT     │◄── Salida de reposo
                    │              ↑ Sale SI estable      │    (si estable)
                    │              │                      │
    3.50V ─────────►│ ████████████ │ ZONA SEGURA         │
                    │              │                      │
                    │   ╔══════════╧══════════╗           │
                    │   ║   HISTÉRESIS 0.6V   ║           │
                    │   ╚══════════╤══════════╝           │
                    │              │                      │
    3.20V ─────────►│ ██████ UTS_LOW_ENTER               │◄── Entrada a reposo
                    │        ↓ Entra a reposo             │    (inmediato)
    3.0V ──────────►│ ███ DAÑO PERMANENTE                │
                    │                                     │
    2.8V ──────────►│ █ BROWNOUT (crash)                 │
                    └─────────────────────────────────────┘
```

---

## 🔬 FILTRADO Y ESTABILIDAD

### Definición: `vBat_filtrada`

Promedio de **N lecturas ADC** para eliminar ruido y transitorios.

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| `VBAT_FILTER_SAMPLES` | 10 | Número de lecturas a promediar |
| `VBAT_FILTER_DELAY_MS` | 50-100 ms | Separación entre lecturas |
| `VBAT_DISCARD_FIRST` | 1 | Descartar primera lectura (ruido) |

### Contrato de Medición de VBAT

| Aspecto | Requisito |
|---------|----------|
| **Punto de medición** | Directo en batería (antes de diodo/regulador) |
| **Representa** | Voltaje de batería real (no voltaje de sistema) |
| **Calibración** | Divisor de voltaje y offset deben estar aplicados ANTES de comparar contra 3.20/3.80 |

> Los umbrales 3.20V y 3.80V asumen voltaje de batería real.
> Si se mide después de un diodo (caída ~0.3V), ajustar umbrales o compensar en lectura.

**Algoritmo:**

> **NOTA:** Usar la clase `ADCSensor` del proyecto (no `analogReadMilliVolts` directamente)
> para obtener **voltaje calibrado real** con el divisor de voltaje del hardware.

```cpp
float readVBatFiltered() {
    float sum = 0.0f;
    
    // Descartar primera lectura (ruido de multiplexor)
    adcSensor.readSensor();
    delay(FIX_V3_VBAT_FILTER_DELAY_MS);
    
    // Tomar N muestras y promediar
    for (int i = 0; i < FIX_V3_VBAT_FILTER_SAMPLES; i++) {
        adcSensor.readSensor();
        sum += adcSensor.getValue();  // Ya calibrado en voltios reales
        delay(FIX_V3_VBAT_FILTER_DELAY_MS);
    }
    
    return sum / FIX_V3_VBAT_FILTER_SAMPLES;
}
```

### Definición: "Estable"

Para salir de modo reposo, `vBat_filtrada` debe cumplir **UNO** de estos criterios:

| Criterio | Parámetro | Valor | Descripción |
|----------|-----------|-------|-------------|
| **Por ciclos** | `STABLE_CYCLES_REQUIRED` | 3 | Ciclos consecutivos ≥ UTS_LOW_EXIT |
| **Por tiempo** | `STABLE_TIME_MINUTES` | 10 min | Tiempo continuo ≥ UTS_LOW_EXIT |

### Regla Anti-Ruido

> Si **cualquier lectura** de `vBat_filtrada` cae por debajo de `UTS_LOW_EXIT` durante la fase de estabilidad, el **contador de estabilidad se reinicia a cero**.

```cpp
// Pseudocódigo de lógica de estabilidad
if (vBat_filtrada >= UTS_LOW_EXIT) {
    stableCounter++;
    if (stableCounter >= STABLE_CYCLES_REQUIRED) {
        exitRestMode();  // Salir de reposo
    }
} else {
    stableCounter = 0;  // REINICIAR - no era estable
}
```

---

## 😴 DEFINICIÓN: MODO REPOSO

### Comportamiento en Modo Reposo

| Acción | Permitida | Prohibida | Nota |
|--------|:---------:|:---------:|------|
| Deep Sleep | ✅ | - | Intervalo igual que modo normal |
| Wake periódico | ✅ | - | Cada ciclo según config |
| Lectura de sensores (ADC, I2C, RS485) | ✅ | - | Sin cambios |
| Lectura de GPS | ⚠️ | - | Solo si primer ciclo post-boot |
| Escritura a buffer | ✅ | - | Tramas se acumulan |
| Encender modem/radio | - | ❌ | **OBJETIVO PRINCIPAL** |
| Iniciar transmisión LTE/TCP | - | ❌ | Pico de 2A bloqueado |
| Abrir conexión TCP | - | ❌ | - |

> **Justificación:** Durante reposo se continúa adquiriendo datos para no perder información.
> Los datos se acumulan en buffer. Cuando la batería se recupere, se transmiten todos.
>
> **Sobre GPS:** El GPS solo se lee en el primer ciclo post-boot (ya existente en lógica normal).
> Si el dispositivo entra en reposo después del primer ciclo, el GPS ya no se vuelve a leer
> hasta el próximo reinicio. Esto no requiere lógica adicional de FIX-V3.

### ⚠️ ALCANCE Y LIMITACIONES

> **FIX-V3 garantiza únicamente el bloqueo de LTE.** No reduce el consumo total del ciclo.
>
> - Sensores (ADC, I2C, RS485) siguen ejecutándose normalmente
> - GPS puede consumir 30mA × 60s si es primer ciclo post-boot
> - Buffer sigue escribiendo a flash
>
> **Tradeoff recuperación vs resolución:** La recuperación a 3.80V depende de que el consumo
> promedio del ciclo (~0.5mA) sea **menor** que la carga disponible del panel. En días nublados
> o con panel pequeño, puede no recuperarse. Ver sección "Intervalo de Ciclo" para mitigación.

### Ciclo en Modo Reposo (Solo-Adquisición)

```
┌─────────────────────────────────────────────────────────────────────┐
│                    MODO REPOSO (Solo-Adquisición)                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ┌──────────┐    ┌──────────────────┐    ┌─────────────────────┐  │
│   │  WAKE    │───►│  Leer Sensores   │───►│  Leer GPS (⚠️)      │  │
│   │          │    │  ADC, I2C, RS485 │    │  (solo 1er ciclo)   │  │
│   └──────────┘    └──────────────────┘    └──────────┬──────────┘  │
│                                                      │              │
│                                                      ▼              │
│                                           ┌──────────────────────┐  │
│                                           │  Guardar en Buffer   │  │
│                                           │  (datos acumulados)  │  │
│                                           └──────────┬───────────┘  │
│                                                      │              │
│                                                      ▼              │
│        ┌─────────────────────────────────────────────────────────┐  │
│        │                    ⛔ LTE BLOQUEADO                      │  │
│        │              (modem NO se enciende)                     │  │
│        └─────────────────────────────────────────────────────────┘  │
│                                                      │              │
│                                                      ▼              │
│                                           ┌──────────────────────┐  │
│                                           │  Medir vBat_filtrada │  │
│                                           └──────────┬───────────┘  │
│                                                      │              │
│                              ┌───────────────────────┴───────────┐  │
│                              │    ¿vBat >= 3.80V estable?        │  │
│                              └───────────────────────┬───────────┘  │
│        ▲                                  NO         │ SÍ           │
│        │         ┌──────────────────────┐            │              │
│        └─────────│     DEEP SLEEP       │◄───────────┘              │
│                  │   (intervalo normal) │            │              │
│                  └──────────────────────┘            ▼              │
│                                           ┌──────────────────────┐  │
│                                           │   SALIR DE REPOSO    │  │
│                                           │   Modo Normal + TX   │  │
│                                           │  (envía buffer)      │  │
│                                           └──────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### Intervalo de Ciclo en Reposo

| Parámetro | Valor por defecto | Descripción |
|-----------|-------------------|-------------|
| `FIX_V3_REST_SLEEP_TIME_US` | Igual a `AppConfig.sleep_time_us` | Intervalo de sleep en reposo |

> **Por defecto:** El intervalo es el **MISMO** que en modo normal para mantener resolución temporal.
>
> **Configuración alternativa:** Si se prioriza recuperación sobre resolución, se puede aumentar:
> ```cpp
> #define FIX_V3_REST_SLEEP_TIME_US  (30ULL * 60 * 1000000)  // 30 minutos
> ```
>
> **Tradeoff:** Intervalo largo = recuperación más rápida, pero pérdida de datos.
> Intervalo normal = datos completos, pero recuperación más lenta.

---

## 🔄 TRANSICIONES (CONTRATO)

### Tabla de Transiciones

| Estado Actual | Condición | Estado Siguiente | Acción |
|---------------|-----------|------------------|--------|
| **NORMAL** | `vBat_filtrada <= 3.20V` | **REPOSO** | Log, guardar estado en RTC, ciclo solo-adquisición |
| **REPOSO** | `vBat_filtrada >= 3.80V` **Y** `estable` | **NORMAL** | Log, limpiar contador, reanudar ciclo completo |
| **REPOSO** | `vBat_filtrada < 3.80V` **O** `!estable` | **REPOSO** | Continuar solo-adquisición (sin LTE) |

### Diagrama de Estados

```
                    vBat_filtrada <= 3.20V
         ┌───────────────────────────────────────┐
         │                                       │
         ▼                                       │
    ┌─────────┐                            ┌─────┴─────┐
    │         │   vBat_filtrada >= 3.80V   │           │
    │ REPOSO  │──────── Y estable ────────►│  NORMAL   │
    │         │                            │           │
    └────┬────┘                            └───────────┘
         │                                       
         │ vBat < 3.80V                          
         │    O                                  
         │ !estable                              
         │                                       
         └──────────┐                            
                    │                            
                    ▼                            
              (continuar                         
               en reposo)                        
```

---

## 🔧 IMPLEMENTACIÓN

### Cambio 1: FeatureFlags.h - Definiciones

```cpp
/**
 * FIX-V3: Modo reposo por baja batería
 * Sistema: Energía / LTE
 * Archivo: AppController.cpp
 * Descripción: Si batería <= UTS_LOW_ENTER, entra a modo reposo.
 *              Solo sale cuando batería >= UTS_LOW_EXIT de forma estable.
 * Requisitos: RF-06, RF-09
 * Estado: Implementado
 */
#define ENABLE_FIX_V3_LOW_BATTERY_MODE    1

// ============================================================
// UMBRALES DE BATERÍA (CONTRATO v1.0)
// ============================================================
// NOTA: Valores configurables por revisión de hardware.
//       El comportamiento (histéresis + estabilidad) es OBLIGATORIO.

/** @brief Umbral de entrada a modo reposo (voltios) */
#define FIX_V3_UTS_LOW_ENTER              3.20f

/** @brief Umbral de salida de modo reposo (voltios) */
#define FIX_V3_UTS_LOW_EXIT               3.80f

// ============================================================
// PARÁMETROS DE FILTRADO
// ============================================================

/** @brief Número de muestras para promediar vBat */
#define FIX_V3_VBAT_FILTER_SAMPLES        10

/** @brief Delay entre muestras ADC (ms) */
#define FIX_V3_VBAT_FILTER_DELAY_MS       50

// ============================================================
// PARÁMETROS DE ESTABILIDAD
// ============================================================

/** @brief Ciclos consecutivos requeridos para considerar "estable" */
#define FIX_V3_STABLE_CYCLES_REQUIRED     3
```

### Cambio 2: AppController.cpp - Variables RTC

```cpp
// ============ [FIX-V3] Variables persistentes en RTC ============
#if ENABLE_FIX_V3_LOW_BATTERY_MODE
/** @brief Flag que indica si estamos en modo reposo */
RTC_DATA_ATTR static bool g_restMode = false;

/** @brief Contador de ciclos estables para salida de reposo */
RTC_DATA_ATTR static uint8_t g_stableCycleCounter = 0;

/** @brief Última lectura de vBat filtrada */
RTC_DATA_ATTR static float g_lastVBatFiltered = 0.0f;
#endif
```

### Cambio 3: AppController.cpp - Función de lectura filtrada

```cpp
#if ENABLE_FIX_V3_LOW_BATTERY_MODE
/**
 * @brief Lee voltaje de batería con filtrado (promedio de N muestras)
 * 
 * Descarta primera lectura para eliminar ruido del multiplexor ADC.
 * Promedia N lecturas separadas por delay configurable.
 * 
 * @return Voltaje filtrado en voltios
 */
static float readVBatFiltered() {
    float sum = 0.0f;
    
    // Descartar primera lectura (ruido de multiplexor)
    adcSensor.readSensor();
    delay(FIX_V3_VBAT_FILTER_DELAY_MS);
    
    // Tomar N muestras y promediar
    for (int i = 0; i < FIX_V3_VBAT_FILTER_SAMPLES; i++) {
        adcSensor.readSensor();
        sum += adcSensor.getValue();
        delay(FIX_V3_VBAT_FILTER_DELAY_MS);
    }
    
    return sum / FIX_V3_VBAT_FILTER_SAMPLES;
}
#endif
```

### Cambio 4: AppController.cpp - Lógica de transiciones

```cpp
#if ENABLE_FIX_V3_LOW_BATTERY_MODE
/**
 * @brief Evalúa si debe entrar/salir de modo reposo
 * 
 * Implementa histéresis con umbrales:
 * - Entrada: vBat <= 3.20V (inmediato)
 * - Salida: vBat >= 3.80V Y estable (3 ciclos consecutivos)
 * 
 * @param vBatFiltered Voltaje filtrado actual
 * @return true si puede operar normalmente, false si debe estar en reposo
 */
static bool evaluateBatteryState(float vBatFiltered) {
    g_lastVBatFiltered = vBatFiltered;
    
    if (!g_restMode) {
        // === MODO NORMAL: Evaluar entrada a reposo ===
        if (vBatFiltered <= FIX_V3_UTS_LOW_ENTER) {
            g_restMode = true;
            g_stableCycleCounter = 0;
            Serial.println(F(""));
            Serial.println(F("╔════════════════════════════════════════════════════╗"));
            Serial.println(F("║  [FIX-V3] BATERÍA BAJA - ENTRANDO A MODO REPOSO    ║"));
            Serial.print(F("║  vBat_filtrada: "));
            Serial.print(vBatFiltered, 2);
            Serial.print(F("V <= "));
            Serial.print(FIX_V3_UTS_LOW_ENTER, 2);
            Serial.println(F("V                  ║"));
            Serial.println(F("║  Radio/LTE BLOQUEADO hasta recuperación            ║"));
            Serial.println(F("╚════════════════════════════════════════════════════╝"));
            return false;
        }
        return true;  // Operar normalmente
        
    } else {
        // === MODO REPOSO: Evaluar salida ===
        if (vBatFiltered >= FIX_V3_UTS_LOW_EXIT) {
            g_stableCycleCounter++;
            Serial.print(F("[FIX-V3] vBat: "));
            Serial.print(vBatFiltered, 2);
            Serial.print(F("V >= "));
            Serial.print(FIX_V3_UTS_LOW_EXIT, 2);
            Serial.print(F("V | Estabilidad: "));
            Serial.print(g_stableCycleCounter);
            Serial.print(F("/"));
            Serial.println(FIX_V3_STABLE_CYCLES_REQUIRED);
            
            if (g_stableCycleCounter >= FIX_V3_STABLE_CYCLES_REQUIRED) {
                g_restMode = false;
                g_stableCycleCounter = 0;
                Serial.println(F(""));
                Serial.println(F("╔════════════════════════════════════════════════════╗"));
                Serial.println(F("║  [FIX-V3] BATERÍA RECUPERADA - SALIENDO DE REPOSO  ║"));
                Serial.println(F("║  Condición de estabilidad cumplida                 ║"));
                Serial.println(F("║  Reanudando operación normal                       ║"));
                Serial.println(F("╚════════════════════════════════════════════════════╝"));
                return true;
            }
        } else {
            // Voltaje cayó: reiniciar contador de estabilidad
            if (g_stableCycleCounter > 0) {
                Serial.print(F("[FIX-V3] vBat: "));
                Serial.print(vBatFiltered, 2);
                Serial.print(F("V < "));
                Serial.print(FIX_V3_UTS_LOW_EXIT, 2);
                Serial.println(F("V | Estabilidad REINICIADA"));
            }
            g_stableCycleCounter = 0;
        }
        
        Serial.print(F("[FIX-V3] Modo REPOSO activo. vBat: "));
        Serial.print(vBatFiltered, 2);
        Serial.println(F("V. Esperando recuperación..."));
        return false;
    }
}
#endif
```

### Cambio 5: AppController.cpp - Lógica de reposo

En modo reposo, el ciclo normal se ejecuta **completo EXCEPTO la transmisión LTE**.
La única modificación es en la transición a `Cycle_SendLTE`:

```cpp
case AppState::Cycle_StoreBuffer: {
    TIMING_START(g_timing, storeBuffer);
    // ... código normal de almacenamiento en buffer ...
    TIMING_END(g_timing, storeBuffer);
    
#if ENABLE_FIX_V3_LOW_BATTERY_MODE
    // ============ [FIX-V3 START] Verificar batería antes de LTE ============
    float vBatFiltered = readVBatFiltered();
    
    if (!evaluateBatteryState(vBatFiltered)) {
        // Estamos en reposo - SALTAR LTE, ir directo a sleep
        Serial.println(F("[FIX-V3] Datos guardados. LTE bloqueado por batería baja."));
        Serial.print(F("[FIX-V3] Buffer tiene tramas pendientes. TX cuando vBat >= "));
        Serial.print(FIX_V3_UTS_LOW_EXIT, 2);
        Serial.println(F("V estable."));
        g_state = AppState::Cycle_Sleep;  // Saltar LTE
        break;
    }
    // ============ [FIX-V3 END] ============
#endif
    
    g_state = AppState::Cycle_SendLTE;  // Batería OK, continuar normal
    break;
}
```

> **Nota:** Los sensores, GPS y buffer se ejecutan normalmente.
> Solo se bloquea el paso a `Cycle_SendLTE` cuando `g_restMode == true`.

### Cambio 6: AppController.cpp - Sin cambios en setup

No se requiere lógica especial en `appSetup()`. El ciclo normal se ejecuta siempre.
La única diferencia es que en modo reposo, la transición `Cycle_StoreBuffer → Cycle_SendLTE` se bloquea.

### Cambio 7: printActiveFlags()

```cpp
#if ENABLE_FIX_V3_LOW_BATTERY_MODE
Serial.print(F("  [X] FIX-V3: Low Battery Mode ("));
Serial.print(FIX_V3_UTS_LOW_ENTER, 2);
Serial.print(F("V/"));
Serial.print(FIX_V3_UTS_LOW_EXIT, 2);
Serial.println(F("V)"));
#else
Serial.println(F("  [ ] FIX-V3: Low Battery Mode"));
#endif
```

---

## ✅ DEFINITION OF DONE

### Criterios Obligatorios

| # | Criterio | Verificación |
|---|----------|--------------|
| 1 | **Nunca intenta LTE por debajo de 3.20V** | Log muestra bloqueo de TX |
| 2 | **No sale de reposo hasta 3.80V estable** | Contador de estabilidad en logs |
| 3 | **En reposo el consumo permite recuperación** | Medición: tendencia de carga positiva |
| 4 | **vBat se lee filtrada (N=10 muestras)** | Debug muestra promedio |
| 5 | **Histéresis de 0.60V funciona** | No oscila entre modos |
| 6 | **Contador se reinicia si vBat cae** | Log muestra "Estabilidad REINICIADA" |
| 7 | **Estado persiste en deep sleep (RTC)** | Reposo → wake → sigue en reposo |

### Criterios de Integración

- [ ] Feature flag permite deshabilitar completamente
- [ ] `printActiveFlags()` muestra estado de FIX-V3 con umbrales
- [ ] Sin conflicto con FIX-V1, FIX-V2
- [ ] Sin conflicto con FEAT-V3 (Crash Diagnostics)
- [ ] Buffer preserva datos durante reposo

---

## 🧪 PLAN DE PRUEBAS

### Caso 1: Entrada a Reposo

| Paso | Acción | Resultado Esperado |
|------|--------|-------------------|
| 1 | Alimentar con 3.25V | Ciclo normal inicia |
| 2 | Reducir a 3.20V | Log: "ENTRANDO A MODO REPOSO" |
| 3 | Verificar | NO intenta encender modem |
| 4 | Verificar | Ciclo solo-adquisición, sleep normal |

**Log esperado:**
```
╔════════════════════════════════════════════════════╗
║  [FIX-V3] BATERÍA BAJA - ENTRANDO A MODO REPOSO    ║
║  vBat_filtrada: 3.18V <= 3.20V                     ║
║  Radio/LTE BLOQUEADO hasta recuperación            ║
╚════════════════════════════════════════════════════╝
```

### Caso 2: Permanencia en Reposo

| Paso | Acción | Resultado Esperado |
|------|--------|-------------------|
| 1 | Mantener en 3.15V | Wake cada ciclo (intervalo normal) |
| 2 | Medir vBat | Log muestra lectura filtrada |
| 3 | Verificar 5 ciclos | NUNCA intenta modem |
| 4 | Sensores | Lee ADC, I2C, RS485 normalmente |
| 5 | GPS | Lee GPS si corresponde (primer ciclo) |
| 6 | Buffer | Escribe trama (sin enviar) |

**Log esperado:**
```
[FIX-V3] === CICLO REPOSO (Solo-Adquisición) ===
[FIX-V3] Sensores leídos. Datos guardados en buffer.
[FIX-V3] Modo REPOSO activo. vBat: 3.15V. Esperando recuperación...
[FIX-V3] LTE BLOQUEADO. Deep sleep (intervalo normal)...
```

### Caso 3: Recuperación con Picos (Anti-Ruido)

| Paso | Acción | Resultado Esperado |
|------|--------|-------------------|
| 1 | Subir de 3.20V a 3.75V | Aún en reposo (< 3.80V) |
| 2 | Subir a 3.82V | Contador = 1/3 |
| 3 | Bajar a 3.70V (pico) | Contador = 0 (REINICIADO) |
| 4 | Subir a 3.85V | Contador = 1/3 (reinicia) |
| 5 | Mantener 3.85V x 2 ciclos más | Contador = 2/3, luego 3/3 |
| 6 | Verificar | Sale de reposo |

**Log esperado:**
```
[FIX-V3] vBat: 3.82V >= 3.80V | Estabilidad: 1/3
[FIX-V3] vBat: 3.70V < 3.80V | Estabilidad REINICIADA
[FIX-V3] vBat: 3.85V >= 3.80V | Estabilidad: 1/3
[FIX-V3] vBat: 3.86V >= 3.80V | Estabilidad: 2/3
[FIX-V3] vBat: 3.84V >= 3.80V | Estabilidad: 3/3

╔════════════════════════════════════════════════════╗
║  [FIX-V3] BATERÍA RECUPERADA - SALIENDO DE REPOSO  ║
║  Condición de estabilidad cumplida                 ║
║  Reanudando operación normal                       ║
╚════════════════════════════════════════════════════╝
```

### Caso 4: Salida y Reanudación Normal

| Paso | Acción | Resultado Esperado |
|------|--------|-------------------|
| 1 | Estabilizar en 3.85V x 3 ciclos | Sale de reposo |
| 2 | Verificar | Ejecuta ciclo normal completo |
| 3 | Verificar | Lee sensores, GPS, buffer |
| 4 | Verificar | Transmite por LTE |
| 5 | Buffer | Envía tramas acumuladas |

### Matriz de Validación por Logs

| Log | Debe aparecer cuando... |
|-----|------------------------|
| `ENTRANDO A MODO REPOSO` | vBat <= 3.20V por primera vez |
| `Modo REPOSO activo` | Cada wake en reposo |
| `Estabilidad: N/3` | vBat >= 3.80V en reposo |
| `Estabilidad REINICIADA` | vBat cae < 3.80V durante estabilización |
| `SALIENDO DE REPOSO` | Estabilidad alcanza 3/3 |
| `Radio/LTE BLOQUEADO` | Al entrar a reposo |

---

## 🔄 ROLLBACK

Para revertir este fix:

```cpp
// En FeatureFlags.h cambiar:
#define ENABLE_FIX_V3_LOW_BATTERY_MODE    0  // Era 1
```

Recompilar y flashear. El comportamiento original (siempre transmitir) se restaura.

---

## 📈 BENEFICIOS ESPERADOS

| Aspecto | Sin FIX-V3 | Con FIX-V3 |
|---------|------------|------------|
| Vida útil con batería baja | Horas (bucle muerte) | Días (reposo) |
| Datos perdidos | Todos (brownout) | Ninguno (buffer acumula) |
| Daño a batería | Descarga profunda | Protegida a 3.20V |
| Recuperación | Manual | Automática a 3.80V estable |
| Consumo en reposo | N/A | ~0.5 mA promedio (*) |

> (*) El consumo promedio en reposo **NO es ~10µA** porque se ejecuta ciclo completo de sensores.
> Se ahorra únicamente el pico de LTE (300-2000mA × 30s), que es el objetivo principal.

---

## 📝 NOTAS DE IMPLEMENTACIÓN

### Calibración ADC

El valor de batería leído por ADC puede tener offset. Medir con multímetro y ajustar:

```cpp
// Si el ADC lee 0.05V menos que voltaje real:
#define FIX_V3_ADC_OFFSET  0.05f  // Sumar a lectura
```

### Interacción con Crash Diagnostics

Si ocurre brownout a pesar de FIX-V3 (umbral mal calibrado), FEAT-V3 capturará el contexto para análisis post-mortem.

### Consumo Detallado: Normal vs Reposo

| Fase | Normal | Reposo |
|------|--------|--------|
| Wake + Sensores | 50 mA × 2s | 50 mA × 2s |
| GPS (primer ciclo) | 30 mA × 60s | 30 mA × 60s |
| Buffer write | 10 mA × 0.1s | 10 mA × 0.1s |
| **LTE TX** | **300-2000 mA × 30s** | **⛔ BLOQUEADO** |
| Deep Sleep | 10 µA × 598s | 10 µA × 598s |

**Ahorro por ciclo en reposo:** ~10-60 mAh (evita pico LTE)

> En reposo, la batería puede recuperarse porque el consumo promedio (~0.5mA) es menor que la carga del panel (~50-500mA).

### NO-Objetivos de FIX-V3

- ❌ No minimiza consumo total del ciclo (sensores siguen corriendo)
- ❌ No evalúa batería al inicio del ciclo (evalúa después de buffer)
- ❌ No deshabilita GPS/RS485 automáticamente en reposo
- ❌ No garantiza recuperación si panel < consumo de ciclo

### Futuras Mejoras (fuera de scope)

- Evaluación temprana de batería al inicio del ciclo para saltar cargas no esenciales
- Modo "ultra-reposo" que deshabilite sensores y solo mida batería
- Intervalo adaptativo según tendencia de carga
