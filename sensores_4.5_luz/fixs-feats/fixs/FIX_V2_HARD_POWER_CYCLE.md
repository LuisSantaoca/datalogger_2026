# FIX-V2: Hard Power Cycle del Modem vía Control de VBAT

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V2 |
| **Tipo** | Fix (Recuperación de Modem) |
| **Sistema** | LTE / Power |
| **Archivos Principales** | `src/data_lte/LTEModule.cpp`, `src/data_lte/LTEModule.h`, `src/data_lte/config_data_lte.h` |
| **Estado** | 📋 Propuesto |
| **Fecha Identificación** | 2026-02-10 |
| **Versión Target** | v1.2.0 |
| **Branch** | `fix-v2/hard-power-cycle` |
| **Depende de** | FIX-V1 (logging estandarizado, recomendado) |
| **Origen** | Portado de JAMR_4.5 `FIX_V9_HARD_POWER_CYCLE` |
| **Prioridad** | **CRÍTICA** |

---

## ✅ CUMPLIMIENTO DE PREMISAS

| Premisa | Descripción | Cumplimiento | Evidencia |
|---------|-------------|--------------|-----------|
| **P1** | Aislamiento total | ✅ | Branch: `fix-v2/hard-power-cycle` |
| **P2** | Cambios mínimos | ✅ | 3 archivos: config_data_lte.h, LTEModule.h, LTEModule.cpp |
| **P3** | Defaults seguros | ✅ | Si flag=0, preserva `powerOn()` original con PWRKEY |
| **P4** | Feature flags | ✅ | `ENABLE_FIX_V2_HARD_POWER_CYCLE` con `#else` |
| **P5** | Logging exhaustivo | ✅ | Cada paso del hard cycle documentado en serial |
| **P6** | No cambiar lógica existente | ✅ | `powerOn()` original intacto en bloque `#else` |
| **P7** | Testing gradual | ✅ | Plan con multímetro y mediciones VBAT |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado

El modem SIM7080G no enciende después de múltiples ciclos de deep sleep. Los 3 intentos de secuencia PWRKEY fallan consistentemente, dejando al dispositivo sin conectividad LTE (ICCID vacío, sin envío de datos).

### Evidencia (Log del 2026-02-05)

```
Encendiendo SIM7080G...
Intento de encendido 1 de 3
PWRKEY toggled
Intento de encendido 2 de 3
PWRKEY toggled
Intento de encendido 3 de 3
PWRKEY toggled
Error: No se pudo encender el modulo    ← FALLO #1 (GetICCID)
...
Encendiendo SIM7080G...
Intento de encendido 1 de 3
PWRKEY toggled
Intento de encendido 2 de 3
PWRKEY toggled
Intento de encendido 3 de 3
PWRKEY toggled
Error: No se pudo encender el modulo    ← FALLO #2 (SendLTE)
```

El modem falla **DOS VECES** en el mismo ciclo: al intentar leer ICCID y al intentar enviar datos. Ambas veces con los 3 intentos PWRKEY agotados.

### Requisitos Violados (según REQUIREMENTS.md)

| Requisito | Descripción | Estado |
|-----------|-------------|--------|
| **LTE-09** | Obtener ICCID encendiendo modem | ❌ ICCID = `00000000000000000000` |
| **LTE-06** | Flujo de conexión LTE completo | ❌ No llega a `configureOperator` |
| **REL-09** | Limpieza apropiada en cada fallo | ⚠️ Parcial — no hay fallback más allá de PWRKEY |

### Causa Raíz

El problema fue diagnosticado exhaustivamente en JAMR_4.5 (ver FIX-V7, FIX-V9). El SIM7080G puede entrar en un **estado zombie** donde:

1. **Zombie Tipo A (PSM):** El modem está en Power Saving Mode profundo y no responde a PWRKEY — solucionable con reset AT.
2. **Zombie Tipo B (Latch-up eléctrico):** El modem está en un estado eléctrico anómalo donde ningún comando AT ni señal PWRKEY lo recupera — **solo se recupera cortando alimentación VBAT físicamente**.

El `powerOn()` actual solo intenta PWRKEY. No tiene mecanismo de fallback para zombie tipo B.

### Aprendizaje de JAMR_4.5

JAMR_4.5 resolvió este problema con FIX-V9 (`FIX_V9_HARD_POWER_CYCLE.md`), que descubrió que **IO13 controla indirectamente la alimentación VBAT del modem** a través del boost converter MIC2288:

```
IO13 → Divisor R23/R24 (10k/10k) → EN (pin 4 MIC2288) → VBAT SIM7080G
```

