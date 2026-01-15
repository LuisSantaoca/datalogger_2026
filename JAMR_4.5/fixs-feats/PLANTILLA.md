# Plantilla de Documentación para FIXs y FEATs

---

## 📁 ESTRUCTURA DE CARPETAS

```
sensores_rv03/
└── fixs-feats/
    ├── feats/
    │   ├── FEAT_V0_VERSION_CONTROL.md
    │   ├── FEAT_V1_FEATURE_FLAGS.md
    │   └── FEAT_Vn_NOMBRE.md
    ├── fixs/
    │   ├── FIX_V1_PDP_REDUNDANTE.md
    │   └── FIX_Vn_NOMBRE.md
    └── PLANTILLA.md
```

---

## 🏷️ CONVENCIÓN DE NOMENCLATURA

### Archivos
| Tipo | Formato | Ejemplo |
|------|---------|---------|
| Feature | `FEAT_Vn_NOMBRE_DESCRIPTIVO.md` | `FEAT_V1_FEATURE_FLAGS.md` |
| Fix | `FIX_Vn_NOMBRE_DESCRIPTIVO.md` | `FIX_V1_PDP_REDUNDANTE.md` |

### En código
| Tipo | Formato | Ejemplo |
|------|---------|---------|
| Comentario línea | `// FEAT-Vn` o `// FIX-Vn` | `// FEAT-V0` |
| Bloque inicio | `// [FEAT-Vn START]` | `// [FIX-V1 START]` |
| Bloque fin | `// [FEAT-Vn END]` | `// [FIX-V1 END]` |
| Flag | `ENABLE_FIX_Vn_NOMBRE` | `ENABLE_FIX_V1_SKIP_RESET_PDP` |

---

## 📄 PLANTILLA PARA FEAT

```markdown
# FEAT-Vn: Título Descriptivo

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-Vn |
| **Tipo** | Feature (Infraestructura/Funcionalidad) |
| **Sistema** | [Nombre del sistema] |
| **Archivo Principal** | `archivo.h` |
| **Estado** | 📋 Propuesto / 🔧 En progreso / ✅ Completado |
| **Fecha** | YYYY-MM-DD |
| **Versión Target** | vX.Y.Z |
| **Depende de** | FEAT-Vx (si aplica) |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
[Descripción del problema o necesidad]

### Síntomas
1. [Síntoma 1]
2. [Síntoma 2]

### Causa Raíz
[Explicación técnica]

---

## 📊 EVALUACIÓN

### Impacto
| Aspecto | Evaluación |
|---------|------------|
| Criticidad | Baja/Media/Alta |
| Riesgo de no implementar | Bajo/Medio/Alto |
| Esfuerzo | Bajo/Medio/Alto |
| Beneficio | Bajo/Medio/Alto |

### Justificación
[Por qué es necesario]

---

## 🔧 IMPLEMENTACIÓN

### Archivos a Crear
| Archivo | Propósito |
|---------|-----------|

### Archivos a Modificar
| Archivo | Cambio | Línea |
|---------|--------|-------|

### Código Principal
[Código relevante con explicación]

---

## 🧪 VERIFICACIÓN

### Output Esperado
[Resultado esperado]

### Criterios de Aceptación
- [ ] Criterio 1
- [ ] Criterio 2

---

## 📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
```

---

## 📄 PLANTILLA PARA FIX

```markdown
# FIX-Vn: Título Descriptivo

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-Vn |
| **Tipo** | Fix (Corrección de Bug) |
| **Sistema** | [LTE/GPS/Sensores/Buffer/etc] |
| **Archivo Principal** | `archivo.cpp` |
| **Estado** | 📋 Propuesto / 🔧 En progreso / ✅ Completado |
| **Fecha Identificación** | YYYY-MM-DD |
| **Versión Target** | vX.Y.Z |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Prioridad** | Baja/Media/Alta/Crítica |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
[Descripción del bug]

### Evidencia
[Logs, métricas, screenshots]

### Ubicación del Bug
**Archivo:** `path/archivo.cpp`  
**Línea:** XXX

```cpp
// Código problemático
```

### Causa Raíz
[Explicación técnica detallada]

---

## 📊 EVALUACIÓN

### Impacto Cuantificado
| Métrica | Actual | Esperado | Diferencia |
|---------|--------|----------|------------|

### Impacto por Área
| Aspecto | Descripción |
|---------|-------------|

---

## 🔧 IMPLEMENTACIÓN

### Estrategia
[Descripción de la solución]

### Cambio N: [Archivo]
```cpp
// ANTES
[código original]

// DESPUÉS
[código nuevo con flag]
```

---

## 🧪 VERIFICACIÓN

### Resultado Esperado
[Output de log esperado]

### Criterios de Aceptación
- [ ] Criterio 1
- [ ] Criterio 2

---

## ⚖️ EVALUACIÓN CRÍTICA (Opcional)

### ¿Por qué no es comportamiento "normal"?
[Argumentos técnicos]

---

## 📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
```

---

## 🔍 BÚSQUEDA DE CAMBIOS

```bash
# Buscar todos los cambios de un fix específico
grep -rn "FIX-V1" sensores_rv03/

# Buscar todas las features
grep -rn "FEAT-V" sensores_rv03/

# Buscar todos los cambios documentados
grep -rn "\(FIX\|FEAT\)-V[0-9]" sensores_rv03/
```

---

## 🔄 FLUJO DE TRABAJO

1. **Identificar** problema o feature → Crear archivo en `fixs-feats/fixs/` o `fixs-feats/feats/`
2. **Documentar** completando la plantilla
3. **Implementar** cambios con comentarios `[TIPO-Vn]` y flags
4. **Actualizar** version_info.h (versión + historial)
5. **Probar** según criterios de aceptación
6. **Commit** con mensaje: `[TIPO-Vn] descripción breve`
7. **Tag** en git: `vX.Y.Z-nombre`

---

## 📊 ESTADOS PERMITIDOS

| Emoji | Estado | Descripción |
|-------|--------|-------------|
| 📋 | Propuesto | Documentado, pendiente implementación |
| 🔧 | En progreso | Implementación iniciada |
| ✅ | Completado | Implementado y verificado |
| ❌ | Descartado | No se implementará (documentar razón) |
