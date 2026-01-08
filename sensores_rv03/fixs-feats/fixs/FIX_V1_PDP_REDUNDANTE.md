# FIX-V1: Reducir Eventos PDP Redundantes

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V1 |
| **Tipo** | Fix (Corrección de Bug) |
| **Sistema** | LTE/Modem |
| **Archivo Principal** | `src/data_lte/LTEModule.cpp` |
| **Estado** | ✅ Completado |
| **Fecha Identificación** | 2026-01-07 |
| **Fecha Implementación** | 2026-01-07 |
| **Versión** | v2.0.2 |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Prioridad** | Media-Alta |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
El firmware genera múltiples eventos PDP (Create/Delete) en cada ciclo de transmisión, incluso cuando ya tiene una operadora guardada en NVS. Se observan 3+ Create/Delete cuando debería ser solo 1+1.

### Evidencia
En el dashboard de la operadora se observan múltiples eventos PDP en el mismo minuto:

```
19:38 - Create PDP Context - AT&T
19:38 - Delete PDP Context - AT&T
19:38 - Create PDP Context - AT&T
19:38 - Delete PDP Context - AT&T
19:38 - Create PDP Context - AT&T
19:38 - Delete PDP Context - AT&T
```

### Ubicación del Bug
**Archivo:** `src/data_lte/LTEModule.cpp`  
**Línea:** 326  

```cpp
bool LTEModule::configureOperator(Operadora operadora) {
    resetModem();  // ← PROBLEMA: Siempre ejecuta AT+CFUN=1,1
    ...
}
```

### Causa Raíz
El comando `AT+CFUN=1,1` dentro de `resetModem()`:

1. Reinicia completamente la radio del modem
2. Puede cerrar sesiones PDP existentes (genera Delete)
3. Fuerza re-registro en la red (genera Create al reconectar)

Esto ocurre **siempre**, incluso cuando:
- El modem acaba de encenderse con `powerOn()` y está limpio
- Ya tiene una operadora guardada en NVS (no necesita re-escanear)

---

## 📊 EVALUACIÓN

### Impacto Cuantificado

| Métrica | Actual | Esperado | Diferencia/Mes |
|---------|--------|----------|----------------|
| Eventos PDP/ciclo | 3+ | 1 | -66% |
| Eventos PDP/día (10min) | 432+ | 144 | -288 eventos |
| Eventos PDP/mes | 12,960+ | 4,320 | **-8,640 eventos** |
| Tiempo extra/ciclo | 5-10s | 0s | **-12+ horas/mes** |

### Impacto por Área

| Aspecto | Descripción |
|---------|-------------|
| **Consumo de datos** | Eventos PDP adicionales consumen presupuesto del SIM |
| **Tiempo de ciclo** | +5-10 segundos por reset innecesario |
| **Batería** | Mayor consumo por operaciones de radio redundantes |
| **Estabilidad** | Riesgo de perder conexión durante el reset |
| **Costos** | Algunos planes M2M cobran por eventos de señalización |

### Perspectiva de Negocio
- Con **100 dispositivos**: **864,000 eventos PDP innecesarios al mes**
- Desgaste prematuro de SIM por ciclos attach/detach excesivos
- Cliente percibe dispositivo problemático al ver 3x eventos en dashboard

---

## 🔧 IMPLEMENTACIÓN

### Estrategia
Agregar parámetro `skipReset` a `configureOperator()` para omitir el reset cuando el modem ya está en estado limpio (recién encendido con operadora guardada).

### Cambio 1: LTEModule.h

**Archivo:** `src/data_lte/LTEModule.h`  
**Línea:** 84  

```cpp
// ANTES
bool configureOperator(Operadora operadora);

// DESPUÉS (con flag FEAT-V1)
#if ENABLE_FIX_V1_SKIP_RESET_PDP
/**
 * @brief Configure network for specific operator
 * @param operadora Operator enum
 * @param skipReset If true, skips modem reset (use when modem just powered on)
 * @return true if configuration successful, false otherwise
 */
bool configureOperator(Operadora operadora, bool skipReset = false);  // FIX-V1
#else
bool configureOperator(Operadora operadora);
#endif
```

### Cambio 2: LTEModule.cpp

**Archivo:** `src/data_lte/LTEModule.cpp`  
**Línea:** 326-327  

```cpp
// ANTES
bool LTEModule::configureOperator(Operadora operadora) {
    resetModem();
    if (operadora >= NUM_OPERADORAS) {

// DESPUÉS (con flag FEAT-V1)
#if ENABLE_FIX_V1_SKIP_RESET_PDP
bool LTEModule::configureOperator(Operadora operadora, bool skipReset) {
    // [FIX-V1 START]
    if (!skipReset) {
        resetModem();
    } else {
        debugPrint("Saltando reset del modem (skipReset=true)");
    }
    // [FIX-V1 END]
    if (operadora >= NUM_OPERADORAS) {
#else
bool LTEModule::configureOperator(Operadora operadora) {
    resetModem();
    if (operadora >= NUM_OPERADORAS) {
#endif
```

### Cambio 3: AppController.cpp

**Archivo:** `AppController.cpp`  
**Línea:** 378  

