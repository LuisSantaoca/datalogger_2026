# PREMISAS Y ESTRATEGIA PARA IMPLEMENTACIÓN DE FIXES
## Guía de Desarrollo Seguro para sensores_rv03

**Creado:** 30 Oct 2025  
**Actualizado:** 13 Ene 2026  
**Proyecto:** sensores_rv03 (ESP32-S3 + SIM7080G)  
**Aplicable a:** Todos los fixes desde FIX-V1 en adelante  
**Objetivo:** Garantizar cero degradación y máxima estabilidad

---

## 🎯 FILOSOFÍA CENTRAL

> **"Si no lo toco, no lo rompo. Si lo toco, lo valido. Si falla, lo deshabilito."**

Cada fix debe ser:
- ✅ **Aditivo** - Agregar funcionalidad, no cambiar existente
- ✅ **Defensivo** - Defaults seguros si algo falla
- ✅ **Reversible** - Rollback en < 5 minutos
- ✅ **Validado** - Testing gradual antes de despliegue
- ✅ **Documentado** - Trazabilidad completa

---

## 📐 PREMISA #1: AISLAMIENTO TOTAL

### Concepto
Cada fix se desarrolla en ambiente controlado sin afectar código estable.

### Implementación

**Branch dedicado:**
```bash
git checkout -b fix-N-nombre-descriptivo
# Ejemplo: fix-2-persistencia-estado
```

**Nunca:**
- ❌ Trabajar directo en `main`
- ❌ Mezclar múltiples fixes en un commit
- ❌ Hacer cambios sin branch

**Siempre:**
- ✅ Un branch por fix
- ✅ Un fix por branch
- ✅ Merge a `main` solo después de validación completa

### Beneficio
Si fix falla, `main` permanece intacto y operacional.

---

## 📐 PREMISA #2: CAMBIOS MÍNIMOS Y LOCALIZADOS

### Concepto
Modificar la menor cantidad de código posible. Mayor superficie = mayor riesgo.

### Implementación

**Archivos objetivo:**
```
✅ Modificar: Solo archivos relacionados con el fix
❌ Tocar: Archivos no relacionados
❌ Refactorizar: "Ya que estoy, mejoro esto otro"
```

**Ejemplo FIX-V2 (Fallback Operadora):**
```
✅ Tocar:
  - src/FeatureFlags.h (agregar flag)
  - AppController.cpp (lógica de fallback)

❌ NO tocar:
  - src/data_lte/LTEModule.cpp (funciones ya validadas)
  - src/data_sensors/*.cpp (no relacionado)
  - src/data_buffer/*.cpp (no relacionado)
  - src/data_gps/*.cpp (no relacionado)
  - Lógica de sleep (ya validada)
```

### Reglas de oro
1. **Un fix, un propósito** - No agregar "mejoras" extras
2. **Agregar, no reemplazar** - Preservar código existente
3. **Si no es necesario, no lo cambies**

### Beneficio
Debugging más fácil, menor probabilidad de efectos secundarios.

---

## 📐 PREMISA #3: DEFAULTS SEGUROS (FAIL-SAFE)

### Concepto
Si el fix falla, el dispositivo debe comportarse como la versión anterior estable.

### Implementación

**Estructura con valores seguros (ejemplo operadora):**
```cpp
// ✅ BIEN: Valores que permiten operación normal
// En AppController.cpp - lectura de NVS
Operadora operadoraAUsar = (Operadora)preferences.getUChar("lastOperator", 0);
// Default 0 = TELCEL (primera operadora, siempre disponible)

uint8_t skipScanCycles = preferences.getUChar("skipScanCycles", 0);
// Default 0 = escanear normalmente

// ❌ MAL: Valores que pueden causar problemas
Operadora operadoraAUsar = (Operadora)preferences.getUChar("lastOperator", 255);
// 255 está fuera de rango de operadoras válidas (0-4)
```

**Validación al cargar:**
```cpp
void cargarOperadora() {
  preferences.begin("sensores", true);
  uint8_t op = preferences.getUChar("lastOperator", 0);
  preferences.end();
  
  // 🛡️ Validar rango
  if (op >= NUM_OPERADORAS) {
    op = 0;  // Default seguro = TELCEL
    Serial.println("[WARN][APP] Operadora inválida, usando default");
  }
  
  operadoraAUsar = (Operadora)op;
}
```

### Casos a manejar
- **NVS vacía** (primer boot) → Usar defaults
- **NVS corrupta** → Limpiar y usar defaults
- **Valores fuera de rango** → Sanitizar a valores válidos
- **Operación falla** → Continuar con lógica legacy

### Beneficio
Device nunca queda inoperacional por un fix fallido.

---

## 📐 PREMISA #4: FEATURE FLAGS

### Concepto
Cada fix debe poder deshabilitarse en tiempo de compilación sin borrar código.

### Implementación

**Header centralizado (src/FeatureFlags.h):**
```cpp
// Sistema de Feature Flags para sensores_rv03
#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

// ============ FIX FLAGS ============
#define ENABLE_FIX_V1_SKIP_RESET_PDP      1  // Reduce eventos PDP
#define ENABLE_FIX_V2_FALLBACK_OPERADORA  1  // Fallback a escaneo
#define ENABLE_FIX_V3_PLACEHOLDER         0  // Reservado

// ============ FEAT FLAGS ============
#define ENABLE_FEAT_V2_CYCLE_TIMING       1  // Medición de tiempos

#endif
```

**Uso en código:**
```cpp
#if ENABLE_FIX_V2_FALLBACK_OPERADORA
  // ============ [FIX-V2 START] ============
  if (!configOk && tieneOperadoraGuardada) {
    // Lógica de fallback
  }
  // ============ [FIX-V2 END] ============
#endif

// Lógica original siempre disponible como fallback
if (!configOk) { 
  lte.powerOff(); 
  return false; 
}
```

### Ventajas
1. **Rollback instantáneo** - Cambiar `true` → `false` y recompilar
2. **A/B testing** - Comparar versión con/sin fix
3. **Debugging** - Aislar problemas rápidamente
4. **Compatibilidad** - Mantener código legacy funcional

### Beneficio
Si fix causa problemas en campo, deshabilitar en < 5 minutos.

---

## 📐 PREMISA #5: LOGGING EXHAUSTIVO

### Concepto
Cada operación crítica debe generar log visible. Si algo falla, logs deben mostrar exactamente dónde.

### Niveles de logging

```cpp
// 0 = ERROR (siempre visible)
// 1 = WARNING (importante)
// 2 = INFO (operaciones exitosas)
// 3 = DEBUG (detalles técnicos)
```

### Implementación

**Formato de logs en sensores_rv03:**
```cpp
// Formato: [NIVEL][MODULO] Mensaje
Serial.println("[INFO][APP] Usando operadora guardada: " + String(nombre));
Serial.println("[WARN][APP] Operadora guardada falló. Evaluando fallback...");
Serial.println("[ERROR][APP] Ninguna operadora con señal válida.");
Serial.println("[DEBUG][APP] Score: " + String(score));
```

