# Metodología de Documentación de Cambios

**Versión:** 1.0  
**Fecha:** 2026-01-07  
**Proyecto:** sensores_rv03  

---

## 📋 RESUMEN

Este documento define las convenciones para documentar cambios en el código del firmware, permitiendo:
- Trazabilidad de cada modificación
- Rollback rápido mediante feature flags
- Búsqueda eficiente de cambios por ID
- Mantenimiento de código legacy mientras se prueba código nuevo

---

## 🏷️ NOMENCLATURA

### Tipos de Cambios

| Prefijo | Uso | Ejemplo |
|---------|-----|---------|
| `FEAT-Vn` | Nueva funcionalidad o infraestructura | `FEAT-V1` Feature Flags |
| `FIX-Vn` | Corrección de bug u optimización | `FIX-V1` PDP Redundante |
| `REFACTOR-Vn` | Reestructuración sin cambio funcional | `REFACTOR-V1` Modularización |
| `DOCS-Vn` | Solo documentación | `DOCS-V1` README |

### Archivos de Documentación

| Tipo | Formato de Archivo | Ubicación |
|------|-------------------|-----------|
| Feature | `FEAT_Vn_NOMBRE.md` | `fixs-feats/feats/` |
| Fix | `FIX_Vn_NOMBRE.md` | `fixs-feats/fixs/` |

---

## 📝 COMENTARIOS EN CÓDIGO

### 1. Include Nuevo

```cpp
#include "version_info.h"   // FEAT-V0
#include "FeatureFlags.h"   // FEAT-V1
```

### 2. Línea Individual Nueva o Modificada

```cpp
bool skipReset = false;  // FIX-V1
```

```cpp
bool configureOperator(Operadora op, bool skipReset = false);  // FIX-V1
```

### 3. Parámetro Nuevo en Función (con explicación)

```cpp
// FIX-V1: Agregar parámetro skipReset para evitar reset innecesario
bool configureOperator(Operadora operadora, bool skipReset = false);  // FIX-V1
```

### 4. Bloque de Código Nuevo (3+ líneas)

```cpp
// ============ [FEAT-V1 START] Imprimir flags activos ============
printActiveFlags();
Serial.println("Sistema inicializado");
// ============ [FEAT-V1 END] ============
```

### 5. Bloque con Compilación Condicional (Feature Flag)

```cpp
#if ENABLE_FIX_V1_SKIP_RESET_PDP
// [FIX-V1 START] Skip reset cuando hay operadora guardada
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

### 6. Archivo Nuevo Completo

```cpp
/**
 * @file FeatureFlags.h
 * @brief Sistema de Feature Flags para compilación condicional
 * @version FEAT-V1
 * @date 2026-01-07
 * 
 * Este archivo centraliza las banderas de compilación condicional
 * para habilitar/deshabilitar fixes y features.
 */
#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

// ... contenido ...

#endif // FEATURE_FLAGS_H
```

---

## 🎯 FEATURE FLAGS

### Propósito
Permitir habilitar/deshabilitar cambios en tiempo de compilación sin modificar código.

### Ubicación
`sensores_rv03/FeatureFlags.h`

### Convención de Nombres

```cpp
// Para fixes
#define ENABLE_FIX_Vn_DESCRIPCION_CORTA    1

// Para features
#define ENABLE_FEAT_Vn_DESCRIPCION_CORTA   1
```

### Uso

```cpp
#define ENABLE_FIX_V1_SKIP_RESET_PDP    1  // 1=Activo, 0=Desactivo

// En el código:
#if ENABLE_FIX_V1_SKIP_RESET_PDP
    // Código nuevo
#else
    // Código original (se mantiene para rollback)
#endif
```

### Valores

| Valor | Significado |
|-------|-------------|
| `1` | Flag activo - usa código nuevo |
| `0` | Flag desactivo - usa código original |

---

## 📁 ESTRUCTURA DE ARCHIVOS

```
sensores_rv03/
├── FeatureFlags.h              ← Flags de compilación (FEAT-V1)
├── version_info.h              ← Control de versiones (FEAT-V0)
├── AppController.cpp           ← Consumidor de flags
├── src/
│   └── data_lte/
│       ├── LTEModule.h         ← Declaraciones
│       └── LTEModule.cpp       ← Implementación con flags
└── fixs-feats/
    ├── METODOLOGIA_DE_CAMBIOS.md   ← Este documento
    ├── PLANTILLA.md                ← Plantilla para documentar
    ├── feats/
    │   ├── FEAT_V0_VERSION_CONTROL.md
    │   └── FEAT_V1_FEATURE_FLAGS.md
    └── fixs/
        └── FIX_V1_PDP_REDUNDANTE.md
