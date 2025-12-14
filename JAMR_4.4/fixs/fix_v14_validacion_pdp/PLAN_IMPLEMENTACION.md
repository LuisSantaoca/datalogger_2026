# Plan de Implementación FIX-14: Validación Activa de PDP

## ✅ Checklist de Implementación

### Fase Preparación
- [ ] Revisar código actual de `startLTE_AUTO_LITE()` en gsmlte.cpp
- [ ] Identificar línea exacta donde insertar validación PDP
- [ ] Backup de archivos antes de modificar
- [ ] Confirmar versión base v4.4.13 compilando sin cambios

### Fase Codificación

#### Paso 1: Actualizar Versión (JAMR_4.4.ino)
- [ ] Cambiar `VERSION` a "v4.4.14"
- [ ] Cambiar `VERSION_NAME` a "validacion-pdp"
- [ ] Cambiar `PATCH` a 14
- [ ] Agregar comentario: `// FIX-14: Validación activa de PDP en AUTO_LITE`

**Ubicación:** Líneas 42-47 aproximadamente

#### Paso 2: Declarar Funciones (gsmlte.h)
- [ ] Agregar declaración: `bool checkPDPContextActive(int context_id = 0);`
- [ ] Agregar declaración: `bool waitForPDPActivation(unsigned long timeout_ms);`

**Ubicación:** Sección de declaraciones de funciones LTE

#### Paso 3: Implementar checkPDPContextActive() (gsmlte.cpp)
- [ ] Crear función con parámetro context_id (default 0)
- [ ] Enviar comando `+CNACT?` con timeout 3000ms
- [ ] Parsear respuesta buscando patrón `+CNACT: X,1,`
- [ ] Retornar true si estado=1 (ACTIVE), false si estado=0 o error
- [ ] Agregar logs con tag `[FIX-14]`

**Ubicación:** Después de funciones auxiliares AT, antes de startLTE_AUTO_LITE()

**Código base:**
```cpp
bool checkPDPContextActive(int context_id) {
    String response = sendATCommand("+CNACT?", 3000);
    
    if (response.length() == 0 || response.indexOf("ERROR") >= 0) {
        logDebug("[FIX-14] +CNACT? timeout/error, asumiendo inactivo");
        return false;
    }
    
    String searchPattern = "+CNACT: " + String(context_id) + ",1,";
    
    if (response.indexOf(searchPattern) >= 0) {
        logDebug("[FIX-14] PDP contexto %d ACTIVE", context_id);
        return true;
    }
    
    logDebug("[FIX-14] PDP contexto %d DEACTIVE", context_id);
    return false;
}
```

#### Paso 4: Implementar waitForPDPActivation() (gsmlte.cpp)
- [ ] Crear función con parámetro timeout_ms
- [ ] Loop con condición `millis() - startTime < timeout_ms`
- [ ] Cada 3 iteraciones (cada 3s), llamar checkPDPContextActive(0)
- [ ] Si PDP activo, registrar tiempo y retornar true
- [ ] Monitorear RSSI cada segundo (diagnóstico)
- [ ] Si timeout, registrar warning y retornar false
- [ ] Agregar logs con tag `[FIX-14]`

**Código base:**
```cpp
bool waitForPDPActivation(unsigned long timeout_ms) {
    unsigned long startTime = millis();
    int checkCount = 0;
    bool pdpActive = false;
    
    logInfo("[FIX-14] Esperando activación PDP (timeout=%lums)", timeout_ms);
    
    while (millis() - startTime < timeout_ms) {
        checkCount++;
        
        if (checkCount % 3 == 0) {
            if (checkPDPContextActive(0)) {
                unsigned long elapsed = millis() - startTime;
                logInfo("[FIX-14] ✅ PDP activado en %lums (check #%d)", 
                        elapsed, checkCount);
                pdpActive = true;
                break;
            }
        }
        
        int rssi = getRSSI();
        logDebug("[FIX-9] AUTO_LITE RSSI: %d (espera PDP, check %d)", 
                 rssi, checkCount);
        
        delay(1000);
    }
    
    if (!pdpActive) {
        unsigned long elapsed = millis() - startTime;
        logWarn("[FIX-14] ⚠️ PDP no se activó después de %lums (%d checks)", 
                elapsed, checkCount);
    }
    
    return pdpActive;
}
```

#### Paso 5: Modificar startLTE_AUTO_LITE() (gsmlte.cpp)
- [ ] Localizar sección después de `+CNACT=0,1`
- [ ] Reemplazar loop RSSI actual con llamada a `waitForPDPActivation(20000)`
- [ ] Manejar retorno false como trigger de fallback
- [ ] Actualizar constante timeout de 45000 a 20000
- [ ] Mantener lógica de fallback a DEFAULT_CATM existente

