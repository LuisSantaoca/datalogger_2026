# FEAT-V7: Diagnóstico de Producción

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V7 |
| **Tipo** | Feature (Diagnóstico) |
| **Sistema** | Core / LittleFS / LTEModule |
| **Archivos** | `AppController.cpp`, `LTEModule.cpp`, `ProductionDiag.h/.cpp` |
| **Estado** | ✅ Implementado |
| **Fecha** | 2026-01-28 |
| **Versión** | v2.7.0+ |
| **Depende de** | FEAT-V3 (Crash Diagnostics) |

---

## 🔍 PROBLEMA

- DEBUG_EMI_DIAGNOSTIC es verbose (hex dumps) - no apto para producción continua
- Sin forma de diagnosticar problemas post-mortem en campo
- Datos de salud del sistema no persisten entre power cycles

---

## 🎯 OBJETIVO

Sistema de diagnóstico ligero, siempre activo, para:
1. Acumular estadísticas de operación (LTE, batería, EMI)
2. Registrar eventos críticos con timestamp
3. Revisar estado al conectar Serial en campo
4. Detectar EMI/ruido sin overhead significativo

---

## 📐 DISEÑO

### Estructura de archivos

```
/diag/
  stats.bin      ← Contadores binarios (~48 bytes)
  events.txt     ← Log circular de eventos (~2KB, max 20 líneas)
```

### Estructura de datos (stats.bin)

```cpp
// Persistente en LittleFS - se actualiza cada ciclo
struct ProductionStats {
    // Identificador y versión
    uint32_t magic;              // 0xDIAG0001 - detectar corrupción
    uint8_t  version;            // Versión del struct (migración futura)
    
    // Contadores de ciclo
    uint32_t totalCycles;        // Ciclos desde primera instalación
    uint32_t cyclesSinceBoot;    // Ciclos desde último power-on
    
    // Contadores LTE
    uint32_t lteSendOk;          // Tramas enviadas OK
    uint32_t lteSendFail;        // Intentos fallidos
    uint16_t atTimeouts;         // Timeouts AT acumulados
    uint16_t operatorFallbacks;  // Fallbacks de operadora
    
    // Contadores batería
    uint16_t lowBatteryCycles;   // Ciclos en modo reposo
    uint16_t lowBatteryEvents;   // Veces que entró a reposo
    
    // Contadores sistema
    uint8_t  feat4Restarts;      // Reinicios periódicos 24h
    uint8_t  crashCount;         // Crashes detectados (de FEAT-V3)
    uint8_t  lastResetReason;    // esp_reset_reason_t
    
    // === CONTADORES EMI ===
    uint32_t atCommandsTotal;    // Total comandos AT ejecutados
    uint32_t atCorrupted;        // Respuestas con bytes inválidos
    uint32_t invalidCharsTotal;  // Bytes 0xFF/0x00/fuera de rango
    uint8_t  worstCorruptPct;    // Peor % corrupción (0-100, x1)
    uint8_t  emiEvents;          // Veces que corrupción > 1%
    
    // Timestamp
    uint32_t lastUpdateEpoch;    // Último update
    
    // Checksum
    uint16_t crc16;              // Validación de integridad
};
```

### Eventos a registrar (events.txt)

| Código | Evento | Dato |
|--------|--------|------|
| `B` | BOOT | Reset reason (P=poweron, W=wdt, S=software, B=brownout) |
| `L` | LTE_FAIL | Operadora (M=Movistar, T=Telcel, A=ATT) |
| `F` | OP_FALLBACK | Nueva operadora seleccionada |
| `E` | LOW_BAT_ENTER | vBat (centésimas: 318 = 3.18V) |
| `X` | LOW_BAT_EXIT | Ciclos en reposo |
| `R` | RESTART_24H | Ciclos antes del restart |
| `G` | GPS_FAIL | Reintentos |
| `I` | EMI_DETECTED | % corrupción (x10: 15 = 1.5%) |
| `S` | EMI_SEVERE | % corrupción (x10) |
| `C` | CRASH | Checkpoint donde ocurrió |

### Formato de log

```
1706428800,B,P
1706429100,L,M
1706429100,F,T
1706515200,E,318
1706537400,X,38
1706600000,I,15
```

Formato: `epoch,código,dato` - compacto, parseable

---

## 🔧 IMPLEMENTACIÓN

### Detección EMI ligera (sin verbose)

```cpp
// En LTEModule::sendCommand() - solo contar, no log
inline bool isInvalidATChar(uint8_t c) {
    return (c == 0xFF || c == 0x00 || (c > 0x7E && c != 0x0D && c != 0x0A));
}

void countEMI(const String& response) {
    uint16_t invalid = 0;
    for (size_t i = 0; i < response.length(); i++) {
        if (isInvalidATChar(response[i])) invalid++;
    }
    g_prodDiag.atCommandsTotal++;
    if (invalid > 0) {
        g_prodDiag.atCorrupted++;
        g_prodDiag.invalidCharsTotal += invalid;
    }
}
```

### Evaluación EMI al final del ciclo LTE

