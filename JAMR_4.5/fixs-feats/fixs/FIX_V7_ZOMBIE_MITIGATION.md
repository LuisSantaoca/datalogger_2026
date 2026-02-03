# FIX-V7: Mitigación de Estado Zombie del Modem SIM7080G

**Versión del documento:** 1.1 (Robustecido)

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V7 |
| **Tipo** | Fix (Mitigación de fallo) |
| **Sistema** | LTE / Modem |
| **Archivo Principal** | `src/data_lte/LTEModule.cpp` |
| **Estado** | 📋 Documentado |
| **Fecha Identificación** | 2026-02-03 |
| **Versión Target** | v2.9.0 |
| **Branch** | `fix-v7-zombie-mitigation` |
| **Depende de** | FIX-V6 (MODEM_POWER_SEQUENCE), FEAT-V7 (ProductionDiag) |
| **Prioridad** | **CRÍTICA** |

---

## ✅ CUMPLIMIENTO DE PREMISAS

| Premisa | Descripción | Cumplimiento | Evidencia |
|---------|-------------|--------------|-----------|
| **P1** | Aislamiento total | ✅ | Branch: `fix-v7-zombie-mitigation` |
| **P2** | Cambios mínimos | ✅ | Solo 4 archivos: FeatureFlags.h, LTEModule.cpp, config_production_diag.h, version_info.h |
| **P3** | Defaults seguros | ✅ | Si fix falla, bloque `#else` ejecuta código original probado |
| **P4** | Feature flags | ✅ | `ENABLE_FIX_V7_ZOMBIE_MITIGATION` con valor 1/0 |
| **P5** | Logging exhaustivo | ✅ | Formato `[LTE] NIVEL: mensaje` en cada operación |
| **P6** | No cambiar lógica existente | ✅ | Código original preservado en `#else` |
| **P7** | Testing gradual | ✅ | Plan de 5 capas documentado abajo |
| **P8** | Métricas objetivas | ✅ | Tabla comparativa baseline vs esperado |
| **P9** | Rollback plan | ✅ | Flag a 0 + recompilar (<5 min) |

---

## 🎯 OBJETIVO

> **Mitigar el estado "zombie" del modem SIM7080G implementando estrategias de recuperación por firmware que reduzcan la frecuencia del problema mientras se soluciona el hardware.**

⚠️ **IMPORTANTE:** Este fix es una **MITIGACIÓN**, no una solución definitiva. La solución definitiva requiere modificación de hardware (control de alimentación del modem).

---

## 🧠 MODELO CONCEPTUAL DEL FALLO

### Estados operativos abstractos del modem

El estado "zombie" no es un fallo binario, sino un conjunto de estados internos no observables directamente desde el MCU.

```
┌─────────────────────────────────────────────────────────────┐
│  S0: OFF          │ Sin alimentación o sin inicialización   │
├─────────────────────────────────────────────────────────────┤
│  S1: BOOTING      │ Alimentado, no listo para UART          │
├─────────────────────────────────────────────────────────────┤
│  S2: RESPONSIVE   │ Responde a comandos AT                  │
├─────────────────────────────────────────────────────────────┤
│  S3: UNRESPONSIVE │ No responde a AT ni PWRKEY              │
└─────────────────────────────────────────────────────────────┘
```

**FIX-V7 actúa sobre transiciones S1→S2 y S3→S2.**  
**FIX-V7 NO puede forzar transición S3→S0** (requiere power cycle físico).

### Clasificación operacional de estados zombie

| Tipo | Estado interno | Causa probable | Recuperable por FW? |
|------|----------------|----------------|---------------------|
| **A** | S1 (BOOTING) tardío | PSM, autobaud, UART no lista | ✅ SÍ |
| **B** | S3 (UNRESPONSIVE) permanente | Latch-up, brown-out, corrupción | ❌ NO |

**Hipótesis operacional (no certeza):** PSM es la causa más consistente con la evidencia disponible para fallos tipo A.

---

## 📐 CONTRATO DE isAlive()

