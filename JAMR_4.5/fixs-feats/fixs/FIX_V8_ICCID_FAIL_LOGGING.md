# FIX-V8: Logging de Fallos ICCID

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V8 |
| **Tipo** | Fix (Diagnóstico) |
| **Sistema** | ProductionDiag / AppController |
| **Archivo Principal** | `AppController.cpp` |
| **Estado** | ✅ Implementado |
| **Fecha** | 2026-02-04 |
| **Versión Target** | v2.9.1 |
| **Depende de** | FEAT-V7 (ProductionDiag), FEAT-V9 (Serial Commands) |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado

Cuando el módem **no responde** durante la lectura de ICCID (timeout de ~16 segundos), el evento **NO se registra** en el log de ProductionDiag. Esto impide diagnosticar la causa raíz de fallos de transmisión.

### Síntomas

1. ICCID muestra `(not read)` en el resumen del ciclo
2. Operator muestra `(no TX)` 
3. Timing de ICCID es ~16,000ms (indica timeout)
4. El comando `LOG` solo muestra eventos `B` (boot), no hay `T` (AT Timeout) ni `L` (LTE Fail)
5. Es imposible distinguir entre "nunca falló" y "falló pero no se registró"

### Causa Raíz

En `AppController.cpp`, estado `Cycle_GetICCID` (~línea 1381-1388):

```cpp
if (lte.powerOn()) {
    g_iccid = lte.getICCID();  // ← Si retorna "", no registra evento
    lte.powerOff();
} else {
    g_iccid = "";              // ← powerOn falló, tampoco registra evento
}
```

**Gap identificado:** No hay llamadas a `ProdDiag::recordATTimeout()` cuando:
1. `lte.powerOn()` falla (módem no responde)
2. `lte.getICCID()` retorna string vacío (timeout o sin SIM)

---

## 📊 EVALUACIÓN

### Impacto

| Aspecto | Evaluación |
|---------|------------|
| Criticidad | Media |
| Riesgo de no implementar | Alto - Imposible diagnosticar fallos en campo |
| Esfuerzo | Muy Bajo (~8 líneas de código) |
| Beneficio | Alto - Visibilidad completa de fallos |

### Justificación

Para encontrar **causa raíz de fallos de transmisión** se necesita saber:
1. ¿El módem encendió? (`powerOn`)
2. ¿El SIM fue detectado? (`getICCID`)
3. ¿Cuántas veces ocurrió? (contador `atTimeouts`)
4. ¿Cuándo ocurrió? (timestamp en log de eventos)

Sin este fix, un datalogger puede fallar silenciosamente y el técnico no tiene forma de saber si es problema de hardware (SIM/módem) o software.

---

## 🔧 IMPLEMENTACIÓN

### Archivos a Modificar

| Archivo | Cambio | Línea |
|---------|--------|-------|
| `AppController.cpp` | Agregar logging en `Cycle_GetICCID` | ~1381-1388 |
| `FeatureFlags.h` | Agregar flag `ENABLE_FIX_V8_ICCID_FAIL_LOGGING` | ~250 |

### Código Original (Preservar en #else)

```cpp
case AppState::Cycle_GetICCID: {
  TIMING_START(g_timing, iccid);
  
  #if DEBUG_MOCK_ICCID
  // [DEBUG][FEAT-V5] ICCID simulado para stress test
  {
    unsigned long mockStart = millis();
    g_iccid = "89520000000000000000";  // ICCID dummy
    Serial.printf("[MOCK][ICCID] %s (%lums)\n", g_iccid.c_str(), millis() - mockStart);
  }
  #else
  if (lte.powerOn()) {
    g_iccid = lte.getICCID();
    lte.powerOff();
  } else {
    g_iccid = "";
  }
  #endif
  
  TIMING_END(g_timing, iccid);
  g_state = AppState::Cycle_BuildFrame;
  break;
}
```

### Código Nuevo (con FIX-V8)

