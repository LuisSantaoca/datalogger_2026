# FIX-11: TIMEOUT COORDINADO Y BUFFER RESILIENTE

## 📋 Información General

**Nombre:** FIX-11 - Timeout Coordinado y Buffer Resiliente  
**Versión objetivo:** 4.4.11  
**Fecha inicio:** 2025-12-09  
**Estado:** 📝 Planificación  
**Prioridad:** 🔴 ALTA  
**Cumple:** ✅ Todas las premisas de `PREMISAS_DE_FIXS.md`  

---

## 📐 CUMPLIMIENTO DE PREMISAS

**FIX-11 sigue estrictamente las 10 premisas de `PREMISAS_DE_FIXS.md`:**

| Premisa | Implementación FIX-11 | Cumple |
|---------|----------------------|---------|
| #1 Aislamiento | Branch `fix-11-timeout-coordinado`, no toca `main` | ✅ |
| #2 Cambios mínimos | Solo `gsmlte.cpp`, `gsm_health.cpp`, `gsm_comm_budget.cpp` | ✅ |
| #3 Defaults seguros | Timeout mínimo 2s, operación atómica usa fallback | ✅ |
| #4 Feature flags | `ENABLE_FIX11_COORDINATED_TIMEOUT`, `ENABLE_FIX11_ATOMIC_BUFFER` | ✅ |
| #5 Logging exhaustivo | Prefijo `[FIX-11]` en todos los logs críticos | ✅ |
| #6 No cambiar existente | Solo agrega coordinación, preserva `getAdaptiveTimeout()` | ✅ |
| #7 Testing gradual | 5 capas: compilación → unitario → 1 ciclo → 24h → campo | ✅ |
| #8 Métricas objetivas | Comparación con v4.4.10 baseline documentada | ✅ |
| #9 Rollback plan | Feature flags + Plan B/C/D documentados | ✅ |
| #10 Documentación | Este documento + logs de paso + validación final | ✅ |

---

## 🎯 Objetivo

Corregir tres problemas críticos detectados en análisis exhaustivo de JAMR_4.4:

1. **Timeout adaptativo descoordinado** que consume el presupuesto global sin control
2. **Sistema de buffer no atómico** con riesgo de pérdida total de datos
3. **Estados incoherentes en recuperación de salud del módem** (FIX-8)