- `IO13 = HIGH` → `Ven = 1.65V` → MIC2288 ON → VBAT = ~3.8V (modem alimentado)
- `IO13 = LOW` → `Ven = 0V` → MIC2288 OFF → VBAT = ~0V (modem sin alimentación)

---

## 📐 CONTEXTO TÉCNICO

### Esquemático del Control de Alimentación

```
         ┌─────────────┐
IO13 ────┤             │
         │  Divisor    │──── EN (pin 4 MIC2288)
         │  R23/R24    │
GND ─────┤  10k/10k    │
         └─────────────┘
                │
                ▼
         ┌─────────────┐
         │   MIC2288   │
         │ Boost Conv. │──── VBAT (3.8V → SIM7080G)
         │    (U2)     │
         └─────────────┘
```

### Cálculo de Voltaje EN

```
Ven = VIO13 × (R24 / (R23 + R24))
Ven = 3.3V × (10kΩ / 20kΩ) = 1.65V
```

Umbral EN del MIC2288: 1.2V típico → 1.65V > umbral → encendido garantizado.

### Secuencia de Hard Power Cycle

```
IO13     ─────┐                              ┌─────────────────
              │                              │
              └──────────────────────────────┘
              
VBAT     ~3.8V │                              │ ~3.8V
              │    ↘                    ↗     │
              │      ~0V ─────────────       │
              
              │←─── toff ───→│←─ tstab ─→│
              │    (5000ms)  │  (1000ms) │
              
Modem         │    Descarga  │  Carga    │  Ready
Estado:  ON   │    completa  │  caps     │  para PWRKEY
```

### Tiempos Críticos

| Parámetro | Valor | Justificación |
|-----------|-------|---------------|
| `toff` | 5000 ms | Descarga completa de capacitores VBAT (conservador) |
| `tstab` | 1000 ms | Estabilización MIC2288 antes de intentar PWRKEY |
| `t_total` | ~6 s | Overhead aceptable vs días de zombie |

---

## 📊 EVALUACIÓN

### Impacto Cuantificado

| Métrica | Sin fix | Con fix |
|---------|---------|---------|
| Recuperación zombie tipo A | 0% | N/A (no aplica) |
| Recuperación zombie tipo B | 0% | ~100% |
| Tiempo perdido por zombie | Indefinido (hasta reset manual) | ~6 segundos |
| Ciclos con ICCID vacío por zombie | Todos hasta intervención | Máximo 1 |
| Datos perdidos por falta de envío | Creciente (buffer lleno) | Mínimo |

### Impacto por Área

| Aspecto | Evaluación |
|---------|------------|
| Criticidad | **Crítica** — Sin LTE, el dispositivo no transmite datos |
| Riesgo de no implementar | **Altísimo** — Pérdida total de datos hasta intervención manual |
| Esfuerzo | Medio — 1 método nuevo + modificar `powerOn()` |
| Beneficio | **Máximo** — Recuperación autónoma de modem zombie |

---

## 🔧 IMPLEMENTACIÓN

### Archivos a Modificar

| Archivo | Cambio |
|---------|--------|
| `src/data_lte/config_data_lte.h` | Agregar constante `MODEM_EN_PIN` |
| `src/data_lte/LTEModule.h` | Agregar declaración `hardPowerCycle()` |
| `src/data_lte/LTEModule.cpp` | Agregar método `hardPowerCycle()`, modificar `powerOn()` |

### Cambio 1: Definición de Pin (config_data_lte.h)

```cpp
// ANTES (al final del archivo, antes de #endif)
#define DB_SERVER_IP "d04.elathia.ai"
#define TCP_PORT " 12608"

#endif

// DESPUÉS
// ============ [FIX-V2 START] Pin de control MIC2288 ============
/**
 * @brief GPIO que controla EN del MIC2288 vía divisor R23/R24.
 * IO13 → R23(10k) → junction → R24(10k) → GND
 * junction → MIC2288 EN (pin 4)
 * Ven = 3.3V × (10k/20k) = 1.65V > umbral 1.2V
 */
static const uint8_t MODEM_EN_PIN = 13;

/** @brief Tiempo con VBAT cortado para descarga completa (ms) */
static const uint32_t FIX_V2_POWEROFF_DELAY_MS = 5000;

/** @brief Tiempo de estabilización tras restaurar VBAT (ms) */
static const uint32_t FIX_V2_STABILIZATION_MS = 1000;

/** @brief Intentos PWRKEY antes de activar hard power cycle */
static const uint16_t FIX_V2_PWRKEY_ATTEMPTS_BEFORE_HARD = 2;
// ============ [FIX-V2 END] ============

#define DB_SERVER_IP "d04.elathia.ai"
#define TCP_PORT " 12608"

#endif
```

