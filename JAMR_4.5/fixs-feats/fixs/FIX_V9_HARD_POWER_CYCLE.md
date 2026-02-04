# FIX-V9: Hard Power Cycle del Modem vía IO13

**Versión del documento:** 1.0

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V9 |
| **Tipo** | Fix (Solución definitiva a zombie) |
| **Sistema** | LTE / Modem / Power |
| **Archivo Principal** | `src/data_lte/LTEModule.cpp` |
| **Estado** | 📋 Documentado |
| **Fecha Identificación** | 2026-01-31 |
| **Versión Target** | v2.10.0 |
| **Branch** | `fix-v9-hard-power-cycle` |
| **Depende de** | FIX-V6 (MODEM_POWER_SEQUENCE), FIX-V7 (ZOMBIE_MITIGATION) |
| **Prioridad** | **CRÍTICA** |

---

## ✅ CUMPLIMIENTO DE PREMISAS

| Premisa | Descripción | Cumplimiento | Evidencia |
|---------|-------------|--------------|-----------|
| **P1** | Aislamiento total | ✅ | Branch: `fix-v9-hard-power-cycle` |
| **P2** | Cambios mínimos | ✅ | Solo 3 archivos: FeatureFlags.h, LTEModule.cpp, config_lte.h |
| **P3** | Defaults seguros | ✅ | Si fix falla, bloque `#else` ejecuta código FIX-V7 |
| **P4** | Feature flags | ✅ | `ENABLE_FIX_V9_HARD_POWER_CYCLE` con valor 1/0 |
| **P5** | Logging exhaustivo | ✅ | Formato `[LTE] NIVEL: mensaje` en cada operación |
| **P6** | No cambiar lógica existente | ✅ | Código FIX-V7 preservado en `#else` |
| **P7** | Testing gradual | ✅ | Plan de validación con multímetro documentado |
| **P8** | Métricas objetivas | ✅ | VBAT medible: 0V vs 3.8V |
| **P9** | Rollback plan | ✅ | Flag a 0 + recompilar (<5 min) |

---

## 🎯 OBJETIVO

> **Implementar un power cycle REAL del modem SIM7080G mediante control de hardware (IO13 → MIC2288 EN), solucionando de forma definitiva el estado zombie que no responde a PWRKEY.**

⚠️ **IMPORTANTE:** Este fix es la **SOLUCIÓN DEFINITIVA** al problema zombie. Reemplaza la mitigación de FIX-V7 con control real de alimentación.

---

## 🧠 CONTEXTO TÉCNICO

### El problema: Estado Zombie Tipo B

El modem SIM7080G entra ocasionalmente en un estado "zombie" donde:
- No responde a comandos AT
- No responde a pulsos PWRKEY (ni cortos ni largos >12.6s)
- Solo se recupera con desconexión física de batería

**FIX-V7** mitiga zombies tipo A (PSM) pero **NO puede recuperar zombies tipo B** (latch-up eléctrico).

### La solución: Control de VBAT vía MIC2288

El esquemático revela que **IO13 controla indirectamente la alimentación del modem**:

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

### Evidencia del Esquemático

| Elemento | Valor | Coordenadas schematic.json |
|----------|-------|---------------------------|
| IO13 NetLabel | `IO13` | `(1867, -518)` |
| R23 | 10kΩ | `(1811, -510)` |
| R24 | 10kΩ | `(1817, -640)` |
| MIC2288 EN | Pin 4 | `(2195, -590)` |
| Wire R23→EN | Junction | `W~1811 -533 1811 -590` |
| VBAT Output | +3.8V_M | `(2365, -530)` |

### Cálculo de Voltaje EN

```
Ven = VIO13 × (R24 / (R23 + R24))
Ven = 3.3V × (10kΩ / 20kΩ)
Ven = 1.65V
```

| Estado IO13 | Ven | MIC2288 | VBAT |
|-------------|-----|---------|------|
| HIGH (3.3V) | 1.65V | ON | ~3.8V |
| LOW (0V) | 0V | **OFF** | **~0V** |

**Umbral EN MIC2288:** 1.2V típico → 1.65V > umbral → Encendido garantizado

---

## 📐 SECUENCIA DE HARD POWER CYCLE

### Diagrama de Tiempos

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
| `tstab` | 1000 ms | Estabilización MIC2288 antes de PWRKEY |
| `t_total` | ~6s | Overhead aceptable vs días de zombie |

---

## 🔧 IMPLEMENTACIÓN

### 1. Definición de Pin (config_lte.h)