```cpp
void evaluateEMICycle(uint16_t atThisCycle, uint16_t corruptedThisCycle) {
    if (atThisCycle == 0) return;
    
    uint8_t pct = (corruptedThisCycle * 100) / atThisCycle;
    
    if (pct > g_prodDiag.worstCorruptPct) {
        g_prodDiag.worstCorruptPct = pct;
    }
    
    if (pct > 1) {
        logEvent('I', pct * 10);  // EMI_DETECTED
        g_prodDiag.emiEvents++;
    }
    if (pct > 5) {
        logEvent('S', pct * 10);  // EMI_SEVERE
    }
}
```

### Comandos Serial

```cpp
void handleDiagCommand(const String& cmd) {
    if (cmd == "STATS") {
        printStats();
    } else if (cmd == "LOG") {
        printEventLog();
    } else if (cmd == "CLEAR") {
        clearDiagnostics();
    }
}
```

### Salida STATS

```
╔══════════════════════════════════════╗
║  DIAGNÓSTICO PRODUCCIÓN v2.5.0       ║
╠══════════════════════════════════════╣
║  Uptime total: 1523 ciclos           ║
║  Desde boot: 847 ciclos (14.1 días)  ║
║  Last reset: POWERON                 ║
╠══════════════════════════════════════╣
║  LTE:                                ║
║    Enviadas OK: 823 (97.2%)          ║
║    Fallidas: 24                      ║
║    AT Timeouts: 156                  ║
║    Operator Fallbacks: 3             ║
╠══════════════════════════════════════╣
║  BATERÍA:                            ║
║    Ciclos en reposo: 12              ║
║    Eventos low-bat: 2                ║
╠══════════════════════════════════════╣
║  EMI STATUS:                         ║
║    AT Commands: 18,420               ║
║    Corrupted: 3 (0.016%) ✅          ║
║    Invalid chars: 7                  ║
║    Worst cycle: 0.3%                 ║
║    EMI Events: 0                     ║
║    Verdict: PCB OK                   ║
╠══════════════════════════════════════╣
║  SISTEMA:                            ║
║    Restarts 24h: 0                   ║
║    Crashes: 0                        ║
╚══════════════════════════════════════╝
```

### Verdicts EMI automáticos

```cpp
const char* getEMIVerdict() {
    float corruptPct = (g_prodDiag.atCorrupted * 100.0f) / g_prodDiag.atCommandsTotal;
    
    if (corruptPct < 0.1f && g_prodDiag.worstCorruptPct < 1) {
        return "PCB OK ✅";
    } else if (corruptPct < 1.0f && g_prodDiag.worstCorruptPct < 5) {
        return "MONITOR ⚠️";
    } else {
        return "PROBLEMA EMI 🔴";
    }
}
```

---

## 💾 RECURSOS

| Recurso | Uso |
|---------|-----|
| LittleFS | ~3 KB (stats + events) |
| RAM | ~60 bytes (struct en memoria) |
| Por AT cmd | ~5 µs (solo conteo) |
| Por ciclo | ~15 ms (save stats + eval EMI) |
| Por evento | ~20 ms (append log) |

---

## ⚠️ CONSIDERACIONES

1. **Siempre activo** - no es flag, es parte del firmware producción
2. **No verbose** - solo contadores y eventos, no hex dumps
3. **Graceful degradation** - si LittleFS falla, continúa sin persistir
4. **CRC16** - detecta corrupción de stats.bin
5. **Log circular** - events.txt nunca crece más de 2KB

---

## 🧪 CRITERIOS DE ACEPTACIÓN

- [x] Stats se persisten entre power cycles
- [x] Eventos se registran con timestamp correcto (post-bugfix v2.7.1)
- [x] Comando STATS muestra resumen completo (via FEAT-V9)
- [x] Comando LOG muestra últimos 20 eventos (via FEAT-V9)
- [x] EMI detection funciona sin overhead notable
- [x] Verdicts EMI son correctos según umbrales
- [x] Archivo events.txt no crece indefinidamente
- [x] CRC detecta corrupción de stats.bin

---

## 🐛 BUGFIXES (v2.7.1)

### Bug 1: DEEPSLEEP no mapeado

**Archivo:** `ProductionDiag.cpp` → `setResetReason()`

**Problema:** Reset reasons 5 y 8 (deep sleep) no tenían case en el switch, resultando en `bootChar = 'U'` (Unknown).

**Fix:** Añadidos `case 5:` y `case 8:` con `bootChar = 'D'`

### Bug 2: Epoch siempre 0 en eventos

**Archivo:** `ProductionDiag.cpp/.h`, `AppController.cpp`

**Problema:** `g_lastKnownEpoch` solo se actualizaba en `saveStats()` (antes de sleep), pero eventos de boot se registran antes.

**Fix:** 
- Nueva función `setCurrentEpoch(uint32_t epoch)`
- Se llama desde `Cycle_BuildFrame` cuando el epoch RTC está disponible
- Eventos de boot siguen con epoch=0 (comportamiento esperado - no hay RTC aún)

---

## 📊 UMBRALES EMI

| Métrica | ✅ OK | ⚠️ Monitor | 🔴 Problema |
|---------|-------|------------|-------------|
| Corrupted % total | < 0.1% | 0.1 - 1% | > 1% |
| Worst cycle % | < 1% | 1 - 5% | > 5% |
| EMI Events | 0 | 1-3 | > 3 |

---

## 🔗 REFERENCIAS

- FEAT-V3: Crash Diagnostics (integración de crash count)
- FEAT-V5: Debug System (hereda isValidATChar)
- FEAT-V6: EMI Report Storage (versión debug, no producción)