### Cambio 2: Declaración en Header (LTEModule.h)

```cpp
// Agregar en sección private de LTEModule:

// ============ [FIX-V2 START] Hard power cycle ============
/**
 * @brief Ejecuta un power cycle REAL del modem cortando VBAT vía IO13.
 * @details Controla MIC2288 EN para colapsar VBAT a ~0V y restaurar.
 * @return true siempre (el ciclo siempre se ejecuta)
 */
bool hardPowerCycle();
// ============ [FIX-V2 END] ============
```

### Cambio 3: Implementación hardPowerCycle() (LTEModule.cpp)

```cpp
// ============ [FIX-V2 START] Hard power cycle via IO13 ============
bool LTEModule::hardPowerCycle() {
    Serial.println(F("[WARN][LTE] HARD-CYCLE: Iniciando power cycle VBAT via IO13"));
    
    pinMode(MODEM_EN_PIN, OUTPUT);
    
    // PASO 1: Cortar alimentación
    Serial.println(F("[INFO][LTE] HARD-CYCLE: IO13=LOW, cortando VBAT..."));
    digitalWrite(MODEM_EN_PIN, LOW);
    
    // PASO 2: Esperar descarga completa
    Serial.print(F("[INFO][LTE] HARD-CYCLE: Esperando "));
    Serial.print(FIX_V2_POWEROFF_DELAY_MS);
    Serial.println(F("ms para descarga..."));
    delay(FIX_V2_POWEROFF_DELAY_MS);
    
    // PASO 3: Restaurar alimentación
    Serial.println(F("[INFO][LTE] HARD-CYCLE: IO13=HIGH, restaurando VBAT..."));
    digitalWrite(MODEM_EN_PIN, HIGH);
    
    // PASO 4: Esperar estabilización
    Serial.print(F("[INFO][LTE] HARD-CYCLE: Esperando "));
    Serial.print(FIX_V2_STABILIZATION_MS);
    Serial.println(F("ms para estabilización..."));
    delay(FIX_V2_STABILIZATION_MS);
    
    // PASO 5: Restaurar GPIO13 a INPUT para no afectar lectura ADC (ADC_VOLT_BAT=13)
    pinMode(MODEM_EN_PIN, INPUT);  // Divisor R23/R24 mantiene EN alto por pull-up
    Serial.println(F("[INFO][LTE] HARD-CYCLE: IO13 restaurado a INPUT (modo ADC seguro)"));
    
    Serial.println(F("[INFO][LTE] HARD-CYCLE: Ciclo completado, listo para PWRKEY"));
    return true;
}
// ============ [FIX-V2 END] ============
```

### Cambio 4: Integración en powerOn() (LTEModule.cpp, líneas 38-73)

Código actual exacto:
```cpp
bool LTEModule::powerOn() {
    debugPrint("Encendiendo SIM7080G...");
    
    if (isAlive()) {
        debugPrint("Modulo ya esta encendido");
     
        delay(2000);
        return true;
    }
    
    for (uint16_t attempt = 0; attempt < LTE_POWER_ON_ATTEMPTS; attempt++) {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Intento de encendido ");
            _debugSerial->print(attempt + 1);
            _debugSerial->print(" de ");
            _debugSerial->println(LTE_POWER_ON_ATTEMPTS);
        }
        
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                debugPrint("SIM7080G encendido correctamente!");
                delay(1000);

                delay(2000);
                return true;
            }
            delay(500);
        }
    }
    
    debugPrint("Error: No se pudo encender el modulo");
    return false;
}
```

