# FIX-V12: Buffer Trim CR

---

## INFORMACION GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FIX-V12 |
| **Tipo** | Fix (Integridad de datos) |
| **Sistema** | Buffer / LittleFS |
| **Archivo Principal** | `src/data_buffer/BUFFERModule.cpp` |
| **Estado** | Aprobado |
| **Fecha Identificacion** | 2026-02-16 |
| **Version Target** | v2.11.0 |
| **Branch** | `fix-v12/buffer-trim-cr` |
| **Depende de** | FEAT-V1 (FeatureFlags.h) |
| **Origen** | Portado de `jarm_4.5_autoswich` FIX-V13 |
| **Requisitos** | **FR-26** (eliminar caracteres de control residuales de buffer) |
| **Prioridad** | **ALTA** (requisito incumplido) |
| **Flujo** | flujos-de-trabajo/FLUJO_UPGRADE_V2.11_A_V2.13.md — Fase 1 |

---

## CUMPLIMIENTO DE PREMISAS

| Premisa | Descripcion | Cumplimiento | Evidencia |
|---------|-------------|--------------|-----------|
| **P1** | Aislamiento total | Si | Branch dedicado |
| **P2** | Cambios minimos | Si | Solo BUFFERModule.cpp (agregar .trim()) |
| **P3** | Defaults seguros | Si | .trim() no puede causar dano |
| **P4** | Feature flags | Si | `ENABLE_FIX_V12_BUFFER_TRIM_CR` con valor 1/0 |
| **P5** | Logging exhaustivo | Si | No requiere logging (operacion silenciosa) |
| **P6** | No cambiar logica existente | Si | Solo agrega .trim() despues de lectura |
| **P7** | Testing gradual | Si | Verificar que tramas no tienen \r |
| **P8** | Metricas objetivas | Si | 0 tramas rechazadas por CR |
| **P9** | Rollback plan | Si | Flag a 0 + recompilar (<5 min) |

---

## DIAGNOSTICO

### Problema Identificado

Las tramas leidas del buffer LittleFS pueden contener caracteres `\r` (Carriage Return, 0x0D) residuales. Esto ocurre porque:

1. LittleFS almacena lineas terminadas en `\n`
2. En algunos escenarios, las lineas se escriben con `\r\n` (CRLF)
3. `readStringUntil('\n')` consume el `\n` pero **deja el `\r` en el string**
4. El servidor receptor puede rechazar tramas con `\r` extra

### Evidencia

En `jarm_4.5_autoswich`, este problema se identifico y corrigio como FIX-V13. Se encontraron **5 ubicaciones** donde `readStringUntil('\n')` necesitaba `.trim()`:

```cpp
// Antes (BUFFERModule.cpp):
lines[count] = file.readStringUntil('\n');
// Trama: "JCwwMDAwMDAwMDAwMDAwMDAwMDAwMCwx...\r"  ← \r residual

// Despues:
lines[count] = file.readStringUntil('\n');
lines[count].trim();
// Trama: "JCwwMDAwMDAwMDAwMDAwMDAwMDAwMCwx..."  ← limpia
```

### Causa Raiz

`readStringUntil('\n')` de Arduino/ESP32 lee hasta encontrar `\n` y lo descarta, pero si la linea fue escrita con `\r\n`, el `\r` queda como ultimo caracter del string.

---

## EVALUACION

### Impacto Cuantificado

| Metrica | Actual (v2.10.1) | Esperado (FIX-V12) |
|---------|------------------|---------------------|
| Tramas con \r residual | Potencialmente todas | 0 |
| Tramas rechazadas por servidor | Variable | 0 por esta causa |

### Impacto por Area

| Aspecto | Descripcion |
|---------|-------------|
| Integridad | Tramas limpias sin caracteres invisibles |
| Compatibilidad | Servidor no rechaza tramas por CR extra |
| Debugging | Tramas visibles son identicas a las reales (sin caracteres ocultos) |