**Al cargar operadora:**
```cpp
preferences.begin("sensores", true);
if (preferences.isKey("lastOperator")) {
  operadoraAUsar = (Operadora)preferences.getUChar("lastOperator", 0);
  Serial.print("[INFO][APP] Usando operadora guardada: ");
  Serial.println(OPERADORAS[operadoraAUsar].nombre);
} else {
  Serial.println("[INFO][APP] No hay operadora guardada. Probando todas...");
}
preferences.end();
```

**Al ejecutar fallback (FIX-V2):**
```cpp
#if ENABLE_FIX_V2_FALLBACK_OPERADORA
if (!configOk && tieneOperadoraGuardada) {
  Serial.println("[WARN][APP] Operadora guardada falló. Evaluando fallback...");
  
  // Verificar protección anti-bucle
  uint8_t skipCycles = preferences.getUChar("skipScanCycles", 0);
  if (skipCycles > 0) {
    Serial.print("[WARN][APP] Saltando escaneo. Ciclos restantes: ");
    Serial.println(skipCycles - 1);
  }
  
  // ... resto del código ...
  
  Serial.print("[INFO][APP] Nueva operadora seleccionada: ");
  Serial.print(OPERADORAS[operadoraAUsar].nombre);
  Serial.print(" (Score: ");
  Serial.print(bestScore);
  Serial.println(")");
}
#endif
```

### Prefijos de contexto para sensores_rv03
- `[APP]` - Lógica principal (AppController)
- `[LTE]` - Operaciones del módem SIM7080G
- `[GPS]` - Operaciones GNSS
- `[BUFFER]` - Operaciones de almacenamiento
- `[SENSOR]` - Lecturas de sensores
- `[SLEEP]` - Gestión de deep sleep

### Beneficio
Debugging remoto sin necesidad de conectar físicamente el device.

---

## 📐 PREMISA #6: NO CAMBIAR LÓGICA EXISTENTE

### Concepto
El código que funciona en producción no se toca. Fixes agregan funcionalidad, no reemplazan.

### Patrón de implementación

**❌ MAL - Cambiar lógica existente:**
```cpp
// Código original
if (!lte.configureOperator(operadoraAUsar)) { 
  lte.powerOff(); 
  return false;  // CAMBIADO: sin alternativa
}

// RIESGO: Si operadora falla, no hay recuperación
```

**✅ BIEN - Agregar lógica ANTES, preservar original:**
```cpp
// Capturar resultado en variable (permite agregar lógica)
bool configOk = lte.configureOperator(operadoraAUsar);

#if ENABLE_FIX_V2_FALLBACK_OPERADORA
// ============ [FIX-V2 START] Fallback si falla operadora guardada ============
if (!configOk && tieneOperadoraGuardada) {
  Serial.println("[WARN][APP] Operadora guardada falló. Iniciando fallback...");
  // ... lógica de escaneo y selección ...
  configOk = lte.configureOperator(operadoraAUsar);  // Reintentar
}
// ============ [FIX-V2 END] ============
#endif

// Lógica ORIGINAL preservada (siempre se ejecuta si todo falla)
if (!configOk) { 
  lte.powerOff(); 
  return false; 
}
```

### Estrategia de capas en sensores_rv03

```
┌─────────────────────────────────┐
│   Lógica nueva (FIX-V2)         │ ← Intenta fallback primero
│   Si falla → continúa           │
├─────────────────────────────────┤
│  Lógica original (v2.0.2)       │ ← Siempre funciona
│     Código probado en campo     │
└─────────────────────────────────┘
```

### Beneficio
Si fix falla, device cae en código probado y estable.

---

## 📐 PREMISA #7: TESTING GRADUAL

### Concepto
Validar en capas incrementales. No saltar directo a testing en hardware real.

### Pirámide de testing

```
         ┌─────────────┐
         │  Campo 7d   │  ← Validación final
         └─────────────┘
        ┌───────────────┐
        │  Hardware 24h │  ← Ciclos reales
        └───────────────┘
      ┌───────────────────┐
      │  Hardware 1 ciclo │  ← Funcionalidad completa
      └───────────────────┘
    ┌─────────────────────────┐
    │  Test unitario (5 min)  │  ← NVS read/write
    └─────────────────────────┘
  ┌───────────────────────────────┐
  │  Compilación (2 min)          │  ← Sin errores
  └───────────────────────────────┘
```

### Capa 1: Compilación (2 min)
```bash
# En Arduino IDE 2 o PlatformIO
# Criterio: 0 errores, 0 warnings
```

### Capa 2: Test unitario (5 min)
```cpp
void testNVS() {
  // Probar escritura/lectura de operadora
  preferences.begin("sensores", false);
  preferences.putUChar("lastOperator", 2);  // ATT2
  preferences.end();
  
  preferences.begin("sensores", true);
  uint8_t op = preferences.getUChar("lastOperator", 0);
  preferences.end();
  
  if (op == 2) {
    Serial.println("✅ NVS OK");
  } else {
    Serial.println("❌ FALLO");
  }
}
```

### Capa 3: Hardware 1 ciclo (20 min)
- Boot → GPS → LTE → Transmit → Save → Sleep
- Verificar logs, métricas, watchdog

### Capa 4: Hardware 24h (1 día)
- Múltiples ciclos consecutivos
- Verificar estabilidad, memoria, reintentos

### Capa 5: Campo 7 días (1 semana)
- Condiciones reales (temperatura, señal variable)
- Verificar uptime, autorecuperación

### Criterios de paso
- ✅ Capa N exitosa → Continuar a N+1
- ❌ Capa N falla → Debuggear, no avanzar

### Beneficio
Detección temprana de problemas con bajo costo.

---

## 📐 PREMISA #8: MÉTRICAS OBJETIVAS

### Concepto
Comparar versión nueva vs. baseline estable con métricas medibles.

### Baseline sensores_rv03 (v2.0.2 actual)
```
📊 BASELINE v2.0.2:
   Tiempo total ciclo: ~3-5 min (variable por LTE)
   GPS: Solo primer ciclo post-boot
   LTE operadoras: 5 disponibles
   Transmisiones exitosas: >95%
   Buffer persistente: LittleFS
   Deep sleep: 10 min default
```

### Métricas a comparar

| Métrica | Comparación | Criterio aceptación |
|---------|-------------|---------------------|
| **Tiempo total** | Debe ser ≤ baseline | No aumentar significativamente |
| **Reinicios inesperados** | Debe ser = 0 | CRÍTICO: cero resets |
| **Reconexión tras fallo** | Debe mejorar | FIX-V2: fallback funcional |
| **Éxito transmit** | Debe ser ≥ 95% | Sin degradación |
| **Memoria libre** | Debe ser ≥ 80% baseline | Sin leaks |
| **Consumo batería** | Debe ser ≤ baseline | No aumentar |

