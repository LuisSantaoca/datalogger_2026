# FIX-V1: Estandarización de Logging en LTEModule

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V1 |
| **Tipo** | Fix (Corrección de Observabilidad) |
| **Sistema** | LTE / Debug |
| **Archivos Principales** | `src/data_lte/LTEModule.cpp`, `src/data_lte/LTEModule.h` |
| **Estado** | 📋 Propuesto |
| **Fecha Identificación** | 2026-02-10 |
| **Versión Target** | v1.1.0 |
| **Branch** | `fix-v1/lte-debug-logging` |
| **Depende de** | Ninguno |
| **Prioridad** | **Alta** |

---

## ✅ CUMPLIMIENTO DE PREMISAS

| Premisa | Descripción | Cumplimiento | Evidencia |
|---------|-------------|--------------|-----------|
| **P1** | Aislamiento total | ✅ | Branch: `fix-v1/lte-debug-logging` |
| **P2** | Cambios mínimos | ✅ | Solo 2 archivos: LTEModule.cpp, LTEModule.h |
| **P3** | Defaults seguros | ✅ | Si flag=0, se preserva `debugPrint()` original |
| **P4** | Feature flags | ✅ | `ENABLE_FIX_V1_LTE_STANDARD_LOGGING` |
| **P5** | Logging exhaustivo | ✅ | Es el propósito mismo del fix |
| **P6** | No cambiar lógica existente | ✅ | Solo cambia formato de mensajes, no lógica |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado

El módulo `LTEModule` utiliza un sistema de debug propietario (`debugPrint()`) que no cumple con el estándar `[NIVEL][MODULO] Mensaje` definido en `DebugConfig.h` y usado por todos los demás módulos del sistema.

### Evidencia (Log del 2026-02-05)

```
[INFO][SLEEP] SleepModule initializing...          ← ✅ Formato estándar
[INFO][APP] Ciclo subsecuente: recuperando...       ← ✅ Formato estándar
Encendiendo SIM7080G...                             ← ❌ Sin prefijo
Intento de encendido 1 de 3                         ← ❌ Sin prefijo
PWRKEY toggled                                      ← ❌ Sin prefijo
Error: No se pudo encender el modulo                ← ❌ Sin prefijo
[DEBUG][APP] Pasando a Cycle_SendLTE                ← ✅ Formato estándar
[DEBUG][APP] Iniciando envio por LTE...             ← ✅ Formato estándar
Encendiendo SIM7080G...                             ← ❌ Sin prefijo (segunda vez)
[DEBUG][APP] Envio completado, pasando a...         ← ❌ Semántica engañosa
```

### Requisitos Violados (según REQUIREMENTS.md)

| Requisito | Descripción | Estado |
|-----------|-------------|--------|
| **DBG-02** | Mensajes de debug deben usar formato `[NIVEL][MODULO] Mensaje` | ❌ Violado |
| **DBG-01** | Sistema de debug configurable por módulo y nivel | ❌ Parcial (LTE no usa macros) |

### Causa Raíz

`LTEModule.cpp` fue desarrollado con **dos mecanismos** de debug propietario que no usan las macros estándar de `DebugConfig.h`:

1. **`debugPrint()` — 66 llamadas directas** (mensajes simples de una línea)
2. **Bloques inline `if (_debugEnabled && _debugSerial)` — 44 bloques adicionales** (mensajes multi-parte con `_debugSerial->print()`)

**Total: 110 puntos de debug** que producen mensajes sin formato estandarizado.

### Ubicación del Bug

**Archivo:** `src/data_lte/LTEModule.cpp`

**Mecanismo 1 — `debugPrint()` (líneas 17-21):**
```cpp
void LTEModule::debugPrint(const char* msg) {
    if (_debugEnabled && _debugSerial) {
        _debugSerial->println(msg);  // ← Sin formato [NIVEL][LTE]
    }
}
```

**Mecanismo 2 — Bloques inline (ejemplo línea 49-54 en `powerOn()`):**
```cpp
if (_debugEnabled && _debugSerial) {
    _debugSerial->print("Intento de encendido ");
    _debugSerial->print(attempt + 1);
    _debugSerial->print(" de ");
    _debugSerial->println(LTE_POWER_ON_ATTEMPTS);
}
```

