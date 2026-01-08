# Sistema de Debug Homogéneo

## Descripción

Este proyecto implementa un sistema de debug centralizado y homogéneo para todos los módulos, permitiendo control granular de los mensajes de depuración sin necesidad de modificar cada módulo individualmente.

## Configuración

El archivo `src/DebugConfig.h` contiene toda la configuración del sistema de debug.

### Control Global

```cpp
// Habilitar/deshabilitar debug en TODOS los módulos
#define DEBUG_GLOBAL_ENABLED true

// Nivel de debug (0-4)
#define DEBUG_LEVEL DEBUG_LEVEL_INFO
```

### Niveles de Debug

| Nivel | Valor | Descripción |
|-------|-------|-------------|
| `DEBUG_LEVEL_NONE` | 0 | Sin mensajes de debug |
| `DEBUG_LEVEL_ERROR` | 1 | Solo errores críticos |
| `DEBUG_LEVEL_WARNING` | 2 | Errores y advertencias |
| `DEBUG_LEVEL_INFO` | 3 | Información general (recomendado) |
| `DEBUG_LEVEL_VERBOSE` | 4 | Todo (modo detallado) |

### Control por Módulo

Puedes habilitar/deshabilitar debug individualmente:

```cpp
#define DEBUG_ADC_ENABLED    true   // Sensor ADC
#define DEBUG_I2C_ENABLED    true   // Sensor I2C
#define DEBUG_RS485_ENABLED  true   // Sensor RS485
#define DEBUG_GPS_ENABLED    true   // Módulo GPS
#define DEBUG_LTE_ENABLED    true   // Módulo LTE
#define DEBUG_BUFFER_ENABLED true   // Buffer/Archivo
#define DEBUG_BLE_ENABLED    true   // Bluetooth
#define DEBUG_FORMAT_ENABLED true   // Formateo de tramas
#define DEBUG_SLEEP_ENABLED  true   // Sleep/Wakeup
#define DEBUG_RTC_ENABLED    true   // Reloj RTC
#define DEBUG_APP_ENABLED    true   // AppController
```

## Uso en el Código

### Macros Disponibles

```cpp
// Mensaje de error
DEBUG_ERROR(MODULE, "mensaje");

// Mensaje de advertencia
DEBUG_WARN(MODULE, "mensaje");

// Mensaje informativo
DEBUG_INFO(MODULE, "mensaje");

// Mensaje verbose (detallado)
DEBUG_VERBOSE(MODULE, "mensaje");

// Mensaje con valor
DEBUG_PRINT_VALUE(MODULE, "Variable: ", valorVariable);
```

### Ejemplo de Uso

```cpp
#include "../DebugConfig.h"

void miFunction() {
    DEBUG_INFO(GPS, "Iniciando búsqueda de satélites...");
    
    if (error) {
        DEBUG_ERROR(GPS, "No se pudo obtener fix GPS");
        return;
    }
    
    DEBUG_PRINT_VALUE(GPS, "Satélites encontrados: ", numSatelites);
    DEBUG_VERBOSE(GPS, "Coordenadas validadas correctamente");
}
```

### Formato de Salida

Los mensajes incluyen etiquetas automáticas:

```
[INFO][GPS] Iniciando búsqueda de satélites...
[ERROR][GPS] No se pudo obtener fix GPS
[INFO][GPS] Satélites encontrados: 8
[VERB][GPS] Coordenadas validadas correctamente
```

## Ventajas del Sistema

✅ **Centralizado**: Una sola configuración controla todo  
✅ **Granular**: Control por módulo y nivel de detalle  
✅ **Sin overhead**: Las macros se eliminan en compilación si están deshabilitadas  
✅ **Etiquetado**: Fácil identificación del origen de cada mensaje  
✅ **Consistente**: Mismo formato en todos los módulos  
✅ **Flexible**: Cambiar configuración sin modificar código de módulos  

## Configuraciones Recomendadas

### Desarrollo/Pruebas
```cpp
#define DEBUG_GLOBAL_ENABLED true
#define DEBUG_LEVEL DEBUG_LEVEL_VERBOSE
// Todos los módulos habilitados
```

### Operación Normal
```cpp
#define DEBUG_GLOBAL_ENABLED true
#define DEBUG_LEVEL DEBUG_LEVEL_INFO
// Solo módulos críticos habilitados (LTE, GPS, APP)
```

### Producción
```cpp
#define DEBUG_GLOBAL_ENABLED false
#define DEBUG_LEVEL DEBUG_LEVEL_NONE
```

## Migración de Código Antiguo

### Antes
```cpp
Serial.println("Módulo iniciado");
Serial.print("Valor: ");
Serial.println(valor);
```

### Después
```cpp
DEBUG_INFO(MODULE, "Módulo iniciado");
DEBUG_PRINT_VALUE(MODULE, "Valor: ", valor);
```

## Módulos Ya Actualizados

- ✅ AppController
- ✅ BLEModule  
- ✅ SLEEPModule
- ✅ RTCModule
- ⚠️ LTEModule (mantiene su propio sistema `setDebug()`)
- ⚠️ GPSModule (usa defines propios)
- ⏳ Sensores (pendiente)

## Notas Importantes

1. **LTEModule** mantiene su sistema `setDebug()` por compatibilidad, pero se puede migrar
2. El sistema NO afecta el rendimiento en producción si `DEBUG_GLOBAL_ENABLED = false`
3. Los mensajes verbose solo aparecen con `DEBUG_LEVEL_VERBOSE`
4. Siempre incluir `#include "../DebugConfig.h"` en los módulos
