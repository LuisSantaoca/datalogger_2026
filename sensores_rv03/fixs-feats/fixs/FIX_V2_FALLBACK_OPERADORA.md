# FIX-V2: Fallback a Escaneo de Operadoras cuando Falla la Guardada

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V2 |
| **Tipo** | Fix (Corrección de Bug) |
| **Sistema** | LTE/Modem - Selección de Operadora |
| **Archivo Principal** | `AppController.cpp` |
| **Estado** | 📋 Propuesto |
| **Fecha Identificación** | 2026-01-13 |
| **Versión Target** | v2.0.3 |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Prioridad** | **Crítica** |
| **Requisito Incumplido** | RF-12 |
| **Premisas** | P1✅ P2✅ P3✅ P4✅ P5✅ P6✅ P7✅ P8✅ P9✅ P10✅ |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
Cuando un dispositivo tiene una operadora guardada en NVS (ej: ATT) pero esa operadora no tiene cobertura en la ubicación actual, el sistema:
1. Intenta conectar con la operadora guardada
2. Falla en `configureOperator()` 
3. **Retorna `false` sin intentar otras operadoras**
4. La operadora problemática **permanece guardada en NVS**
5. El ciclo se repite indefinidamente, incluso tras reinicios

### Evidencia de Campo
- Sensor desplegado en zona rural
- Operadora guardada: ATT (de ubicación anterior)
- Operadora disponible en zona: TELCEL
- **Resultado:** El sensor nunca transmite porque siempre intenta ATT

### Ubicación del Bug
**Archivo:** `AppController.cpp`  
**Función:** `sendBufferOverLTE_AndMarkProcessed()`  
**Líneas:** 386-390

```cpp
#if ENABLE_FIX_V1_SKIP_RESET_PDP
  if (!lte.configureOperator(operadoraAUsar, tieneOperadoraGuardada)) { 
      lte.powerOff(); 
      return false;  // ❌ BUG: Retorna sin intentar otras operadoras
  }
#else
  if (!lte.configureOperator(operadoraAUsar)) { 
      lte.powerOff(); 
      return false;  // ❌ BUG: Retorna sin intentar otras operadoras
  }
#endif
```

### Causa Raíz
El flujo actual no cumple RF-12 porque:

1. **Si hay operadora guardada** → la usa directamente sin validar disponibilidad
2. **Si `configureOperator()` falla** → retorna `false` inmediatamente
3. **El código de limpieza de NVS** (líneas 456-461) solo se ejecuta si `anySent==false` **Y** llegó hasta el final del proceso de envío
4. **Como retorna antes**, el código de limpieza nunca se ejecuta

```
Flujo actual (INCORRECTO):
┌─────────────────────────────────────────────────────────┐
│ powerOn() → Lee NVS → configureOperator() → FALLA      │
│                                              ↓         │
│                                         return false   │
│                                    (NVS NO se limpia)  │
└─────────────────────────────────────────────────────────┘

Flujo esperado (RF-12):
┌─────────────────────────────────────────────────────────┐
│ powerOn() → Lee NVS → configureOperator() → FALLA      │
│                                              ↓         │
│                              Borrar NVS + Escanear     │
│                                              ↓         │
│                              Seleccionar mejor         │
│                                              ↓         │
│                              Continuar con la nueva    │
└─────────────────────────────────────────────────────────┘
```

---

## 📊 EVALUACIÓN

### Impacto Cuantificado

| Métrica | Actual | Esperado |
|---------|--------|----------|
| Reconexión tras cambio de zona | **NUNCA** | Automática |
| Intervención manual requerida | **Sí** (borrar NVS) | No |
| Pérdida de datos | **Indefinida** | Máximo 1 ciclo |

### Impacto por Área

| Aspecto | Descripción |
|---------|-------------|
| **Disponibilidad** | Dispositivo queda offline indefinidamente |
| **Pérdida de datos** | Datos acumulados en buffer nunca se envían |
| **Operación** | Requiere intervención física para recuperar |
| **Confiabilidad** | Incumple principio PRINC-02 de continuidad |

### Requisito Violado