Los 44 bloques inline se usan para mensajes que concatenan valores dinámicos (índices de operadora, strings ICCID, respuestas AT, etc.) donde `debugPrint()` no era suficiente.

---

## 📊 EVALUACIÓN

### Impacto Cuantificado

| Métrica | Actual | Esperado |
|---------|--------|----------|
| Mensajes LTE con formato estándar | 0/110 (0%) | 110/110 (100%) |
| Llamadas `debugPrint()` a migrar | 66 | 0 |
| Bloques inline `_debugEnabled` a migrar | 44 | 0 |
| Capacidad de filtrar errores LTE por nivel | Ninguna | Completa |
| Mensajes que se pueden silenciar por config | 0 | Todos |

### Impacto por Área

| Aspecto | Evaluación |
|---------|------------|
| Criticidad | Alta — dificulta diagnóstico de fallos de conectividad |
| Riesgo de no implementar | Alto — sin visibilidad de errores vs info vs verbose |
| Esfuerzo | **Alto** — 66 `debugPrint()` + 44 bloques inline = 110 puntos a refactorizar |
| Beneficio | Alto — logging unificado en todo el sistema |

### Problema Colateral: Log Semántico Engañoso

El `AppController.cpp` imprime `"Envio completado, pasando a CompactBuffer"` incluso cuando `sendBufferOverLTE_AndMarkProcessed()` retorna `false`. Este mensaje a nivel `[DEBUG]` no es incorrecto sintácticamente (el envío "completó" su ejecución) pero es **semánticamente confuso** para el operador.

---

## 🔧 IMPLEMENTACIÓN

### Estrategia

Reemplazar las **66 llamadas** a `debugPrint()` y los **44 bloques inline** `if (_debugEnabled && _debugSerial)` con las macros `DEBUG_*` de `DebugConfig.h`, manteniendo `debugPrint()` disponible para rollback.

Para los bloques inline que concatenan valores dinámicos, usar `String()` del Arduino framework:
```cpp
// Bloque inline actual (ejemplo línea 49):
if (_debugEnabled && _debugSerial) {
    _debugSerial->print("Intento de encendido ");
    _debugSerial->print(attempt + 1);
    _debugSerial->print(" de ");
    _debugSerial->println(LTE_POWER_ON_ATTEMPTS);
}

// Reemplazo con macro:
DEBUG_VERBOSE(LTE, String("Intento de encendido ") + (attempt+1) + "/" + LTE_POWER_ON_ATTEMPTS);
```

### Archivos a Modificar

| Archivo | Cambio | Puntos afectados |
|---------|--------|------------------|
| `src/data_lte/LTEModule.cpp` | Reemplazar `debugPrint()` por macros `DEBUG_*` | 66 llamadas |
| `src/data_lte/LTEModule.cpp` | Reemplazar bloques inline por macros `DEBUG_*` | 44 bloques |
| `src/data_lte/LTEModule.cpp` | Agregar `#include "../DebugConfig.h"` | 1 línea |
| `AppController.cpp` | Mejorar log semántico en `Cycle_SendLTE` | 3 líneas |

### Cambio 1: Include de DebugConfig.h (LTEModule.cpp, línea 1)

```cpp
// ANTES (líneas 1-3)
#include "LTEModule.h"
#include <string.h>
#include <stdlib.h>

// DESPUÉS
#include "LTEModule.h"
#include "../DebugConfig.h"  // FIX-V1
#include <string.h>
#include <stdlib.h>
```

### Cambio 2: Ejemplo — powerOn() (LTEModule.cpp, líneas 38-73)

Código actual exacto:
```cpp
bool LTEModule::powerOn() {
    debugPrint("Encendiendo SIM7080G...");
    
    if (isAlive()) {
        debugPrint("Modulo ya esta encendido");
     
        delay(2000);
        return true;
    }
    
    for (uint16_t attempt = 0; attempt < LTE_POWER_ON_ATTEMPTS; attempt++) {
        if (_debugEnabled && _debugSerial) {           // ← Bloque inline
            _debugSerial->print("Intento de encendido ");
            _debugSerial->print(attempt + 1);
            _debugSerial->print(" de ");
            _debugSerial->println(LTE_POWER_ON_ATTEMPTS);
        }
        
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                debugPrint("SIM7080G encendido correctamente!");
                delay(1000);

                delay(2000);
                return true;
            }
            delay(500);
        }
    }
    
    debugPrint("Error: No se pudo encender el modulo");
    return false;
}
```

