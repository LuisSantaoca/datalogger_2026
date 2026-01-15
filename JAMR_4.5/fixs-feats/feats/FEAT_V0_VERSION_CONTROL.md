# FEAT-V0: Sistema de Control de Versiones Centralizado

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V0 |
| **Tipo** | Feature (Infraestructura) |
| **Sistema** | Versionado |
| **Archivo Principal** | `version_info.h` |
| **Estado** | ✅ Completado |
| **Fecha Implementación** | 2026-01-07 |
| **Versión Target** | v2.0.0 |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
La versión del firmware estaba declarada en múltiples archivos (sensores_rv03.ino, AppController.h, AppController.cpp) con el tag `@version` en comentarios Doxygen.

### Síntomas
1. **Inconsistencia:** Fácil olvidar actualizar todos los archivos
2. **Sin visibilidad:** La versión no se mostraba al usuario en runtime
3. **Sin historial:** No había registro de cambios anteriores
4. **Difícil trazabilidad:** No se podía saber qué cambios correspondían a cada versión

### Causa Raíz
Falta de un sistema centralizado de versionado.

---

## 📊 EVALUACIÓN

### Impacto
| Aspecto | Evaluación |
|---------|------------|
| Criticidad | Media |
| Riesgo de no implementar | Bajo (funcional, problemas de mantenimiento) |
| Esfuerzo | Bajo |
| Beneficio | Alto (mejora mantenibilidad) |

### Justificación
Sistema básico necesario antes de implementar cualquier otro fix, permite trazabilidad de cambios.

---

## 🔧 IMPLEMENTACIÓN

### Archivos Creados

| Archivo | Propósito |
|---------|-----------|
| `version_info.h` | Archivo centralizado de versión |

### Archivos Modificados

| Archivo | Cambio | Línea |
|---------|--------|-------|
| `AppController.cpp` | Agregar `#include "version_info.h"` | 30 |
| `AppController.cpp` | Llamar `printFirmwareVersion()` en AppInit | 506 |

### Código Principal (version_info.h)

```cpp
#ifndef VERSION_INFO_H
#define VERSION_INFO_H

// Sección a modificar para cada versión:
#define FW_VERSION_STRING   "v2.0.0"
#define FW_VERSION_DATE     "2025-12-18"
#define FW_VERSION_NAME     "release-inicial"

// Función para imprimir versión
inline void printFirmwareVersion() {
    Serial.println(F("========================================"));
    Serial.print(F("[INFO] Firmware: "));
    Serial.print(FW_VERSION_STRING);
    Serial.print(F(" ("));
    Serial.print(FW_VERSION_NAME);
    Serial.println(F(")"));
    Serial.print(F("[INFO] Fecha version: "));
    Serial.println(FW_VERSION_DATE);
    Serial.print(F("[INFO] Compilado: "));
    Serial.print(__DATE__);
    Serial.print(F(" "));
    Serial.println(__TIME__);
    Serial.println(F("========================================"));
}

#endif // VERSION_INFO_H
```

### Integración en AppController.cpp

```cpp
// Línea 30 - Nuevo include
#include "version_info.h"  // FEAT-V0

// Línea 506 - En AppInit()
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

## 🧪 VERIFICACIÓN

### Output Esperado
```
========================================
[INFO] Firmware: v2.0.0 (release-inicial)
[INFO] Fecha version: 2025-12-18
[INFO] Compilado: Jan  7 2026 20:30:45
========================================
```

### Criterios de Aceptación
- [x] Versión definida en un solo archivo
- [x] Versión se imprime al iniciar el firmware
- [x] Historial de versiones documentado
- [x] Compila sin errores

---

## 📝 USO FUTURO

Para cambiar versión, editar SOLO `version_info.h`:

```cpp
// 1. Cambiar versión activa
#define FW_VERSION_STRING   "v2.0.1"
#define FW_VERSION_DATE     "2026-01-08"
#define FW_VERSION_NAME     "pdp-fix"

// 2. Agregar al historial comentado
// v2.0.1  | 2026-01-08 | pdp-fix         | FIX-V1: Reducir eventos PDP
// v2.0.0  | 2025-12-18 | release-inicial | Versión inicial
```

---

## 📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
| 2026-01-07 | Implementado | v2.0.0 |