### Definición actual (código existente)

```cpp
bool LTEModule::isAlive() {
    clearBuffer();
    for (int i = 0; i < 3; i++) {      // 3 reintentos internos
        _serial.println("AT");
        if (waitForOK(1000)) {         // 1s timeout por intento
            return true;
        }
        delay(300);
    }
    return false;
}
```

### Semántica operacional

| Condición | Significado |
|-----------|-------------|
| `isAlive() == true` | Modem respondió "OK" a un comando AT dentro del timeout |
| `isAlive() == false` | Ausencia de respuesta en 3 intentos (3×1.3s ≈ 4s) |

**Importante:** `isAlive()` ya implementa reintentos internos. FIX-V7 **NO debe agregar reintentos adicionales de AT antes de llamar a `isAlive()`** para evitar redundancia.

### Garantías y límites

| Garantía | Límite |
|----------|--------|
| Falso positivo es poco probable | Falso negativo es posible (modem lento, ruido) |
| Timeout total conocido (~4s) | No distingue entre S1 (booting) y S3 (corrupto) |

---

## 🔧 ALCANCE DEL RESET POR PWRKEY (>12.6s)

### Qué hace (según SIMCOM datasheet)

> "After the PWRKEY continues to pull down more than 12S, the system will automatically reset."

- Reinicia el firmware interno del modem
- Fuerza reinicialización de stack de protocolos

### Qué NO hace

- **NO reinicia el dominio de alimentación** (VBAT sigue alimentado)
- **NO elimina latch-up** en transistores de potencia
- **NO corrige brown-out persistente** ni estados eléctricos anómalos
- **NO garantiza recuperación** de estados tipo B

### Dependencia crítica: Polaridad PWRKEY

El reset depende de que `LTE_PWRKEY_ACTIVE_HIGH` esté correctamente configurado:

```cpp
digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? HIGH : LOW);
```

**Validación obligatoria antes de despliegue:** Medir con osciloscopio/multímetro el nivel real en pin PWRKEY del SIM7080G durante el pulso. Si la polaridad está invertida, este reset no hará nada.

---

## 📚 EVIDENCIA Y FUENTES

| Fuente | Hallazgo relevante |
|--------|-------------------|
| M5Stack Community | "First AT after PSM wake is always lost" |
| GitHub botletics #322 | Modem no responde, PWRKEY inefectivo, requiere desconexión física |
| SIMCOM HW Design V1.04 | Reset forzado tras PWRKEY >12s |
| LilyGo-T-SIM7080G #164 | Problema idéntico documentado |

**Nota:** Estas fuentes sugieren PSM como causa probable, pero no son concluyentes para esta plataforma específica.

---

## 🔍 DIAGNÓSTICO

### Problema: Modem en Estado "Zombie"

El modem SIM7080G deja de responder a comandos AT y a la secuencia PWRKEY. La única forma de recuperación documentada es desconexión física de la batería.

### Qué es PSM (Power Saving Mode)

PSM es el modo de ahorro de energía del SIM7080G donde el modem entra en estado de ultra bajo consumo (~3µA). **El problema:** al despertar, el primer comando AT puede perderse.

```
ACTIVO ──► IDLE ──► PSM ──► ACTIVO
 ~200mA    ~10mA    ~3µA    (primer AT perdido)
```

**Hipótesis operacional:** PSM es la causa más consistente con fallos tipo A, pero no es la única posible (autobaud, UART no lista, ruido).

---

## 🔧 SOLUCIÓN SIMPLIFICADA

### Principio: No agregar complejidad innecesaria

**Hallazgo crítico:** `isAlive()` ya implementa 3 reintentos de AT con 1s timeout cada uno.

```cpp
// Código ACTUAL - NO MODIFICAR
bool LTEModule::isAlive() {
    clearBuffer();
    for (int i = 0; i < 3; i++) {      // Ya reintenta 3 veces
        _serial.println("AT");
        if (waitForOK(1000)) return true;
        delay(300);
    }
    return false;
}
```