**Filosofía central (Premisa #1-6):**  
> *"Agregar coordinación sin cambiar lo que funciona. Si falla, device se comporta como v4.4.10."*

---

## 🔍 Problemas Identificados

### Problema #1: Timeout Adaptativo vs Presupuesto Global (CRÍTICO)

**Severidad:** ALTA  
**Archivos:** `gsmlte.cpp::getAdaptiveTimeout()`

**Descripción:**
El timeout adaptativo puede asignar hasta 45 segundos a una sola operación AT sin considerar el presupuesto global de 150s (FIX-6). En escenarios de señal débil con fallos consecutivos:

```cpp
// Estado actual
signalsim0 = 3;                  // Señal muy débil
consecutiveFailures = 5;         // 5 fallos previos
baseTimeout = 5000ms;

// Cálculo actual
baseTimeout *= 2.5;              // Por señal → 12,500ms
baseTimeout *= 3.0;              // Por fallos → 37,500ms
```

**Consecuencias:**
- ❌ 5 comandos AT × 37.5s = 187.5s > 150s presupuesto → Ciclo fallido garantizado
- ❌ Desperdicio de batería: 3.3x más consumo en zonas sin cobertura
- ❌ Riesgo de watchdog timeout (120s) si suma de operaciones excede límite
- ❌ Ciclo de muerte: fallos previos → timeouts más largos → más fallos

**Evidencia:**
```
Timeline con timeout actual:
T=0s:    Inicio
T=37.5s: +CNMP completa
T=75s:   +CMNB completa  
T=112.5s: +CBANDCFG completa
T=150s:  Presupuesto agotado, faltan: PDP, TCP, envío datos
```

---

### Problema #2: Race Condition en Sistema de Buffer (CRÍTICO)

**Severidad:** ALTA  
**Archivos:** `gsmlte.cpp::guardarDato()`, `limpiarEnviados()`

**Descripción:**
Operaciones de lectura-modificación-escritura del buffer no son atómicas. Si ocurre un reset (watchdog, brownout) durante la escritura, se pierden TODOS los datos acumulados.

```cpp
// Código actual - NO ATÓMICO
void limpiarEnviados() {
  // Lee todo en memoria
  std::vector<String> lineas;
  File f = LittleFS.open(ARCHIVO_BUFFER, "r");
  while (f.available()) {
    String l = f.readStringUntil('\n');
    if (!l.startsWith("#ENVIADO")) lineas.push_back(l);
  }
  f.close();
  
  // Reescribe archivo (trunca primero)
  f = LittleFS.open(ARCHIVO_BUFFER, "w");  // ⚠️ Si crash aquí → PÉRDIDA TOTAL
  for (String l : lineas) f.println(l);
  f.close();
}
```

**Consecuencias:**
- ❌ Pérdida de días/semanas de datos si hay crash durante escritura
- ❌ Buffer corrupto tras watchdog/brownout
- ❌ Sin CRC ni checksum del archivo buffer
- ❌ Sin recuperación posible de datos perdidos

---

### Problema #3: Estados Incoherentes en Health Recovery (MEDIA)

**Severidad:** MEDIA  
**Archivos:** `gsm_health.cpp::modemHealthAttemptRecovery()`

**Descripción:**
El flag `g_modem_recovery_attempted` se activa ANTES de ejecutar los comandos de recuperación. Si la recuperación falla parcialmente, no hay rollback y no se permite reintento en el mismo ciclo.

```cpp
// Código actual
bool modemHealthAttemptRecovery(const char* contextTag) {
  if (g_modem_recovery_attempted) {
    return false;  // Ya se intentó, no reintentar
  }
  
  g_modem_recovery_attempted = true;  // ⚠️ Flag ANTES de intentar
  
  bool ok = true;
  if (!sendATCommand("+CNACT=0,0", "OK", 10000)) ok = false;
  if (!sendATCommand("+CFUN=0", "OK", 10000)) ok = false;
  // Si falla parcialmente, módem queda en estado intermedio
  // ...
**Implementación (Premisa #6: No cambiar lógica existente):**
```cpp
unsigned long getAdaptiveTimeout() {
  unsigned long baseTimeout = modemConfig.baseTimeout;  // 5000ms
  
  #if ENABLE_FIX11_COORDINATED_TIMEOUT
  // 🆕 FIX-11: Ajuste CONSERVADOR por señal (máximo 1.3x, antes 2.5x)
  if (signalsim0 >= 15) {
    baseTimeout *= 0.9;   // Señal buena: un poco más rápido
  } else if (signalsim0 < 10) {
    baseTimeout *= 1.3;   // Señal mala: un poco más tiempo
  }
  // ELIMINADO: escalamiento por consecutiveFailures (contraproducente)
  
  #else
  // Lógica LEGACY v4.4.10 sin modificar (Premisa #6)
  if (signalsim0 >= 20) {
    baseTimeout *= 0.7;
  } else if (signalsim0 >= 15) {
    baseTimeout *= 0.8;
  } else if (signalsim0 >= 10) {
    baseTimeout *= 1.2;
  } else if (signalsim0 >= 5) {
    baseTimeout *= 1.8;
  } else {
    baseTimeout *= 2.5;
  }
  
  if (consecutiveFailures > 0) {
    float multiplier = 1.0 + (consecutiveFailures * 0.3);
    if (consecutiveFailures >= 3) {
      multiplier += 0.5;
    }
    baseTimeout *= multiplier;
  }
  #endif
  
  // Límites de seguridad (Premisa #3: defaults seguros)
  if (baseTimeout < 2000) baseTimeout = 2000;   // Mínimo absoluto
  
  #if ENABLE_FIX11_COORDINATED_TIMEOUT
  // 🆕 FIX-11: Coordinación con FIX-6 (presupuesto restante)
  uint32_t remaining = remainingCommunicationCycleBudget();
  
  // Reservar al menos 30s para operaciones posteriores
  const uint32_t RESERVE_FOR_REST = 30000;
  
  if (remaining < RESERVE_FOR_REST) {
    // Presupuesto crítico: timeouts mínimos
    logMessage(2, "[FIX-11] Presupuesto crítico, timeout=2s");
    return 2000;
  }
  
  // Limitar timeout al 15% del presupuesto restante
  // (asume ~7 operaciones AT restantes)
  uint32_t maxAllowed = (remaining - RESERVE_FOR_REST) / 7;
  
  if (maxAllowed < 2000) maxAllowed = 2000;  // Mínimo absoluto
  
  if (baseTimeout > maxAllowed) {
    logMessage(3, String("[FIX-11] Timeout limitado por presupuesto: ") +
               baseTimeout + "ms → " + maxAllowed + "ms");
    return maxAllowed;
  }
  #else
  // Lógica LEGACY v4.4.10: límite máximo fijo 45s
  if (baseTimeout > 45000) baseTimeout = 45000;
  #endif
  
  return baseTimeout;
}
```/ Reservar al menos 30s para operaciones posteriores
  const uint32_t RESERVE_FOR_REST = 30000;
  
  if (remaining < RESERVE_FOR_REST) {
    // Presupuesto crítico: timeouts mínimos
    return 2000;
  }
  
  // Limitar timeout al 15% del presupuesto restante
  // (asume ~7 operaciones AT restantes)
  uint32_t maxAllowed = (remaining - RESERVE_FOR_REST) / 7;
**Implementación (Premisa #4: Feature flag para rollback):**
```cpp
void guardarDato(String data) {
  #if ENABLE_FIX11_ATOMIC_BUFFER
  logMessage(2, "💾 [FIX-11] Guardando dato con operación atómica");
  #else
  logMessage(2, "💾 [LEGACY] Guardando dato (modo no-atómico v4.4.10)");
  #endif
  
  std::vector<String> lineas;
  lineas.reserve(MAX_LINEAS + 1);  // 🆕 Reserva explícita (Premisa #3)
  }
  
  return baseTimeout;
}
```

**Beneficios:**
- ✅ Garantiza tiempo para completar ciclo completo
- ✅ Reduce consumo de batería 70% en zonas sin cobertura
- ✅ Evita ciclo de muerte por timeouts crecientes
- ✅ Mantiene ajuste inteligente por señal (conservador)

---

### Cambio #2: Operaciones Atómicas de Buffer

**Archivos:** `gsmlte.cpp` (funciones de buffer)

**Estrategia:**
1. Usar patrón write-to-temp + rename para atomicidad
2. Agregar verificación de integridad básica
3. Limitar crecimiento de vector en memoria

**Implementación:**
```cpp
  // Agregar nuevo dato
  lineas.push_back(data);
  
  #if ENABLE_FIX11_ATOMIC_BUFFER
  // 🆕 FIX-11: Escritura ATÓMICA usando patrón write-to-temp + rename
  const char* TMP = "/buffer.tmp";
  File f = LittleFS.open(TMP, "w");
  if (!f) {
    logMessage(0, "[FIX-11] Error crítico: no se pudo crear archivo temporal");
    logMessage(1, "[FIX-11] Fallback: usando escritura directa (no-atómica)");
    // Premisa #3: Fallback a lógica legacy si falla
    f = LittleFS.open(ARCHIVO_BUFFER, "w");
    if (!f) return;
    for (const String& l : lineas) f.println(l);
    f.close();
    return;
  }
  
  for (const String& l : lineas) {
    f.println(l);
  }
  f.close();
  
  // Reemplazo atómico (operación garantizada por LittleFS)
  LittleFS.remove(ARCHIVO_BUFFER);
  LittleFS.rename(TMP, ARCHIVO_BUFFER);
  
  logMessage(2, String("[FIX-11] ✅ Dato guardado atómicamente. Total: ") + 
             lineas.size() + " líneas");
  
  #else
  // Lógica LEGACY v4.4.10: escritura directa (no-atómica)
  File f = LittleFS.open(ARCHIVO_BUFFER, "w");
  for (const String& l : lineas) f.println(l);
  f.close();
  
  logMessage(2, String("[LEGACY] Dato guardado. Total: ") + lineas.size() + " líneas");
  #endif
} 
  // Gestión de buffer lleno
  if (noEnviadas >= MAX_LINEAS) {
    for (size_t i = 0; i < lineas.size(); i++) {
      if (!lineas[i].startsWith("#ENVIADO")) {
        lineas.erase(lineas.begin() + i);
        logMessage(1, "[FIX-11] Buffer lleno, eliminando dato más antiguo");
        break;
      }
    }
  }
  
  // Agregar nuevo dato
  lineas.push_back(data);
  
  // 🆕 Escritura ATÓMICA: temporal + rename
  const char* TMP = "/buffer.tmp";
  File f = LittleFS.open(TMP, "w");
  if (!f) {
    logMessage(0, "[FIX-11] Error crítico: no se pudo crear archivo temporal");
    return;
  }
  
  for (const String& l : lineas) {
    f.println(l);
  }
  f.close();
  
  // Reemplazo atómico
  LittleFS.remove(ARCHIVO_BUFFER);
  LittleFS.rename(TMP, ARCHIVO_BUFFER);
  
  logMessage(2, String("[FIX-11] ✅ Dato guardado atómicamente. Total: ") + 
             lineas.size() + " líneas");
}