Reemplazo con feature flag:
```cpp
#if ENABLE_FIX_V1_LTE_STANDARD_LOGGING
// ============ [FIX-V1 START] Logging estandarizado en powerOn ============
bool LTEModule::powerOn() {
    DEBUG_INFO(LTE, "Encendiendo SIM7080G...");
    
    if (isAlive()) {
        DEBUG_INFO(LTE, "Modulo ya esta encendido");
        delay(2000);
        return true;
    }
    
    for (uint16_t attempt = 0; attempt < LTE_POWER_ON_ATTEMPTS; attempt++) {
        DEBUG_VERBOSE(LTE, String("Intento de encendido ") + (attempt+1) + "/" + LTE_POWER_ON_ATTEMPTS);
        
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                DEBUG_INFO(LTE, "SIM7080G encendido correctamente");
                delay(1000);
                delay(2000);
                return true;
            }
            delay(500);
        }
    }
    
    DEBUG_ERROR(LTE, "No se pudo encender el modulo tras todos los intentos");
    return false;
}
// ============ [FIX-V1 END] ============
#else
// Código original preservado (líneas 38-73)
bool LTEModule::powerOn() {
    debugPrint("Encendiendo SIM7080G...");
    
    if (isAlive()) {
        debugPrint("Modulo ya esta encendido");
     
        delay(2000);
        return true;
    }
    
    for (uint16_t attempt = 0; attempt < LTE_POWER_ON_ATTEMPTS; attempt++) {
        if (_debugEnabled && _debugSerial) {
            _debugSerial->print("Intento de encendido ");
            _debugSerial->print(attempt + 1);
            _debugSerial->print(" de ");
            _debugSerial->println(LTE_POWER_ON_ATTEMPTS);
        }
        
        togglePWRKEY();
        delay(LTE_PWRKEY_POST_DELAY_MS);
        
        uint32_t startTime = millis();
        while (millis() - startTime < LTE_AT_READY_TIMEOUT_MS) {
            if (isAlive()) {
                debugPrint("SIM7080G encendido correctamente!");
                delay(1000);
                delay(2000);
                return true;
            }
            delay(500);
        }
    }
    
    debugPrint("Error: No se pudo encender el modulo");
    return false;
}
#endif
```

### Cambio 3: Inventario completo de mensajes `debugPrint()` (66 llamadas)

Cada `debugPrint()` se reclasifica al nivel correcto:

