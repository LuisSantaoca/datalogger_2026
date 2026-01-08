# FEAT-V2: Cycle Timing (Instrumentación de Tiempos)

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V2 |
| **Tipo** | Feature (Diagnóstico/Profiling) |
| **Sistema** | AppController / Ciclo Principal |
| **Archivo Principal** | `AppController.cpp` |
| **Estado** | 📋 Propuesto |
| **Fecha Propuesta** | 2026-01-08 |
| **Versión Target** | v2.1.0 |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Prioridad** | Media |

---

## 🎯 REQUISITO TÉCNICO

> **REQ-FEAT-V2**: El sistema debe registrar y mostrar en log la duración (en milisegundos) de cada fase del ciclo de operación, permitiendo análisis de rendimiento y detección de cuellos de botella.

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
- No hay visibilidad del tiempo que tarda cada fase del ciclo
- Imposible identificar cuellos de botella sin mediciones
- No se pueden comparar optimizaciones de forma cuantitativa
- Difícil diagnosticar problemas de timeout

### Síntomas
1. Afirmaciones de tiempo basadas en estimaciones, no datos
2. Optimizaciones sin métricas de antes/después
3. Usuarios reportan "lentitud" sin datos específicos

### Causa Raíz
Falta de instrumentación de tiempos en el firmware.

---

## 📊 EVALUACIÓN

### Impacto
| Aspecto | Evaluación |
|---------|------------|
| Criticidad | Media |
| Riesgo de no implementar | Bajo (funcional, mejora diagnóstico) |
| Esfuerzo | Bajo-Medio |
| Beneficio | Alto (datos para optimizar) |

### Justificación
- Permite medir impacto real de FIX-V1 y futuros fixes
- Facilita diagnóstico remoto de problemas
- Habilita decisiones basadas en datos

---

## 🔧 IMPLEMENTACIÓN

### Fases a Medir

| Fase | Código | Descripción |
|------|--------|-------------|
| `BLE` | `BleOnly` | Tiempo en modo BLE (hasta timeout o conexión) |
| `SENSORS` | `Cycle_ReadSensors` | Lectura de sensores ADC, I2C, RS485 |
| `GPS` | `Cycle_GPS` | Adquisición GPS (solo primer ciclo) |
| `ICCID` | `Cycle_GetICCID` | Obtener ICCID del SIM |
| `BUILD` | `Cycle_BuildFrame` | Construcción de trama |
| `BUFFER` | `Cycle_BufferWrite` | Escritura a buffer persistente |
| `LTE_TOTAL` | `Cycle_SendLTE` | Ciclo LTE completo |
| `LTE_POWERON` | - | Encender modem |
| `LTE_CONFIG` | - | Configurar operadora |
| `LTE_ATTACH` | - | Attach a red |
| `LTE_PDP` | - | Activar PDP |
| `LTE_TCP` | - | Abrir conexión TCP |
| `LTE_SEND` | - | Enviar datos |
| `LTE_CLOSE` | - | Cerrar y apagar |
| `COMPACT` | `Cycle_CompactBuffer` | Compactar buffer |
| `CYCLE_TOTAL` | - | Ciclo completo (sin sleep) |

### Estructura de Datos

```cpp
// En AppController.h o nuevo archivo src/CycleTiming.h

struct CycleTiming {
    unsigned long cycleStart;
    unsigned long bleTime;
    unsigned long sensorsTime;
    unsigned long gpsTime;
    unsigned long iccidTime;
    unsigned long buildTime;
    unsigned long bufferWriteTime;
    unsigned long lteTotal;
    unsigned long ltePowerOn;
    unsigned long lteConfig;
    unsigned long lteAttach;
    unsigned long ltePdp;
    unsigned long lteTcp;
    unsigned long lteSend;
    unsigned long lteClose;
    unsigned long compactTime;
    unsigned long cycleTotal;
};

// Variable global o estática
static CycleTiming g_timing;
```

### Macros de Medición

```cpp
// En src/CycleTiming.h

#if ENABLE_FEAT_V2_CYCLE_TIMING

#define TIMING_START(phase) \
    unsigned long _timing_##phase##_start = millis()

#define TIMING_END(phase, target) \
    target = millis() - _timing_##phase##_start; \
    Serial.printf("[TIMING] %s: %lu ms\n", #phase, target)

#define TIMING_RESET() \
    memset(&g_timing, 0, sizeof(g_timing)); \
    g_timing.cycleStart = millis()

#define TIMING_PRINT_SUMMARY() \
    printTimingSummary()

#else

#define TIMING_START(phase)
#define TIMING_END(phase, target)
#define TIMING_RESET()
#define TIMING_PRINT_SUMMARY()

#endif
```

### Función de Resumen