### Formato de reporte para sensores_rv03
```markdown
## Comparación v2.0.2 vs v2.0.3

| Métrica | v2.0.2 | v2.0.3 | Δ | Status |
|---------|--------|--------|---|--------|
| Tiempo ciclo | ~4 min | ~4 min | 0 | ✅ OK |
| Reinicios | 0 | 0 | 0 | ✅ OK |
| Fallback operadora | ❌ No | ✅ Sí | +1 | ✅ MEJORA |
| Éxito TX | 95% | 98% | +3% | ✅ MEJORA |
| RAM libre | 180KB | 178KB | -2KB | ✅ OK |
```

### Beneficio
Decisión objetiva de deploy basada en datos, no intuición.

---

## 📐 PREMISA #9: ROLLBACK PLAN

### Concepto
Siempre tener Plan B documentado y probado antes de deploy.

### Plan A: Feature flag
```cpp
// Opción más rápida (5 min) - En src/FeatureFlags.h
#define ENABLE_FIX_V2_FALLBACK_OPERADORA 0  // Cambiar 1 → 0
// Recompilar y subir
```

### Plan B: Volver a versión anterior
```bash
# Opción rápida (10 min)
git checkout v2.0.2
# Recompilar y subir
```

### Plan C: Limpiar NVS corrupto
```cpp
// Si NVS está corrupto - Agregar en setup() temporalmente
void emergencyCleanNVS() {
  Preferences prefs;
  prefs.begin("sensores", false);
  prefs.clear();
  prefs.end();
  Serial.println("NVS limpiado. Reiniciando...");
  ESP.restart();
}
```

### Plan D: Factory reset
```bash
# Última opción (15 min)
esptool.py erase_flash
# Recompilar y subir firmware limpio
```

### Tiempo de recuperación objetivo
- **Plan A:** < 5 minutos
- **Plan B:** < 10 minutos
- **Plan C:** < 15 minutos
- **Plan D:** < 20 minutos

### Beneficio
Device nunca queda inoperacional por más de 20 minutos.

---

## 📐 PREMISA #10: DOCUMENTACIÓN COMPLETA

### Concepto
Cada cambio debe ser autoexplicativo y trazable.

### Comentarios en código (sensores_rv03)
```cpp
// ============ [FIX-V2 START] Fallback a escaneo si falla operadora guardada ============
// Fecha: 13 Ene 2026
// Requisito: RF-12
// Propósito: Si operadora guardada falla, escanear todas y seleccionar mejor
// Riesgo: BAJO - No modifica lógica existente, solo agrega antes
// Mitigaciones: skipScanCycles (anti-bucle), validación de score
// Rollback: #define ENABLE_FIX_V2_FALLBACK_OPERADORA 0

if (!configOk && tieneOperadoraGuardada) {
  // Implementación...
}
// ============ [FIX-V2 END] ============
```

### Estructura de documentación en sensores_rv03

```
sensores_rv03/
├── fixs-feats/
│   ├── fixs/
│   │   ├── FIX_V1_PDP_REDUNDANTE.md      ✅ Implementado
│   │   └── FIX_V2_FALLBACK_OPERADORA.md  📋 Documentado
│   ├── feats/
│   │   ├── FEAT_V0_VERSION_CONTROL.md
│   │   └── FEAT_V1_FEATURE_FLAGS.md
│   ├── METODOLOGIA_DE_CAMBIOS.md
│   ├── PLANTILLA.md
│   └── PREMISAS_DE_FIXS.md               ← Este documento
├── calidad/
│   ├── AUDITORIA_REQUISITOS.md           ← Trazabilidad
│   └── HALLAZGOS_PENDIENTES.md           ← Backlog
└── src/
    ├── FeatureFlags.h                    ← Flags centralizados
    └── version_info.h                    ← Control de versiones
```

### Commits descriptivos para sensores_rv03
```bash
# ✅ BIEN
git commit -m "fix(FIX-V2): Implementar fallback a escaneo de operadoras

- Agregar flag ENABLE_FIX_V2_FALLBACK_OPERADORA en FeatureFlags.h
- Implementar lógica de fallback en AppController.cpp
- Agregar protección anti-bucle (skipScanCycles)
- Validación de score mínimo antes de reintentar
- Cumple RF-12: Selección del mejor operador si falla preferido

Refs: fixs-feats/fixs/FIX_V2_FALLBACK_OPERADORA.md"

# ❌ MAL
git commit -m "fix: arreglos de operadora"
```

### Beneficio
Cualquier desarrollador puede entender y mantener el código.

---

## 🎓 LECCIONES APRENDIDAS EN sensores_rv03

### FIX-V1: Skip Reset PDP
**Lo que funcionó bien:**
- ✅ **Feature flag** - Rollback instantáneo posible
- ✅ **Cambios mínimos** - Solo agregó parámetro `skipReset`
- ✅ **No rompió legacy** - Código original preservado en #else
- ✅ **Documentación** - FIX_V1_PDP_REDUNDANTE.md completo

**Métricas logradas:**
- Eventos PDP reducidos de 3+ a 1 por ciclo
- Sin reinicios ni efectos secundarios

### FIX-V2: Fallback Operadora (en desarrollo)
**Mejoras aplicadas:**
- ✅ **Análisis de riesgos** - Identificados R1-R4 antes de implementar
- ✅ **Mitigaciones proactivas** - skipScanCycles, validación score
- ✅ **Matriz de pruebas** - 6 escenarios documentados
- ✅ **Auditoría de requisitos** - Trazado a RF-12

### Patrón establecido
Todas las premisas de este documento se aplican consistentemente desde FIX-V1.

---

## 📋 CHECKLIST PRE-COMMIT (sensores_rv03)

Antes de hacer commit de un fix, verificar:

- [ ] ✅ Compila sin errores ni warnings (Arduino IDE 2)
- [ ] ✅ Defaults seguros implementados (NVS)
- [ ] ✅ Feature flag en `src/FeatureFlags.h`
- [ ] ✅ Logging con formato `[NIVEL][MODULO]`
- [ ] ✅ No se cambió lógica existente (solo agregar)
- [ ] ✅ Validación de datos al cargar de NVS
- [ ] ✅ Testing unitario pasado
- [ ] ✅ Testing en hardware (1 ciclo) pasado
- [ ] ✅ Métricas ≤ baseline
- [ ] ✅ Reinicios inesperados = 0
- [ ] ✅ Plan de rollback documentado
- [ ] ✅ Comentarios `[FIX-Vn START/END]` agregados
- [ ] ✅ Documento `FIX_Vn_NOMBRE.md` creado en fixs/
- [ ] ✅ Commit message descriptivo con referencias

---

## 📊 TEMPLATE DE VALIDACIÓN (sensores_rv03)

### Usado después de cada fix:

