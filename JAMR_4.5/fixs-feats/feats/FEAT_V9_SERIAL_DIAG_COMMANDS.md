# FEAT-V9: Comandos Serial para Diagnóstico

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V9 |
| **Tipo** | Feature (Diagnóstico) |
| **Sistema** | Core / Diagnóstico |
| **Archivo Principal** | `AppController.cpp` |
| **Estado** | ✅ Implementado |
| **Fecha** | 2026-01-29 |
| **Versión** | v2.7.1 |
| **Depende de** | FEAT-V3 (CrashDiag), FEAT-V7 (ProductionDiag) |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado

Los sistemas de diagnóstico FEAT-V3 (CrashDiagnostics) y FEAT-V7 (ProductionDiag) **guardan datos valiosos** pero no todos son accesibles via Serial:

| Sistema | Datos Guardados | Comando Serial |
|---------|-----------------|----------------|
| FEAT-V3 | Crashes, checkpoints | ✅ `DIAG`, `HISTORY` |
| FEAT-V7 | Contadores LTE, EMI, GPS | ❌ No hay |
| FEAT-V7 | Log de eventos con epoch | ❌ No hay |

### Síntomas

1. No se puede consultar `STATS` para ver contadores acumulados (LTE OK/FAIL, EMI events)
2. No se puede consultar `LOG` para ver eventos con timestamps
3. Las funciones `ProdDiag::printStats()` y `ProdDiag::printEventLog()` existen pero no están conectadas al handler Serial

### Causa Raíz

El bloque de comandos Serial en `AppController.cpp` (~línea 1168) solo maneja comandos de FEAT-V3:
- `DIAG` → `CrashDiag::printReport()`
- `HISTORY` → `CrashDiag::printHistory()`
- `CLEAR` → `CrashDiag::clearHistory()`

Faltan comandos para FEAT-V7:
- `STATS` → `ProdDiag::printStats()`
- `LOG` → `ProdDiag::printEventLog()`

---

## 📊 EVALUACIÓN

### Impacto

| Aspecto | Evaluación |
|---------|------------|
| Criticidad | Media |
| Riesgo de no implementar | Medio - Dificulta diagnóstico de causa raíz |
| Esfuerzo | Muy Bajo (~4 líneas de código) |
| Beneficio | Alto - Acceso completo a datos de diagnóstico |

### Justificación

Para encontrar **causa raíz de errores** se necesita:
1. **STATS**: Ver tasa de éxito LTE, eventos EMI, crashes totales
2. **LOG**: Ver timeline de eventos con timestamps (cuándo ocurrió cada fallo)

Sin estos comandos, los datos se guardan pero no se pueden consultar sin modificar código.

---

## 🔧 IMPLEMENTACIÓN

### Archivos a Modificar

| Archivo | Cambio | Línea |
|---------|--------|-------|
| `AppController.cpp` | Reestructurar bloque Serial + agregar STATS/LOG | ~1165-1180 |

### Principios de Diseño (Revisión 2026-01-29)

1. **Independencia de flags:** El lector Serial NO debe depender de FEAT-V3. Cada comando se condiciona por su propio flag.
2. **Prevención de bloqueos:** Usar `Serial.setTimeout(50)` para evitar congelar el ciclo si el usuario abre Serial Monitor sin enviar comando.
3. **Case-insensitive:** Mantener `trim()` y `toUpperCase()` para compatibilidad con diferentes monitores.

### Código a Implementar

**Ubicación:** `AppController.cpp`, reemplazar el bloque actual de comandos Serial (~líneas 1165-1180):

```cpp
// ============ [FEAT-V9 START] Comandos Serial Diagnóstico ============
// Lector Serial INDEPENDIENTE de flags específicos
if (Serial.available()) {
    Serial.setTimeout(50);  // Prevenir bloqueo por monitor serial abierto
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    // Comandos FEAT-V3: CrashDiagnostics
    #if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
    if (cmd == "DIAG") {
        CrashDiag::printReport();
    } else if (cmd == "HISTORY") {
        CrashDiag::printHistory();
    } else if (cmd == "CLEAR") {
        CrashDiag::clearHistory();
    }
    #endif
    
    // Comandos FEAT-V7: ProductionDiag
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    if (cmd == "STATS") {
        ProdDiag::printStats();
    } else if (cmd == "LOG") {
        ProdDiag::printEventLog();
    }
    #endif
}
// ============ [FEAT-V9 END] ============
```

### Cambios vs Código Original

| Aspecto | Antes | Después |
|---------|-------|---------|
| Scope del lector | Dentro de `#if FEAT_V3` | Independiente |
| setTimeout | No tenía | `50ms` |
| Comandos V7 | No existían | `STATS`, `LOG` |

### Feature Flag

**NO requiere flag propio** - usa los flags existentes:
- `ENABLE_FEAT_V3_CRASH_DIAGNOSTICS` (ya activo) - para DIAG/HISTORY/CLEAR
- `ENABLE_FEAT_V7_PRODUCTION_DIAG` (ya activo) - para STATS/LOG

**Beneficio:** Si se compila sin V3 pero con V7, los comandos STATS/LOG siguen disponibles.

---