```

---

## 🔄 FLUJO DE TRABAJO

### 1. Identificar y Documentar
```
1. Identificar problema o feature
2. Crear archivo en fixs-feats/fixs/ o fixs-feats/feats/
3. Completar secciones: Diagnóstico, Evaluación, Implementación
4. Estado: 📋 Propuesto
```

### 2. Crear Branch
```bash
git checkout -b feat-vN/nombre-descriptivo
# o
git checkout -b fix-vN/nombre-descriptivo
```

### 3. Implementar
```
1. Agregar flag en FeatureFlags.h (si aplica)
2. Implementar cambios con marcadores [TIPO-Vn]
3. Usar #if ENABLE_... para código condicional
4. Mantener código original en #else
```

### 4. Actualizar Versión
```cpp
// En version_info.h:
#define FW_VERSION_STRING   "v2.0.1"
#define FW_VERSION_DATE     "2026-01-07"
#define FW_VERSION_NAME     "feature-flags-pdp-fix"
```

### 5. Actualizar Documentación
```
1. Cambiar estado a ✅ Completado en el archivo .md
2. Agregar fecha de implementación
```

### 6. Commit y Push
```bash
git add -A
git commit -m "[FEAT-V1] Implementar sistema de feature flags"
git push origin feat-v1/nombre-descriptivo
```

### 7. Merge (cuando esté probado)
```bash
git checkout main
git merge feat-v1/nombre-descriptivo
git push origin main
```

---

## 🔍 BÚSQUEDA DE CAMBIOS

### Por ID específico
```bash
grep -rn "FIX-V1" sensores_rv03/
grep -rn "FEAT-V1" sensores_rv03/
```

### Todos los fixes
```bash
grep -rn "FIX-V[0-9]" sensores_rv03/
```

### Todos los features
```bash
grep -rn "FEAT-V[0-9]" sensores_rv03/
```

### Bloques START/END
```bash
grep -rn "\[.*START\]\|\[.*END\]" sensores_rv03/
```

### Flags activos
```bash
grep -n "ENABLE_.*1$" sensores_rv03/FeatureFlags.h
```

---

## 📊 ESTADOS DE CAMBIOS

| Emoji | Estado | Descripción |
|-------|--------|-------------|
| 📋 | Propuesto | Documentado, pendiente implementación |
| 🔧 | En progreso | Implementación iniciada |
| ✅ | Completado | Implementado y verificado |
| ❌ | Descartado | No se implementará |

---

## ✅ CHECKLIST DE IMPLEMENTACIÓN

- [ ] Archivo de documentación creado en `fixs-feats/`
- [ ] Branch creado con nombre descriptivo
- [ ] Flag agregado en `FeatureFlags.h` (si aplica)
- [ ] Código implementado con marcadores `[TIPO-Vn]`
- [ ] Código original preservado en `#else`
- [ ] `version_info.h` actualizado
- [ ] Estado actualizado a ✅ en documentación
- [ ] Compila sin errores con flag en `1`
- [ ] Compila sin errores con flag en `0`
- [ ] Commit con prefijo `[TIPO-Vn]`
- [ ] Push al branch

---

## 🛡️ PRINCIPIOS

1. **Agregar, no reemplazar**: El código original siempre se mantiene en `#else`
2. **Rollback instantáneo**: Cambiar `1` a `0` revierte al comportamiento original
3. **Trazabilidad total**: Cada línea modificada tiene su marcador
4. **Documentación junto al código**: Los comentarios explican el "qué" y "por qué"
5. **Un cambio = Un ID**: No mezclar múltiples fixes en un solo marcador

---

## 📚 EJEMPLO COMPLETO

### Archivo: `LTEModule.cpp`

```cpp
#include "LTEModule.h"
#include "FeatureFlags.h"  // FEAT-V1

// ... código existente ...

#if ENABLE_FIX_V1_SKIP_RESET_PDP
// FIX-V1: Agregar parámetro skipReset
bool LTEModule::configureOperator(Operadora operadora, bool skipReset) {
    // [FIX-V1 START] Skip reset cuando hay operadora guardada
    if (!skipReset) {
        resetModem();
    } else {
        debugPrint("Saltando reset del modem (skipReset=true)");
    }
    // [FIX-V1 END]
#else
bool LTEModule::configureOperator(Operadora operadora) {
    resetModem();  // Comportamiento original
#endif
    
    if (operadora >= NUM_OPERADORAS) {
        return false;
    }
    // ... resto del código ...
}
```

---

*Documento creado: 2026-01-07*  
*Última actualización: 2026-01-07*