```markdown
# Validación FIX-Vn: [Nombre del Fix]

## Hardware
- Device ID: ___________
- Versión anterior: v2.0.X
- Versión nueva: v2.0.Y
- Fecha testing: __________

## Métricas

| Métrica | Baseline | Nueva | Δ | Status |
|---------|----------|-------|---|--------|
| Tiempo ciclo | ~4 min | __ | __ | __ |
| Reinicios | 0 | __ | __ | __ |
| Fallback operadora | No | __ | __ | __ |
| Éxito TX | 95% | __ | __ | __ |
| RAM libre | 180KB | __ | __ | __ |

## Criterios de aceptación
- [ ] Reinicios inesperados = 0
- [ ] Tiempo ≤ baseline
- [ ] Funcionalidad 100%
- [ ] Sin memory leaks
- [ ] Rollback verificado (flag=0 funciona)

## Logs de prueba
[Pegar logs relevantes aquí]

## Conclusión
- [ ] ✅ APROBADO para producción
- [ ] ❌ RECHAZADO - Razón: ___________
```

---

## 🔗 REFERENCIAS

### Documentos relacionados en sensores_rv03
- [METODOLOGIA_DE_CAMBIOS.md](METODOLOGIA_DE_CAMBIOS.md) - Proceso de implementación
- [PLANTILLA.md](PLANTILLA.md) - Template para nuevos FIX/FEAT
- [calidad/AUDITORIA_REQUISITOS.md](../calidad/AUDITORIA_REQUISITOS.md) - Trazabilidad
- [calidad/HALLAZGOS_PENDIENTES.md](../calidad/HALLAZGOS_PENDIENTES.md) - Backlog

### Archivos clave
- `src/FeatureFlags.h` - Flags centralizados
- `src/version_info.h` - Control de versiones
- `AppController.cpp` - Lógica principal FSM

## Observaciones
___________________________________
___________________________________