```cpp
inline void printTimingSummary() {
    Serial.println(F("=== CYCLE TIMING SUMMARY ==="));
    Serial.printf("  BLE:        %5lu ms\n", g_timing.bleTime);
    Serial.printf("  Sensors:    %5lu ms\n", g_timing.sensorsTime);
    Serial.printf("  GPS:        %5lu ms\n", g_timing.gpsTime);
    Serial.printf("  ICCID:      %5lu ms\n", g_timing.iccidTime);
    Serial.printf("  Build:      %5lu ms\n", g_timing.buildTime);
    Serial.printf("  Buffer:     %5lu ms\n", g_timing.bufferWriteTime);
    Serial.println(F("  --- LTE Breakdown ---"));
    Serial.printf("    PowerOn:  %5lu ms\n", g_timing.ltePowerOn);
    Serial.printf("    Config:   %5lu ms\n", g_timing.lteConfig);
    Serial.printf("    Attach:   %5lu ms\n", g_timing.lteAttach);
    Serial.printf("    PDP:      %5lu ms\n", g_timing.ltePdp);
    Serial.printf("    TCP:      %5lu ms\n", g_timing.lteTcp);
    Serial.printf("    Send:     %5lu ms\n", g_timing.lteSend);
    Serial.printf("    Close:    %5lu ms\n", g_timing.lteClose);
    Serial.printf("  LTE Total:  %5lu ms\n", g_timing.lteTotal);
    Serial.printf("  Compact:    %5lu ms\n", g_timing.compactTime);
    Serial.println(F("  -----------------------"));
    Serial.printf("  CYCLE TOTAL: %lu ms (%.1f s)\n", 
                  g_timing.cycleTotal, g_timing.cycleTotal / 1000.0);
    Serial.println(F("============================"));
}
```

### Uso en AppController.cpp

```cpp
// Al inicio del ciclo
TIMING_RESET();

// Ejemplo en Cycle_ReadSensors:
case AppState::Cycle_ReadSensors: {
    TIMING_START(sensors);
    // ... código existente de lectura de sensores ...
    TIMING_END(sensors, g_timing.sensorsTime);
    break;
}

// Antes de sleep:
TIMING_PRINT_SUMMARY();
```

---

## 🧪 VERIFICACIÓN

### Output Esperado

```
=== CYCLE TIMING SUMMARY ===
  BLE:            0 ms
  Sensors:      234 ms
  GPS:        45230 ms
  ICCID:       1205 ms
  Build:         12 ms
  Buffer:        45 ms
  --- LTE Breakdown ---
    PowerOn:   2340 ms
    Config:    1523 ms
    Attach:    3210 ms
    PDP:       1845 ms
    TCP:       2156 ms
    Send:       432 ms
    Close:      987 ms
  LTE Total:  12493 ms
  Compact:       23 ms
  -----------------------
  CYCLE TOTAL: 59242 ms (59.2 s)
============================
```

### Criterios de Aceptación
- [ ] Archivo `src/CycleTiming.h` creado
- [ ] Flag `ENABLE_FEAT_V2_CYCLE_TIMING` en FeatureFlags.h
- [ ] Macros TIMING_* implementadas
- [ ] Todas las fases instrumentadas
- [ ] Resumen impreso antes de sleep
- [ ] Compila con flag en 0 (sin overhead)
- [ ] Compila con flag en 1 (con timing)
- [ ] Overhead < 1ms cuando deshabilitado

---

## 📊 BENEFICIOS

| Beneficio | Descripción |
|-----------|-------------|
| **Métricas reales** | Datos precisos vs estimaciones |
| **Comparación A/B** | Antes/después de optimizaciones |
| **Diagnóstico remoto** | Usuario puede enviar timing summary |
| **Detección de anomalías** | Fases que tardan más de lo normal |
| **Zero overhead** | Deshabilitado compila sin código extra |

---

## 🔗 DEPENDENCIAS

### Este FEAT depende de:
- **FEAT-V1**: FeatureFlags.h (para flag de compilación)

### Fixes que podrían beneficiarse:
- **FIX-V1**: Medir impacto real del skip reset
- Futuros fixes de timeout GPS
- Optimizaciones de LTE

---

## 📁 ARCHIVOS A CREAR/MODIFICAR

| Archivo | Acción | Descripción |
|---------|--------|-------------|
| `src/CycleTiming.h` | Crear | Struct, macros, función resumen |
| `src/FeatureFlags.h` | Modificar | Agregar ENABLE_FEAT_V2_CYCLE_TIMING |
| `AppController.cpp` | Modificar | Instrumentar cada fase |
| `src/version_info.h` | Modificar | Actualizar a v2.1.0 |

---

## 📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
| 2026-01-08 | Documento creado | - |
| - | Pendiente implementación | v2.1.0 |
