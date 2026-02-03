# FIX-V6: Secuencia Robusta de Power On/Off del Modem SIM7080G

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V6 |
| **Tipo** | Fix (Estabilidad de comunicaciones) |
| **Sistema** | LTE / Modem |
| **Archivo Principal** | `src/data_lte/LTEModule.cpp` |
| **Estado** | 📋 Documentado |
| **Fecha Identificación** | 2026-01-29 |
| **Versión Target** | v2.8.0 |
| **Depende de** | FEAT-V1 (FeatureFlags.h), FEAT-V7 (ProductionDiag) |
| **Prioridad** | **CRÍTICA** |
| **Premisas** | P1✅ P2✅ P3✅ P4✅ P5✅ P6✅ |

---

## 🎯 OBJETIVO

> **Prevenir que el modem SIM7080G entre en estado "zombie" irrecuperable, implementando la secuencia de power on/off según especificaciones oficiales de SIMCOM.**

---

## 📚 DOCUMENTACIÓN DE SOPORTE

| Documento | Ubicación | Contenido |
|-----------|-----------|-----------|
| Diagnóstico inicial | [DIAGNOSTICO_MODEM_ZOMBIE_2026-01-29.md](../../calidad/DIAGNOSTICO_MODEM_ZOMBIE_2026-01-29.md) | Análisis de dispositivos 6948, 6963 |
| Investigación exhaustiva | [INVESTIGACION_MODEM_ZOMBIE.md](../../calidad/INVESTIGACION_MODEM_ZOMBIE.md) | Evidencia de foros, datasheet, código de referencia |
| Datasheet SIMCOM | [Hardware Design V1.04](https://www.texim-europe.com/Cmsfile/SMM-SIM7080G-Hardware-Design-V1.04-DS-200525-TE.pdf) | Especificaciones oficiales de tiempos |

---

## 🔍 DIAGNÓSTICO

### Problema: Modem en Estado "Zombie"

**Descripción:**
Los dataloggers dejan de transmitir durante **días consecutivos** aunque el ciclo de sensores continúa ejecutándose. El modem no responde a comandos AT ni a la secuencia PWRKEY.

**Dispositivos afectados confirmados:** 6948, 6963

**Síntomas en logs:**
```
Encendiendo SIM7080G...
Intento de encendido 1 de 3
PWRKEY toggled
Intento de encendido 2 de 3
PWRKEY toggled
Intento de encendido 3 de 3
PWRKEY toggled
Error: No se pudo encender el modulo
```

**Evidencia temporal (dispositivo 6948):**

| Epoch | GPS | ICCID | LTE | Batería |
|-------|-----|-------|-----|---------|
| 1769691720 | ❌ 3x | ❌ 3x | ❌ 3x | 4.07V |
| 1769691952 | ❌ 3x | ❌ 3x | ❌ 3x | 4.07V |
| 1769692106 | ❌ 3x | ❌ 3x | ❌ 3x | 4.07V |

**Recuperación:** Solo power cycle físico (desconectar batería)

### Causa Raíz Identificada

El código actual de `LTEModule.cpp` **viola las especificaciones del datasheet SIMCOM**:

| Código Actual | Problema según Datasheet |
|---------------|--------------------------|
| `if (!isAlive()) return true` | Falso positivo: modem puede no responder pero estar encendido |
| `delay(2000)` después de CPOWD | **NO espera URC "NORMAL POWER DOWN"** |
| PWRKEY pulse 1200ms | En límite mínimo (datasheet: >1.2s) |
| No hay reset >12.6s | No usa capacidad de reset forzado interno |
| No maneja PSM | Primer AT después de PSM se pierde |

### Evidencia de Foros (Casos Idénticos)

**GitHub botletics/SIM7000-LTE-Shield #322:**
> "module is not responsive to commands. Even PWR_KEY stops working and I can't turn the module back up other than physically disconnecting the battery"

**M5Stack Community:**
> "the first AT command after the modem wakes up from PSM sleep is always ignored / lost"

---

## 📊 EVALUACIÓN

### Impacto

| Aspecto | Evaluación |
|---------|------------|
| Criticidad | **CRÍTICA** - Pérdida total de transmisión |
| Riesgo de no implementar | Dispositivos muertos en campo (días sin datos) |
| Esfuerzo | Medio (~4h) |
| Beneficio | Muy Alto |

### Limitación Conocida

⚠️ **Este fix es una MITIGACIÓN, no una solución definitiva.**

La solución definitiva requiere **modificación de hardware**:
- MOSFET para corte de VBAT del modem
- O GPIO conectado a pin RESET del SIM7080G

Sin embargo, este fix puede:
- Reducir significativamente la frecuencia del problema
- Recuperar de estados PSM
- Detectar y reportar cuando ocurre estado zombie real

---

## 🔧 SOLUCIÓN

### Especificaciones del Datasheet SIMCOM (Tabla 8 y 9)

| Parámetro | Valor | Uso |
|-----------|-------|-----|
| Power On | >1 segundo LOW | Encender módulo |
| Power Off | >1.2 segundos LOW | Apagar módulo |
| **Reset Forzado** | **>12.6 segundos LOW** | Reset interno automático |
| UART Ready | 1.8s después de power-on | Esperar antes de AT |
| Toff-on | ≥2 segundos | Buffer entre apagado y encendido |
| URC Apagado | "NORMAL POWER DOWN" | Confirmar apagado exitoso |

### Cambios a Implementar

#### 1. Feature Flag en `FeatureFlags.h`

```cpp
// ============================================================
// FIX-V6: Secuencia robusta de power on/off del modem
// ============================================================
// Implementa especificaciones del datasheet SIMCOM:
// - Espera URC "NORMAL POWER DOWN" en powerOff()
// - PWRKEY extendido (1.5s) y reset forzado (>12.6s)
// - Manejo de PSM con AT+CPSMS=0
// - Reintentos múltiples de AT después de wake
// Documentación: fixs-feats/fixs/FIX_V6_MODEM_POWER_SEQUENCE.md
#define ENABLE_FIX_V6_MODEM_POWER_SEQUENCE  1
```

#### 2. Constantes en `config_lte.h`

```cpp
// ============ [FIX-V6 START] Tiempos según datasheet SIMCOM ============
#if ENABLE_FIX_V6_MODEM_POWER_SEQUENCE
    // Datasheet SIM7080G Hardware Design V1.04, Tabla 8
    constexpr uint16_t PWRKEY_ON_TIME_MS = 1500;      // >1s, usar 1.5s por seguridad
    constexpr uint16_t PWRKEY_OFF_TIME_MS = 1500;     // >1.2s, usar 1.5s
    constexpr uint16_t PWRKEY_RESET_TIME_MS = 13000;  // >12.6s reset forzado
    constexpr uint16_t UART_READY_DELAY_MS = 2500;    // 1.8s + margen
    constexpr uint16_t TOFF_TON_BUFFER_MS = 2000;     // ≥2s entre off y on
    constexpr uint16_t URC_WAIT_TIMEOUT_MS = 10000;   // Espera de URC (10s, datasheet: ~1.8s típico)
    constexpr uint8_t  AT_RETRY_COUNT = 5;            // Reintentos de AT
    constexpr uint16_t AT_RETRY_DELAY_MS = 500;       // Delay entre reintentos
#else
    // Valores originales
    constexpr uint16_t PWRKEY_ON_TIME_MS = 1200;
    constexpr uint16_t PWRKEY_OFF_TIME_MS = 1200;
#endif
// ============ [FIX-V6 END] ============
```

#### 3. Nuevo `powerOff()` en `LTEModule.cpp`

```cpp
bool LTEModule::powerOff() {
#if ENABLE_FIX_V6_MODEM_POWER_SEQUENCE
    // ============ [FIX-V6 START] Apagado robusto según datasheet ============
    
    // 1. Vaciar buffer UART (puede tener URCs pendientes)
    while (_serial.available()) _serial.read();
    
    // 2. Intentar apagado graceful con AT+CPOWD
    debugPrint("[LTE] Enviando AT+CPOWD=1");
    _serial.println("AT+CPOWD=1");
    
    // 3. CRÍTICO: Esperar URC "NORMAL POWER DOWN" (datasheet: ~1.8s típico)
    unsigned long start = millis();
    while (millis() - start < URC_WAIT_TIMEOUT_MS) {
        if (_serial.available()) {
            String response = _serial.readStringUntil('\n');
            response.trim();  // Limpiar \r\n y espacios
            if (response.indexOf("NORMAL POWER DOWN") != -1) {
                debugPrint("[LTE] URC recibido - apagado confirmado");
                delay(500);
                return true;
            }
        }
        delay(100);
    }
    
    // 4. Si no recibió URC, intentar PWRKEY extendido
    debugPrint("[LTE] WARN: URC no recibido, intentando PWRKEY");
    digitalWrite(_pwrkeyPin, LOW);
    delay(PWRKEY_OFF_TIME_MS);
    digitalWrite(_pwrkeyPin, HIGH);
    delay(TOFF_TON_BUFFER_MS);  // Esperar settling
    
    if (!isAlive()) {
        debugPrint("[LTE] Apagado por PWRKEY exitoso");
        return true;
    }
    
    // 5. ÚLTIMO RECURSO: Reset forzado (>12.6s según datasheet)
    debugPrint("[LTE] WARN: Forzando reset por PWRKEY >12.6s");
    digitalWrite(_pwrkeyPin, LOW);
    delay(PWRKEY_RESET_TIME_MS);
    digitalWrite(_pwrkeyPin, HIGH);
    delay(TOFF_TON_BUFFER_MS);
    
    bool success = !isAlive();
    if (!success) {
        debugPrint("[LTE] ERROR: Modem no responde a reset - posible zombie");
        #if ENABLE_FEAT_V7_PRODUCTION_DIAG
        ProductionDiag::incrementZombieCount();
        #endif
    }
    return success;
    
    // ============ [FIX-V6 END] ============
#else
    // Código original preservado
    if (!isAlive()) {
        debugPrint("Modulo ya esta apagado");
        return true;
    }
    _serial.println("AT+CPOWD=1");
    delay(2000);
    // ... resto del código original
#endif
}
```

#### 4. Nuevo `powerOn()` en `LTEModule.cpp`

```cpp
bool LTEModule::powerOn() {
#if ENABLE_FIX_V6_MODEM_POWER_SEQUENCE
    // ============ [FIX-V6 START] Encendido robusto según datasheet ============
    
    // 1. Verificar si ya está encendido
    if (isAlive()) {
        debugPrint("[LTE] Modem ya está encendido");
        return true;
    }
    
    for (int attempt = 0; attempt < 3; attempt++) {
        debugPrint("[LTE] Intento de encendido " + String(attempt + 1) + " de 3");
        
        // 2. PWRKEY LOW por >1s (usar 1.5s por seguridad)
        digitalWrite(_pwrkeyPin, LOW);
        delay(PWRKEY_ON_TIME_MS);
        digitalWrite(_pwrkeyPin, HIGH);
        
        // 3. Esperar UART ready (1.8s según datasheet + margen)
        delay(UART_READY_DELAY_MS);
        
        // 4. Vaciar buffer antes de enviar AT
        while (_serial.available()) _serial.read();
        
        // 5. Enviar múltiples AT (primer comando puede perderse por PSM)
        for (int i = 0; i < AT_RETRY_COUNT; i++) {
            _serial.println("AT");
            delay(AT_RETRY_DELAY_MS);
            if (isAlive()) {
                // 6. Deshabilitar PSM para operación confiable
                debugPrint("[LTE] Modem respondió, deshabilitando PSM");
                _serial.println("AT+CPSMS=0");
                delay(200);
                return true;
            }
        }
        
        debugPrint("[LTE] Intento " + String(attempt + 1) + " fallido");
    }
    
    // 7. Si falló 3 veces, intentar reset forzado (>12.6s)
    debugPrint("[LTE] WARN: 3 intentos fallidos, ejecutando reset forzado");
    digitalWrite(_pwrkeyPin, LOW);
    delay(PWRKEY_RESET_TIME_MS);
    digitalWrite(_pwrkeyPin, HIGH);
    delay(UART_READY_DELAY_MS);
    
    // 8. Vaciar buffer y última verificación
    while (_serial.available()) _serial.read();
    for (int i = 0; i < AT_RETRY_COUNT; i++) {
        _serial.println("AT");
        delay(AT_RETRY_DELAY_MS);
        if (isAlive()) {
            _serial.println("AT+CPSMS=0");
            debugPrint("[LTE] Recuperado después de reset forzado");
            return true;
        }
    }
    
    // 9. Si todo falla, el modem está en estado zombie
    debugPrint("[LTE] ERROR CRÍTICO: Modem en estado zombie - requiere power cycle");
    #if ENABLE_FEAT_V7_PRODUCTION_DIAG
    ProductionDiag::incrementZombieCount();
    ProductionDiag::logEvent("MODEM_ZOMBIE");
    #endif
    
    return false;
    
    // ============ [FIX-V6 END] ============
#else
    // Código original preservado
    if (isAlive()) {
        return true;
    }
    // ... resto del código original
#endif
}
```

---

## 📁 ARCHIVOS A MODIFICAR

| Archivo | Cambio | Líneas aprox |
|---------|--------|--------------|
| `src/FeatureFlags.h` | Agregar flag `ENABLE_FIX_V6_MODEM_POWER_SEQUENCE` | +10 |
| `src/data_lte/config_lte.h` | Agregar constantes de tiempos | +15 |
| `src/data_lte/LTEModule.cpp` | Modificar `powerOn()` y `powerOff()` | +80 |
| `src/data_diagnostics/ProductionDiag.cpp` | Agregar `incrementZombieCount()` | +10 |
| `src/version_info.h` | Actualizar a v2.8.0 | +5 |

---

## ✅ CRITERIOS DE ACEPTACIÓN

| # | Criterio | Verificación |
|---|----------|--------------|
| 1 | `powerOff()` espera URC "NORMAL POWER DOWN" | Log muestra "URC recibido" |
| 2 | `powerOn()` reintenta AT 5 veces | Log muestra reintentos |
| 3 | Reset forzado (>12.6s) se ejecuta como último recurso | Log muestra "reset forzado" |
| 4 | PSM se deshabilita con `AT+CPSMS=0` | Comando enviado después de wake |
| 5 | Contador de zombies se incrementa | ProductionDiag registra eventos |
| 6 | Código original preservado en `#else` | Rollback instantáneo posible |

---

## 🔄 ROLLBACK

```cpp
// En FeatureFlags.h cambiar:
#define ENABLE_FIX_V6_MODEM_POWER_SEQUENCE  0  // Deshabilitado
```

Recompilar y subir. El comportamiento vuelve al código original.

---

## 📈 MÉTRICAS DE ÉXITO

| Métrica | Baseline (sin fix) | Objetivo (con fix) |
|---------|-------------------|-------------------|
| Frecuencia de zombie | ~1 por semana | <1 por mes |
| Recovery sin power cycle | 0% | >80% (casos PSM) |
| Detección de zombie real | No detectado | 100% registrado |

---

## ⚠️ NOTAS IMPORTANTES

1. **Este fix NO elimina el problema al 100%** - El estado zombie "real" solo se recupera con power cycle físico (requiere hardware)

2. **Mejora significativa esperada** - Muchos "zombies" pueden ser en realidad:
   - PSM activo (se recupera con reintentos + CPSMS=0)
   - Apagado incompleto (se recupera con URC wait)
   - Estado intermedio (se recupera con reset >12.6s)

3. **Diagnóstico mejorado** - Ahora sabremos cuántos zombies "reales" ocurren vs recuperables

---

## 📚 REFERENCIAS

1. [SIM7080G Hardware Design V1.04](https://www.texim-europe.com/Cmsfile/SMM-SIM7080G-Hardware-Design-V1.04-DS-200525-TE.pdf) - Tabla 8, 9
2. [SIM7080 AT Command Manual V1.05](https://edworks.co.kr/wp-content/uploads/2022/04/SIM7070_SIM7080_SIM7090-Series_AT-Command-Manual_V1.05.pdf)
3. [GitHub botletics #322](https://github.com/botletics/SIM7000-LTE-Shield/issues/322) - Caso idéntico
4. [M5Stack Community](https://community.m5stack.com/topic/4694/uiflow-and-cat-m-module-sim7080g) - PSM y primer AT
5. [ModularSensors](https://envirodiy.github.io/ModularSensors/group__modem__sim7080.html) - Reset por PWRKEY

---

*Documento creado: 2026-01-29*  
*Basado en: DIAGNOSTICO_MODEM_ZOMBIE_2026-01-29.md, INVESTIGACION_MODEM_ZOMBIE.md*