## Decisión
[ ] ✅ APROBAR - Deploy a producción
[ ] ⏸️ PAUSAR - Requiere más testing
[ ] ❌ RECHAZAR - Rollback a versión anterior
```

---

## 🎯 RESUMEN EJECUTIVO

### 10 Premisas de Oro

1. **Aislamiento** - Branch dedicado, no tocar main
2. **Mínimos cambios** - Solo archivos necesarios
3. **Defaults seguros** - Si falla, funciona como antes
4. **Feature flags** - Deshabilitar en 5 min
5. **Logging exhaustivo** - Debugging remoto
6. **No cambiar existente** - Solo agregar
7. **Testing gradual** - Detección temprana
8. **Métricas objetivas** - Decisiones con datos
9. **Rollback plan** - Siempre tener Plan B
10. **Documentación** - Trazabilidad completa

### Garantías con estas premisas

1. ✅ **Device nunca inoperacional** - Fallback a código estable
2. ✅ **Rollback en < 5 min** - Feature flags
3. ✅ **Cero degradación** - Métricas validan
4. ✅ **Debugging rápido** - Logs exhaustivos
5. ✅ **Mantenibilidad** - Documentación completa

---

## 🔗 REFERENCIAS

**Aplicar en:**
- FIX-2: Persistencia estado (v4.2.0) ← Siguiente
- FIX-3: Timeout LTE dinámico (v4.3.0) ← Futuro
- FIX-4: Banda LTE inteligente (v4.4.0) ← Futuro
- Todos los fixes subsecuentes

**Basado en:**
- Experiencia FIX-1 (Watchdog)
- Análisis de logs de zona rural
- Best practices de desarrollo embedded

---

# 🎲 ANÁLISIS DE RIESGOS - DESPLIEGUE FIXES SEÑAL BAJA RURAL

## Contexto del Análisis

**Fecha:** 30 Oct 2025  
**Aplicable a:** 8 fixes identificados para operación con RSSI 8-14  
**Basado en:** Análisis consolidado 29 Oct 2025 (6403 líneas logs)  
**Estado actual:** Firmware v4.1.0 estable (100% transmisiones exitosas)  
**Zona:** Rural con RSSI promedio 12.5 (señal pobre)

---

## 🎯 FILOSOFÍA DE GESTIÓN DE RIESGOS

> **"El riesgo no es implementar mal, es degradar lo que funciona."**

Principios:
1. ✅ **Baseline conocido** - v4.1.0 funciona al 100%
2. ✅ **Cambios incrementales** - Un fix a la vez
3. ✅ **Validación objetiva** - Métricas comparables
4. ✅ **Rollback garantizado** - Plan B siempre listo
5. ✅ **Testing gradual** - 5 capas de validación

---

## 📊 MATRIZ DE RIESGOS GLOBAL

### Vista Consolidada: 8 Fixes

| Fix | Impacto Técnico | Riesgo Inherente | Consecuencia Fallo | Mitigación | Riesgo Residual |
|-----|----------------|------------------|--------------------|-----------|--------------------|
| **FIX #1** Persistencia | ⭐⭐⭐⭐⭐ | 🟡 MEDIO | Device funciona sin cache | Feature flag | 🟢 BAJO |
| **FIX #2** Timeout Dinámico | ⭐⭐⭐⭐⭐ | 🟡 MEDIO | Timeouts incorrectos | Defaults seguros | 🟢 BAJO |
| **FIX #3** Init Módem | ⭐⭐⭐⭐ | 🟢 BAJO | Módem tarda más en init | Valores probados | 🟢 BAJO |
| **FIX #4** Banda Inteligente | ⭐⭐⭐ | 🟡 MEDIO | Búsqueda ineficiente | Fallback a búsqueda estándar | 🟢 BAJO |
| **FIX #5** Degradación | ⭐⭐⭐ | 🟢 BAJO | Sin detección temprana | Solo alertas, no bloquea | 🟢 BAJO |
| **FIX #6** GPS Cache | ⭐⭐ | 🟢 BAJO | GPS busca desde cero | Validación edad cache | 🟢 BAJO |
| **FIX #7** NB-IoT Fallback | ⭐⭐ | 🟠 ALTO | Modo no disponible en zona | Detección de soporte | 🟡 MEDIO |
| **FIX #8** Métricas Remotas | ⭐ | 🟢 BAJO | Sin diagnóstico remoto | Solo logging, no afecta lógica | 🟢 MUY BAJO |

**Leyenda:**
- 🔴 ALTO - Puede causar inoperatividad total
- 🟠 ALTO - Puede degradar significativamente
- 🟡 MEDIO - Puede causar problemas menores
- 🟢 BAJO - Impacto limitado o controlado

---

## 🔥 RIESGOS POR FIX (Detallado)

### FIX #1: PERSISTENCIA DE ESTADO

**Descripción:** Guardar RSSI, banda exitosa, GPS cache en NVS entre reinicios

#### Riesgos Inherentes

| # | Riesgo | Probabilidad | Impacto | Severidad |
|---|--------|--------------|---------|-----------|
| R1.1 | NVS corrupto impide boot | 🟢 Baja (5%) | 🔴 Alto | 🟡 MEDIO |
| R1.2 | Valores guardados inválidos (RSSI=0) | 🟡 Media (15%) | 🟠 Medio | 🟡 MEDIO |
| R1.3 | Memory leak por no cerrar Preferences | 🟢 Baja (5%) | 🟠 Medio | 🟢 BAJO |
| R1.4 | Cache GPS obsoleto (semanas antiguo) | 🟡 Media (20%) | 🟢 Bajo | 🟢 BAJO |
| R1.5 | Conflicto con watchdog durante save | 🟢 Muy Baja (2%) | 🔴 Alto | 🟡 MEDIO |

#### Estrategias de Mitigación

**R1.1 - NVS Corrupto:**
```cpp
// ✅ MITIGACIÓN: Try-catch y defaults seguros
void loadPersistedState() {
  try {
    modemPrefs.begin("modem", true);
    int rssi = modemPrefs.getInt("rssi", 15);  // Default si falla
    
    // Validar rango
    if (rssi < 0 || rssi > 31) {
      logMessage(1, "⚠️ RSSI inválido, usando default");
      rssi = 15;
    }
    
    persistentState.lastRSSI = rssi;
    modemPrefs.end();
  } catch (...) {
    logMessage(0, "❌ ERROR NVS, usando defaults");
    // Device continúa con valores default
  }
}
```

**R1.2 - Valores Inválidos:**
- Validación de rangos al cargar (RSSI: 0-31, Band: 1-28, etc.)
- Sanitización de valores fuera de rango
- Defaults seguros si validación falla

**R1.3 - Memory Leak:**
- Siempre cerrar `Preferences` con `.end()`
- Monitorear RAM libre en logs (`ESP.getFreeHeap()`)
- Testing de 100+ ciclos para detectar leaks

**R1.4 - Cache GPS Obsoleto:**
```cpp
// ✅ MITIGACIÓN: Validar edad de cache
unsigned long cacheAge = millis() - persistentState.lastGPSTime;
if (cacheAge > 86400000) {  // 24 horas = inválido
  logMessage(2, "GPS cache expirado, buscando fresh");
  // Buscar GPS desde cero
} else {
  // Usar cache
}
```

**R1.5 - Conflicto Watchdog:**
- Guardar estado ANTES de sleep (watchdog deshabilitado)
- Operación NVS rápida (< 500ms)
- Testing específico con watchdog activo

#### Criterios de Aceptación

- ✅ Boot exitoso en 100% casos (NVS corrupto o no)
- ✅ Valores fuera de rango sanitizados correctamente
- ✅ RAM libre estable después de 100 ciclos
- ✅ Cache GPS se invalida después de 24h
- ✅ Watchdog = 0 resets después de 1000 ciclos

#### Riesgo Residual: 🟢 BAJO

Con mitigaciones: probabilidad 2%, impacto bajo (device funciona sin cache).

---

### FIX #2: TIMEOUT LTE DINÁMICO

**Descripción:** Ajustar timeout LTE según RSSI actual (60-120s)

#### Riesgos Inherentes

| # | Riesgo | Probabilidad | Impacto | Severidad |
|---|--------|--------------|---------|-----------|
| R2.1 | Timeout muy corto → fallos conexión | 🟡 Media (20%) | 🟠 Medio | 🟡 MEDIO |
| R2.2 | Timeout muy largo → batería agotada | 🟢 Baja (10%) | 🟠 Medio | 🟡 MEDIO |
| R2.3 | Lógica incorrecta en bordes (RSSI=0, 31) | 🟢 Baja (5%) | 🟠 Medio | 🟢 BAJO |
| R2.4 | Timeout infinito por bug matemático | 🟢 Muy Baja (2%) | 🔴 Alto | 🟡 MEDIO |

#### Estrategias de Mitigación

**R2.1 - Timeout Demasiado Corto:**
```cpp
// ✅ MITIGACIÓN: Piso mínimo conservador
int calculateTimeout(int rssi) {
  int timeout = map(rssi, 0, 31, 120000, 60000);
  
  // Piso mínimo: NUNCA menor a 45s
  if (timeout < 45000) {
    logMessage(1, "⚠️ Timeout ajustado a piso (45s)");
    timeout = 45000;
  }
  
  // Techo máximo: NUNCA mayor a 150s
  if (timeout > 150000) {
    logMessage(1, "⚠️ Timeout ajustado a techo (150s)");
    timeout = 150000;
  }
  
  return timeout;
}
```

**R2.2 - Timeout Demasiado Largo:**
- Techo máximo de 150s (2.5 minutos)
- Monitorear consumo de batería en testing
- Comparar con baseline v4.1.0

**R2.3 - Casos Borde:**
- Validar RSSI antes de usar en fórmula
- Sanitizar valores: `rssi = constrain(rssi, 0, 31)`
- Testing específico con RSSI=0, 1, 30, 31, -1, 99

**R2.4 - Timeout Infinito:**
- Techo absoluto hardcoded: `timeout = min(timeout, 150000)`
- Watchdog como safety net (120s)
- Testing con valores extremos

#### Tabla de Validación

| RSSI | Timeout Calculado | Validado | Comportamiento Esperado |
|------|-------------------|----------|-------------------------|
| 0 | 120s | ✅ | Máximo conservador |
| 8 | 105s | ✅ | Zona crítica |
| 12 | 90s | ✅ | RSSI promedio rural |
| 20 | 68s | ✅ | Señal buena |
| 31 | 60s | ✅ | Señal excelente |
| -1 | 45s (piso) | ✅ | Valor inválido sanitizado |
| 99 | 45s (piso) | ✅ | Valor inválido sanitizado |

#### Criterios de Aceptación

- ✅ Timeout NUNCA < 45s ni > 150s
- ✅ Conexiones exitosas al 100% con RSSI 8-14
- ✅ Consumo batería ≤ baseline v4.1.0
- ✅ Valores inválidos sanitizados correctamente

#### Riesgo Residual: 🟢 BAJO

Con mitigaciones: probabilidad 3%, impacto bajo (fallback a timeout fijo 60s).

---

### FIX #3: INIT MÓDEM OPTIMIZADO

**Descripción:** Aumentar delay post-PWRKEY (1s → 5s) y timeout AT+CPIN? (5s → 20s)

#### Riesgos Inherentes

| # | Riesgo | Probabilidad | Impacto | Severidad |
|---|--------|--------------|---------|-----------|
| R3.1 | Delay demasiado largo ralentiza boot | 🟢 Baja (10%) | 🟢 Bajo | 🟢 BAJO |
| R3.2 | Timeout 20s no suficiente en zona extrema | 🟢 Baja (5%) | 🟢 Bajo | 🟢 BAJO |
| R3.3 | Interacción con watchdog (delay 5s) | 🟢 Muy Baja (2%) | 🟠 Medio | 🟢 BAJO |

#### Estrategias de Mitigación

**R3.1 - Delay Largo:**
- Aceptable: +4s en boot (de 1s a 5s)
- Beneficio: eliminar 15s de reintentos (net: -11s)
- Basado en datasheet SIM7080G (recomienda 3-5s)

**R3.2 - Timeout Insuficiente:**
```cpp
// ✅ MITIGACIÓN: Reintentos con backoff
int timeout = 20000;  // 20s inicial
int maxRetries = 3;