**RF-12 — Selección del mejor operador si el preferido falla:**
> *"Si el intento de conexión falla y hay múltiples operadores, el sistema deberá evaluar RSSI, estabilidad y disponibilidad y seleccionar el más adecuado"*

**RF-13 — Memorización persistente del operador exitoso:**
> *"Después de que se establezca una conexión exitosa, el subsistema de comunicaciones deberá almacenar en memoria persistente el operador usado"*

El sistema actualmente:
- ❌ NO evalúa otras operadoras cuando falla la guardada
- ❌ NO actualiza NVS cuando la operadora guardada no funciona
- ❌ NO cumple con la resiliencia esperada

---

## ⚠️ ANÁLISIS DE RIESGOS

### Riesgos Identificados

| # | Riesgo | Prob. | Impacto | Severidad | Mitigación |
|---|--------|-------|---------|-----------|------------|
| R1 | Escaneo consume batería excesiva | Media | Alto | 🟠 | Solo escanea si falla guardada |
| R2 | Timeout largo bloquea ciclo (~10min) | Media | Medio | 🟡 | Comportamiento existente, no regresión |
| R3 | Bucle infinito en zona sin cobertura | Alta | Alto | 🔴 | **skipScanCycles** (ver abajo) |
| R4 | Regresión en flujo normal | Baja | Alto | 🟠 | Condición `!configOk` protege flujo |

### Mitigación R3: Protección Anti-Bucle

**Problema:** Si ninguna operadora funciona, cada ciclo haría escaneo completo.

**Solución:** Variable `skipScanCycles` en NVS que salta escaneos por N ciclos tras fallo total.

### Mitigación R4: Análisis de No-Regresión

```
Flujo cuando operadora guardada FUNCIONA:
  configOk = true → condición (!configOk && tieneOperadoraGuardada) = FALSE
  → Bloque FIX-V2 NO se ejecuta → Flujo idéntico al original ✅
```

### Estrategia de Rollback

```cpp
#define ENABLE_FIX_V2_FALLBACK_OPERADORA 0  // Cambiar a 0 = código original
```

---

## 🌿 BRANCH DE DESARROLLO

**Cumplimiento de Premisa #1 (Aislamiento)**

```bash
# Crear branch dedicado ANTES de implementar
git checkout -b fix-2-fallback-operadora

# Trabajar exclusivamente en este branch
# No mezclar con otros fixes

# Solo hacer merge a main después de validación completa en campo
git checkout main
git merge fix-2-fallback-operadora
```

---

## 🔧 IMPLEMENTACIÓN

### Estrategia
Modificar el flujo para que si `configureOperator()` falla con operadora guardada:
1. **Verificar** si debemos saltar escaneo (protección anti-bucle)
2. Borre la operadora de NVS
3. Ejecute escaneo completo de todas las operadoras
4. Seleccione la mejor según score
5. **Validar** que el score sea válido (>-999)
6. Si no hay operadoras válidas, **activar protección** para próximos ciclos
7. Continúe el proceso de conexión con la nueva operadora

### Cambio 1: FeatureFlags.h

**Archivo:** `src/FeatureFlags.h`  
Agregar flag para el nuevo fix:

```cpp
// ANTES (al final del archivo)
#endif // FEATURE_FLAGS_H

// DESPUÉS
/**
 * FIX-V2: Fallback a escaneo cuando falla operadora guardada
 * 
 * Problema: Si configureOperator() falla con operadora de NVS,
 *           el sistema retorna sin intentar otras operadoras.
 * 
 * Solución: Borrar NVS y ejecutar escaneo completo si falla.
 * 
 * Mitigaciones incluidas:
 *   - skipScanCycles: Evita bucle infinito en zonas sin cobertura
 *   - Validación de score mínimo antes de reintentar
 * 
 * Requisito: RF-12
 */
#define ENABLE_FIX_V2_FALLBACK_OPERADORA 1

/** @brief Ciclos a saltar tras escaneo fallido (protección anti-bucle) */
#define FIX_V2_SKIP_CYCLES_ON_FAIL 3

#endif // FEATURE_FLAGS_H
```

### Cambio 2: AppController.cpp

**Archivo:** `AppController.cpp`  
**Líneas:** 383-393 (aproximadamente)