| Línea | Función | Mensaje actual (`debugPrint`) | Nivel |
|-------|---------|-------------------------------|-------|
| 39 | `powerOn` | `"Encendiendo SIM7080G..."` | INFO |
| 42 | `powerOn` | `"Modulo ya esta encendido"` | INFO |
| 62 | `powerOn` | `"SIM7080G encendido correctamente!"` | INFO |
| 72 | `powerOn` | `"Error: No se pudo encender el modulo"` | ERROR |
| 77 | `powerOff` | `"Apagando SIM7080G..."` | INFO |
| 80 | `powerOff` | `"Modulo ya esta apagado"` | INFO |
| 88 | `powerOff` | `"SIM7080G apagado correctamente"` | INFO |
| 92 | `powerOff` | `"Apagado por software fallo, usando PWRKEY..."` | WARN |
| 97 | `powerOff` | `"SIM7080G apagado correctamente"` | INFO |
| 101 | `powerOff` | `"Error: No se pudo apagar el modulo"` | ERROR |
| 131 | `togglePWRKEY` | `"PWRKEY toggled"` | VERBOSE |
| 164 | `resetModem` | `"Reiniciando funcionalidad del modem..."` | WARN |
| 184 | `resetModem` | `"Error: CFUN fallo tras 3 intentos"` | ERROR |
| 190 | `resetModem` | `"Esperando SIM READY..."` | VERBOSE |
| 195 | `resetModem` | `"SIM READY"` | INFO |
| 203 | `resetModem` | `"Advertencia: SIM no esta lista"` | WARN |
| 234 | `getICCID` | `"Obteniendo ICCID..."` | INFO |
| 245 | `getICCID` | `"Error: Sin respuesta al comando AT+CCID"` | ERROR |
| 297 | `getICCID` | `"Error: No se pudo extraer el ICCID"` | ERROR |
| 337 | `configureOperator` | `"Error: Operadora invalida"` | ERROR |
| 353 | `configureOperator` | `"Error: CNMP fallo"` | ERROR |
| 363 | `configureOperator` | `"Error: CMNB fallo"` | ERROR |
| 397 | `configureOperator` | `"Error: BANDCFG fallo"` | ERROR |
| 408 | `configureOperator` | `"Error: BANDCFG fallo"` | ERROR |
| 461 | `configureOperator` | `"Invocando callback de error COPS..."` | WARN |
| 475 | `configureOperator` | `"Error: CGDCONT fallo"` | ERROR |
| 488 | `attachNetwork` | `"Attach a red..."` | INFO |
| 521 | `attachNetwork` | `"Error: CGATT fallo"` | ERROR |
| 526 | `attachNetwork` | `"Attach exitoso"` | INFO |
| 531 | `activatePDP` | `"Activando PDP context..."` | INFO |
| 534 | `activatePDP` | `"Error: CNACT fallo"` | ERROR |
| 539 | `activatePDP` | `"PDP activado"` | INFO |
| 544 | `deactivatePDP` | `"Desactivando PDP context..."` | INFO |
| 547 | `deactivatePDP` | `"Error: Deactivacion CNACT fallo"` | ERROR |
| 551 | `deactivatePDP` | `"PDP desactivado"` | INFO |
| 556 | `detachNetwork` | `"Detach de red..."` | INFO |
| 599 | `detachNetwork` | `"Detach exitoso"` | INFO |
| 604 | `detachNetwork` | `"Advertencia: Detach CGATT fallo tras 3 intentos"` | WARN |
| 609 | `getNetworkInfo` | `"Obteniendo info de red..."` | VERBOSE |
| 625 | `testOperator` | `"Error: Operadora invalida"` | ERROR |
| 642 | `testOperator` | `"Error en configuracion"` | ERROR |
| 649 | `testOperator` | `"Error en activacion PDP"` | ERROR |
| 674 | `getCurrentOperator` | `"Consultando operadora actual..."` | VERBOSE |
| 687 | `getCurrentBands` | `"Consultando bandas configuradas..."` | VERBOSE |
| 700 | `scanNetworks` | `"Escaneando redes disponibles..."` | INFO |
| 703 | `scanNetworks` | `"Error: No se pudo reiniciar el modem"` | ERROR |
| 724 | `checkSMSService` | `"Consultando servicio SMS..."` | VERBOSE |
| 737 | `sendSMS` | `"Enviando SMS..."` | INFO |
| 747 | `sendSMS` | `"Error: CMGF fallo (modo texto)"` | ERROR |
| 779 | `sendSMS` | `"Error: No se recibio prompt '>'"` | ERROR |
| 815 | `sendSMS` | `"SMS enviado exitosamente"` | INFO |
| 818 | `sendSMS` | `"Error: Fallo al enviar SMS"` | ERROR |
| 942 | `openTCPConnection` | `"Abriendo conexion TCP..."` | INFO |
| 944 | `openTCPConnection` | `"Cerrando conexion previa si existe..."` | VERBOSE |
| 1004 | `openTCPConnection` | `"Conexion TCP abierta exitosamente"` | INFO |
| 1007 | `openTCPConnection` | `"Error: Fallo al abrir conexion TCP tras 3 intentos"` | ERROR |
| 1011 | `openTCPConnection` | `"Invocando callback de error TCP..."` | WARN |
| 1020 | `closeTCPConnection` | `"Cerrando conexion TCP..."` | VERBOSE |
| 1023 | `closeTCPConnection` | `"Error: CACLOSE fallo"` | ERROR |
| 1028 | `closeTCPConnection` | `"Conexion TCP cerrada"` | INFO |
| 1052 | `sendTCPData` | `"Enviando datos por TCP..."` | INFO |
| 1055 | `sendTCPData` | `"Error: No hay datos para enviar"` | ERROR |
| 1060 | `sendTCPData` | `"Advertencia: Datos mayores a 1460 bytes, truncando"` | WARN |
| 1105 | `sendTCPData` | `"Error: No se recibio prompt '>' para envio TCP"` | ERROR |
| 1141 | `sendTCPData` | `"Datos enviados exitosamente por TCP"` | INFO |
| 1144 | `sendTCPData` | `"Error: Fallo al enviar datos por TCP"` | ERROR |