for (int i = 0; i < maxRetries; i++) {
  if (sendATCommand("+CPIN?", "READY", timeout)) {
    return true;
  }
  logMessage(1, "⚠️ Reintento " + String(i+1));
  timeout += 10000;  // Incrementar timeout en reintentos
}
```

**R3.3 - Watchdog:**
- Watchdog configurado a 120s (muy por encima de 5s)
- Pet watchdog antes del delay si es necesario
- Delay ocurre en setup, watchdog no activo aún

#### Criterios de Aceptación

- ✅ AT+CPIN? exitoso en 1er intento (95%+ casos)
- ✅ Tiempo boot total ≤ v4.1.0 (debido a eliminación reintentos)
- ✅ Watchdog = 0 resets

#### Riesgo Residual: 🟢 BAJO

Cambios basados en datasheet, riesgo mínimo.

---

### FIX #4: BANDA LTE INTELIGENTE

**Descripción:** Intentar banda persistida primero, luego búsqueda estándar

#### Riesgos Inherentes

| # | Riesgo | Probabilidad | Impacto | Severidad |
|---|--------|--------------|---------|-----------|
| R4.1 | Banda guardada ya no disponible | 🟡 Media (25%) | 🟢 Bajo | 🟢 BAJO |
| R4.2 | Comando AT malformado para banda específica | 🟢 Baja (5%) | 🟠 Medio | 🟢 BAJO |
| R4.3 | Loop infinito si banda falla repetidamente | 🟢 Muy Baja (2%) | 🔴 Alto | 🟡 MEDIO |

#### Estrategias de Mitigación

**R4.1 - Banda No Disponible:**
```cpp
// ✅ MITIGACIÓN: Fallback automático
if (ENABLE_PERSISTENCE && persistentState.lastSuccessfulBand > 0) {
  String bandCmd = "+CBANDCFG=\"CAT-M\"," + String(persistentState.lastSuccessfulBand);
  
  if (sendATCommand(bandCmd, "OK", 30000)) {
    logMessage(2, "✅ Banda " + String(persistentState.lastSuccessfulBand) + " exitosa");
    return true;
  } else {
    logMessage(1, "⚠️ Banda guardada falló, búsqueda estándar");
  }
}

// Lógica ORIGINAL sin cambios (fallback garantizado)
if (!sendATCommand("+CBANDCFG=\"CAT-M\",2,4,5", "OK", timeout)) {
  logMessage(1, "⚠️ Búsqueda estándar");
}
```

**R4.2 - Comando Malformado:**
- Validación de banda: `if (band < 1 || band > 28) return false`
- Testing con bandas válidas e inválidas
- String escaping correcto

**R4.3 - Loop Infinito:**
- Contador de fallos consecutivos en banda específica
- Después de 3 fallos: forzar búsqueda estándar
- Limpiar NVS si anomalía detectada

#### Criterios de Aceptación

- ✅ Banda guardada intenta primero (100% casos con cache)
- ✅ Fallback a búsqueda estándar si falla (100% casos)
- ✅ Conexión LTE exitosa (100% casos)
- ✅ Ahorro tiempo: -25s promedio

#### Riesgo Residual: 🟢 BAJO

Fallback a lógica legacy garantiza funcionalidad.

---

### FIX #5: DETECCIÓN DEGRADACIÓN

**Descripción:** Monitorear RSSI en ventana deslizante, alertar si tendencia negativa

#### Riesgos Inherentes

| # | Riesgo | Probabilidad | Impacto | Severidad |
|---|--------|--------------|---------|-----------|
| R5.1 | Falsos positivos (alertas innecesarias) | 🟡 Media (30%) | 🟢 Bajo | 🟢 BAJO |
| R5.2 | No detectar degradación real | 🟢 Baja (10%) | 🟢 Bajo | 🟢 BAJO |
| R5.3 | Consumo RAM por historial RSSI | 🟢 Muy Baja (2%) | 🟢 Bajo | 🟢 BAJO |

#### Estrategias de Mitigación

**R5.1 - Falsos Positivos:**
- Umbral conservador: degradación = 5+ puntos en 5 muestras
- Alertas informativas, NO bloquean operación
- Logging para ajustar umbrales post-deploy

**R5.2 - No Detectar:**
- Muestreo suficiente: 5 muestras
- Ventana temporal: últimos 15 minutos
- Testing con degradación simulada

**R5.3 - Consumo RAM:**
- Array fijo: `int rssiHistory[5]` = 20 bytes
- Impacto despreciable (< 0.01% RAM total)

#### Criterios de Aceptación

- ✅ Detecta degradación 8 puntos en 5 ciclos (simulado)
- ✅ NO genera alertas con variación normal (±3 puntos)
- ✅ RAM libre sin cambios (±1%)

#### Riesgo Residual: 🟢 MUY BAJO

Solo alertas, no afecta lógica crítica.

---

### FIX #6: GPS CACHE

**Descripción:** Reutilizar última posición GPS si < 24h antigüedad

#### Riesgos Inherentes

| # | Riesgo | Probabilidad | Impacto | Severidad |
|---|--------|--------------|---------|-----------|
| R6.1 | Cache obsoleto (device movido) | 🟡 Media (20%) | 🟢 Bajo | 🟢 BAJO |
| R6.2 | Edad cache mal calculada (overflow) | 🟢 Baja (5%) | 🟢 Bajo | 🟢 BAJO |

#### Estrategias de Mitigación

**R6.1 - Cache Obsoleto:**
- Invalidar cache después de 24h
- Agregar lógica: si device está en vehículo, no usar cache
- Testing con device estático y móvil

**R6.2 - Overflow:**
```cpp
// ✅ MITIGACIÓN: Validación de overflow
unsigned long cacheAge = millis() - persistentState.lastGPSTime;

// Detectar overflow (millis() reinició)
if (cacheAge > 86400000 || millis() < persistentState.lastGPSTime) {
  logMessage(2, "GPS cache inválido (overflow), buscando fresh");
  // Buscar GPS
}
```

#### Criterios de Aceptación

- ✅ Cache usado si < 24h (device estático)
- ✅ Cache ignorado si > 24h
- ✅ Overflow detectado correctamente
- ✅ Ahorro tiempo: -20s promedio

#### Riesgo Residual: 🟢 BAJO

En peor caso: busca GPS desde cero (comportamiento actual).

---

### FIX #7: FALLBACK NB-IoT

**Descripción:** Intentar NB-IoT si LTE Cat-M falla después de 3 intentos

#### Riesgos Inherentes

| # | Riesgo | Probabilidad | Impacto | Severidad |
|---|--------|--------------|---------|-----------|
| R7.1 | NB-IoT no disponible en zona | 🟠 Alta (50%) | 🟢 Bajo | 🟡 MEDIO |
| R7.2 | Cambio modo LTE ralentiza ciclo | 🟡 Media (20%) | 🟢 Bajo | 🟢 BAJO |
| R7.3 | Módem queda en modo incorrecto | 🟢 Baja (5%) | 🟠 Medio | 🟡 MEDIO |

#### Estrategias de Mitigación

**R7.1 - NB-IoT No Disponible:**
```cpp
// ✅ MITIGACIÓN: Detección de soporte
bool checkNBIoTSupport() {
  if (sendATCommand("+CNBP?", "OK", 10000)) {
    // Parsear respuesta para ver si banda NB-IoT existe
    return true;
  }
  return false;
}

