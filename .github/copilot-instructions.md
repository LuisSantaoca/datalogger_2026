# Instrucciones Copilot - Datalogger ESP32-S3 (JAMR_4.5)

## Resumen del Proyecto

Firmware de datalogger agrícola ESP32-S3 con módem LTE SIM7080G. Usa **arquitectura FSM** con `AppController` central mediando todos los módulos. Versión activa: `JAMR_4.5/` (v2.5.0+).

## Arquitectura (Patrones Obligatorios)

### Patrón Mediador con FSM
- `AppController.cpp` es el **orquestador central** - los módulos NUNCA se comunican directamente
- Estados FSM: `INIT → SENSORS → GPS → CONNECT → SEND → SLEEP → (repetir)`
- Toda coordinación pasa por AppController

### Estructura de Módulos
```
src/data_<nombre>/
  ├── config_<nombre>.h     # Constantes (usar constexpr, NO #define)
  ├── <Nombre>Module.h      # Header con Doxygen
  └── <Nombre>Module.cpp    # Implementación
```

**Patrón de clase requerido:**
```cpp
class NombreModule {
public:
    bool begin();           // Inicializar
    bool execute(...);      // Operación principal (retorna éxito/fallo)
    void end();             // Limpieza
private:
    bool _initialized = false;
    // Miembros privados usan _camelCase
};
```

### Excepción RTC_DATA_ATTR
Variables que persisten en deep sleep **deben** ser estáticas a nivel de archivo en `.cpp`:
```cpp
// ✅ Correcto (en archivo .cpp)
RTC_DATA_ATTR static bool g_restMode = false;

// ❌ Incorrecto - no pueden ser miembros de clase (limitación hardware ESP32)
```

## Sistema de Feature Flags

**Ubicación:** [JAMR_4.5/src/FeatureFlags.h](JAMR_4.5/src/FeatureFlags.h)

```cpp
// Nomenclatura: ENABLE_FIX_Vn_DESCRIPCION o ENABLE_FEAT_Vn_DESCRIPCION
#define ENABLE_FIX_V3_LOW_BATTERY_MODE  1  // 1=habilitado, 0=rollback

#if ENABLE_FIX_V3_LOW_BATTERY_MODE
    // Código nuevo
#else
    // Comportamiento original (siempre preservar)
#endif
```

**Cada fix/feature debe:**
1. Tener un flag en FeatureFlags.h
2. Preservar código original en bloque `#else` para rollback instantáneo
3. Registrarse en función `printActiveFlags()`

## Protocolo de Modificación de Código

### Marcadores de Comentarios (OBLIGATORIO)
```cpp
// Cambio de línea única
bool skipReset = false;  // FIX-V1

// Cambios en bloque (3+ líneas)
// ============ [FIX-V1 START] Descripción ============
codigo();
// ============ [FIX-V1 END] ============
```

### Documentación por Cambio
Crear archivo markdown en:
- Fixes: `JAMR_4.5/fixs-feats/fixs/FIX_Vn_NOMBRE.md`
- Features: `JAMR_4.5/fixs-feats/feats/FEAT_Vn_NOMBRE.md`

## Principios de Desarrollo (PREMISAS)

1. **Defaults Seguros** - Si el fix falla, dispositivo se comporta como versión estable anterior
2. **Superficie Mínima** - Tocar solo archivos relacionados con el fix
3. **Aditivo, No Reemplazo** - Preservar código funcional, agregar lógica nueva
4. **Reversible** - Rollback en <5 minutos vía feature flag
5. **Logging Exhaustivo** - Formato: `[NIVEL][MODULO] Mensaje` (ej: `[INFO][APP]`, `[WARN][LTE]`)

## Flujo de Trabajo para Cambios

1. **Documentar** - Crear `FIX_Vn_NOMBRE.md` o `FEAT_Vn_NOMBRE.md` en `fixs-feats/`
2. **Branch** - `git checkout -b fix-vN/nombre` o `feat-vN/nombre`
3. **Flag** - Agregar en `FeatureFlags.h` con código original en `#else`
4. **Implementar** - Usar marcadores `[FIX-Vn START/END]`
5. **Versión** - Actualizar `version_info.h` con historial de cambios

## Directorios Clave

| Ruta | Propósito |
|------|-----------|
| `JAMR_4.5/` | Firmware activo (trabajar aquí) |
| `JAMR_4.4/` | Referencia legacy (solo lectura) |
| `memoria_de_proyecto/` | Memoria del proyecto, docs arquitectura |
| `datasheets/` | Specs hardware, PCB, BOM |

## Convenciones de Nomenclatura

| Tipo | Formato | Ejemplo |
|------|---------|---------|
| Clase | `PascalCase` + `Module` | `LTEModule` |
| Método público | `camelCase` | `sendData()` |
| Miembro privado | `_camelCase` | `_isConnected` |
| Constante | `UPPER_SNAKE` | `MAX_RETRIES` |
| Archivo config | `config_<nombre>.h` | `config_lte.h` |

## Contexto de Hardware

- **MCU:** ESP32-S3-WROOM-1-N8 (dual core, deep sleep ~10µA)
- **Módem:** SIM7080G (LTE Cat-M1/NB-IoT + GNSS integrado)
- **RTC:** DS3231MZ+ (I²C, batería CR1220)
- **Almacenamiento:** LittleFS (buffer), NVS (config), memoria RTC (estado)
- **Sensores:** ADC (batería), I2C (temp/hum), RS485 Modbus (industriales)
- **Reguladores:** LM2596S-5.0 (entrada), TLV62569 (3.3V)

## Formato de Trama

```
$,<iccid>,<epoch>,<lat>,<lng>,<alt>,<var1-7>#  → Base64 → envío TCP
```
Variables: var1-4=RS485 Modbus, var5=temp, var6=hum, var7=vBat

## Selección de Operadora

Score = `(4 × SINR) + 2 × (RSRP + 120) + (RSRQ + 20)`  
Operadoras: Telcel, AT&T (×2), Movistar, Altan  
Persistencia: NVS namespace `"sensores"`, key `"lastOperator"`
- **Almacenamiento:** LittleFS (buffer), NVS (config), memoria RTC (estado)
- **Sensores:** ADC, I2C, RS485 Modbus

## Compilación

**Herramienta:** Arduino IDE con soporte ESP32  
**Board:** ESP32-S3 Dev Module

```
Archivo → Preferencias → URLs adicionales:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

Herramientas → Placa → ESP32 Arduino → ESP32S3 Dev Module
Herramientas → Puerto → COM<X>
```

**Verificar:** Ctrl+R (o Sketch → Verificar/Compilar)  
**Subir:** Ctrl+U (o Sketch → Subir)

## Reglas Críticas

1. **Nunca saltarse AppController** - módulos no deben llamarse entre sí
2. **Siempre usar Doxygen** - `@file`, `@brief`, `@param`, `@return` en APIs públicas
3. **Preservar código original** - usar bloques `#else`, nunca borrar lógica funcional
4. **Probar flags individualmente** - un flag = un cambio de comportamiento
5. **Logs con prefijos** - `[LTE] ERROR:`, `[GPS] INFO:`, `[SYS] WARN:`