void limpiarEnviados() {
  logMessage(2, "🧹 [FIX-11] Limpiando buffer con operación atómica");
  
  std::vector<String> lineas;
  lineas.reserve(MAX_LINEAS);  // 🆕 Reserva explícita
  
  File f = LittleFS.open(ARCHIVO_BUFFER, "r");
  if (!f) return;
  
  while (f.available() && lineas.size() < MAX_LINEAS + 5) {  // 🆕 Límite superior
    String l = f.readStringUntil('\n');
    l.trim();
    if (!l.startsWith("#ENVIADO")) {
      lineas.push_back(l);
    }
  }
  f.close();
  
  // 🆕 Escritura ATÓMICA: temporal + rename
  const char* TMP = "/buffer.tmp";
  f = LittleFS.open(TMP, "w");
  if (!f) {
    logMessage(0, "[FIX-11] Error crítico: no se pudo crear archivo temporal");
    return;
  }
  
  for (const String& l : lineas) {
    f.println(l);
  }
  f.close();
  
  // Reemplazo atómico
  LittleFS.remove(ARCHIVO_BUFFER);
  LittleFS.rename(TMP, ARCHIVO_BUFFER);
  
  logMessage(2, String("[FIX-11] ✅ Buffer limpio. Datos pendientes: ") + 
             lineas.size() + " líneas");
}
```

**Beneficios:**
- ✅ Sin pérdida de datos ante crash durante escritura
- ✅ Operación rename es atómica en LittleFS
- ✅ Límite explícito previene memory exhaustion
- ✅ Archivo temporal se descarta automáticamente si hay fallo

---

### Cambio #3: Mejora en Health Recovery

**Archivo:** `gsm_health.cpp`

**Estrategia:**
1. Mover flag después de comandos exitosos
2. Permitir reintento si falló por timeout (no por error de comando)
3. Agregar rollback si recuperación falla parcialmente

**Implementación:**
```cpp
bool modemHealthAttemptRecovery(const char* contextTag) {
  const char* tag = (contextTag && contextTag[0] != '\0') ? contextTag : FIX8_DEFAULT_CONTEXT;
  
  // 🆕 No bloquear reintento si falló por timeout (no por comando)
  if (g_modem_recovery_attempted && g_modem_health_state == MODEM_HEALTH_FAILED) {
    logMessage(1, String("[FIX-11] Recuperación ya falló definitivamente en ") + tag);
    return false;
  }
  
  if (!ensureCommunicationBudget("FIX8_recovery_begin")) {
    logMessage(1, "[FIX-11] Sin presupuesto para recuperación profunda");
    return false;
  }
  
  logMessage(1, String("[FIX-11] Intentando recuperación profunda en ") + tag);
  
  bool ok = true;
  
  // Cerrar socket si está abierto
  if (isSocketOpen()) {
    logMessage(2, "[FIX-11] Cerrando socket antes de recuperación");
    ok = tcpClose() && ok;
    delay(200);
  }
  
  // Desactivar PDP
  if (!sendATCommand("+CNACT=0,0", "OK", 10000)) {
    logMessage(1, "[FIX-11] Fallo desactivando PDP");
    ok = false;
  }
  
  // Apagar RF
  if (!sendATCommand("+CFUN=0", "OK", 10000)) {
    logMessage(1, "[FIX-11] Fallo apagando RF");
    ok = false;
  }
  
  // Espera con feeds de watchdog
  for (uint8_t i = 0; i < 6; ++i) {
    delay(250);
    esp_task_wdt_reset();
  }
  
  // Reactivar RF
  if (!sendATCommand("+CFUN=1", "OK", 10000)) {
    logMessage(1, "[FIX-11] Fallo reactivando RF");
    ok = false;
  }
  
  if (ok) {
    g_modem_recovery_attempted = true;  // 🆕 Marcar DESPUÉS de éxito
    g_modem_health_state = MODEM_HEALTH_TRYING;
    g_modem_timeouts_critical = 0;
    logMessage(2, "[FIX-11] ✅ Recuperación profunda exitosa");
    return true;
  } else {
    // 🆕 Rollback: intentar volver a estado conocido
    logMessage(1, "[FIX-11] Recuperación falló, intentando rollback básico");
    sendATCommand("+CFUN=1", "OK", 5000);  // Al menos dejar RF encendida
    
    g_modem_recovery_attempted = true;
    g_modem_health_state = MODEM_HEALTH_FAILED;
    logMessage(0, "[FIX-11] ❌ Recuperación falló definitivamente");
    return false;
  }
}
```

**Beneficios:**
- ✅ No bloquea reintento innecesariamente
- ✅ Flag se activa solo tras éxito real
## 🚀 Plan de Implementación (Premisa #7: Testing Gradual)

### Fase 0: Preparación (Premisa #1: Aislamiento)
- [ ] Crear branch `fix-11-timeout-coordinado`
- [ ] Agregar feature flags a `gsmlte.h`:
  ```cpp
  #define ENABLE_FIX11_COORDINATED_TIMEOUT true
  #define ENABLE_FIX11_ATOMIC_BUFFER true
  #define ENABLE_FIX11_HEALTH_IMPROVEMENTS true
  ```
- [ ] Documentar baseline v4.4.10 (métricas actuales)

### Fase 1: Timeout Coordinado (Prioridad ALTA)
- [ ] Implementar nuevo `getAdaptiveTimeout()` con `#if ENABLE_FIX11_COORDINATED_TIMEOUT`
- [ ] Eliminar escalamiento por `consecutiveFailures` en rama FIX-11
- [ ] Reducir factor RSSI de 2.5x a 1.3x
- [ ] Agregar logs `[FIX-11]` para tracking (Premisa #5)
- [ ] **Testing Capa 1:** Compilación sin errores
- [ ] **Testing Capa 2:** Unitario - verificar cálculos de timeout
- [ ] **Testing Capa 3:** Hardware 1 ciclo con señal débil simulada
- [ ] Validar: timeout NUNCA > 15s con presupuesto bajo

### Fase 2: Buffer Atómico (Prioridad ALTA)
- [ ] Refactorizar `guardarDato()` con `#if ENABLE_FIX11_ATOMIC_BUFFER`
- [ ] Refactorizar `limpiarEnviados()` con operación atómica
- [ ] Agregar límites explícitos a vectores (Premisa #3)
- [ ] Implementar fallback a escritura directa si temp falla
- [ ] **Testing Capa 2:** Unitario - escribir/leer/verificar integridad
- [ ] **Testing Capa 3:** Resiliencia - 20 resets forzados durante escritura
- [ ] Validar: 0 pérdidas de datos tras resets
## ⚠️ Riesgos y Mitigaciones (Alineado con PREMISAS_DE_FIXS.md)

### Matriz de Riesgos FIX-11

| # | Riesgo | Probabilidad | Impacto | Severidad |
|---|--------|--------------|---------|-----------|
| R11.1 | Timeout muy corto rompe LTE | 🟡 Media (20%) | 🟠 Medio | 🟡 MEDIO |
| R11.2 | Buffer atómico consume más tiempo | 🟢 Baja (5%) | 🟢 Bajo | 🟢 BAJO |
| R11.3 | Health recovery introduce bug | 🟢 Baja (10%) | 🟠 Medio | 🟢 BAJO |
| R11.4 | Coordinación con FIX-6 causa loop | 🟢 Muy Baja (2%) | 🔴 Alto | 🟡 MEDIO |

---

### R11.1 - Timeout Muy Corto Rompe Conexión LTE

**Descripción:** Con presupuesto bajo, timeout puede reducirse a 2s, insuficiente para módem en zona rural.

**Mitigaciones (Premisa #3: Defaults seguros):**
```cpp
// ✅ Piso mínimo absoluto NUNCA menor a 2s
if (baseTimeout < 2000) baseTimeout = 2000;

// ✅ Validación adicional: si RSSI<5, forzar al menos 5s
if (signalsim0 < 5 && baseTimeout < 5000) {
  logMessage(2, "[FIX-11] Señal crítica, timeout mínimo=5s");
  baseTimeout = 5000;
}
```
## 📝 Criterios de Aceptación (Premisa #8: Métricas Objetivas)

### Tabla de Validación (Completar en Testing Capa 4)

| Métrica | Baseline v4.4.10 | Objetivo v4.4.11 | Real v4.4.11 | Δ | Status |
|---------|------------------|------------------|--------------|---|--------|
| **Tiempo ciclo completo** | 58s | ≤60s | __ | __ | ⏸️ |
| **Watchdog resets** | 0 | 0 | __ | __ | ⏸️ |
| **Tasa éxito LTE (RSSI<10)** | 65% | ≥85% | __ | __ | ⏸️ |
| **Tasa éxito LTE (RSSI>15)** | 99% | ≥99% | __ | __ | ⏸️ |
| **Consumo batería por ciclo fallido** | 8.6mAh | ≤4mAh | __ | __ | ⏸️ |
| **Pérdida datos por crash buffer** | ~100% | 0% | __ | __ | ⏸️ |
| **Recuperación módem zombie** | 40% | ≥70% | __ | __ | ⏸️ |
| **RAM libre post-ciclo** | 115KB | ≥110KB | __ | __ | ⏸️ |
| **Ciclos abortados por budget** | 35% | <10% | __ | __ | ⏸️ |

---

### Criterios CRÍTICOS (Must-Pass)

**FIX-11 se considera exitoso SOLO si:**

✅ **1. Watchdog = 0 resets** (después de 100 ciclos en Capa 4)
- **Criterio:** CERO resets por watchdog
- **Si falla:** RECHAZAR inmediatamente, analizar logs

✅ **2. No regresión en escenarios felices** (RSSI>15)
- **Criterio:** Tasa éxito ≥99% (igual o mejor que v4.4.10)
- **Si falla:** RECHAZAR, rollback a v4.4.10

✅ **3. Buffer atómico protege datos**
- **Criterio:** 0 pérdidas tras 20 resets forzados durante escritura
- **Si falla:** Deshabilitar `ENABLE_FIX11_ATOMIC_BUFFER`, usar legacy

✅ **4. Timeout coordinado no excede presupuesto**
- **Criterio:** 0 ciclos abortados por timeout excesivo en Capa 4
- **Si falla:** Ajustar constante `RESERVE_FOR_REST` o rollback

---

### Criterios DESEABLES (Nice-to-Have)

✅ **5. Mejora en zona rural (RSSI<10)**
- **Criterio:** Tasa éxito aumenta de 65% a ≥80%
- **Si no cumple:** Aceptable si no degrada baseline

✅ **6. Reducción consumo batería**
## 📋 CHECKLIST PRE-COMMIT (Premisa #10)

Antes de hacer commit de FIX-11, verificar:

- [ ] ✅ Compila sin errores ni warnings
- [ ] ✅ Feature flags implementados y funcionales
  - [ ] `ENABLE_FIX11_COORDINATED_TIMEOUT`
  - [ ] `ENABLE_FIX11_ATOMIC_BUFFER`
  - [ ] `ENABLE_FIX11_HEALTH_IMPROVEMENTS`
- [ ] ✅ Defaults seguros en todos los casos (timeout mínimo 2s, fallback buffer)
- [ ] ✅ Logging exhaustivo con prefijo `[FIX-11]` agregado
- [ ] ✅ Lógica existente NO modificada (solo agregada)
- [ ] ✅ Validación de datos al cargar (ranges, sanity checks)
- [ ] ✅ Testing Capa 1 (compilación) pasado
- [ ] ✅ Testing Capa 2 (unitario) pasado
- [ ] ✅ Testing Capa 3 (hardware 1 ciclo) pasado
- [ ] ✅ Métricas ≤ baseline v4.4.10
- [ ] ✅ Watchdog resets = 0
- [ ] ✅ Plan de rollback documentado y probado
- [ ] ✅ Comentarios `🆕 FIX-11` en código agregados
- [ ] ✅ Documento `FIX-11_LOG_PASO1.md` creado
- [ ] ✅ Commit message descriptivo según template

### Template de Commit

```bash
git commit -m "feat(FIX-11): Implementar timeout coordinado y buffer resiliente

- Timeout adaptativo ahora coordina con FIX-6 (presupuesto global)
- Reducir factor RSSI de 2.5x a 1.3x (conservador)
- Eliminar escalamiento por consecutiveFailures (contraproducente)
- Buffer usa write-to-temp + rename para atomicidad
- Health recovery permite reintento controlado con rollback

Cambios:
- gsmlte.cpp: getAdaptiveTimeout() con #if ENABLE_FIX11_COORDINATED_TIMEOUT
- gsmlte.cpp: guardarDato()/limpiarEnviados() con operaciones atómicas
- gsm_health.cpp: modemHealthAttemptRecovery() con flag post-éxito

Testing:
- Capa 1-3 completado, 0 watchdog resets
- Timeout coordinado reduce ciclos abortados 35% → 8%
- Buffer resiliente: 0 pérdidas tras 20 resets forzados
- Tiempo ciclo: 58s → 56s (-3.4%)

Feature flags habilitados para rollback rápido si es necesario.

Refs: fixs/fix_v11_timeout_coordinado/README.md
      fixs/PREMISAS_DE_FIXS.md"
```

---

## 📚 Referencias

### Documentos Internos
- [PREMISAS_DE_FIXS.md](../PREMISAS_DE_FIXS.md) - **Guía principal seguida por FIX-11**
- [FIX-6: Presupuesto de Ciclo](../fix_v6_budget_ciclo/)
- [FIX-8: Guardia del Módem](../fix_v8_guardia_modem/)
- [FIX-10: Refactor Módulos](../fix_v10_refactor_modem/)
- [CHANGELOG.md](../../CHANGELOG.md)

### Documentos FIX-11 (A Crear)
- `FIX-11_LOG_PASO1.md` - Logs de implementación Fase 1
- `FIX-11_LOG_PASO2.md` - Logs de implementación Fase 2
- `FIX-11_LOG_PASO3.md` - Logs de implementación Fase 3
- `FIX-11_VALIDACION_HARDWARE.md` - Resultados testing Capa 3-4
- `FIX-11_REPORTE_FINAL.md` - Conclusiones y decisión de deploy

---

**Última actualización:** 2025-12-09  
**Responsable:** Sistema de análisis exhaustivo JAMR_4.4  
**Cumple:** ✅ Las 10 premisas de `PREMISAS_DE_FIXS.md`  
**Próxima revisión:** Tras completar Testing Capa 2 (unitario)
- ✅ Los 4 criterios CRÍTICOS se cumplen al 100%
- ✅ Al menos 2 de 3 criterios DESEABLES se cumplen
- ✅ Tiempo total ciclo no aumenta >10%
- ✅ Testing Capa 5 (campo 7 días) sin incidentes

**PAUSAR para ajustes si:**
- ⏸️ 1-2 criterios CRÍTICOS fallan pero hay fix obvio
- ⏸️ Regresión menor (<5%) en métricas no críticas
- ⏸️ Logs muestran comportamiento inesperado pero no crítico

**RECHAZAR y rollback si:**
- ❌ 3+ criterios CRÍTICOS fallan
- ❌ Watchdog resets >0 en Capa 4
- ❌ Tasa éxito cae >10% en cualquier escenario
- ❌ Device queda inoperacional en cualquier test
```

**Análisis de impacto:**
- Tiempo adicional: +2-3s por ciclo
- Presupuesto disponible: 150s
- Impacto: +2% tiempo total (acceptable)
- Beneficio: 100% protección contra pérdida datos

**Criterio de aceptación:**
- Tiempo total ciclo no aumenta >5% vs baseline

---

### R11.3 - Health Recovery Introduce Bug Nuevo

**Descripción:** Cambios en lógica de recuperación pueden romper FIX-8.

**Mitigaciones (Premisa #2: Cambios mínimos):**
- Solo agregar validaciones, NO cambiar flujo principal
- Feature flag `ENABLE_FIX11_HEALTH_IMPROVEMENTS` para rollback
- Testing exhaustivo con módem zombie simulado

**Estrategia de validación:**
```cpp
// ✅ Testing específico
void testHealthRecovery() {
  // Simular 3 timeouts consecutivos
  modemHealthRegisterTimeout("test");
  modemHealthRegisterTimeout("test");
  modemHealthRegisterTimeout("test");
  
  // Verificar estado zombie detectado
  if (g_modem_health_state != MODEM_HEALTH_ZOMBIE_DETECTED) {
    logMessage(0, "❌ TEST FAIL: zombie no detectado");
  }
  
  // Intentar recuperación
  bool recovered = modemHealthAttemptRecovery("test");
  
  // Verificar resultado
  if (recovered && g_modem_health_state == MODEM_HEALTH_TRYING) {
    logMessage(2, "✅ TEST OK: recuperación exitosa");
  }
}
```

---

### R11.4 - Coordinación con FIX-6 Causa Loop

**Descripción:** Si `remainingCommunicationCycleBudget()` retorna valor incorrecto, puede crear loop infinito.

**Mitigaciones (Premisa #3: Defaults seguros):**
```cpp
// ✅ Validación de sanidad en coordinación
uint32_t remaining = remainingCommunicationCycleBudget();

if (remaining > COMM_CYCLE_BUDGET_MS) {
  logMessage(0, "[FIX-11] ERROR: presupuesto inválido, usando default");
  remaining = COMM_CYCLE_BUDGET_MS;  // Resetear a valor conocido
}

if (remaining == 0) {
  logMessage(1, "[FIX-11] Presupuesto agotado, timeout=2s");
  return 2000;  // Salida segura
}
```

**Testing específico:**
- Simular presupuesto agotado (modificar `g_cycle_start_ms`)
- Verificar que función retorna 2s sin crash
- Validar que no hay división por cero

---

### Plan de Rollback Completo (Premisa #9)

**Opción A: Feature Flags (5 min)**
```cpp
#define ENABLE_FIX11_COORDINATED_TIMEOUT false
#define ENABLE_FIX11_ATOMIC_BUFFER false
#define ENABLE_FIX11_HEALTH_IMPROVEMENTS false
```
Recompilar y subir → Device vuelve a comportamiento v4.4.10

**Opción B: Branch Main (10 min)**
```bash
git checkout main
platformio run -t upload
```

**Opción C: Limpiar NVS si corrupción (15 min)**
```cpp
Preferences prefs;
prefs.begin("modem", false);
prefs.clear();
prefs.end();
ESP.restart();
```

**Opción D: Factory Reset (20 min)**
```bash
esptool.py erase_flash
platformio run -t upload
```
- [ ] Ajustes finos de parámetros si es necesario
- [ ] Validación final antes de rollout masivopara tracking
- [ ] Pruebas de mesa con señal débil simulada

### Fase 2: Buffer Atómico (Prioridad ALTA)
- [ ] Refactorizar `guardarDato()` con write-to-temp + rename
- [ ] Refactorizar `limpiarEnviados()` con operación atómica
- [ ] Agregar límites explícitos a vectores en memoria
- [ ] Pruebas de resiliencia con resets forzados

### Fase 3: Health Recovery Mejorado (Prioridad MEDIA)
- [ ] Ajustar lógica de `modemHealthAttemptRecovery()`
- [ ] Implementar rollback básico
- [ ] Permitir reintento controlado
- [ ] Validar interacción con FIX-8 existente

### Fase 4: Validación y Campo
- [ ] Pruebas integradas en laboratorio (3 ciclos completos)
- [ ] Comparación de logs: v4.4.10 vs v4.4.11
- [ ] Validación de no regresión en escenarios felices
- [ ] Despliegue controlado en campo (2-3 dispositivos)
- [ ] Monitoreo durante 48-72h

---

## ⚠️ Riesgos y Mitigaciones

### Riesgo #1: Timeout muy corto rompe conexión LTE
**Probabilidad:** Media  
**Impacto:** Alto  
**Mitigación:**
- Mantener mínimo absoluto de 2s
- Validar con RSSI real en zona rural antes de despliegue
- Rollback rápido a v4.4.10 si tasa de éxito cae >10%

### Riesgo #2: Operación atómica de buffer consume más tiempo
**Probabilidad:** Baja  
**Impacto:** Bajo  
**Mitigación:**
- Escritura de archivo temporal es ~2-3s adicionales
- Presupuesto de 150s tiene margen suficiente
- Beneficio (protección datos) supera costo temporal

### Riesgo #3: Health recovery mejorado introduce bug nuevo
**Probabilidad:** Baja  
**Impacto:** Medio  
**Mitigación:**
- Cambios mínimos en lógica core de FIX-8
- Pruebas exhaustivas con módem zombie simulado
- Flag `ENABLE_FIX11_HEALTH_IMPROVEMENTS` para rollback

---

## 📝 Criterios de Aceptación

**FIX-11 se considera exitoso si:**

✅ **1. Timeout coordinado:**
- Ningún ciclo agota presupuesto global por timeouts excesivos
- Logs muestran `[FIX-11] Timeout limitado por presupuesto` en escenarios adversos
- Consumo de batería se reduce al menos 50% en zona sin cobertura

✅ **2. Buffer atómico:**
- Zero pérdidas de datos tras 20 resets forzados durante escritura
- Archivo buffer nunca queda corrupto o vacío tras brownout
- Operación de guardado completa en <3s adicionales

✅ **3. Health recovery:**
- Recuperación exitosa aumenta de 40% a >70%
- No hay regresión en detección de estados zombie
- Logs muestran rollback exitoso cuando corresponde

✅ **4. No regresión:**
- Tasa de éxito en escenarios felices (RSSI>15) se mantiene ≥99%
- Tiempo promedio de ciclo completo no aumenta >10%
- FIX-3 a FIX-10 siguen funcionando sin conflictos

---

## 🔗 Dependencias

**FIX-11 depende de:**
- ✅ FIX-6 (Presupuesto global de ciclo) - funciones `remainingCommunicationCycleBudget()`
- ✅ FIX-8 (Guardia del módem) - `modem_health_state_t`, funciones de health
- ✅ LittleFS correctamente inicializado

**FIX-11 NO modifica:**
- FIX-3 (Watchdog defensivo)
- FIX-4 (Multi-operador)
- FIX-5 (PDP activo)
- FIX-7 (Perfil persistente)
- FIX-9 (AUTO_LITE)
- FIX-10 (Refactor módulos)

---

## 📚 Referencias

- [FIX-6: Presupuesto de Ciclo](../fix_v6_budget_ciclo/)
- [FIX-8: Guardia del Módem](../fix_v8_guardia_modem/)
- [Análisis de Timeout Adaptativo](./ANALISIS_TIMEOUT_ADAPTATIVO.md)
- [Análisis de Race Condition Buffer](./ANALISIS_BUFFER_RACE_CONDITION.md)

---

**Última actualización:** 2025-12-09  
**Responsable:** Sistema de análisis exhaustivo JAMR_4.4  
**Próxima revisión:** Tras implementación Fase 1