**Búsqueda en código:**
```cpp
// Buscar patrón similar a:
sendATCommand("+CNACT=0,1", 8000);

// Loop actual con RSSI que debe reemplazarse
while (...) {
    int rssi = getRSSI();
    logDebug("[FIX-9] AUTO_LITE RSSI: %d", rssi);
    ...
}
```

**Reemplazo:**
```cpp
sendATCommand("+CNACT=0,1", 8000);

// ✅ FIX-14: Validación activa de PDP en lugar de loop RSSI
const unsigned long PDP_ACTIVATION_TIMEOUT = 20000;  // 20s (vs 45s anterior)

if (!waitForPDPActivation(PDP_ACTIVATION_TIMEOUT)) {
    logWarn("[FIX-14] Timeout esperando PDP, fallback a DEFAULT_CATM");
    return false;
}

logInfo("[FIX-9] ✅ AUTO_LITE conectado a LTE con PDP activo");
// Continuar con validación FIX-5 y transmisión...
```

### Fase Compilación
- [ ] Compilar proyecto completo
- [ ] Verificar 0 errores
- [ ] Verificar 0 warnings críticos
- [ ] Revisar tamaño de firmware (no debe exceder límites)

**Comando:**
```bash
# Si usas PlatformIO
pio run

# Si usas Arduino IDE
# Verificar desde IDE: Sketch → Verify/Compile
```

### Fase Validación Sintaxis
- [ ] Buscar referencias a funciones nuevas en logs de compilación
- [ ] Confirmar que FIX-14 tags aparecen en código
- [ ] Verificar indentación y formato consistente
- [ ] Revisar que no hay comentarios `// TODO FIX-14` sin resolver

---

## 📍 Ubicaciones Exactas de Modificación

### Archivo: JAMR_4.4.ino
**Líneas a modificar:** ~42-47

**Antes:**
```cpp
#define VERSION "v4.4.13"
#define VERSION_NAME "prevencion-critica"
#define MAJOR 4
#define MINOR 4
#define PATCH 13
```

**Después:**
```cpp
#define VERSION "v4.4.14"
#define VERSION_NAME "validacion-pdp"  // FIX-14: Validación activa de PDP
#define MAJOR 4
#define MINOR 4
#define PATCH 14  // FIX-14: Loop PDP validation en AUTO_LITE
```

---

### Archivo: gsmlte.h
**Buscar sección:** Declaraciones de funciones (después de includes)

**Agregar:**
```cpp
// FIX-14: Validación activa de PDP en AUTO_LITE
bool checkPDPContextActive(int context_id = 0);
bool waitForPDPActivation(unsigned long timeout_ms);
```

---

### Archivo: gsmlte.cpp
**Sección 1 - Nuevas funciones (antes de startLTE_AUTO_LITE):**

```cpp
// ============================================
// FIX-14: Validación Activa de PDP
// ============================================

/**
 * @brief Verifica si un contexto PDP está activo
 * @param context_id ID del contexto PDP (0-3), default 0
 * @return true si PDP en estado ACTIVE, false si DEACTIVE o error
 * 
 * Comando: +CNACT?
 * Respuesta esperada: +CNACT: 0,1,"100.116.56.23"
 *                              ^ ^
 *                              | estado: 1=ACTIVE, 0=DEACTIVE
 *                              context_id
 */
bool checkPDPContextActive(int context_id) {
    // ... código del paso 3
}

/**
 * @brief Espera activamente que el contexto PDP se active
 * @param timeout_ms Timeout máximo en milisegundos
 * @return true si PDP se activó antes del timeout, false si timeout
 * 
 * Realiza checks cada 3 segundos para detectar activación PDP.
 * Monitorea RSSI cada segundo como diagnóstico secundario.
 */
bool waitForPDPActivation(unsigned long timeout_ms) {
    // ... código del paso 4
}
```

**Sección 2 - Modificar startLTE_AUTO_LITE():**

**Buscar patrón:**
```cpp
bool startLTE_AUTO_LITE() {
    // ... código inicial
    
    String response = sendATCommand("+CNACT=0,1", 8000);
    
    // ← AQUÍ: Reemplazar loop RSSI por waitForPDPActivation
    
    // ... resto de función
}
```

---

## 🧪 Validación Post-Compilación

### Test 1: Syntax Check
```bash
# Buscar funciones nuevas en binario
grep -r "checkPDPContextActive" build/
grep -r "waitForPDPActivation" build/
grep -r "FIX-14" build/

# Confirmar versión en logs
grep "v4.4.14" build/*.elf
```