## 🧪 VERIFICACIÓN

### Comandos Disponibles Después de Implementar

| Comando | Sistema | Output |
|---------|---------|--------|
| `DIAG` | FEAT-V3 | Reporte de crash actual |
| `HISTORY` | FEAT-V3 | Log de crashes (LittleFS) |
| `CLEAR` | FEAT-V3 | Borra historial crashes |
| **`STATS`** | **FEAT-V7** | **Tabla de contadores** |
| **`LOG`** | **FEAT-V7** | **Eventos con timestamp** |

### Output Esperado - Comando `STATS`

```
╔══════════════════════════════════════╗
║  DIAGNÓSTICO PRODUCCIÓN v2.7.0       ║
╠══════════════════════════════════════╣
║  CICLOS:
║    Total: 431
║    Desde boot: 23
╠══════════════════════════════════════╣
║  LTE:
║    Enviadas OK: 419 (97.2%)
║    Fallidas: 12
║    AT Timeouts: 156
║    Op. Fallbacks: 3
╠══════════════════════════════════════╣
║  BATERÍA:
║    Ciclos en reposo: 0
║    Eventos low-bat: 0
╠══════════════════════════════════════╣
║  EMI STATUS:
║    AT Commands: 4310
║    Corrupted: 0 (0.000%)
║    Verdict: PCB OK
╠══════════════════════════════════════╣
║  SISTEMA:
║    Restarts 24h: 0
║    Crashes: 12
║    GPS Fails: 1
╚══════════════════════════════════════╝
```

### Output Esperado - Comando `LOG`

```
=== LOG DE EVENTOS ===
Formato: epoch,código,dato
Códigos: B=Boot L=LTE_Fail F=Fallback E=LowBat_Enter X=LowBat_Exit
         R=Restart24h G=GPS_Fail I=EMI S=EMI_Severe C=Crash T=AT_Timeout
---
1769637810,B,P
1769637810,G,100
1769638500,C,102
1769651447,F,T
1769652000,B,B
=== FIN LOG ===
```

### Criterios de Aceptación

- [ ] Comando `STATS` imprime tabla de contadores
- [ ] Comando `LOG` imprime eventos con timestamps
- [ ] Comandos son case-insensitive (stats, STATS, Stats)
- [ ] No afecta funcionamiento de comandos existentes (DIAG, HISTORY, CLEAR)
- [ ] Compila sin warnings

---

## 📊 VALOR PARA DIAGNÓSTICO DE CAUSA RAÍZ

### Con STATS puedes responder:

| Pregunta | Dato en STATS |
|----------|---------------|
| ¿Cuántos crashes ha tenido? | `Crashes: 12` |
| ¿Tasa de éxito LTE? | `Enviadas OK: 97.2%` |
| ¿Hay problema de EMI? | `Verdict: PCB OK` |
| ¿Cuántos timeouts AT? | `AT Timeouts: 156` |
| ¿Problemas de batería? | `Eventos low-bat: 0` |

### Con LOG puedes responder:

| Pregunta | Cómo encontrar en LOG |
|----------|----------------------|
| ¿Cuándo crasheó? | Línea con `C,checkpoint` + epoch |
| ¿Después de qué evento? | Línea anterior al crash |
| ¿Patrón de fallos? | Secuencia de eventos `L` (LTE fail) |
| ¿Correlación con hora del día? | Convertir epoch a fecha |

---

## 📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
| 2026-01-29 | Documentación creada | - |
| 2026-01-29 | Revisión: separar scope V3/V7, agregar setTimeout(50ms) | - |
| 2026-01-29 | ✅ Implementación comandos STATS/LOG | v2.7.1 |
| 2026-01-29 | 🐛 Bugfix DEEPSLEEP: casos 5,8 en setResetReason() | v2.7.1 |
| 2026-01-29 | 🐛 Bugfix epoch=0: añadido setCurrentEpoch() | v2.7.1 |

---

## 🐛 BUGFIXES APLICADOS

### Bug 1: DEEPSLEEP mostraba 'U' (Unknown)

**Síntoma:** Comando `LOG` mostraba `85` (ASCII 'U') en lugar de `68` (ASCII 'D') para boots desde deep sleep.

**Causa:** `setResetReason()` no tenía cases para `ESP_SLEEP_WAKEUP_DEEP_SLEEP` (5) ni el caso alternativo (8).

**Fix:** Añadidos `case 5:` y `case 8:` → `bootChar = 'D'`

### Bug 2: Todos los eventos tenían epoch=0

**Síntoma:** Comando `LOG` mostraba `0,B,68` - epoch siempre cero.

**Causa:** `g_lastKnownEpoch` solo se actualizaba en `saveStats()` (antes de deep sleep), pero eventos se registran antes de obtener timestamp RTC.

**Fix:** 
1. Nueva función `ProdDiag::setCurrentEpoch(uint32_t epoch)`
2. Se llama desde `Cycle_BuildFrame` después de `getEpochString()`
3. Eventos posteriores al primer ciclo tendrán timestamp correcto

**Nota:** Eventos de BOOT siempre tendrán epoch=0 porque se registran antes de obtener el timestamp RTC - esto es comportamiento esperado.
