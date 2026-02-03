# Revisión de Código: sensores_rv03 v2.2.0

**Fecha:** 2026-01-14  
**Versión Revisada:** v2.2.0 (fallback-operadora)  
**Revisor:** GitHub Copilot (Claude Opus 4.5)  
**Calificación General:** 8/10

---

## 📋 Resumen Ejecutivo

El proyecto sensores_rv03 presenta una arquitectura sólida basada en una máquina de estados finitos (FSM) bien diseñada. El código es profesional, modular y está excelentemente documentado. Se identificaron 6 problemas menores que no afectan la funcionalidad core pero podrían optimizarse.

---

## ✅ Aspectos Positivos

| Área | Observación | Archivos |
|------|-------------|----------|
| **Arquitectura FSM** | Máquina de estados bien definida con 11 estados claros y transiciones documentadas | `AppController.cpp` |
| **Modularidad** | Separación clara en módulos independientes (GPS, LTE, Sensores, Buffer, Sleep) | `src/` |
| **Documentación Doxygen** | Comentarios completos con `@brief`, `@param`, `@return`, `@note` en todas las funciones públicas | Todos los `.h` y `.cpp` |
| **Feature Flags** | Sistema robusto para activar/desactivar fixes sin modificar código, permite rollbacks seguros | `src/FeatureFlags.h` |
| **Control de Versiones** | Historial detallado con cambios, fechas, autores y archivos afectados | `src/version_info.h` |
| **Sistema de Debug** | Debug configurable por módulo con niveles (ERROR/WARN/INFO/VERBOSE) | `src/DebugConfig.h` |
| **Timing Instrumentado** | Medición de tiempos de cada fase del ciclo para diagnóstico de performance | `src/CycleTiming.h` |
| **Persistencia NVS** | Almacenamiento correcto de coordenadas GPS y operadora LTE entre ciclos | `AppController.cpp` |
| **Buffer Persistente** | Sistema de buffer en LittleFS con marcado de tramas procesadas | `src/data_buffer/BUFFERModule.cpp` |
| **Selección Inteligente de Operadora** | Algoritmo de scoring basado en SINR, RSRP, RSRQ con fallback automático | `src/data_lte/LTEModule.cpp` |

---

## ⚠️ Problemas Identificados

### P1: RS485Module - Falta desactivar potencia al finalizar

**Archivo:** `src/data_sensors/RS485Module.cpp`  
**Líneas:** 5-9  
**Severidad:** Media  
**Tipo:** Consumo de energía

**Descripción:**  
El método `begin()` llama a `enablePow()` para encender el sensor RS485, pero en ninguna parte del ciclo se llama `disablePow()` antes del deep sleep.

**Código actual:**
```cpp
bool RS485Module::begin() {
  enablePow();  // Se enciende aquí...
  serialPort_ = &Serial2;
  // ...pero nunca se apaga después de leer
```

**Impacto:**  
Consumo de energía innecesario durante deep sleep. El sensor RS485 permanece energizado.

**Solución propuesta:**  
Agregar método `end()` que llame a `disablePow()` y llamarlo antes de `Cycle_Sleep`.

---

### P2: Buffer - Falta recortar caracteres \r de líneas

**Archivo:** `src/data_buffer/BUFFERModule.cpp`  
**Líneas:** 47-48  
**Severidad:** Baja  
**Tipo:** Formato de datos

**Descripción:**  
Al leer líneas del buffer con `readStringUntil('\n')`, puede quedar el carácter `\r` al final de la línea.

**Código actual:**
```cpp
while (file.available() && count < maxLines) {
    lines[count] = file.readStringUntil('\n');  // Puede contener '\r'
    count++;
}
```

**Impacto:**  
Tramas con caracteres extra que podrían fallar validación en el servidor.

**Solución propuesta:**  
Agregar `lines[count].trim()` después de la lectura.

---

### P3: RS485Module - Falta timeout explícito en Modbus

**Archivo:** `src/data_sensors/RS485Module.cpp`  
**Líneas:** 14-25  
**Severidad:** Media  
**Tipo:** Robustez

**Descripción:**  
No hay manejo de timeout explícito en la lectura Modbus. Si el dispositivo esclavo no responde, podría bloquear.

**Código actual:**
```cpp
uint8_t result = node_.readHoldingRegisters(
    MODBUS_START_ADDRESS,
    MODBUS_REGISTER_COUNT
);
// Sin timeout configurable visible
```

**Impacto:**  
Posible bloqueo del sistema si sensor RS485 falla o desconecta.

**Solución propuesta:**  
Verificar configuración de timeout en la librería ModbusMaster o implementar watchdog timer.

---

### P4: AppController - Delay innecesario en loop de envío

**Archivo:** `AppController.cpp`  
**Líneas:** 489  
**Severidad:** Baja  
**Tipo:** Performance

**Descripción:**  
Hay un `delay(50)` dentro del loop de envío de tramas que aumenta el tiempo de ciclo.

**Código actual:**
```cpp
if (sentOk) {
  buffer.markLineAsProcessed(i);
  anySent = true;
  sentCount++;
  delay(50);  // ¿Necesario?
```

**Impacto:**  
Aumenta tiempo de ciclo innecesariamente (~50ms por trama).

