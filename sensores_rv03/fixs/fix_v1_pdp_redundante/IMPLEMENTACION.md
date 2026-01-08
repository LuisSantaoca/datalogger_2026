# Implementación - FIX-V1: Reducir Eventos PDP Redundantes

## Cambio 1: LTEModule.h

**Archivo:** `src/data_lte/LTEModule.h`  
**Línea aproximada:** 82  

### Antes
```cpp
bool configureOperator(Operadora operadora);
```

### Después
```cpp
/**
 * @brief Configure network for specific operator
 * @param operadora Operator enum
 * @param skipReset If true, skips modem reset (use when modem just powered on)
 * @return true if configuration successful, false otherwise
 */
bool configureOperator(Operadora operadora, bool skipReset = false);
```

---

## Cambio 2: LTEModule.cpp

**Archivo:** `src/data_lte/LTEModule.cpp`  
**Línea:** 326-328  

### Antes
```cpp
bool LTEModule::configureOperator(Operadora operadora) {
    resetModem();
    if (operadora >= NUM_OPERADORAS) {
```

### Después
```cpp
bool LTEModule::configureOperator(Operadora operadora, bool skipReset) {
    if (!skipReset) {
        resetModem();
    } else {
        debugPrint("Saltando reset del modem (skipReset=true)");
    }
    if (operadora >= NUM_OPERADORAS) {
```

---

## Cambio 3: AppController.cpp

**Archivo:** `AppController.cpp`  
**Línea:** 376  

### Antes
```cpp
if (!lte.configureOperator(operadoraAUsar))   { lte.powerOff(); return false; }
```

### Después
```cpp
// Si tiene operadora guardada, skip reset (modem recién encendido está limpio)
if (!lte.configureOperator(operadoraAUsar, tieneOperadoraGuardada)) { 
    lte.powerOff(); 
    return false; 
}
```

---

## Lógica del Fix

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

## Resultado Esperado

### Con operadora guardada (ciclo normal desde sleep)
```
[INFO][APP] Usando operadora guardada: AT&T MEXICO (334050)
Saltando reset del modem (skipReset=true)
Configurando operadora: AT&T MEXICO (334050)
...
PDP activado                    ← 1 Create PDP
...
PDP desactivado                 ← 1 Delete PDP
```

### Sin operadora guardada (boot frío)
```
[INFO][APP] No hay operadora guardada. Probando todas...
Reiniciando funcionalidad del modem...   ← Reset para TELCEL
...
Reiniciando funcionalidad del modem...   ← Reset para AT&T
...
[INFO][APP] Mejor operadora seleccionada: AT&T MEXICO (334050)
Saltando reset del modem (skipReset=false)  ← Reset final (NO skip porque viene de escaneo)
```

---

## Notas de Implementación

1. El parámetro `skipReset` tiene valor por defecto `false` para mantener compatibilidad con código existente.

2. Solo se pasa `true` cuando `tieneOperadoraGuardada` es verdadero, lo que indica que el modem recién se encendió y no necesita reset.

3. Si el escaneo de operadoras se ejecutó, `tieneOperadoraGuardada` es `false`, por lo que el reset final SÍ se ejecutará (comportamiento actual preservado).
