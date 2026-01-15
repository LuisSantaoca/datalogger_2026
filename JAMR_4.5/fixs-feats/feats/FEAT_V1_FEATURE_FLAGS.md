# FEAT-V1: Sistema de Feature Flags

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V1 |
| **Tipo** | Feature (Infraestructura) |
| **Sistema** | Configuración de Compilación |
| **Archivo Principal** | `src/FeatureFlags.h` |
| **Estado** | ✅ Completado |
| **Fecha Propuesta** | 2026-01-07 |
| **Fecha Implementación** | 2026-01-07 |
| **Versión** | v2.0.1 |
| **Depende de** | FEAT-V0 |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
- Los fixes se implementan directamente en el código
- Para revertir un fix hay que modificar múltiples archivos
- No hay forma rápida de probar versiones con/sin ciertos fixes
- Difícil saber qué correcciones están activas en un binario

### Síntomas
1. Rollback de fixes requiere modificar código fuente
2. No hay trazabilidad de qué fixes están activos
3. Testing A/B imposible sin múltiples binarios
4. Riesgo de introducir bugs al revertir cambios manualmente

### Causa Raíz
Falta de sistema de compilación condicional centralizado.

---

## 📊 EVALUACIÓN

### Impacto
| Aspecto | Evaluación |
|---------|------------|
| Criticidad | Media-Alta |
| Riesgo de no implementar | Medio (dificulta rollback y testing) |
| Esfuerzo | Bajo |
| Beneficio | Alto (infraestructura para todos los fixes) |

### Justificación
Prerequisito para implementar FIX-V1 y futuros fixes de forma segura. Permite rollback instantáneo cambiando un `1` por un `0`.

### Principio JAMR_4.4 Aplicado
> "Agregar, no reemplazar" - El código original se mantiene, activable via flag.

---

## 🔧 IMPLEMENTACIÓN

### Archivos a Crear

| Archivo | Propósito |
|---------|-----------|
| `FeatureFlags.h` | Banderas de compilación condicional |

### Archivos a Modificar

| Archivo | Cambio | Línea |
|---------|--------|-------|
| `AppController.cpp` | Agregar `#include "FeatureFlags.h"` | 31 |
| `AppController.cpp` | Llamar `printActiveFlags()` | 508 |

### Código Principal (FeatureFlags.h)

```cpp
#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

// ============================================================
// FEATURE FLAGS - Control de Compilación Condicional
// ============================================================
// Este archivo centraliza todas las banderas para habilitar
// o deshabilitar fixes y features en tiempo de compilación.
//
// USO:
//   1 = Habilitado (código nuevo activo)
//   0 = Deshabilitado (comportamiento original)
//
// CONVENCIÓN DE NOMBRES:
//   ENABLE_FIX_Vn_DESCRIPCION  - Para correcciones de bugs
//   ENABLE_FEAT_Vn_DESCRIPCION - Para nuevas funcionalidades
// ============================================================

// ------------------------------------------------------------
// FIX-V1: Skip Reset en PDP
// Sistema: LTE/Modem
// Archivo: LTEModule.cpp
// Descripción: Evita resetModem() cuando ya hay operadora guardada
// ------------------------------------------------------------
#define ENABLE_FIX_V1_SKIP_RESET_PDP    1

// ------------------------------------------------------------
// FIX-V2: [Reservado]
// ------------------------------------------------------------
#define ENABLE_FIX_V2_PLACEHOLDER       0

// ------------------------------------------------------------
// FIX-V3: [Reservado]
// ------------------------------------------------------------
#define ENABLE_FIX_V3_PLACEHOLDER       0

// ============================================================
// FUNCIÓN DE DEBUG: Imprimir flags activos
// ============================================================
inline void printActiveFlags() {
    Serial.println(F("=== FEATURE FLAGS ACTIVOS ==="));
    
    #if ENABLE_FIX_V1_SKIP_RESET_PDP
    Serial.println(F("  [X] FIX-V1: Skip Reset PDP"));
    #else
    Serial.println(F("  [ ] FIX-V1: Skip Reset PDP"));
    #endif
    
    #if ENABLE_FIX_V2_PLACEHOLDER
    Serial.println(F("  [X] FIX-V2: Placeholder"));
    #else
    Serial.println(F("  [ ] FIX-V2: Placeholder"));
    #endif
    
    #if ENABLE_FIX_V3_PLACEHOLDER
    Serial.println(F("  [X] FIX-V3: Placeholder"));
    #else
    Serial.println(F("  [ ] FIX-V3: Placeholder"));
    #endif
    
    Serial.println(F("============================="));
}

#endif // FEATURE_FLAGS_H
```

### Integración en AppController.cpp

```cpp
// Línea 31 - Nuevo include
#include "FeatureFlags.h"  // FEAT-V1

// Línea 508 - En AppInit(), después de printFirmwareVersion()
// ============ [FEAT-V1 START] Imprimir flags activos ============
printActiveFlags();
// ============ [FEAT-V1 END] ============
```

---

## 🔧 USO EN CÓDIGO (Patrón)

### Para implementar un fix con flag:

```cpp
#include "FeatureFlags.h"

void algunaFuncion() {
    // ... código existente ...
    
    #if ENABLE_FIX_V1_SKIP_RESET_PDP
    // [FIX-V1 START]
    // Código nuevo del fix
    // [FIX-V1 END]
    #else
    // Código original (comportamiento legacy)
    #endif
    
    // ... más código ...
}
```

### Ejemplo Concreto (FIX-V1 en LTEModule.cpp):

```cpp
#if ENABLE_FIX_V1_SKIP_RESET_PDP
// [FIX-V1 START]
if (!skipReset) {
    resetModem();
} else {
    debugPrint("Saltando reset del modem (skipReset=true)");
}
// [FIX-V1 END]
#else
resetModem();  // Comportamiento original
#endif
```

---

## 🧪 VERIFICACIÓN

### Output Esperado en Boot
```
========================================
[INFO] Firmware: v2.0.1 (feature-flags)
[INFO] Fecha version: 2026-01-07
[INFO] Compilado: Jan  7 2026 21:00:00
========================================
=== FEATURE FLAGS ACTIVOS ===
  [X] FIX-V1: Skip Reset PDP
  [ ] FIX-V2: Placeholder
  [ ] FIX-V3: Placeholder
=============================
```

### Criterios de Aceptación
- [ ] Archivo `FeatureFlags.h` creado en raíz de `sensores_rv03/`
- [ ] Flag `ENABLE_FIX_V1_SKIP_RESET_PDP` definido
- [ ] Función `printActiveFlags()` implementada
- [ ] Llamada a `printActiveFlags()` en boot (AppController)
- [ ] Compila sin errores con flags en 0 y en 1

---

## 📊 BENEFICIOS

| Beneficio | Descripción |
|-----------|-------------|
| **Rollback Instantáneo** | Cambiar `1` a `0` y recompilar |
| **Testing Selectivo** | Probar fixes individualmente |
| **Trazabilidad** | `printActiveFlags()` muestra qué está activo |
| **Zero Runtime Cost** | Compilación condicional, no if/else en runtime |
| **Documentación Integrada** | Cada flag tiene su descripción en el header |

---

## 🔗 DEPENDENCIAS

### Este FEAT depende de:
- **FEAT-V0**: `version_info.h` (para versionado coherente)

### Fixes que dependen de este FEAT:
- **FIX-V1**: PDP Redundante
- **FIX-V2+**: Futuros fixes

---

## 📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
| 2026-01-07 | Documento creado | - |
| - | Pendiente implementación | v2.0.1 |