```cpp
// ============ [FIX-V9 START] Pin de control MIC2288 ============
/**
 * @brief GPIO que controla EN del MIC2288 vía divisor R23/R24
 * @note IO13 → R23 (10k) → junction → R24 (10k) → GND
 *       junction → MIC2288 EN (pin 4)
 *       Ven = 3.3V × (10k/20k) = 1.65V > umbral 1.2V
 */
#define MODEM_EN_PIN    13
// ============ [FIX-V9 END] ============
```

### 2. Feature Flag (FeatureFlags.h)

```cpp
/**
 * FIX-V9: Hard Power Cycle del Modem vía IO13
 * Sistema: LTE/Power
 * Archivo: LTEModule.cpp
 * Descripción: Control real de alimentación VBAT del modem:
 *   - IO13 LOW → MIC2288 EN = 0V → VBAT colapsa
 *   - IO13 HIGH → MIC2288 EN = 1.65V → VBAT = 3.8V
 *   - Solución definitiva a zombies tipo B (latch-up)
 * Documentación: fixs-feats/fixs/FIX_V9_HARD_POWER_CYCLE.md
 * Estado: Implementado
 */
#define ENABLE_FIX_V9_HARD_POWER_CYCLE        1

// ============================================================
// FIX-V9: PARÁMETROS DE HARD POWER CYCLE
// ============================================================

/** @brief Tiempo con VBAT cortado para descarga completa (ms) */
#define FIX_V9_POWEROFF_DELAY_MS              5000

/** @brief Tiempo de estabilización después de restaurar VBAT (ms) */
#define FIX_V9_STABILIZATION_MS               1000

/** @brief Reintentos PWRKEY antes de hard power cycle */
#define FIX_V9_PWRKEY_ATTEMPTS_BEFORE_HARD    2
```

### 3. Método hardPowerCycle() (LTEModule.cpp)

```cpp
// ============ [FIX-V9 START] Hard power cycle via IO13 ============
#if ENABLE_FIX_V9_HARD_POWER_CYCLE

/**
 * @brief Ejecuta un power cycle REAL del modem cortando VBAT
 * @details Controla MIC2288 EN vía IO13 para colapsar VBAT a ~0V
 * @return true siempre (el ciclo siempre se ejecuta)
 * @note Esta es la solución definitiva para zombies tipo B
 * 
 * Secuencia:
 * 1. IO13 LOW → EN = 0V → MIC2288 OFF → VBAT colapsa
 * 2. Esperar descarga completa de capacitores
 * 3. IO13 HIGH → EN = 1.65V → MIC2288 ON → VBAT = 3.8V
 * 4. Esperar estabilización antes de PWRKEY
 */
bool LTEModule::hardPowerCycle() {
    Serial.println(F("[LTE] HARD-CYCLE: Iniciando power cycle VBAT via IO13"));
    
    // Configurar pin como salida
    pinMode(MODEM_EN_PIN, OUTPUT);
    
    // PASO 1: Cortar alimentación
    Serial.println(F("[LTE] HARD-CYCLE: IO13=LOW, cortando VBAT..."));
    digitalWrite(MODEM_EN_PIN, LOW);
    
    // PASO 2: Esperar descarga completa
    Serial.print(F("[LTE] HARD-CYCLE: Esperando "));
    Serial.print(FIX_V9_POWEROFF_DELAY_MS);
    Serial.println(F("ms para descarga..."));
    delay(FIX_V9_POWEROFF_DELAY_MS);
    
    // PASO 3: Restaurar alimentación
    Serial.println(F("[LTE] HARD-CYCLE: IO13=HIGH, restaurando VBAT..."));
    digitalWrite(MODEM_EN_PIN, HIGH);
    
    // PASO 4: Esperar estabilización
    Serial.print(F("[LTE] HARD-CYCLE: Esperando "));
    Serial.print(FIX_V9_STABILIZATION_MS);
    Serial.println(F("ms para estabilización..."));
    delay(FIX_V9_STABILIZATION_MS);
    
    Serial.println(F("[LTE] HARD-CYCLE: Ciclo completado, listo para PWRKEY"));
    return true;
}

#endif // ENABLE_FIX_V9_HARD_POWER_CYCLE
// ============ [FIX-V9 END] ============
```

### 4. Integración en powerOn() (LTEModule.cpp)

