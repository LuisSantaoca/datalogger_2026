# Arquitectura JAMR 4.5

> **Documento de Referencia Obligatoria**  
> Versión: 1.0  
> Fecha: 2026-01-15  
> Propósito: Definir conceptos, premisas y convenciones para asegurar consistencia absoluta en todo desarrollo futuro.

---

## 1. Nombre de la Arquitectura

**"Arquitectura Modular con Controlador Central basado en FSM"**

### 1.1 Definición

Sistema donde módulos especializados e independientes son orquestados por un controlador central (`AppController`) que opera como máquina de estados finitos (FSM). Los módulos no se comunican entre sí directamente; toda coordinación pasa por el controlador.

### 1.2 Patrón de Diseño Base

- **Mediator Pattern**: `AppController` actúa como mediador central
- **FSM (Finite State Machine)**: Estados discretos con transiciones definidas
- **Segregación de Configuración**: Constantes en archivos `config_*.h` separados

---

## 2. Estructura de Carpetas

```
JAMR_4.5/
├── JAMR_4.5.ino          # Entry point (setup/loop mínimos)
├── AppController.cpp/h   # Controlador central FSM
├── src/
│   ├── FeatureFlags.h    # Banderas de features activos
│   ├── version_info.h    # Versión del firmware
│   ├── CycleTiming.h     # Tiempos de ciclo
│   ├── DebugConfig.h     # Configuración de debug
│   │
│   ├── data_lte/         # Módulo LTE/GSM
│   │   ├── config_lte.h
│   │   ├── LTEModule.h
│   │   └── LTEModule.cpp
│   │
│   ├── data_gps/         # Módulo GPS
│   │   ├── config_gps.h
│   │   ├── GPSModule.h
│   │   └── GPSModule.cpp
│   │
│   ├── data_sensors/     # Módulo Sensores
│   │   ├── config_sensors.h
│   │   ├── ADCSensorModule.h/cpp
│   │   ├── I2CSensorModule.h/cpp
│   │   └── RS485Module.h/cpp
│   │
│   ├── data_buffer/      # Módulo Buffer/Almacenamiento
│   │   ├── config_data_buffer.h
│   │   ├── BUFFERModule.h/cpp
│   │   └── FileManager.h/cpp
│   │
│   ├── data_diagnostics/ # Módulo Diagnósticos
│   │   ├── config_crash_diagnostics.h
│   │   ├── CrashDiagnostics.h
│   │   └── CrashDiagnostics.cpp
│   │
│   └── data_*/           # Patrón para nuevos módulos
```

### 2.1 Regla de Carpetas

| Prefijo | Contenido |
|---------|-----------|
| `data_*` | Módulos funcionales del sistema |
| `config_*.h` | Constantes y configuración del módulo |
| `*Module.h/cpp` | Implementación OOP del módulo |

---

## 3. Paradigma de Programación

### 3.1 Obligatorio: Programación Orientada a Objetos (OOP)

Todos los módulos **DEBEN** implementarse como clases C++.

#### Plantilla de Clase Módulo

```cpp
/**
 * @file NombreModule.h
 * @brief Descripción breve del módulo
 * @version 1.0.0
 */

#ifndef NOMBRE_MODULE_H
#define NOMBRE_MODULE_H

#include <Arduino.h>
#include "config_nombre.h"

/**
 * @class NombreModule
 * @brief Descripción de la responsabilidad del módulo
 */
class NombreModule {
public:
    /**
     * @brief Inicializa el módulo
     * @return true si inicialización exitosa
     */
    bool begin();
    
    /**
     * @brief Ejecuta operación principal
     * @param param Descripción del parámetro
     * @return Descripción del retorno
     */
    bool execute(int param);
    
    /**
     * @brief Libera recursos del módulo
     */
    void end();

private:
    bool _initialized = false;
    // Variables de estado internas
};

#endif // NOMBRE_MODULE_H
```

### 3.2 Prohibido

| ❌ Prohibido | ✅ Alternativa |
|-------------|----------------|
| `namespace` para módulos | `class` con métodos |
| Variables globales sueltas | Miembros privados de clase |
| Funciones libres | Métodos de clase |
| `#define` para config | `constexpr` en config_*.h |

### 3.3 Excepciones Documentadas

#### Excepción 1: Variables RTC_DATA_ATTR

`RTC_DATA_ATTR` para persistencia en deep sleep - estas variables deben ser estáticas a nivel de archivo en el .cpp, nunca expuestas en el .h.

#### Excepción 2: Módulos con RTC Memory (CrashDiagnostics)

**Justificación técnica**: `RTC_DATA_ATTR` solo funciona con variables estáticas/globales. No puede ser miembro de una clase porque RTC memory requiere direcciones fijas en tiempo de compilación.