**Decisión:** FIX-V7 **NO agrega reintentos de AT adicionales antes de `isAlive()`**. Solo agrega:
1. Más intentos de PWRKEY
2. Verificación/deshabilitación de PSM
3. Reset forzado como último recurso

### Feature Flag en `FeatureFlags.h`

```cpp
/**
 * FIX-V7: Mitigación de estado zombie del modem
 * Sistema: LTE/Modem
 * Archivo: LTEModule.cpp
 * Descripción: Estrategia de recuperación por capas:
 *   - Más intentos de PWRKEY (3x) con isAlive() entre cada uno
 *   - Deshabilitar PSM (AT+CPSMS=0) tras primera respuesta
 *   - Verificar PSM deshabilitado (AT+CPSMS?)
 *   - Reset forzado (>12.6s) como último recurso
 * Limitación: NO soluciona zombies tipo B (latch-up/eléctrico)
 * Documentación: fixs-feats/fixs/FIX_V7_ZOMBIE_MITIGATION.md
 */
#define ENABLE_FIX_V7_ZOMBIE_MITIGATION    1

// Parámetros (conservadores, basados en contrato de isAlive)
#define FIX_V7_PWRKEY_ATTEMPTS             3      // Intentos antes de reset forzado
#define FIX_V7_DISABLE_PSM                 1      // Enviar AT+CPSMS=0
#define FIX_V7_VERIFY_PSM                  1      // Verificar con AT+CPSMS?
#define FIX_V7_MAX_RECOVERY_PER_BOOT       1      // Máximo 1 ciclo completo por boot
```

### Nuevo `powerOn()` en `LTEModule.cpp`