---

## SOLUCION

### Feature Flag en FeatureFlags.h

```cpp
/**
 * FIX-V12: Buffer Trim CR
 * Sistema: Buffer / LittleFS
 * Archivo: src/data_buffer/BUFFERModule.cpp
 * Descripcion: Agrega .trim() despues de readStringUntil('\n') para
 *              eliminar \r residual de tramas leidas del buffer.
 * Origen: Portado de jarm_4.5_autoswich FIX-V13
 * Documentacion: fixs-feats/fixs/FIX_V12_BUFFER_TRIM_CR.md
 * Estado: Aprobado
 */
#define ENABLE_FIX_V12_BUFFER_TRIM_CR    1
```

### Patron de cambio en BUFFERModule.cpp

En **cada** funcion que lee lineas del archivo buffer:

```cpp
// ANTES:
lines[count] = file.readStringUntil('\n');

// DESPUES:
lines[count] = file.readStringUntil('\n');
#if ENABLE_FIX_V12_BUFFER_TRIM_CR
lines[count].trim();  // FIX-V12: eliminar \r residual
#endif
```

### Funciones afectadas (5 ubicaciones)

| Funcion | Variable | Requiere trim |
|---------|----------|---------------|
| `readLines()` | `lines[count]` | Si |
| `readUnprocessedLines()` | `line` | Si |
| `markLineAsProcessed()` | `allLines[totalLines]` | Si |
| `markLinesAsProcessed()` | `allLines[totalLines]` | Si |
| `removeProcessedLines()` | `line` | Si |

**NOTA:** En `readUnprocessedLines()` y `removeProcessedLines()`, el `.trim()` debe ir inmediatamente despues de `readStringUntil()`, antes del `startsWith(PROCESSED_MARKER)`. El `\r` al final no afecta `startsWith`, pero el dato debe estar limpio en todo el flujo.

---

## ARCHIVOS A MODIFICAR

| Archivo | Cambio | Lineas aprox |
|---------|--------|--------------|
| `src/FeatureFlags.h` | Agregar flag `ENABLE_FIX_V12_BUFFER_TRIM_CR` | +8 |
| `src/data_buffer/BUFFERModule.cpp` | Agregar `.trim()` en 5 ubicaciones | +15 |
| `src/version_info.h` | Actualizar version | +3 |

---

## CRITERIOS DE ACEPTACION

| # | Criterio | Verificacion |
|---|----------|--------------|
| 1 | Tramas leidas del buffer no contienen \r | Hex dump de trama enviada |
| 2 | Tramas se envian correctamente por TCP | Log: "Datos enviados exitosamente" |
| 3 | Servidor acepta tramas | Verificar DB |
| 4 | Flag a 0 restaura comportamiento original | Compilar y probar |
| 5 | .trim() no afecta tramas que ya estan limpias | Test con tramas normales |

---

## ROLLBACK

```cpp
// En FeatureFlags.h cambiar:
#define ENABLE_FIX_V12_BUFFER_TRIM_CR    0  // Cambiar a 0
// Recompilar y flashear
```

**Tiempo de rollback:** <5 minutos

---

## LIMITACIONES

| Limitacion | Razon | Mitigacion |
|------------|-------|------------|
| .trim() remueve TODOS los whitespace al inicio y fin | Podria afectar datos con espacios intencionales | Tramas Base64 no tienen espacios, es seguro |
| No previene, solo limpia | El \r se escribe igualmente | Podria limpiarse en appendLine() tambien |

---

## HISTORIAL

| Fecha | Accion | Version |
|-------|--------|---------|
| 2026-02-16 | Documentacion inicial. Portado de jarm_4.5_autoswich FIX-V13 | 1.0 |
| 2026-02-16 | Corregir tabla funciones (5 reales), alinear estado Aprobado, eliminar ambiguedad 3-5 | 1.1 |

---

*Fin del documento*