**Solución propuesta:**  
Evaluar si el delay es necesario para estabilidad de conexión TCP. Si no, reducir o eliminar.

---

### P5: AppController - Falta validación de ICCID vacío

**Archivo:** `AppController.cpp`  
**Líneas:** 798-805  
**Severidad:** Media  
**Tipo:** Validación de datos

**Descripción:**  
Si falla la lectura del ICCID, se usa string vacío en la trama.

**Código actual:**
```cpp
if (lte.powerOn()) {
  g_iccid = lte.getICCID();
  lte.powerOff();
} else {
  g_iccid = "";  // Trama se construye con ICCID vacío
}
```

**Impacto:**  
Tramas con ICCID vacío podrían ser rechazadas o causar problemas de identificación en el servidor.

**Solución propuesta:**  
Usar valor por defecto como `"00000000000000000000"` (20 ceros) o marcar como error y saltar ciclo.

---

### P6: AppController - Resultado de envío LTE ignorado

**Archivo:** `AppController.cpp`  
**Líneas:** 857-862  
**Severidad:** Baja  
**Tipo:** Diagnóstico

**Descripción:**  
El resultado de `sendBufferOverLTE_AndMarkProcessed()` se ignora con `(void)`.

**Código actual:**
```cpp
case AppState::Cycle_SendLTE: {
  TIMING_START(g_timing, sendLte);
  Serial.println("[DEBUG][APP] Iniciando envio por LTE...");
  (void)sendBufferOverLTE_AndMarkProcessed();  // Se ignora resultado
  Serial.println("[DEBUG][APP] Envio completado, pasando a CompactBuffer");
```

**Impacto:**  
No hay registro ni acción diferenciada si falla el envío completo. Dificulta diagnóstico remoto.

**Solución propuesta:**  
Agregar contador de fallos consecutivos en NVS para diagnóstico y posible acción correctiva.

---

## 🔧 Recomendaciones Priorizadas

| # | Prioridad | Recomendación | Esfuerzo |
|---|-----------|---------------|----------|
| 1 | **Alta** | Agregar `disablePow()` para RS485 antes de deep sleep | 30 min |
| 2 | **Alta** | Agregar `trim()` a líneas leídas del buffer | 15 min |
| 3 | **Media** | Implementar watchdog timer para casos de bloqueo | 2 hrs |
| 4 | **Media** | Agregar validación de ICCID antes de construir trama | 30 min |
| 5 | **Baja** | Revisar necesidad de `delay(50)` en loop de envío | 15 min |
| 6 | **Baja** | Agregar contador de fallos LTE consecutivos | 1 hr |

---

## 📊 Métricas del Código

| Archivo | Líneas | Complejidad | Documentación |
|---------|--------|-------------|---------------|
| `AppController.cpp` | 895 | Alta (FSM 11 estados) | Excelente |
| `LTEModule.cpp` | 1138 | Alta (comandos AT) | Buena |
| `GPSModule.cpp` | 422 | Media | Buena |
| `BUFFERModule.cpp` | 259 | Baja | Buena |
| `RS485Module.cpp` | 57 | Baja | Básica |
| `FeatureFlags.h` | 123 | Baja | Excelente |
| `DebugConfig.h` | 137 | Baja | Excelente |
| `version_info.h` | ~80 | Baja | Excelente |

**Total de líneas revisadas:** ~3,111  
**Cobertura de revisión:** 100% de archivos core

---

## 📁 Estructura del Proyecto

```
sensores_rv03/
├── sensores_rv03.ino          # Entry point minimalista
├── AppController.cpp/.h       # FSM principal (~900 líneas)
├── src/
│   ├── version_info.h         # Control de versiones
│   ├── FeatureFlags.h         # Feature flags (FIX-V1, FIX-V2, FEAT-V2)
│   ├── DebugConfig.h          # Sistema de debug por módulo
│   ├── CycleTiming.h          # Medición de tiempos
│   ├── data_buffer/           # Buffer LittleFS + BLE config
│   ├── data_format/           # Formateo de tramas
│   ├── data_gps/              # Módulo GPS (SIM7080G GNSS)
│   ├── data_lte/              # Módulo LTE Cat-M/NB-IoT
│   ├── data_sensors/          # ADC, I2C, RS485 Modbus
│   ├── data_sleepwakeup/      # Deep sleep management
│   └── data_time/             # RTC DS1307
├── calidad/                   # Documentos de QA
├── memoria_de_proyecto/       # Documentación técnica
└── fixs-feats/                # Historial de fixes
```

---

## ✨ Conclusión

El proyecto **sensores_rv03 v2.2.0** presenta código de calidad profesional con una arquitectura bien pensada. Los problemas identificados son menores y no afectan la funcionalidad core del sistema. Se recomienda abordar las correcciones de prioridad Alta en la próxima iteración.

### Próximos pasos sugeridos:
1. Implementar correcciones P1 y P2 (Alta prioridad)
2. Evaluar implementación de watchdog timer (P3)
3. Agregar validación de ICCID (P4)

---

**Firma digital:** Revisión automática por GitHub Copilot  
**Hash de revisión:** v2.2.0-review-20260114