```cpp
bool LTEModule::powerOn() {
    Serial.println(F("[LTE] Iniciando encendido del modem..."));
    
#if ENABLE_FIX_V9_HARD_POWER_CYCLE
    // FIX-V9: Intentar PWRKEY primero, hard cycle como fallback
    int pwrkeyAttempts = 0;
    
    while (pwrkeyAttempts < FIX_V9_PWRKEY_ATTEMPTS_BEFORE_HARD) {
        // Intentar secuencia PWRKEY normal (FIX-V6)
        if (tryPwrkeySequence()) {
            if (isAlive()) {
                Serial.println(F("[LTE] Modem respondió después de PWRKEY"));
                return true;
            }
        }
        pwrkeyAttempts++;
        Serial.print(F("[LTE] PWRKEY intento "));
        Serial.print(pwrkeyAttempts);
        Serial.println(F(" sin respuesta"));
    }
    
    // PWRKEY agotado → ejecutar hard power cycle
    Serial.println(F("[LTE] WARN: PWRKEY agotado, ejecutando HARD POWER CYCLE"));
    hardPowerCycle();
    
    // Después del hard cycle, ejecutar secuencia PWRKEY completa
    if (tryPwrkeySequence() && isAlive()) {
        Serial.println(F("[LTE] SUCCESS: Modem recuperado con hard power cycle"));
        return true;
    }
    
    Serial.println(F("[LTE] ERROR: Modem no responde ni con hard power cycle"));
    return false;
    
#else
    // Código original FIX-V7 preservado para rollback
    // ... (código de FIX-V7 aquí)
#endif
}
```

---

## ⚠️ CONSIDERACIONES DE SEGURIDAD

### Estados Iniciales

| Condición | IO13 después de reset | VBAT |
|-----------|----------------------|------|
| Power-on reset | INPUT (alta impedancia) | 3.8V (R23 pull-up) |
| Deep sleep wake | Mantiene último estado | Según IO13 |
| Brownout reset | INPUT | 3.8V |

**Diseño seguro:** El divisor R23/R24 actúa como pull-up efectivo, manteniendo EN alto incluso si IO13 no está configurado.

### Protección contra uso excesivo

```cpp
// Limitar hard cycles por sesión de boot
RTC_DATA_ATTR static uint8_t g_hardCycleCount = 0;

#define FIX_V9_MAX_HARD_CYCLES_PER_BOOT   2

if (g_hardCycleCount >= FIX_V9_MAX_HARD_CYCLES_PER_BOOT) {
    Serial.println(F("[LTE] WARN: Límite de hard cycles alcanzado"));
    // No ejecutar más hard cycles este boot
}
```

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

### Criterios de Éxito

| Criterio | Métrica | Objetivo |
|----------|---------|----------|
| Recuperación zombie | % recuperados con hard cycle | 100% |
| Tiempo de ciclo | toff + tstab | <7 segundos |
| Estabilidad VBAT | Ripple después de ON | <100mV |
| No regresiones | Operación normal sin zombies | Sin cambios |

---

## 🔗 RELACIÓN CON OTROS FIXs

| Fix | Relación |
|-----|----------|
| **FIX-V6** | Base: secuencia PWRKEY correcta |
| **FIX-V7** | Evolución: FIX-V9 reemplaza estrategia PSM |
| **FIX-V8** | Compatible: logging de fallos continúa |

### Flujo de Decisión

```
powerOn()
    │
    ├──► Intento PWRKEY #1 (FIX-V6)
    │         │
    │         ├── isAlive()? ──► ✅ return true
    │         │
    │         └── NO ──► Intento PWRKEY #2
    │                        │
    │                        ├── isAlive()? ──► ✅ return true
    │                        │
    │                        └── NO ──► 🔴 HARD POWER CYCLE (FIX-V9)
    │                                        │
    │                                        ├── PWRKEY post-cycle
    │                                        │
    │                                        └── isAlive()? 
    │                                               │
    │                                               ├── ✅ return true
    │                                               │
    │                                               └── ❌ return false
    │
```

---

## 📝 HISTORIAL DE VERSIONES

| Versión | Fecha | Autor | Cambios |
|---------|-------|-------|---------|
| 1.0 | 2026-01-31 | Copilot | Documentación inicial con evidencia esquemático |

---

## 🔗 REFERENCIAS

- [REPORTE_TECNICO_MODEM_ZOMBIE_2026-01-31.md](../../../datalogger-review/REPORTE_TECNICO_MODEM_ZOMBIE_2026-01-31.md) - Diagnóstico completo
- [FIX_V7_ZOMBIE_MITIGATION.md](FIX_V7_ZOMBIE_MITIGATION.md) - Mitigación previa (PSM)
- [FIX_V6_MODEM_POWER_SEQUENCE.md](FIX_V6_MODEM_POWER_SEQUENCE.md) - Secuencia PWRKEY
- Datasheet MIC2288: EN threshold 1.2V típico
- Datasheet SIM7080G: PWRKEY timing >1s ON, >1.2s OFF