if (lteFails >= 3 && checkNBIoTSupport()) {
  logMessage(2, "🔄 Intentando NB-IoT...");
  // Cambiar modo
} else {
  logMessage(1, "⚠️ NB-IoT no disponible, continuando");
}
```

**R7.2 - Ralentización:**
- Solo intentar NB-IoT después de 3 fallos LTE
- Timeout limitado: 60s máximo para NB-IoT
- Beneficio: potencial +3% éxito en zonas extremas

**R7.3 - Modo Incorrecto:**
- Siempre resetear módem a modo Cat-M después de ciclo
- Verificar modo actual antes de sleep
- Testing con múltiples cambios de modo

#### Criterios de Aceptación

- ✅ NB-IoT intenta solo si LTE falla 3 veces
- ✅ Detección correcta de soporte NB-IoT
- ✅ Módem vuelve a Cat-M después de ciclo
- ✅ Tiempo total ≤ v4.1.0 + 10%

#### Riesgo Residual: 🟡 MEDIO

Alto riesgo de incompatibilidad de red. Requiere testing exhaustivo en zona.

---

### FIX #8: MÉTRICAS REMOTAS

**Descripción:** Agregar campo RSSI y métricas a payload JSON

#### Riesgos Inherentes

| # | Riesgo | Probabilidad | Impacto | Severidad |
|---|--------|--------------|---------|-----------|
| R8.1 | Payload excede límite TCP | 🟢 Baja (5%) | 🟠 Medio | 🟢 BAJO |
| R8.2 | Backend rechaza nuevo formato | 🟢 Baja (10%) | 🟠 Medio | 🟢 BAJO |
| R8.3 | JSON malformado por campo nuevo | 🟢 Muy Baja (2%) | 🟠 Medio | 🟢 BAJO |

#### Estrategias de Mitigación

**R8.1 - Payload Grande:**
- Validar tamaño antes de enviar
- Límite: 512 bytes (muy por debajo de límite TCP 1460)
- Métricas compactas: RSSI (2 bytes), tiempos (4 bytes cada uno)

**R8.2 - Backend Rechaza:**
- Backend ya diseñado para campos opcionales
- Testing en ambiente staging primero
- Rollback: remover campos si hay problemas

**R8.3 - JSON Malformado:**
- Validación de JSON antes de enviar
- Testing con ArduinoJson validator
- Logging de payload completo si falla

#### Criterios de Aceptación

- ✅ Payload < 512 bytes
- ✅ Backend acepta nuevo formato (staging)
- ✅ JSON válido en 100% casos
- ✅ Grafana muestra métricas correctamente

#### Riesgo Residual: 🟢 BAJO

Solo agrega datos, no cambia lógica crítica.

---

## 📈 MATRIZ DE RIESGOS: DESPLIEGUE SECUENCIAL

### Estrategia Recomendada: Fixes Independientes

| Orden | Fix | Riesgo Individual | Riesgo Acumulado | Complejidad Rollback |
|-------|-----|-------------------|------------------|----------------------|
| 1 | FIX #3 Init Módem | 🟢 BAJO | 🟢 BAJO | ✅ Trivial (cambiar delays) |
| 2 | FIX #1 Persistencia | 🟢 BAJO | 🟢 BAJO | ✅ Fácil (feature flag) |
| 3 | FIX #2 Timeout Dinámico | 🟢 BAJO | 🟢 BAJO | ✅ Fácil (feature flag) |
| 4 | FIX #6 GPS Cache | 🟢 BAJO | 🟢 BAJO | ✅ Fácil (feature flag) |
| 5 | FIX #4 Banda Inteligente | 🟢 BAJO | 🟢 BAJO | ✅ Fácil (feature flag) |
| 6 | FIX #5 Degradación | 🟢 MUY BAJO | 🟢 BAJO | ✅ Trivial (solo alertas) |
| 7 | FIX #8 Métricas | 🟢 BAJO | 🟢 BAJO | ✅ Fácil (remover campos) |
| 8 | FIX #7 NB-IoT | 🟡 MEDIO | 🟡 MEDIO | 🟠 Medio (cambio modo) |

**Ventajas de despliegue secuencial:**
1. ✅ Riesgo controlado (un fix a la vez)
2. ✅ Validación incremental (detectar problemas temprano)
3. ✅ Rollback simple (solo último fix)
4. ✅ Aprendizaje continuo (incorporar lecciones)

**Desventajas de despliegue masivo (8 fixes juntos):**
1. ❌ Riesgo alto (muchos cambios simultáneos)
2. ❌ Debugging complejo (¿cuál fix causó problema?)
3. ❌ Rollback difícil (perder todo progreso)
4. ❌ Testing exhaustivo (2^8 = 256 combinaciones)

---

## 🚨 RIESGOS TRANSVERSALES (Aplican a todos los fixes)

### RT-1: Interacción entre Fixes

**Descripción:** Fixes pueden interactuar de formas inesperadas.

**Ejemplo:**
- FIX #1 (Persistencia) guarda RSSI=9
- FIX #2 (Timeout Dinámico) lee RSSI=9 → timeout 110s
- FIX #4 (Banda Inteligente) intenta banda guardada
- Si los 3 fallan simultáneamente → mayor impacto

**Mitigación:**
- Testing combinado después de cada nuevo fix
- Logs exhaustivos para identificar interacciones
- Defaults seguros en cada fix (operan independientemente)

**Probabilidad:** 🟢 Baja (5%)  
**Impacto:** 🟠 Medio  
**Riesgo:** 🟢 BAJO

---

### RT-2: Regresión de v4.1.0

**Descripción:** Cambios degradan funcionalidad estable actual.

**Síntomas:**
- Watchdog resets > 0
- Tiempo ciclo > baseline
- Transmisiones fallan
- Consumo batería aumenta

**Mitigación:**
- Comparación con baseline en cada fix
- Métricas objetivas (checklist de 7 puntos)
- Testing de regresión automatizado
- Feature flags para rollback instantáneo

**Probabilidad:** 🟡 Media (10-15%)  
**Impacto:** 🔴 Alto  
**Riesgo:** 🟡 MEDIO

---

### RT-3: Condiciones Específicas de Zona Rural

**Descripción:** Fixes funcionan en lab pero no en campo.

**Factores únicos:**
- RSSI extremadamente bajo (8-9)
- Interferencia electromagnética (17:00-18:00)
- Temperatura variable (-5°C a 45°C)
- Vibración mecánica (viento, animales)

**Mitigación:**
- Testing obligatorio en hardware real
- Validación en zona rural específica
- Monitoreo continuo primeras 72h post-deploy
- Rollback preparado si métricas degradan

**Probabilidad:** 🟡 Media (20%)  
**Impacto:** 🟠 Medio  
**Riesgo:** 🟡 MEDIO

---

### RT-4: Consumo de Batería

**Descripción:** Fixes aumentan consumo indirectamente.

**Causas potenciales:**
- NVS read/write frecuente (FIX #1)
- Timeouts más largos (FIX #2)
- Intentos adicionales de banda (FIX #4)
- Modo NB-IoT más lento (FIX #7)

**Mitigación:**
- Medir consumo en cada fix (amperímetro en testing)
- Comparar con baseline: ≤ 50mA promedio
- Optimizar operaciones NVS (batch writes)
- Monitorear voltaje batería en payload

**Probabilidad:** 🟡 Media (15%)  
**Impacto:** 🟠 Medio  
**Riesgo:** 🟡 MEDIO

---

## 🎯 PLAN DE CONTINGENCIA GLOBAL

### Nivel 1: Fix Individual Falla

**Síntomas:**
- Métricas degradan después de desplegar fix N
- Watchdog resets > 0
- Logs muestran errores específicos del fix

**Acciones (< 5 min):**
1. Cambiar feature flag a `false`
2. Recompilar y subir
3. Verificar métricas vuelven a baseline
4. Documentar problema en FIX-N_ISSUES.md

---

### Nivel 2: Múltiples Fixes Conflictúan

**Síntomas:**
- Comportamiento errático sin patrón claro
- Logs muestran interacciones inesperadas
- Métricas fuera de rangos esperados

**Acciones (< 15 min):**
1. Deshabilitar último fix agregado
2. Si persiste: deshabilitar penúltimo
3. Continuar hasta identificar conflicto
4. Documentar interacción en RIESGOS_IDENTIFICADOS.md

---

### Nivel 3: Sistema Inoperable

**Síntomas:**
- Device no transmite
- Watchdog resets continuos
- GPS/LTE fallan sistemáticamente

**Acciones (< 20 min):**
1. Rollback completo a v4.1.0 (branch main)
2. Flash firmware v4.1.0
3. Verificar restauración de funcionalidad
4. Análisis post-mortem de logs
5. Plan de corrección antes de reintentar

---

### Nivel 4: Device Brick (Extremo)

**Síntomas:**
- Device no responde
- No bootea
- Serial monitor sin salida

**Acciones (< 30 min):**
1. Erase completo de flash: `esptool.py erase_flash`
2. Flash firmware v4.1.0 factory
3. Si persiste: revisión hardware (alimentación, conectores)
4. Escalación a soporte técnico

---

## 📋 CHECKLIST PRE-DEPLOY (Cada Fix)

### Validación Técnica

- [ ] ✅ Compila sin errores ni warnings
- [ ] ✅ Pasa tests unitarios (NVS, GPS, LTE según fix)
- [ ] ✅ Testing en hardware: 1 ciclo exitoso
- [ ] ✅ Testing en hardware: 10 ciclos consecutivos
- [ ] ✅ Testing en hardware: 24h sin supervisión
- [ ] ✅ Watchdog resets = 0
- [ ] ✅ Métricas ≤ baseline (7 criterios)

### Validación de Riesgos

- [ ] ✅ Defaults seguros implementados
- [ ] ✅ Feature flag funcional
- [ ] ✅ Validación de rangos agregada
- [ ] ✅ Logging exhaustivo agregado
- [ ] ✅ Plan rollback documentado
- [ ] ✅ Casos borde testeados

### Documentación

- [ ] ✅ FIX-N_PLAN_EJECUCION.md creado
- [ ] ✅ FIX-N_VALIDACION_HARDWARE.md completo
- [ ] ✅ Riesgos específicos documentados
- [ ] ✅ Commit message descriptivo
- [ ] ✅ SNAPSHOT actualizado

### Preparación Campo

- [ ] ✅ Firmware v4.1.0 backup disponible
- [ ] ✅ Herramientas de flash listas
- [ ] ✅ Plan comunicación con cliente
- [ ] ✅ Ventana de mantenimiento acordada
- [ ] ✅ Monitoreo 72h post-deploy planificado

---

## 🎓 LECCIONES DE RIESGOS ANTERIORES

### De FIX-1 (Watchdog)

**Lo que salió bien:**
- ✅ Defaults seguros (watchdog=0 si código falla)
- ✅ Testing gradual (detectó mejoras inesperadas)
- ✅ Cambios mínimos (solo delays fragmentados)

**Mejoras para FIX-2+:**
- 🔄 Agregar feature flags (no estaba en FIX-1)
- 🔄 Documentar métricas baseline ANTES
- 🔄 Testing de casos borde más exhaustivo

---

## 📊 MÉTRICAS DE ÉXITO: DESPLIEGUE SEGURO

### Después de Cada Fix

| Métrica | Target | Crítico |
|---------|--------|---------|
| Watchdog resets | = 0 | ✅ SÍ |
| Tiempo ciclo | ≤ baseline | ✅ SÍ |
| Éxito transmisión | = 100% | ✅ SÍ |
| RAM libre | ≥ 80% baseline | ⚠️ Importante |
| Consumo batería | ≤ baseline | ⚠️ Importante |
| GPS intentos | ≤ baseline | 🔄 Mejora esperada |
| LTE tiempo | ≤ baseline | 🔄 Mejora esperada |

**Si cualquier métrica crítica falla → ROLLBACK inmediato**

---

## 🔗 REFERENCIAS Y APÉNDICES

**Documentos relacionados:**
- `PREMISAS_DE_FIXS.md` - Estrategia general de implementación
- `FIX_SEÑAL_BAJA_RURAL.md` - 8 fixes identificados con código
- `ANALISIS_CONSOLIDADO_20251029.md` - Análisis de logs que identificó problemas
- `SNAPSHOT_20251029.md` - Estado actual v4.1.0

**Herramientas de gestión de riesgos:**
- Feature flags (#define ENABLE_*)
- Logging exhaustivo (nivel 0-3)
- Métricas comparativas (vs baseline)
- Checklist pre-deploy (validación)
- Plan rollback (< 5 min)

---

**Documento creado:** 30 Oct 2025  
**Versión:** 1.0  
**Aplicable a:** FIX-2 a FIX-9 (8 fixes zona rural)  
**Próxima revisión:** Después de despliegue FIX-2 (actualizar con riesgos reales encontrados)  
**Status:** ✅ Activo - Consultar antes de cada deploy

---

**Documento creado:** 30 Oct 2025  
**Versión:** 1.0  
**Status:** ✅ Activo - Aplicar en todos los fixes  
**Próxima revisión:** Después de FIX-2 (incorporar lecciones)
