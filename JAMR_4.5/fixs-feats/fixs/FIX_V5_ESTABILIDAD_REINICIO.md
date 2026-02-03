# FIX-V5: Watchdog de Sistema

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V5 |
| **Tipo** | Fix (Estabilidad y protección de sistema) |
| **Sistema** | Core / Watchdog |
| **Archivo Principal** | `AppController.cpp` |
| **Estado** | 📋 Documentado |
| **Fecha Identificación** | 2026-01-28 |
| **Versión Target** | v2.3.x |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Prioridad** | **Crítica** |
| **Premisas** | P1✅ P2✅ P3✅ P4✅ P5✅ P6✅ |

---

## 🎯 OBJETIVO

> **Garantizar que el dispositivo pueda recuperarse automáticamente de cualquier cuelgue de software.**

Sin watchdog de sistema, un cuelgue (módem zombie, I2C lock, FS corrupto) = equipo muerto hasta reset físico.

**NOTA:** Se evaluó también un "skip-cycle post-restart" para FEAT-V4, pero fue **descartado por sobreingeniería**. FIX-V3 (modo batería baja) ya protege contra brownout, y el ciclo "extra" produce datos válidos.

---

## 🔍 DIAGNÓSTICO

### Problema: Sin Watchdog de Sistema

**Descripción:**
No hay Task Watchdog Timer (TWDT) configurado. Si el módem SIM7080G entra en estado zombie o cualquier operación se cuelga indefinidamente, el sistema queda bloqueado.

**Síntomas:**
1. Equipo desaparece del dashboard
2. No responde a nada
3. Solo se recupera con reset físico o agotamiento de batería

**Causa Raíz:**
Los timeouts en código dependen de `millis()` y bucles. Si el código se cuelga antes de evaluar el timeout (o en una función bloqueante), no hay recovery.

**Escenarios de cuelgue:**
- Módem en zona de cobertura marginal, AT commands no responden
- I2C lock (sensor no libera bus)
- LittleFS operación corrupta
- Bug no descubierto en cualquier estado de la FSM

---

## 📊 EVALUACIÓN

### Impacto

| Aspecto | Evaluación |
|---------|------------|
| Criticidad | Crítica |
| Riesgo de no implementar | Equipos muertos sin recovery automático |
| Esfuerzo | Bajo (~1h) |
| Beneficio | Muy Alto |

### Justificación

En IoT de campo, la regla de oro es: **el equipo debe poder recuperarse solo de cualquier situación**. Sin watchdog, esto no está garantizado.

---

## 🔧 SOLUCIÓN

### Task Watchdog Timer (TWDT)

**Concepto:**
Configurar watchdog de FreeRTOS que resetea el ESP32 si el loop principal no hace "feed" en 60 segundos.

**Implementación:**
```cpp
#include <esp_task_wdt.h>

// En AppInit():
esp_task_wdt_init(60, true);  // 60 segundos, panic on timeout
esp_task_wdt_add(NULL);       // Agregar tarea actual (loop)

// En AppLoop() al inicio:
esp_task_wdt_reset();         // Feed del watchdog
```

**Timeout de 60 segundos porque:**
- Suficiente para operaciones individuales (GPS, LTE)
- Si algo se cuelga >60s sin progreso, es definitivamente un problema
- Balance entre recovery rápido y evitar falsos positivos

---

## 📁 ARCHIVOS A MODIFICAR

| Archivo | Cambio | Sección |
|---------|--------|---------|
| `FeatureFlags.h` | Agregar `ENABLE_FIX_V5_WATCHDOG` | Flags |
| `AppController.cpp` | Incluir `<esp_task_wdt.h>` | Includes |
| `AppController.cpp` | Inicializar TWDT | `AppInit()` |
| `AppController.cpp` | Feed TWDT | `AppLoop()` |

---

## ✅ FEATURE FLAG

```cpp
// ============================================================
// FIX-V5: Watchdog de Sistema
// ============================================================
// Protección contra cuelgues de software.
// Si AppLoop() no hace "feed" en 60 segundos, reset automático.
// 
// Con flag=1: TWDT activo, recovery automático
// Con flag=0: Sin watchdog (comportamiento anterior)
// ============================================================
#define ENABLE_FIX_V5_WATCHDOG 1

#if ENABLE_FIX_V5_WATCHDOG
  #define FIX_V5_WATCHDOG_TIMEOUT_S 60  // Segundos antes de reset automático
#endif
```

---

## 🧪 CRITERIOS DE ACEPTACIÓN

| # | Criterio | Verificación |
|---|----------|--------------|
| 1 | TWDT se inicializa en boot | Monitor serial: "[FIX-V5] Watchdog iniciado (60s)" |
| 2 | Operación normal no dispara watchdog | No resets inesperados en ciclos normales |
| 3 | Cuelgue dispara reset | Test: `while(1){}` en un estado → reset en 60s |
| 4 | Post-reset watchdog, sistema recupera | Ciclo normal después del reset |

---

## 🔄 ROLLBACK

Si hay problemas, deshabilitar con:
```cpp
#define ENABLE_FIX_V5_WATCHDOG 0
```

---

## 📊 RESUMEN DE IMPACTO

| Métrica | Sin FIX-V5 | Con FIX-V5 |
|---------|------------|------------|
| Recovery de cuelgue | Manual (visita) | Automático (60s) |
| Overhead de código | - | ~15 líneas |
| Overhead de RAM | - | Ninguno |

---

**Firma:** Documentación actualizada 2026-01-28  
**Cambio:** Eliminado skip-cycle (sobreingeniería). Solo watchdog.