Reemplazo — el feature flag envuelve SOLO el bucle `for`, preservando el preámbulo `isAlive()`:
```cpp
// DESPUÉS (con feature flag)
bool LTEModule::powerOn() {
    debugPrint("Encendiendo SIM7080G...");
    
    if (isAlive()) {
        debugPrint("Modulo ya esta encendido");
        delay(2000);
        return true;
    }

#if ENABLE_FIX_V2_HARD_POWER_CYCLE
    // [FIX-V2 START] Intentar PWRKEY primero, hard cycle como fallback
    for (uint16_t attempt = 0; attempt < FIX_V2_PWRKEY_ATTEMPTS_BEFORE_HARD; attempt++) {
        Serial.print(F("[INFO][LTE] Intento PWRKEY "));
        Serial.print(attempt + 1);
        Serial.print(F("/"));
        Serial.println(FIX_V2_PWRKEY_ATTEMPTS_BEFORE_HARD);
        
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                Serial.println(F("[INFO][LTE] Modem respondió después de PWRKEY"));
                delay(2000);
                return true;
            }
            delay(500);
        }
    }
    
    // PWRKEY agotado → ejecutar hard power cycle
    Serial.println(F("[WARN][LTE] PWRKEY agotado, ejecutando HARD POWER CYCLE"));
    hardPowerCycle();
    
    // Después del hard cycle, intentar PWRKEY una vez más
    togglePWRKEY();
    delay(LTE_PWRKEY_POST_DELAY_MS);
    
    uint32_t startTime = millis();
    while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
        if (isAlive()) {
            Serial.println(F("[INFO][LTE] SUCCESS: Modem recuperado con hard power cycle"));
            delay(2000);
            return true;
        }
        delay(500);
    }
    
    Serial.println(F("[ERROR][LTE] Modem no responde ni con hard power cycle"));
    return false;
    // [FIX-V2 END]
    
#else
    // Código original preservado para rollback
    for (uint16_t attempt = 0; attempt < LTE_POWER_ON_ATTEMPTS; attempt++) {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Intento de encendido ");
            _debugSerial->print(attempt + 1);
            _debugSerial->print(" de ");
            _debugSerial->println(LTE_POWER_ON_ATTEMPTS);
        }
        
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                debugPrint("SIM7080G encendido correctamente!");
                delay(1000);
                delay(2000);
                return true;
            }
            delay(500);
        }
    }
    
    debugPrint("Error: No se pudo encender el modulo");
    return false;
#endif
}
```

### Flujo de Decisión Completo

```
powerOn()
    │
    ├──► isAlive()? ──► ✅ return true (ya encendido)
    │
    ├──► Intento PWRKEY #1
    │         │
    │         ├── isAlive()? ──► ✅ return true
    │         │
    │         └── NO ──► Intento PWRKEY #2
    │                        │
    │                        ├── isAlive()? ──► ✅ return true
    │                        │
    │                        └── NO ──► 🔴 HARD POWER CYCLE
    │                                        │
    │                                        ├── IO13 LOW (5s)
    │                                        ├── IO13 HIGH (1s estab.)
    │                                        ├── IO13 → INPUT (restore ADC)
    │                                        ├── PWRKEY post-cycle
    │                                        │
    │                                        └── isAlive()? 
    │                                               │
    │                                               ├── ✅ return true
    │                                               │
    │                                               └── ❌ return false
```

---

## ⚠️ CONSIDERACIONES DE SEGURIDAD

### Conflicto con IO13 (ADC_VOLT_BAT)

En `config_data_sensors.h`, GPIO13 está asignado como `ADC_VOLT_BAT`:

```cpp
#define ADC_VOLT_BAT 13
#define ADC_PIN ADC_VOLT_BAT
```

Este **doble uso** del pin GPIO13 requiere atención:
- En modo **ADC** (lectura de batería): GPIO13 es INPUT, no afecta el divisor R23/R24
- En modo **hard power cycle**: GPIO13 se configura como OUTPUT temporalmente

**Mitigación:** Incluida en la implementación de `hardPowerCycle()` — PASO 5 restaura GPIO13 a INPUT:

```cpp
// Al final de hardPowerCycle() — PASO 5:
pinMode(MODEM_EN_PIN, INPUT);  // Devolver a modo ADC
```

El divisor R23/R24 actúa como pull-up efectivo: con GPIO13 en alta impedancia (INPUT), R23 queda flotante pero R24 mantiene el nodo EN a un voltaje suficiente (VBAT a través del circuito) para mantener MIC2288 encendido.

**Secuencia segura en AppController:**
1. `ReadSensors` → lee ADC en GPIO13 (INPUT) → obtiene voltaje batería ✅
2. `GetICCID` / `SendLTE` → si necesita hard cycle → OUTPUT temporalmente → restaura INPUT ✅

### Estados Iniciales del Pin

| Condición | IO13 después de reset | VBAT |
|-----------|----------------------|------|
| Power-on reset | INPUT (alta impedancia) | 3.8V (R23 pull-up) |
| Deep sleep wake | INPUT (liberado por `gpio_hold_dis`) | 3.8V |
| Brownout reset | INPUT | 3.8V |

**Diseño seguro:** El divisor R23/R24 actúa como pull-up efectivo, manteniendo EN alto incluso si IO13 es INPUT.

---

## 📊 VALIDACIÓN

