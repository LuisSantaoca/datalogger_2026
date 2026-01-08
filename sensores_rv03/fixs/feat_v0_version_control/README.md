# FEAT-V0: Sistema de Control de Versiones Centralizado

**Versión objetivo:** v2.0.0 (mejora interna)  
**Fecha de implementación:** 2026-01-07  
**Estado:** Completado  
**Prioridad:** Media  

---

## Resumen

Implementación de un sistema centralizado de control de versiones que permite:
- Modificar la versión en un solo archivo
- Imprimir automáticamente la versión al iniciar el firmware
- Mantener historial de cambios documentado
- Facilitar trazabilidad de modificaciones

---

## Problema Identificado

La versión del firmware estaba declarada en múltiples archivos (sensores_rv03.ino, AppController.h, AppController.cpp) con el tag `@version` en comentarios Doxygen. Esto causaba:

1. **Inconsistencia:** Fácil olvidar actualizar todos los archivos
2. **Sin visibilidad:** La versión no se mostraba al usuario en runtime
3. **Sin historial:** No había registro de cambios anteriores
4. **Difícil trazabilidad:** No se podía saber qué cambios correspondían a cada versión

---

## Solución Implementada

### Archivos Creados

| Archivo | Propósito |
|---------|-----------|
| `version_info.h` | Archivo centralizado de versión |
| `fixs/PLANTILLA_DOCUMENTACION.md` | Guía para documentar futuros cambios |
| `fixs/feat_v0_version_control/README.md` | Esta documentación |

### Archivos Modificados

| Archivo | Cambio | Línea |
|---------|--------|-------|
| `AppController.cpp` | Agregar `#include "version_info.h"` | 30 |
| `AppController.cpp` | Llamar `printFirmwareVersion()` en AppInit | 506 |

---

## Implementación

### version_info.h (Nuevo)

```cpp
// Sección a modificar para cada versión:
#define FW_VERSION_STRING   "v2.0.0"
#define FW_VERSION_DATE     "2025-12-18"
#define FW_VERSION_NAME     "release-inicial"

// Historial de versiones comentado
// Función printFirmwareVersion() para imprimir en Serial
```

### AppController.cpp (Modificaciones)

```cpp
// Línea 30 - Nuevo include
#include "version_info.h"  // FEAT-V0

// Línea 506 - Llamada a imprimir versión
void AppInit(const AppConfig& cfg) {
  ...
  Serial.begin(115200);
  delay(200);

  // ============ [FEAT-V0 START] Imprimir versión al iniciar ============
  printFirmwareVersion();
  // ============ [FEAT-V0 END] ============
  ...
}
```

---

## Output Esperado

Al iniciar el firmware:

```
========================================
[INFO] Firmware: v2.0.0 (release-inicial)
[INFO] Fecha version: 2025-12-18
[INFO] Compilado: Jan  7 2026 20:30:45
========================================
```

---

## Criterios de Aceptación

- [x] Versión definida en un solo archivo
- [x] Versión se imprime al iniciar el firmware
- [x] Historial de versiones documentado
- [x] Plantilla para documentar futuros cambios
- [x] Compila sin errores

---

## Uso Futuro

Para cambiar versión, editar SOLO `version_info.h`:

```cpp
// 1. Cambiar versión activa
#define FW_VERSION_STRING   "v2.0.1"
#define FW_VERSION_DATE     "2026-01-08"
#define FW_VERSION_NAME     "pdp-fix"

// 2. Agregar al historial (arriba de la anterior)
// v2.0.1  | 2026-01-08 | pdp-fix         | FIX-V1: Reducir eventos PDP
// v2.0.0  | 2025-12-18 | release-inicial | Versión inicial
```