```cpp
bool LTEModule::powerOn() {
#if ENABLE_FIX_V7_ZOMBIE_MITIGATION
    // ============ [FIX-V7 START] Mitigación estado zombie (v1.1) ============
    CRASH_CHECKPOINT(CP_MODEM_POWER_ON_START);
    debugPrint("[LTE] Encendiendo SIM7080G (FIX-V7 v1.1)...");
    
    // Backoff: evitar loops de recuperación
    static RTC_DATA_ATTR uint8_t s_recoveryAttempts = 0;
    if (s_recoveryAttempts >= FIX_V7_MAX_RECOVERY_PER_BOOT) {
        debugPrint("[LTE] WARN: Ya se intentó recuperación este boot, saltando FIX-V7");
        // Fallback a comportamiento original
    }
    
    // 0. Verificar si ya está encendido (idempotencia)
    if (isAlive()) {
        debugPrint("[LTE] Modem ya esta encendido");
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);
        return true;
    }
    
    // 1. Intentos normales de PWRKEY
    //    isAlive() ya tiene 3 reintentos de AT internos (~4s cada llamada)
    for (uint8_t attempt = 0; attempt < FIX_V7_PWRKEY_ATTEMPTS; attempt++) {
        debugPrint("[LTE] Intento PWRKEY " + String(attempt + 1) + "/" + String(FIX_V7_PWRKEY_ATTEMPTS));
        
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_PWRKEY);
        togglePWRKEY();
        delay(FIX_V6_UART_READY_DELAY_MS);  // ~2.5s según datasheet
        
        // isAlive() = 3 reintentos AT × 1.3s = ~4s
        if (isAlive()) {
            debugPrint("[LTE] Modem respondio en intento PWRKEY " + String(attempt + 1));
            
            #if FIX_V7_DISABLE_PSM
            // Deshabilitar PSM
            debugPrint("[LTE] Deshabilitando PSM...");
            _serial.println("AT+CPSMS=0");
            delay(500);
            
            #if FIX_V7_VERIFY_PSM
            // Verificar que PSM quedó deshabilitado
            _serial.println("AT+CPSMS?");
            delay(500);
            String resp = "";
            while (_serial.available()) resp += (char)_serial.read();
            if (resp.indexOf("+CPSMS: 0") != -1) {
                debugPrint("[LTE] PSM deshabilitado correctamente");
            } else {
                debugPrint("[LTE] WARN: No se pudo verificar PSM: " + resp);
                #if ENABLE_FEAT_V7_PRODUCTION_DIAG
                ProdDiag::logEvent(EVT_PSM_DISABLE_FAILED, 0);
                #endif
            }
            #endif
            #endif
            
            CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);
            return true;
        }
    }
    
    // 2. ÚLTIMO RECURSO: Reset forzado por PWRKEY >12.6s
    debugPrint("[LTE] WARN: Intentos PWRKEY agotados, reset forzado 12.6s...");
    s_recoveryAttempts++;  // Marcar que ya se intentó
    
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::logEvent(EVT_MODEM_ZOMBIE_RECOVERY_ATTEMPT, s_recoveryAttempts);
    #endif
    
    // NOTA: Este reset solo reinicia firmware del modem, NO corta alimentación
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? HIGH : LOW);
    delay(FIX_V6_PWRKEY_RESET_TIME_MS);  // 13000ms (>12.6s)
    digitalWrite(LTE_PWRKEY_PIN, LTE_PWRKEY_ACTIVE_HIGH ? LOW : HIGH);
    
    delay(FIX_V6_UART_READY_DELAY_MS);
    
    // Verificar recuperación
    if (isAlive()) {
        debugPrint("[LTE] Recuperado despues de reset forzado");
        
        #if FIX_V7_DISABLE_PSM
        _serial.println("AT+CPSMS=0");
        delay(200);
        #endif
        
        #if ENABLE_FEAT_V7_PRODUCTION_DIAG
        ProdDiag::logEvent(EVT_MODEM_ZOMBIE_RECOVERED, 0);
        #endif
        
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);
        return true;
    }
    
    // 3. Estado zombie irrecuperable (tipo B)
    debugPrint("[LTE] ERROR: Modem zombie irrecuperable - requiere power cycle");
    
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::logEvent(EVT_MODEM_ZOMBIE, 0);
    #endif
    
    return false;
    
    // ============ [FIX-V7 END] ============
#else
    // Código original preservado (pre FIX-V7)
    CRASH_CHECKPOINT(CP_MODEM_POWER_ON_START);
    debugPrint("Encendiendo SIM7080G...");
    
    if (isAlive()) {
        debugPrint("Modulo ya esta encendido");
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);
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
        
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_PWRKEY);
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        CRASH_CHECKPOINT(CP_MODEM_POWER_ON_WAIT);
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                debugPrint("SIM7080G encendido correctamente!");
                CRASH_CHECKPOINT(CP_MODEM_POWER_ON_OK);
                delay(1000);
                return true;
            }
            delay(500);
        }
    }
    
    debugPrint("Error: No se pudo encender el modulo");
    return false;
#endif
}

---

## ⏱️ ANÁLISIS DE TIEMPOS

### Tiempo por intento PWRKEY

| Fase | Duración |
|------|----------|
| togglePWRKEY() | ~1.5s |
| UART_READY_DELAY | ~2.5s |
| isAlive() (3 AT × 1.3s) | ~4s |
| **Total por intento** | **~8s** |

### Tiempo total peor caso

| Escenario | Cálculo | Tiempo |
|-----------|---------|--------|
| 3 intentos PWRKEY | 3 × 8s | 24s |
| Reset forzado | 13s + 2.5s | 15.5s |
| isAlive() final | 4s | 4s |
| **Total máximo** | | **~44s** |

### Guardrail de backoff

```cpp
static RTC_DATA_ATTR uint8_t s_recoveryAttempts = 0;
if (s_recoveryAttempts >= FIX_V7_MAX_RECOVERY_PER_BOOT) {
    // Solo 1 ciclo completo por boot para evitar loops
}
```

**Justificación:** Si un ciclo de 44s no recupera el modem, repetir solo gasta batería.

---

## 📁 ARCHIVOS A MODIFICAR

| Archivo | Cambio | Líneas aprox |
|---------|--------|--------------|
| `src/FeatureFlags.h` | Agregar flag `ENABLE_FIX_V7_ZOMBIE_MITIGATION` + parámetros | +15 |
| `src/data_lte/LTEModule.cpp` | Modificar `powerOn()` con bloque FIX-V7 | +70 |
| `src/data_diagnostics/config_production_diag.h` | Agregar eventos zombie + PSM | +4 |
| `src/version_info.h` | Actualizar a v2.9.0 | +3 |

---

## ✅ CRITERIOS DE ACEPTACIÓN

| # | Criterio | Verificación |
|---|----------|--------------|
| 1 | `powerOn()` intenta PWRKEY 3 veces antes de reset | Log muestra "Intento PWRKEY N/3" |
| 2 | `isAlive()` se usa sin reintentos redundantes | No hay loop de AT antes de isAlive() |
| 3 | `AT+CPSMS=0` se envía tras primera respuesta OK | Log muestra "Deshabilitando PSM" |
| 4 | `AT+CPSMS?` verifica deshabilitación | Log muestra "+CPSMS: 0" o warning |
| 5 | Reset forzado (>12.6s) solo tras agotar intentos | Log muestra "reset forzado 12.6s" |
| 6 | Backoff evita loops infinitos | Solo 1 ciclo completo por boot |
| 7 | Eventos registrados en ProductionDiag | STATS muestra contadores |
| 8 | Código original preservado en `#else` | Rollback instantáneo posible |