### Cambio 4: Inventario de bloques inline (44 bloques)

Los bloques inline usan `_debugSerial->print()` directamente para concatenar valores dinámicos. Se reemplazan con macros `DEBUG_*` usando `String()`:

| Línea | Función | Contenido del bloque | Nivel |
|-------|---------|----------------------|-------|
| 49 | `powerOn` | `"Intento de encendido X de Y"` | VERBOSE |
| 169 | `resetModem` | `"CFUN intento X..."` | VERBOSE |
| 238 | `getICCID` | `"ICCID raw: ..."` | VERBOSE |
| 268 | `getICCIDBytes` | Respuesta AT raw | VERBOSE |
| 287 | `getICCIDBytes` | Línea parseada | VERBOSE |
| 317 | `configureOperator` | `"Configurando operadora idx=X"` | INFO |
| 343 | `configureOperator` | `"CNMP: ..."` | VERBOSE |
| 348 | `configureOperator` | `"CMNB: ..."` | VERBOSE |
| 358 | `configureOperator` | `"CBANDCFG: ..."` | VERBOSE |
| 370 | `configureOperator` | `"CBANDCFG GSM: ..."` | VERBOSE |
| 387 | `configureOperator` | `"CBANDCFG Cat-M: ..."` | VERBOSE |
| 393 | `configureOperator` | `"CBANDCFG NB: ..."` | VERBOSE |
| 404 | `configureOperator` | `"CBANDCFG fallback: ..."` | VERBOSE |
| 413 | `configureOperator` | `"COPS manual: ..."` | INFO |
| 419 | `configureOperator` | `"Esperando COPS (120s)..."` | VERBOSE |
| 449 | `configureOperator` | `"COPS exitoso"` | INFO |
| 455 | `configureOperator` | `"Error: COPS fallo"` | ERROR |
| 467 | `configureOperator` | `"CGDCONT: ..."` | VERBOSE |
| 479 | `configureOperator` | `"Operadora configurada OK"` | INFO |
| 515 | `attachNetwork` | `"CGATT esperando (75s)..."` | VERBOSE |
| 561 | `detachNetwork` | `"CGATT=0 intento X..."` | VERBOSE |
| 593 | `detachNetwork` | `"Esperando detach..."` | VERBOSE |
| 613 | `getNetworkInfo` | `"Red info: ..."` | VERBOSE |
| 631 | `testOperator` | `"Probando operadora idx=X"` | INFO |
| 663 | `testOperator` | `"Calidad señal: ..."` | INFO |
| 678 | `getCurrentOperator` | `"Operadora actual: ..."` | VERBOSE |
| 691 | `getCurrentBands` | `"Bandas: ..."` | VERBOSE |
| 707 | `scanNetworks` | `"Scan timeout: ..."` | VERBOSE |
| 713 | `scanNetworks` | `"Redes encontradas: ..."` | INFO |
| 728 | `checkSMSService` | `"SMS service: ..."` | VERBOSE |
| 739 | `sendSMS` | `"Destinatario: ..."` | INFO |
| 775 | `sendSMS` | `"Esperando prompt SMS..."` | VERBOSE |
| 809 | `sendSMS` | `"Respuesta SMS: ..."` | VERBOSE |
| 871 | `parseSignalQuality` | `"SINR/RSRP/RSRQ: ..."` | VERBOSE |
| 892 | `getBestOperator` | `"Probando operadora X..."` | INFO |
| 898 | `getBestOperator` | `"Score: ..."` | INFO |
| 917 | `getBestOperator` | `"Mejor operadora: ..."` | INFO |
| 953 | `openTCPConnection` | `"CAOPEN intento X..."` | VERBOSE |
| 962 | `openTCPConnection` | `"CAOPEN cmd: ..."` | VERBOSE |
| 993 | `openTCPConnection` | `"CAOPEN respuesta: ..."` | VERBOSE |
| 1035 | `isTCPConnected` | `"TCP estado: ..."` | VERBOSE |
| 1066 | `sendTCPData` | `"CASEND len=X..."` | VERBOSE |
| 1101 | `sendTCPData` | `"Esperando prompt TCP..."` | VERBOSE |
| 1135 | `sendTCPData` | `"TCP respuesta: ..."` | VERBOSE |