```cpp
// ANTES
#if ENABLE_FIX_V1_SKIP_RESET_PDP
  if (!lte.configureOperator(operadoraAUsar, tieneOperadoraGuardada)) { lte.powerOff(); return false; }
#else
  if (!lte.configureOperator(operadoraAUsar))   { lte.powerOff(); return false; }
#endif

// DESPUÉS
#if ENABLE_FIX_V1_SKIP_RESET_PDP
  bool configOk = lte.configureOperator(operadoraAUsar, tieneOperadoraGuardada);
#else
  bool configOk = lte.configureOperator(operadoraAUsar);
#endif

#if ENABLE_FIX_V2_FALLBACK_OPERADORA
  // ============ [FIX-V2 START] Fallback a escaneo si falla operadora guardada ============
  // Fecha: 13 Ene 2026
  // Autor: Luis Ocaranza
  // Requisito: RF-12
  // Premisas: P2 (mínimo), P3 (defaults), P4 (flag), P5 (logs), P6 (aditivo)
  if (!configOk && tieneOperadoraGuardada) {
    Serial.println("[WARN][APP] Operadora guardada falló. Evaluando fallback...");
    
    // --- PROTECCIÓN ANTI-BUCLE: Verificar si debemos saltar escaneo ---
    preferences.begin("sensores", false);
    uint8_t skipCycles = preferences.getUChar("skipScanCycles", 0);
    if (skipCycles > 0) {
      preferences.putUChar("skipScanCycles", skipCycles - 1);
      preferences.end();
      Serial.print("[WARN][APP] Saltando escaneo. Ciclos restantes: ");
      Serial.println(skipCycles - 1);
      lte.powerOff();
      return false;
    }
    preferences.end();
    // --- FIN PROTECCIÓN ANTI-BUCLE ---
    
    Serial.println("[INFO][APP] Iniciando escaneo de todas las operadoras...");
    
    // Borrar operadora de NVS inmediatamente
    preferences.begin("sensores", false);
    preferences.remove("lastOperator");
    preferences.end();
    Serial.println("[INFO][APP] Operadora eliminada de NVS");
    
    // Escanear todas las operadoras
    for (uint8_t i = 0; i < NUM_OPERADORAS; i++) {
      lte.testOperator((Operadora)i);
    }
    
    // Seleccionar la mejor
    operadoraAUsar = lte.getBestOperator();
    int bestScore = lte.getOperatorScore(operadoraAUsar);
    tieneOperadoraGuardada = false;  // Ya no tiene guardada
    
    Serial.print("[INFO][APP] Nueva operadora seleccionada: ");
    Serial.print(OPERADORAS[operadoraAUsar].nombre);
    Serial.print(" (Score: ");
    Serial.print(bestScore);
    Serial.println(")");
    
    // --- VALIDACIÓN DE SCORE: Verificar que hay señal válida ---
    if (bestScore <= -999) {
      Serial.println("[ERROR][APP] Ninguna operadora con señal válida.");
      Serial.print("[INFO][APP] Activando protección: saltando próximos ");
      Serial.print(FIX_V2_SKIP_CYCLES_ON_FAIL);
      Serial.println(" ciclos de escaneo.");
      
      preferences.begin("sensores", false);
      preferences.putUChar("skipScanCycles", FIX_V2_SKIP_CYCLES_ON_FAIL);
      preferences.end();
      
      lte.powerOff();
      return false;
    }
    // --- FIN VALIDACIÓN ---
    
    // Intentar configurar con la nueva operadora
    configOk = lte.configureOperator(operadoraAUsar);
  }
  // ============ [FIX-V2 END] ============
#endif

  if (!configOk) { 
    lte.powerOff(); 
    return false; 
  }
```

---

## 🧪 VERIFICACIÓN

### Escenario de Prueba
1. Configurar dispositivo en zona con ATT disponible
2. Dejar que transmita y guarde ATT en NVS
3. Mover a zona sin ATT pero con TELCEL
4. Verificar comportamiento

### Output Esperado (con FIX-V2)