```cpp
// ANTES
if (!lte.configureOperator(operadoraAUsar))   { lte.powerOff(); return false; }

// DESPUÉS (con flag FEAT-V1)
#if ENABLE_FIX_V1_SKIP_RESET_PDP
// [FIX-V1 START] Si tiene operadora guardada, skip reset (modem recién encendido está limpio)
if (!lte.configureOperator(operadoraAUsar, tieneOperadoraGuardada)) { 
    lte.powerOff(); 
    return false; 
}
// [FIX-V1 END]
#else
if (!lte.configureOperator(operadoraAUsar)) { lte.powerOff(); return false; }
#endif
```

---

## 📊 DIAGRAMA DE FLUJO

```
┌─────────────────────────────────────────────────────────────┐
│                    sendBufferOverLTE()                       │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  powerOn() ─────────────────────────────────────────────┐   │
│       │                                                  │   │
│       ▼                                                  │   │
│  ¿Tiene operadora guardada en NVS?                       │   │
│       │                                                  │   │
│   ┌───┴───┐                                              │   │
│   │       │                                              │   │
│  SÍ      NO                                              │   │
│   │       │                                              │   │
│   │       ▼                                              │   │
│   │   Escanear todas las operadoras                      │   │
│   │   (resetModem en cada testOperator)                  │   │
│   │       │                                              │   │
│   │       ▼                                              │   │
│   │   Seleccionar mejor operadora                        │   │
│   │       │                                              │   │
│   └───┬───┘                                              │   │
│       │                                                  │   │
│       ▼                                                  │   │
│  configureOperator(op, tieneOperadoraGuardada)           │   │
│       │                                                  │   │
│   ┌───┴───┐                                              │   │
│   │       │                                              │   │
│  skipReset=true    skipReset=false                       │   │
│   │                │                                     │   │
│   │                ▼                                     │   │
│   │           resetModem()                               │   │
│   │                │                                     │   │
│   └───────┬────────┘                                     │   │
│           │                                              │   │
│           ▼                                              │   │
│   Configurar bandas, COPS, etc.                          │   │
│           │                                              │   │
│           ▼                                              │   │
│   attachNetwork() → activatePDP() → TCP                  │   │
│                                                          │   │
└─────────────────────────────────────────────────────────────┘
```

---

## 🧪 VERIFICACIÓN

### Resultado Esperado - Con operadora guardada (ciclo normal)
```
[INFO][APP] Usando operadora guardada: AT&T MEXICO (334050)
Saltando reset del modem (skipReset=true)
Configurando operadora: AT&T MEXICO (334050)
...
PDP activado                    ← 1 Create PDP
...
PDP desactivado                 ← 1 Delete PDP
```

### Resultado Esperado - Sin operadora guardada (boot frío)
```
[INFO][APP] No hay operadora guardada. Probando todas...
Reiniciando funcionalidad del modem...   ← Reset para TELCEL
...
Reiniciando funcionalidad del modem...   ← Reset para AT&T
...
[INFO][APP] Mejor operadora seleccionada: AT&T MEXICO (334050)
Reiniciando funcionalidad del modem...   ← Reset final (skipReset=false)
```

### Criterios de Aceptación
- [ ] Con operadora guardada: **1 Create + 1 Delete PDP** por ciclo
- [ ] Sin operadora guardada (escaneo): Múltiples eventos (aceptable)
- [ ] Log debe mostrar: `Saltando reset del modem (skipReset=true)`
- [ ] Tiempo de ciclo reducido ~5-10 segundos
- [ ] Sin errores de conexión TCP
- [ ] Compilación sin warnings
- [ ] Con flag en `0`: comportamiento original intacto

---

## ⚖️ EVALUACIÓN CRÍTICA

### ¿Por qué NO es "normal" el comportamiento actual?

| Argumento del desarrollador | Contraargumento |
|-----------|-----------------|
| "El modem necesita reset para garantizar estado limpio" | El modem **acaba de encenderse** con `powerOn()`. Ya está limpio. |
| "Es más seguro hacer reset siempre" | Programación defensiva excesiva. Hay formas de verificar estado sin reset. |
| "Funciona, no hay errores" | Funcionar ≠ Óptimo. El cliente paga por cada evento PDP. |
| "Así lo recomienda el fabricante" | El datasheet de SIM7080G NO recomienda reset antes de cada operación. |

### Evidencia en el propio código

```cpp
// En powerOn() ya verifica que el modem esté listo:
if (isAlive()) {
    debugPrint("Modulo ya esta encendido");
    return true;
}
```

Si `powerOn()` ya garantiza que el modem responde, ¿para qué resetear inmediatamente después?

### Veredicto

| Categoría | Evaluación |
|-----------|------------|
| ¿Funciona? | ✅ Sí |
| ¿Es correcto? | ❌ No |
| ¿Es óptimo? | ❌ No |
| ¿Es profesional? | ⚠️ Cuestionable |
| ¿Es "normal"? | ❌ Es **deuda técnica** justificada como normalidad |

---

## 📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
| 2026-01-07 | Problema identificado y documentado | - |
| 2026-01-07 | Solución diseñada | - |
| - | Pendiente implementación | v2.0.1 |