**Patrón permitido**:
```cpp
// En .cpp (OBLIGATORIO - restricción de ESP32)
RTC_DATA_ATTR static CrashContext g_crash_ctx;

// En .h - Se permite namespace en lugar de clase
namespace CrashDiag {
    bool init();
    void setCheckpoint(CrashCheckpoint cp);
    // ...
}
```

**Aplica a**: `src/data_diagnostics/CrashDiagnostics.h/cpp`

**Razón**: Convertir a clase sería un "wrapper cosmético" - los métodos igual accederían a la variable global RTC. Documentar la excepción es más honesto técnicamente.

---

## 4. Documentación con Doxygen

### 4.1 Obligatorio en Todo Archivo

```cpp
/**
 * @file nombre_archivo.cpp
 * @brief Descripción breve del archivo
 * @version X.Y.Z
 * 
 * @details
 * Descripción detallada si es necesaria.
 */
```

### 4.2 Obligatorio en Toda Función/Método Público

```cpp
/**
 * @brief Descripción de qué hace la función
 * @param nombre_param Descripción del parámetro
 * @return Descripción de lo que retorna
 * 
 * @note Notas importantes (opcional)
 * @warning Advertencias (opcional)
 */
```

### 4.3 Tags Doxygen Permitidos

| Tag | Uso |
|-----|-----|
| `@file` | Inicio de archivo |
| `@brief` | Descripción corta |
| `@details` | Descripción extendida |
| `@param` | Parámetro de función |
| `@return` | Valor de retorno |
| `@note` | Nota informativa |
| `@warning` | Advertencia importante |
| `@see` | Referencia cruzada |
| `@version` | Versión del componente |
| `@class` | Documentación de clase |

---

## 5. Convenciones de Nomenclatura

### 5.1 Archivos

| Tipo | Formato | Ejemplo |
|------|---------|---------|
| Módulo Header | `NombreModule.h` | `LTEModule.h` |
| Módulo Source | `NombreModule.cpp` | `LTEModule.cpp` |
| Configuración | `config_nombre.h` | `config_lte.h` |
| Feature/Fix Doc | `TIPO_VX_NOMBRE.md` | `FEAT_V3_CRASH.md` |

### 5.2 Clases y Tipos

| Tipo | Formato | Ejemplo |
|------|---------|---------|
| Clase | `PascalCase` + `Module` | `GPSModule` |
| Enum | `PascalCase` | `ModemState` |
| Enum Values | `UPPER_SNAKE` | `STATE_IDLE` |
| Struct | `PascalCase` | `CrashContext` |

### 5.3 Variables y Funciones

| Tipo | Formato | Ejemplo |
|------|---------|---------|
| Método público | `camelCase` | `sendData()` |
| Método privado | `_camelCase` | `_parseResponse()` |
| Variable miembro | `_camelCase` | `_isConnected` |
| Constante | `UPPER_SNAKE` | `MAX_RETRIES` |
| Variable local | `camelCase` | `retryCount` |

### 5.4 Macros y Constantes

```cpp
// En config_*.h
constexpr uint32_t LTE_TIMEOUT_MS = 30000;
constexpr uint8_t  LTE_MAX_RETRIES = 3;
constexpr char     LTE_APN[] = "internet";

// Feature flags (excepción: usar #define para compilación condicional)
#define ENABLE_FEAT_V3_CRASH_DIAGNOSTICS 1
```

---

## 6. AppController: El Controlador Central

### 6.1 Responsabilidades

1. **Orquestación**: Coordina secuencia de operaciones entre módulos
2. **FSM**: Mantiene y transiciona entre estados del sistema
3. **Mediación**: Único punto de comunicación entre módulos
4. **Ciclo de Vida**: Controla inicialización, ejecución y shutdown

### 6.2 Estados del Sistema

```
INIT → SENSORS → GPS → CONNECT → SEND → SLEEP → (repeat)
         ↓         ↓       ↓        ↓
       ERROR ← ← ← ← ← ← ← ← ← ← ← ←
```

### 6.3 Regla de Comunicación

```
┌─────────┐     ┌─────────────────┐     ┌─────────┐
│ Módulo  │ ──► │  AppController  │ ◄── │ Módulo  │
│   A     │     │   (Mediador)    │     │   B     │
└─────────┘     └─────────────────┘     └─────────┘
     ▲                   │                   ▲
     └───────────────────┴───────────────────┘
           Los módulos NUNCA se llaman entre sí
```

---

## 7. Feature Flags

### 7.1 Ubicación

Todas las banderas en `src/FeatureFlags.h`