---

## 🔄 ROLLBACK

```cpp
// En FeatureFlags.h cambiar:
#define ENABLE_FIX_V7_ZOMBIE_MITIGATION    0  // ← Cambiar a 0

// Recompilar y flashear
```

**Tiempo de rollback:** <5 minutos

---

## ⚠️ LIMITACIÓN ESTRUCTURAL DEL ENFOQUE POR FIRMWARE

### Lo que FIX-V7 SÍ puede hacer

| Acción | Efecto |
|--------|--------|
| Reintentar PWRKEY | Recupera modems en estado S1 (booting tardío) |
| Deshabilitar PSM | Previene futuros "falsos zombies" por AT perdido |
| Reset forzado 12.6s | Reinicia firmware interno del modem |
| Logging detallado | Produce telemetría para diagnóstico |

### Lo que FIX-V7 NO puede hacer

| Limitación | Razón técnica |
|------------|---------------|
| Cortar VBAT del modem | No hay load switch ni control de EN |
| Limpiar latch-up | Requiere interrupción de alimentación |
| Corregir brown-out persistente | Estado eléctrico anómalo |
| Garantizar 100% recuperación | Estados tipo B son irrecuperables por FW |

### Veredicto técnico

> **FIX-V7 es una tregua técnica bien pensada, no una solución definitiva.**

Compra tiempo, datos y estabilidad operativa mientras se prepara la modificación de hardware.

---

## 🧪 PLAN DE TESTING GRADUAL (P7)

### Pirámide de validación

```
         ┌─────────────┐
    5    │  Campo 7d   │  ← Condiciones reales
         └─────────────┘
        ┌───────────────┐
    4   │  Hardware 24h │  ← Estabilidad multi-ciclo
        └───────────────┘
      ┌───────────────────┐
    3 │  Hardware 1 ciclo │  ← Funcionalidad completa
      └───────────────────┘
    ┌─────────────────────────┐
  2 │  Test unitario (10 min) │  ← powerOn() en banco
    └─────────────────────────┘
  ┌───────────────────────────────┐
1 │  Compilación (2 min)          │  ← Sin errores/warnings
  └───────────────────────────────┘
```

### Capa 1: Compilación (2 min)

```bash
# Criterios de paso:
# - 0 errores
# - 0 warnings nuevos
# - RAM libre >= 80% baseline
```

### Capa 2: Test unitario (10 min)