### Test 2: Memory Check
```bash
# Verificar tamaño de firmware
ls -lh build/*.bin

# Comparar con v4.4.13
# Esperado: +2-3KB por nuevas funciones (aceptable si <10KB)
```

### Test 3: Dependency Check
- [ ] Verificar que sendATCommand() existe y es accesible
- [ ] Verificar que getRSSI() existe y es accesible
- [ ] Verificar que logInfo/logDebug/logWarn existen
- [ ] Confirmar que constantes AUTO_LITE están definidas

---

## 📦 Archivos a Commit

```
JAMR_4.4/
├── JAMR_4.4.ino          # Versión actualizada
├── gsmlte.h              # Declaraciones nuevas
└── gsmlte.cpp            # Implementación FIX-14

fixs/
└── fix_v14_validacion_pdp/
    ├── README.md         # Documentación completa
    └── PLAN_IMPLEMENTACION.md  # Este archivo
```

---

## ⏱️ Estimación de Tiempo

| Fase | Duración | Acumulado |
|------|----------|-----------|
| Preparación y backup | 5 min | 5 min |
| Paso 1: Versión | 2 min | 7 min |
| Paso 2: Headers | 3 min | 10 min |
| Paso 3: checkPDPContextActive | 10 min | 20 min |
| Paso 4: waitForPDPActivation | 15 min | 35 min |
| Paso 5: Modificar AUTO_LITE | 10 min | 45 min |
| Compilación y fix de errores | 10 min | 55 min |
| Validación sintaxis | 5 min | 60 min |

**Total estimado:** ~60 minutos de desarrollo

---

## 🚨 Puntos Críticos de Atención

### ⚠️ CRÍTICO 1: Identificar Loop RSSI Exacto
El código actual puede tener variantes del loop RSSI. Buscar patrones:
- `while` con `millis()` check
- Llamadas a `getRSSI()` dentro del loop
- `delay(1873)` o similar
- Tags `[FIX-9]` relacionados con AUTO_LITE

**Acción:** Leer contexto de 50 líneas antes/después del `+CNACT=0,1`

### ⚠️ CRÍTICO 2: Mantener Fallback a DEFAULT_CATM
No eliminar lógica de fallback existente. Solo reemplazar el método de espera.

**Verificar que se mantiene:**
```cpp
if (!waitForPDPActivation(...)) {
    // ✅ Esto debe disparar fallback existente
    return false;  // o código similar que active DEFAULT_CATM
}
```

### ⚠️ CRÍTICO 3: Validación de Presupuesto Global
Confirmar que timeout 20s no rompe lógica de FIX-6 (presupuesto global 150s).

**Validar:**
- 20s PDP wait + 8s comandos + 52s DEFAULT_CATM fallback = 80s < 150s ✅
- Margen suficiente: 150s - 80s = 70s para GPS/sensores ✅

---

## 🔄 Plan de Rollback

Si compilación falla o comportamiento anómalo:

### Rollback Rápido (5 minutos)
```bash
# Restaurar desde git (si committeado)
git checkout HEAD~1 JAMR_4.4/JAMR_4.4.ino
git checkout HEAD~1 JAMR_4.4/gsmlte.h
git checkout HEAD~1 JAMR_4.4/gsmlte.cpp

# O desde backup manual
cp backup/JAMR_4.4.ino.bak JAMR_4.4/JAMR_4.4.ino
cp backup/gsmlte.h.bak JAMR_4.4/gsmlte.h
cp backup/gsmlte.cpp.bak JAMR_4.4/gsmlte.cpp

# Recompilar
pio run
```

### Rollback Parcial (mantener FIX-14.1 solo)
Si `waitForPDPActivation` causa problemas pero `checkPDPContextActive` funciona:
- Mantener función `checkPDPContextActive()`
- Revertir modificación de loop en `startLTE_AUTO_LITE()`
- Usar validación manual después del loop existente

---

## ✅ Definición de "Done"

Implementación considerada completa cuando:
- [x] Código compila sin errores ni warnings
- [x] Versión actualizada a v4.4.14 en logs de boot
- [x] Tags `[FIX-14]` aparecen en logs al ejecutar
- [x] Timeout 20s funciona (PDP activa o fallback en <25s)
- [ ] 5 ciclos consecutivos sin crashes ni watchdog resets
- [ ] Buffer no acumula >2 datos
- [ ] Tiempo medio de ciclo <150s

**Next step después de "Done":** Validación Fase 2 (10 ciclos) y Fase 3 (24h)

---

**Creado:** 14 Dic 2025  
**Para versión:** v4.4.13 → v4.4.14  
**Autor:** Plan de implementación FIX-14
