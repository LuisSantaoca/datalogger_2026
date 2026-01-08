# Plantilla Estándar para Documentación de Fixes

## Convención de Nomenclatura

| Prefijo | Uso | Ejemplo |
|---------|-----|---------|
| `FEAT-Vx` | Nueva funcionalidad | `FEAT-V1` control de versiones |
| `FIX-Vx` | Corrección de bug/optimización | `FIX-V1` PDP redundante |
| `REFACTOR-Vx` | Reestructuración sin cambio funcional | `REFACTOR-V1` modularización |
| `DOCS-Vx` | Solo documentación | `DOCS-V1` README |

---

## Comentarios en Código

### Línea Individual
```cpp
bool skipReset = false;  // FEAT-V1
```

### Bloque de Código (3+ líneas)
```cpp
// ============ [FEAT-V1 START] Descripción breve ============
if (!skipReset) {
    resetModem();
} else {
    debugPrint("Saltando reset");
}
// ============ [FEAT-V1 END] ============
```

### Include Nuevo
```cpp
#include "nuevo_archivo.h"  // FEAT-V1
```

### Parámetro Nuevo en Función
```cpp
// FIX-V1: Agregar parámetro skipReset para evitar reset innecesario
bool configureOperator(Operadora op, bool skipReset = false);  // FIX-V1
```

---

## Estructura de Carpeta de Fix

```
fixs/
└── fix_v1_nombre_descriptivo/
    ├── README.md              # Descripción del problema y solución
    ├── IMPLEMENTACION.md      # Detalles técnicos del cambio
    └── EVALUACION_CRITICA.md  # (Opcional) Análisis de decisiones
```

---

## Plantilla README.md para Fix

```markdown
# [TIPO]-V[N]: Título Descriptivo

**Versión objetivo:** vX.Y.Z  
**Fecha de identificación:** YYYY-MM-DD  
**Estado:** Pendiente | En progreso | Completado | Descartado  
**Prioridad:** Baja | Media | Alta | Crítica  

---

## Resumen
[Descripción en 2-3 líneas]

## Problema Identificado
[Descripción detallada del bug o necesidad]

## Causa Raíz
[Ubicación y explicación técnica]

## Solución Propuesta
[Descripción de la solución]

## Archivos Afectados
| Archivo | Tipo de Cambio |
|---------|----------------|
| archivo.cpp | Modificar |
| nuevo.h | Crear |

## Criterios de Aceptación
1. [ ] Criterio 1
2. [ ] Criterio 2

## Plan de Pruebas
1. Prueba 1
2. Prueba 2
```

---

## Registro en version_info.h

```cpp
/*******************************************************************************
 * HISTORIAL DE VERSIONES (más reciente arriba)
 ******************************************************************************/

// VERSION  | FECHA      | NOMBRE              | DESCRIPCIÓN
// ---------|------------|---------------------|------------------------------------------
// v2.0.1   | 2026-01-08 | pdp-fix             | FIX-V1: Reducir eventos PDP redundantes
//          |            |                     | Cambios: LTEModule.h(L82), LTEModule.cpp(L326-335)
//          |            |                     | AppController.cpp(L376)
// v2.0.0   | 2025-12-18 | release-inicial     | Versión inicial con arquitectura FSM
//          | 2026-01-07 | +version-control    | FEAT-V0: Agregar sistema de control de versiones
//          |            |                     | Cambios: version_info.h (nuevo), AppController.cpp(L30,L506)
```

---

## Búsqueda de Cambios

```bash
# Buscar todos los cambios de un fix específico
grep -rn "FIX-V1" sensores_rv03/

# Buscar todas las features
grep -rn "FEAT-V" sensores_rv03/

# Buscar todos los cambios documentados
grep -rn "\(FIX\|FEAT\|REFACTOR\)-V[0-9]" sensores_rv03/
```

---

## Flujo de Trabajo

1. **Identificar** problema o feature → Crear carpeta en `fixs/`
2. **Documentar** en README.md de la carpeta
3. **Implementar** cambios con comentarios `[TIPO-Vx]`
4. **Actualizar** version_info.h (versión + historial)
5. **Probar** según criterios de aceptación
6. **Commit** con mensaje: `[TIPO-Vx] descripción breve`
7. **Tag** en git: `vX.Y.Z-nombre`