```cpp
case AppState::Cycle_GetICCID: {
  TIMING_START(g_timing, iccid);
  
  #if DEBUG_MOCK_ICCID
  // [DEBUG][FEAT-V5] ICCID simulado para stress test
  {
    unsigned long mockStart = millis();
    g_iccid = "89520000000000000000";  // ICCID dummy
    Serial.printf("[MOCK][ICCID] %s (%lums)\n", g_iccid.c_str(), millis() - mockStart);
  }
  #else
  if (lte.powerOn()) {
    g_iccid = lte.getICCID();
    // ============ [FIX-V8 START] Logging de fallo ICCID ============
    #if ENABLE_FIX_V8_ICCID_FAIL_LOGGING
    if (g_iccid.length() == 0) {
      Serial.println(F("[WARN][APP] ICCID vacío - registrando timeout AT"));
      #if ENABLE_FEAT_V7_PRODUCTION_DIAG
      ProdDiag::recordATTimeout();
      #endif
    }
    #endif
    // ============ [FIX-V8 END] ============
    lte.powerOff();
  } else {
    g_iccid = "";
    // ============ [FIX-V8 START] Logging de fallo powerOn ============
    #if ENABLE_FIX_V8_ICCID_FAIL_LOGGING
    Serial.println(F("[WARN][APP] powerOn falló - registrando timeout AT"));
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProdDiag::recordATTimeout();
    #endif
    #endif
    // ============ [FIX-V8 END] ============
  }
  #endif
  
  TIMING_END(g_timing, iccid);
  g_state = AppState::Cycle_BuildFrame;
  break;
}
```

### Feature Flag

**Archivo:** `FeatureFlags.h`

```cpp
/**
 * FIX-V8: Logging de fallos ICCID
 * 
 * Registra evento AT_TIMEOUT cuando:
 * - powerOn() falla (módem no responde)
 * - getICCID() retorna vacío (timeout o SIM no detectada)
 * 
 * Beneficio: Permite diagnosticar fallos de ICCID en campo via comando LOG
 * Rollback: Deshabilitar = comportamiento original (sin logging)
 */
#define ENABLE_FIX_V8_ICCID_FAIL_LOGGING    1
```

---

## 🧪 VERIFICACIÓN

### Test Manual

1. **Simular fallo:** Quitar SIM o desconectar antena
2. **Ejecutar ciclo:** Esperar que complete
3. **Verificar LOG:** Ejecutar comando `LOG` en Serial Monitor
4. **Esperado:** Ver evento `T` (AT Timeout) con timestamp

### Resultado Esperado en LOG

```
=== LOG DE EVENTOS ===
0,B,68        ← Boot desde deep sleep
1707091234,T,0   ← NUEVO: AT Timeout durante ICCID
0,B,68
1707091534,T,0   ← NUEVO: Otro timeout
```

### Métricas de Validación

| Métrica | Antes | Después |
|---------|-------|---------|
| Eventos `T` en LOG cuando ICCID falla | 0 | ≥1 |
| Contador `atTimeouts` en STATS | No incrementa | Incrementa |
| Visibilidad de fallos | ❌ Cero | ✅ Completa |

---

## 📝 NOTAS

### Premisas Aplicadas

- **P1 (Defaults seguros):** Si flag deshabilitado, comportamiento original
- **P2 (Mínimo):** Solo 8 líneas de código nuevo
- **P3 (Aditivo):** No modifica lógica existente, solo agrega logging
- **P4 (Reversible):** Flag permite rollback instantáneo
- **P5 (Logs):** Agrega visibilidad a operación crítica

### Consideraciones

1. **No afecta funcionamiento:** Solo agrega logging, no cambia flujo
2. **Bajo overhead:** Una llamada a `recordATTimeout()` = ~1ms
3. **Depende de FEAT-V7:** Si ProductionDiag no está activo, solo imprime warning

---

## ✅ CHECKLIST PRE-IMPLEMENTACIÓN

- [x] Documentación creada
- [x] Flag definido en FeatureFlags.h
- [x] Código implementado con marcadores [FIX-V8 START/END]
- [x] logEvent agregado en recordATTimeout() para visibilidad en LOG
- [ ] Test manual ejecutado
- [ ] version_info.h actualizado