### 7.2 Formato

```cpp
// Formato: ENABLE_<TIPO>_V<NUM>_<NOMBRE>
#define ENABLE_FEAT_V3_CRASH_DIAGNOSTICS  1
#define ENABLE_FIX_V2_FALLBACK_OPERATOR   1

// Uso en código
#if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
    #include "src/data_diagnostics/CrashDiagnostics.h"
#endif
```

### 7.3 Función de Reporte

Toda feature flag debe registrarse en `printActiveFlags()`:

```cpp
void printActiveFlags() {
    Serial.println(F("=== Feature Flags Activos ==="));
    #if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
        Serial.println(F("[X] FEAT-V3: Crash Diagnostics"));
    #endif
    // ... más flags
}
```

---

## 8. Persistencia de Datos

### 8.1 Jerarquía de Almacenamiento

| Nivel | Mecanismo | Sobrevive | Velocidad | Uso |
|-------|-----------|-----------|-----------|-----|
| 1 | `RTC_DATA_ATTR` | Deep Sleep, WDT | Muy rápida | Contexto crítico |
| 2 | NVS (Preferences) | Brownout, reboot | Rápida | Configuración |
| 3 | LittleFS | Todo | Media | Historial, logs |

### 8.2 Regla de Prioridad de Lectura

```
Al iniciar: RTC → NVS → LittleFS
(Primera fuente válida gana)
```

### 8.3 Regla de Escritura

```
Siempre escribir en TODOS los niveles disponibles
(Redundancia para máxima confiabilidad)
```

---

## 9. Manejo de Errores

### 9.1 Patrón de Retorno

```cpp
// Métodos que pueden fallar retornan bool
bool LTEModule::connect() {
    if (!_initialized) return false;
    // ... operación
    return success;
}

// AppController maneja el resultado
if (!lte.connect()) {
    _handleError(ErrorType::LTE_CONNECTION);
}
```

### 9.2 Logging de Errores

```cpp
// Usar Serial con prefijo de módulo
Serial.println(F("[LTE] ERROR: Connection timeout"));
Serial.println(F("[GPS] WARN: Low signal"));
Serial.println(F("[SYS] INFO: Entering sleep mode"));
```

### 9.3 Prefijos de Log

| Prefijo | Significado |
|---------|-------------|
| `[XXX] ERROR:` | Error crítico |
| `[XXX] WARN:` | Advertencia |
| `[XXX] INFO:` | Información |
| `[XXX] DEBUG:` | Solo en modo debug |

---

## 10. Versionado

### 10.1 Formato Semántico

```
MAJOR.MINOR.PATCH
  │     │     └── Correcciones (FIX)
  │     └──────── Nuevas funcionalidades (FEAT)
  └────────────── Cambios incompatibles
```

### 10.2 Ubicación

```cpp
// En src/version_info.h
#define FIRMWARE_VERSION "4.5.0"
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__
```

---

## 11. Proceso de Desarrollo

### 11.1 Para Nuevas Features

1. Crear documento `FEAT_VX_NOMBRE.md` en `fixs-feats/feats/`
2. Definir requisitos, alcance, DoD
3. Implementar siguiendo esta arquitectura
4. Agregar feature flag en `FeatureFlags.h`
5. Documentar con Doxygen
6. Probar y validar

### 11.2 Para Correcciones (Fixes)

1. Crear documento `FIX_VX_NOMBRE.md` en `fixs-feats/fixs/`
2. Identificar causa raíz
3. Implementar corrección mínima
4. Agregar flag si es condicional
5. Probar regresiones

---

## 12. Checklist de Consistencia

Antes de considerar cualquier código como completo:

- [ ] ¿Usa clase en lugar de namespace/funciones libres?
- [ ] ¿Tiene documentación Doxygen completa?
- [ ] ¿Configuración en archivo `config_*.h` separado?
- [ ] ¿Nomenclatura sigue convenciones?
- [ ] ¿Comunicación solo via AppController?
- [ ] ¿Feature flag agregado si aplica?
- [ ] ¿Manejo de errores con retorno bool?
- [ ] ¿Logging con prefijo de módulo?

---

## 13. Anexo: Hardware de Referencia

| Componente | Modelo | Notas |
|------------|--------|-------|
| MCU | ESP32-S3 | Dual core, WiFi/BLE |
| Módem | SIM7080G | LTE Cat-M1/NB-IoT + GNSS |
| Almacenamiento | Flash interno | LittleFS |
| Sensores | ADC/I2C/RS485 | Agrícolas |

---

> **Nota Final**: Este documento es la fuente de verdad para decisiones arquitectónicas.  
> Cualquier desviación debe documentarse y justificarse explícitamente.