### Cambio 5: Log semántico en AppController.cpp (líneas 823-829)

Código actual exacto:
```cpp
case AppState::Cycle_SendLTE:
    {
        Serial.println("[DEBUG][APP] Iniciando envio por LTE...");
        (void)sendBufferOverLTE_AndMarkProcessed();
        Serial.println("[DEBUG][APP] Envio completado, pasando a CompactBuffer");
        g_state = AppState::Cycle_CompactBuffer;
        break;
    }
```

Reemplazo:
```cpp
case AppState::Cycle_SendLTE:
    {
        Serial.println("[INFO][APP] Iniciando envio por LTE...");
        bool lteResult = sendBufferOverLTE_AndMarkProcessed();  // FIX-V1
        // ============ [FIX-V1 START] Log semántico de resultado LTE ============
        if (lteResult) {
            Serial.println("[INFO][APP] Envio LTE exitoso, pasando a CompactBuffer");
        } else {
            Serial.println("[WARN][APP] Envio LTE fallido, pasando a CompactBuffer");
        }
        // ============ [FIX-V1 END] ============
        g_state = AppState::Cycle_CompactBuffer;
        break;
    }
```

---

## 🧪 VERIFICACIÓN

### Output Esperado (Ciclo Normal — Modem Responde)

```
[INFO][SLEEP] SleepModule initializing...
[INFO][APP] Ciclo subsecuente: recuperando coordenadas GPS de NVS
[INFO][LTE] Encendiendo SIM7080G...
[INFO][LTE] SIM7080G encendido correctamente
[INFO][LTE] Obteniendo ICCID...
[INFO][LTE] ICCID: 89520000001234567890
[INFO][APP] === TRAMA NORMAL ===
...
[INFO][APP] Iniciando envio por LTE...
[INFO][LTE] Encendiendo SIM7080G...
[INFO][LTE] Modulo ya esta encendido
[INFO][APP] Envio LTE exitoso, pasando a CompactBuffer
```

### Output Esperado (Modem No Responde)

```
[INFO][LTE] Encendiendo SIM7080G...
[VERBOSE][LTE] Intento de encendido 1/3
[VERBOSE][LTE] PWRKEY toggled
[VERBOSE][LTE] Intento de encendido 2/3
[VERBOSE][LTE] PWRKEY toggled
[VERBOSE][LTE] Intento de encendido 3/3
[VERBOSE][LTE] PWRKEY toggled
[ERROR][LTE] No se pudo encender el modulo tras todos los intentos
...
[WARN][APP] Envio LTE fallido, pasando a CompactBuffer
```

### Criterios de Aceptación

- [ ] Todos los mensajes de LTEModule usan formato `[NIVEL][LTE]`
- [ ] Las 66 llamadas `debugPrint()` reemplazadas por macros `DEBUG_*`
- [ ] Los 44 bloques inline `_debugEnabled && _debugSerial` reemplazados por macros `DEBUG_*`
- [ ] Los mensajes son filtrables via `DEBUG_LTE_ENABLED` y `DEBUG_LEVEL`
- [ ] Con `DEBUG_LTE_ENABLED=false`, no se imprime ningún mensaje LTE
- [ ] El log de AppController distingue éxito vs fallo de envío (líneas 823-829)
- [ ] Con flag `ENABLE_FIX_V1_LTE_STANDARD_LOGGING=0`, se usa `debugPrint()` original
- [ ] No hay cambios en lógica funcional (mismo flujo, mismas decisiones)
- [ ] El método `debugPrint()` y su overload `debugPrint(msg, value)` se preservan en la clase para rollback

---

## 📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
| 2026-02-10 | Documentación inicial basada en análisis de log del 2026-02-05 | 1.0 |
| 2026-02-10 | Revisión de precisión: conteo exacto (66 debugPrint + 44 inline = 110 puntos), inventario completo con líneas, código ANTES exacto de powerOn() líneas 38-73, bloque #else con código original verbatim | 1.1 |