**Test A - powerOn() con modem apagado:**
```
1. Flashear firmware
2. Forzar modem apagado (AT+CPOWD=1 o desconectar)
3. Llamar powerOn()
4. Verificar logs:
   - "[LTE] Intento PWRKEY 1/3"
   - "[LTE] Modem respondio"
   - "[LTE] Deshabilitando PSM..."
   - "[LTE] PSM deshabilitado correctamente" o warning
```

**Test B - Verificar backoff:**
```
1. Simular zombie (no responde a AT)
2. Verificar que solo se intenta 1 ciclo completo por boot
3. Logs deben mostrar "Ya se intentó recuperación"
```

### Capa 3: Hardware 1 ciclo (20 min)

- Boot → Sensores → GPS → LTE (powerOn) → Transmit → Sleep
- Verificar que ciclo completo funciona con FIX-V7 activo
- Revisar timing: powerOn() no debería exceder 44s

### Capa 4: Hardware 24h (1 día)

- 144 ciclos (10 min cada uno)
- Métricas a monitorear:
  - Reinicios inesperados: debe ser 0
  - Eventos `EVT_MODEM_ZOMBIE_RECOVERY_ATTEMPT`: contar
  - Eventos `EVT_MODEM_ZOMBIE_RECOVERED`: contar
  - Eventos `EVT_MODEM_ZOMBIE`: debe ser mínimo

### Capa 5: Campo 7 días

- Desplegar en 1-2 dispositivos de prueba
- Condiciones reales: temperatura, señal variable
- Comparar métricas vs baseline v2.7.1

### Criterios de paso

| Capa | Criterio | Acción si falla |
|------|----------|-----------------|
| 1 | 0 errores compilación | Corregir código |
| 2 | Logs correctos | Revisar lógica |
| 3 | Ciclo completo OK | Debug con serial |
| 4 | 0 reinicios, zombies recuperados | Ajustar parámetros |
| 5 | Métricas >= baseline | Rollback o ajustar |

---

## 🔬 VALIDACIONES OBLIGATORIAS ANTES DE DESPLIEGUE

### 1. Verificar polaridad PWRKEY

**Método:** Con multímetro u osciloscopio, medir nivel en pin PWRKEY del SIM7080G durante `togglePWRKEY()`.

**Resultado esperado:** Si `LTE_PWRKEY_ACTIVE_HIGH = true`, debe verse pulso HIGH de ~1.5s.

**Si falla:** Invertir macro `LTE_PWRKEY_ACTIVE_HIGH` en config_lte.h.

### 2. Verificar PSM deshabilitado

**Método:** En log de debug, confirmar que aparece `[LTE] PSM deshabilitado correctamente` o `+CPSMS: 0`.

**Si falla:** PSM puede reactivarse por configuración de red. Considerar enviar `AT+CPSMS=0` en cada conexión, no solo en powerOn.

### 3. Test de recuperación forzada

**Método:** Inducir estado zombie (desconectar TX/RX momentáneamente) y verificar que FIX-V7 intenta recuperación y eventualmente declara zombie.

**Resultado esperado:** Log muestra intentos PWRKEY, luego reset forzado, luego "zombie irrecuperable".

---

## 📊 MÉTRICAS DE ÉXITO

| Métrica | Antes (v2.7.1) | Esperado (v2.9.0) |
|---------|----------------|-------------------|
| Frecuencia de zombie irrecuperable | ~1 por semana | <1 por mes |
| Tasa de recuperación por FW | 0% | 70-80% (solo tipo A) |
| Tramas perdidas por zombie | ~50+ | <10 |
| Tiempo máximo de powerOn() | ~15s | ~44s (peor caso) |

---

## 📅 HISTORIAL

| Fecha | Versión | Cambio |
|-------|---------|--------|
| 2026-02-03 | 1.0 | Documentación inicial |
| 2026-02-03 | 1.1 | Robustecido: modelo de estados, contrato isAlive(), eliminación de sobreingeniería, backoff, verificación PSM |
| 2026-02-03 | 1.2 | Agregado: cumplimiento de premisas, plan de testing gradual (P7), branch |

---

*Fin del documento*