**Caso 1: Operadora guardada falla, alternativa encontrada**
```
[INFO][APP] Usando operadora guardada: AT&T MEXICO (334090)
[WARN][APP] Operadora guardada falló. Evaluando fallback...
[INFO][APP] Iniciando escaneo de todas las operadoras...
[INFO][APP] Operadora eliminada de NVS
========================================
Prueba completa: TELCEL
========================================
...
[INFO][APP] Nueva operadora seleccionada: TELCEL (Score: 145)
[INFO][APP] Operadora guardada para futuros envios: TELCEL
```

**Caso 2: Zona sin cobertura (ninguna operadora funciona)**
```
[INFO][APP] Usando operadora guardada: AT&T MEXICO (334090)
[WARN][APP] Operadora guardada falló. Evaluando fallback...
[INFO][APP] Iniciando escaneo de todas las operadoras...
[INFO][APP] Operadora eliminada de NVS
...
[INFO][APP] Nueva operadora seleccionada: TELCEL (Score: -999)
[ERROR][APP] Ninguna operadora con señal válida.
[INFO][APP] Activando protección: saltando próximos 3 ciclos de escaneo.
```

**Caso 3: Ciclo posterior en zona sin cobertura (protección activa)**
```
[INFO][APP] No hay operadora guardada. Probando todas...
[WARN][APP] Saltando escaneo. Ciclos restantes: 2
```

### Criterios de Aceptación
- [ ] Si operadora guardada falla, se ejecuta escaneo automático
- [ ] La operadora fallida se borra de NVS antes del escaneo
- [ ] Se selecciona la mejor operadora disponible
- [ ] La nueva operadora exitosa se guarda en NVS
- [ ] El dispositivo no queda bloqueado tras cambio de zona
- [ ] Compatible con FIX-V1 (skipReset)
- [ ] **NUEVO:** Si ninguna operadora tiene señal, activa protección anti-bucle
- [ ] **NUEVO:** Protección decrementa cada ciclo hasta permitir nuevo escaneo
- [ ] **NUEVO:** Rollback funcional cambiando flag a 0

---

## ⚖️ EVALUACIÓN CRÍTICA

### ¿Por qué no es comportamiento "normal"?

El comportamiento actual es un **bug**, no un diseño intencional, porque:

1. **Viola requisito explícito RF-12** que exige fallback a escaneo
2. **El código de limpieza de NVS existe** (líneas 456-461) pero nunca se ejecuta en este caso
3. **El dispositivo queda inutilizable** sin intervención manual
4. **Contradice PRINC-02** de continuidad operativa

### Consideraciones de Energía (RF-14)

El escaneo completo consume energía, pero:
- Solo se ejecuta cuando **falla** la operadora guardada
- Es preferible gastar energía una vez que quedar offline indefinidamente
- **Protección anti-bucle** (`skipScanCycles`) limita escaneos en zonas sin cobertura
- FIX-V4 (futuro) agregará contador de escaneos/día para cumplir RF-14 completamente

---

## 🧪 MATRIZ DE PRUEBAS

| # | Escenario | Entrada | Resultado Esperado | Riesgo Mitigado |
|---|-----------|---------|-------------------|-----------------|
| T1 | Operadora guardada funciona | ATT en zona ATT | Flujo normal, sin cambios | R4 |
| T2 | Operadora guardada no existe | ATT en zona TELCEL | Escanea, selecciona TELCEL, guarda | - |
| T3 | Zona sin cobertura | Ninguna operadora | Escanea 1 vez, activa skip 3 ciclos | R3 |
| T4 | Ciclo 2 sin cobertura | skipCycles=2 | Salta escaneo, decrementa a 1 | R3, R1 |
| T5 | Ciclo 4 sin cobertura | skipCycles=0 | Reintenta escaneo | - |
| T6 | Rollback | Flag=0 | Comportamiento original exacto | R4 |

---

## � MÉTRICAS BASELINE vs ESPERADO

**Cumplimiento de Premisa #8 (Métricas Objetivas)**