### Mediciones Esperadas (Multímetro)

| Punto de medida | IO13=HIGH | IO13=LOW |
|-----------------|-----------|----------|
| IO13 (GPIO) | 3.3V | 0V |
| Junction R23/R24 | 1.65V | 0V |
| MIC2288 EN (pin 4) | 1.65V | 0V |
| VBAT (+3.8V_M) | 3.7-3.9V | <0.5V (decae) |
| Corriente modem | ~15mA idle | 0mA |

### Output Esperado (Modem Zombie Recuperado)

```
[INFO][LTE] Encendiendo SIM7080G...
[INFO][LTE] Intento PWRKEY 1/2
[VERBOSE][LTE] PWRKEY toggled
[INFO][LTE] Intento PWRKEY 2/2
[VERBOSE][LTE] PWRKEY toggled
[WARN][LTE] PWRKEY agotado, ejecutando HARD POWER CYCLE
[WARN][LTE] HARD-CYCLE: Iniciando power cycle VBAT via IO13
[INFO][LTE] HARD-CYCLE: IO13=LOW, cortando VBAT...
[INFO][LTE] HARD-CYCLE: Esperando 5000ms para descarga...
[INFO][LTE] HARD-CYCLE: IO13=HIGH, restaurando VBAT...
[INFO][LTE] HARD-CYCLE: Esperando 1000ms para estabilización...
[INFO][LTE] HARD-CYCLE: IO13 restaurado a INPUT (modo ADC seguro)
[INFO][LTE] HARD-CYCLE: Ciclo completado, listo para PWRKEY
[VERBOSE][LTE] PWRKEY toggled
[INFO][LTE] SUCCESS: Modem recuperado con hard power cycle
```

### Criterios de Aceptación

- [ ] Hard power cycle se ejecuta solo después de N intentos PWRKEY fallidos
- [ ] VBAT colapsa a <0.5V durante el ciclo (medición con multímetro)
- [ ] VBAT se recupera a ~3.8V tras IO13 HIGH (medición con multímetro)
- [ ] Modem responde AT después del hard cycle + PWRKEY
- [ ] GPIO13 se restaura a INPUT tras el hard cycle (no afecta lectura ADC)
- [ ] Con flag `ENABLE_FIX_V2_HARD_POWER_CYCLE=0`, se usa `powerOn()` original
- [ ] Operación normal (modem que responde a PWRKEY) no ejecuta hard cycle
- [ ] No introduce latencia adicional en ciclo normal

---

## 🔗 RELACIÓN CON OTROS FIXs

| Fix | Relación |
|-----|----------|
| **FIX-V1** | Complementario: logging estandarizado mejora trazabilidad del hard cycle |
| **JAMR_4.5 FIX-V6** | Base: secuencia PWRKEY correcta (ya implementada en sensores_4.5_luz) |
| **JAMR_4.5 FIX-V7** | Evolución: FIX-V2 resuelve zombies tipo B que V7 no podía |
| **JAMR_4.5 FIX-V9** | **Origen directo**: Este fix es el port de FIX-V9 a sensores_4.5_luz |

---

## 📝 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
| 2026-02-10 | Documentación inicial, portada desde JAMR_4.5 FIX-V9 con adaptaciones para sensores_4.5_luz | 1.0 |
| 2026-02-10 | Revisión de precisión: código ANTES exacto de powerOn() líneas 38-73, PASO 5 agregado a hardPowerCycle() (restaurar GPIO13→INPUT para ADC), flujo de decisión actualizado | 1.1 |

---

## 🔗 REFERENCIAS

- [JAMR_4.5/fixs-feats/fixs/FIX_V9_HARD_POWER_CYCLE.md](../../../JAMR_4.5/fixs-feats/fixs/FIX_V9_HARD_POWER_CYCLE.md) — Fix original
- [JAMR_4.5/fixs-feats/fixs/FIX_V7_ZOMBIE_MITIGATION.md](../../../JAMR_4.5/fixs-feats/fixs/FIX_V7_ZOMBIE_MITIGATION.md) — Mitigación previa
- [REQUIREMENTS.md](../../REQUIREMENTS.md) — Requisitos del sistema (LTE-06, LTE-09, REL-09)
- [calidad/logs/test modem 2026-02-05 08-48-24-876.txt](../../../JAMR_4.5/calidad/logs/test%20modem%202026-02-05%2008-48-24-876.txt) — Log con evidencia del fallo
- Datasheet MIC2288: EN threshold 1.2V típico
- Datasheet SIM7080G: PWRKEY timing >1s ON, >1.2s OFF