| Métrica | Baseline v2.0.2 | Esperado v2.0.3 | Criterio |
|---------|-----------------|-----------------|----------|
| **Tiempo ciclo** | ~4 min | ~4 min (+15s máx en fallback) | No degradar >10% |
| **Reinicios inesperados** | 0 | 0 | ✅ CRÍTICO: cero |
| **Fallback operadora** | ❌ No existe | ✅ Funcional | MEJORA |
| **Éxito TX** | 95% | ≥95% | No degradar |
| **Reconexión tras cambio zona** | ❌ Manual | ✅ Automática | MEJORA |
| **RAM libre** | 180KB | ≥175KB | Máx -5KB |
| **Escaneos en zona sin cobertura** | ∞ (bucle) | Máx 1/3 ciclos | MEJORA (R3) |

### Cómo medir
```cpp
// Al inicio del ciclo:
Serial.print("[DEBUG][APP] Free heap: ");
Serial.println(ESP.getFreeHeap());

// Al final del ciclo:
Serial.print("[DEBUG][APP] Tiempo ciclo: ");
Serial.print((millis() - cycleStart) / 1000);
Serial.println(" segundos");
```

---

## 🔬 PIRÁMIDE DE TESTING

**Cumplimiento de Premisa #7 (Testing Gradual)**

```
         ┌─────────────┐
    5.   │  Campo 7d   │  ← 2 dispositivos en zonas diferentes
         └─────────────┘
        ┌───────────────┐
    4.  │ Hardware 24h  │  ← 24 ciclos reales, monitoreo logs
        └───────────────┘
      ┌───────────────────┐
    3.│ Hardware 1 ciclo  │  ← Boot → LTE → Fallback → TX → Sleep
      └───────────────────┘
    ┌─────────────────────────┐
  2.│ Test unitario NVS (5m)  │  ← skipScanCycles read/write
    └─────────────────────────┘
  ┌───────────────────────────────┐
1.│ Compilación sin errores (2m)  │  ← 0 errors, 0 warnings
  └───────────────────────────────┘
```

### Capa 1: Compilación (2 min)
- [ ] Compila sin errores
- [ ] Compila sin warnings
- [ ] Flag=0 compila correctamente (rollback)

### Capa 2: Test unitario NVS (5 min)
```cpp
void testSkipCyclesNVS() {
  // Test escritura
  preferences.begin("sensores", false);
  preferences.putUChar("skipScanCycles", 3);
  preferences.end();
  
  // Test lectura
  preferences.begin("sensores", true);
  uint8_t val = preferences.getUChar("skipScanCycles", 0);
  preferences.end();
  
  Serial.println(val == 3 ? "✅ NVS OK" : "❌ FALLO");
}
```

### Capa 3: Hardware 1 ciclo (20 min)
- [ ] Simular fallo de operadora guardada (desconectar antena brevemente)
- [ ] Verificar logs de fallback
- [ ] Verificar nueva operadora guardada en NVS

### Capa 4: Hardware 24h (1 día)
- [ ] 24 ciclos sin reinicio inesperado
- [ ] Memoria estable (no leak)
- [ ] Logs consistentes

### Capa 5: Campo 7d (1 semana)
- [ ] Dispositivo A: zona con cobertura estable
- [ ] Dispositivo B: zona con cobertura intermitente
- [ ] Verificar autorecuperación

---

## ⏪ PLAN DE ROLLBACK

**Cumplimiento de Premisa #9**

### Plan A: Feature flag (5 min)
```cpp
// En src/FeatureFlags.h
#define ENABLE_FIX_V2_FALLBACK_OPERADORA 0  // Cambiar 1 → 0
// Recompilar y subir
```

### Plan B: Volver a commit anterior (10 min)
```bash
git log --oneline -5
git checkout <commit-antes-de-fix-v2>
# Recompilar y subir
```

### Plan C: Limpiar NVS corrupto (15 min)
```cpp
// En setup() temporalmente
preferences.begin("sensores", false);
preferences.remove("skipScanCycles");
preferences.remove("lastOperator");
preferences.end();
Serial.println("NVS limpiado");
ESP.restart();
```

---

## �📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
| 2026-01-13 | Problema identificado en producción | - |
| 2026-01-13 | Documentación creada | - |
| 2026-01-13 | Agregadas mitigaciones de riesgo (anti-bucle, validación score) | - |
| 2026-01-13 | Actualizado para cumplir premisas (P1, P7, P8, P9, P10) | - |
| PENDIENTE | Implementación | v2.0.3 |
| PENDIENTE | Verificación en campo | v2.0.3 |
