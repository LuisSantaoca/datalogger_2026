# REQ-004: Versionamiento Dinámico del Firmware

**Versión:** 1.0  
**Fecha:** 2025-10-29  
**Prioridad:** MEDIA  
**Estado:** PENDIENTE

---

## 🎯 Objetivo (QUÉ)

Cada transmisión **DEBE** incluir la versión exacta del firmware que la generó, permitiendo auditoría, troubleshooting y gestión de despliegues sin ambigüedad.

---

## 📋 Requisitos Funcionales

### RF-001: Versionamiento Semántico
El firmware **DEBE** usar versionamiento semántico (MAJOR.MINOR.PATCH).

**Criterio de aceptación:**
- Formato: `X.Y.Z` donde X, Y, Z son números
- MAJOR: Cambios incompatibles con versiones anteriores
- MINOR: Nueva funcionalidad compatible hacia atrás
- PATCH: Bug fixes que no cambian funcionalidad

**Ejemplo:**
- `3.0.0`: Primera versión JAMR_4
- `3.1.0`: Agregar feature (ej: DTR sleep mode)
- `3.1.1`: Bug fix en DTR sleep mode

### RF-002: Versión en Payload
La versión **DEBE** incluirse en cada transmisión de datos.

**Criterio de aceptación:**
- Codificada en 3 bytes: [MAJOR][MINOR][PATCH]
- Posición fija en el payload
- Decodificable sin ambigüedad

**Ejemplo codificación:**
```
Versión 3.1.5:
  Byte 0: 0x03 (MAJOR = 3)
  Byte 1: 0x01 (MINOR = 1)
  Byte 2: 0x05 (PATCH = 5)
```

### RF-003: Single Source of Truth
La versión **DEBE** definirse en un solo lugar del código.

**Criterio de aceptación:**
- Constantes en un header file centralizado
- Todos los usos referencian estas constantes (no hardcoded)
- Compilación falla si versión no está definida

### RF-004: Persistencia en Backend
El backend **DEBE** almacenar la versión con cada registro de telemetría.

**Criterio de aceptación:**
- Campo `firmware_version` en tabla de datos
- Formato: string "X.Y.Z" o separado en columnas MAJOR, MINOR, PATCH
- Indexable para queries por versión

---

## 🚫 Anti-Requisitos (QUÉ NO HACER)

### ANR-001: NO Hardcodear Versión en Múltiples Lugares
**PROHIBIDO:** Duplicar definición de versión en listeners, parsers, o servicios.

**Razón:**
- Crea inconsistencias cuando se actualiza
- Imposible garantizar sincronización
- Versión debe venir **solo** del firmware

### ANR-002: NO Asumir Versión por Otros Campos
**PROHIBIDO:** Inferir versión del firmware por presencia/ausencia de campos en payload.

**Razón:**
- Ambiguo: múltiples versiones pueden tener misma estructura
- Frágil: pequeños cambios rompen la inferencia
- Explícito > Implícito

### ANR-003: NO Versión Manual en Deployment
**PROHIBIDO:** Depender de etiquetas, nombres de archivo, o documentación para saber qué versión está deployed.

**Razón:**
- Propenso a error humano
- Puede desincronizarse con realidad
- Telemetría debe ser self-describing

---

## 📊 Métricas de Éxito

### Métricas Primarias
| Métrica | Objetivo | Método de Medición |
|---------|----------|-------------------|
| Versión presente en telemetría | 100% | Query: `WHERE firmware_version IS NOT NULL` |
| Versión correcta vs deployed | 100% | Comparar payload vs expected version |
| Consistencia en ventana 24h | 100% | Todos los registros mismo device, misma versión |

### Métricas Secundarias
| Métrica | Objetivo | Método de Medición |
|---------|----------|-------------------|
| Time-to-identify version | < 1 min | Query backend por device_id |
| Auditoría de despliegues | 100% trazable | Logs de flashing + telemetría correlacionan |

---

## 🔍 Casos de Uso

### CU-001: Nuevo Despliegue de Firmware
**Precondición:** Se actualiza firmware de v3.0.0 a v3.1.0

**Flujo:**
1. Developer actualiza constantes:
   ```cpp
   #define FW_VERSION_MAJOR 3
   #define FW_VERSION_MINOR 1
   #define FW_VERSION_PATCH 0
   ```
2. Firmware se compila e incluye versión en payload
3. Device se flashea con nuevo firmware
4. Primera transmisión post-flash incluye: `[0x03, 0x01, 0x00]`
5. Backend decodifica: "3.1.0"
6. Dashboard muestra: Device X ahora en v3.1.0
7. Operador confirma: deployment exitoso

**Postcondición:**
- Trazabilidad completa del deployment
- No ambigüedad sobre qué versión está corriendo

### CU-002: Troubleshooting de Bug Reportado
**Precondición:** Device reporta comportamiento anómalo

**Flujo:**
1. Operador consulta backend: "¿Qué versión corre Device X?"
2. Query: `SELECT firmware_version FROM datos WHERE device_id = X ORDER BY timestamp DESC LIMIT 1`
3. Resultado: "3.0.5"
4. Developer revisa CHANGELOG: "v3.0.5 - Bug conocido en GPS parsing"
5. Se programa actualización a v3.1.1 que corrige el bug
6. Después del flash: telemetría confirma "3.1.1"
7. Comportamiento anómalo desaparece

**Postcondición:**
- Bug identificado y resuelto rápidamente
- Correlación clara entre versión y síntoma

### CU-003: Auditoría de Flota
**Precondición:** Operador necesita inventario de versiones en campo

**Flujo:**
1. Query: `SELECT device_id, firmware_version, MAX(timestamp) as last_seen FROM datos GROUP BY device_id`
2. Resultado:
   ```
   Device A: v3.1.0 (last seen: 2025-10-29 10:00)
   Device B: v3.0.5 (last seen: 2025-10-29 09:45)
   Device C: v3.1.0 (last seen: 2025-10-29 10:15)
   Device D: v2.9.0 (last seen: 2025-10-28 22:00) ← desactualizado
   ```
3. Operador identifica: Device D necesita actualización
4. Se programa visita de campo para flash

**Postcondición:**
- Visibilidad completa de versiones deployed
- Planificación informada de actualizaciones

---

## 🔗 Dependencias

### Firmware
- Constantes de versión definidas en header
- Inclusión en payload builder
- Tests de compilación validan presencia

### Ingesta
- Parser actualizado para extraer 3 bytes de versión
- Conversión a formato string "X.Y.Z"
- Validación: valores deben ser razonables (0-255)

### Backend
- Campo `firmware_version` en schema de datos_sensores
- Tipo: VARCHAR(20) o columnas separadas (major INT, minor INT, patch INT)
- Default: NULL (para datos históricos sin versión)

### Dashboard
- Visualización de versión por device
- Alertas si versión desactualizada
- Distribución de versiones en flota

---

## ✅ Criterios de Validación

### Validación en Desarrollo
- [ ] Constantes definidas en header único
- [ ] Payload builder incluye 3 bytes de versión
- [ ] Tests unitarios validan codificación correcta
- [ ] Compilación sin warnings ni errores

### Validación en Ingesta
- [ ] Parser extrae versión correctamente
- [ ] Test: payload con v3.1.5 → database recibe "3.1.5"
- [ ] Test: payload malformado → error logged, versión = NULL
- [ ] Backward compatible: payloads viejos sin versión no causan crash

### Validación en Campo
- [ ] 100% de transmisiones incluyen versión
- [ ] Versión coincide con expected version post-flash
- [ ] No cambios de versión sin flash manual

---

## 📝 Notas de Implementación

### Definición en Firmware

```cpp
// type_def.h o version.h
#define FW_VERSION_MAJOR 3
#define FW_VERSION_MINOR 1
#define FW_VERSION_PATCH 0

// Para logging
#define FW_VERSION_STRING "3.1.0"
```

### Inclusión en Payload

```cpp
// En función que construye payload
uint8_t payload[43]; // 40 datos + 3 versión

// ... llenar datos de sensores ...

// Últimos 3 bytes: versión
payload[40] = FW_VERSION_MAJOR;
payload[41] = FW_VERSION_MINOR;
payload[42] = FW_VERSION_PATCH;
```

### Decodificación en Backend

```javascript
// listener_encrypted/src/parser.js
function parseVersion(buffer) {
  const major = buffer.readUInt8(40);
  const minor = buffer.readUInt8(41);
  const patch = buffer.readUInt8(42);
  
  // Validación básica
  if (major > 99 || minor > 99 || patch > 99) {
    logger.warn('Version out of expected range');
    return null;
  }
  
  return `${major}.${minor}.${patch}`;
}

// En insert a database
const version = parseVersion(payload);
await query(`
  INSERT INTO datos_sensores (..., firmware_version)
  VALUES (..., $version)
`, { version });
```

### Schema de Database

```sql
-- Opción 1: String
ALTER TABLE datos_sensores ADD COLUMN firmware_version VARCHAR(20);

-- Opción 2: Columnas separadas (mejor para queries)
ALTER TABLE datos_sensores 
  ADD COLUMN fw_major INT,
  ADD COLUMN fw_minor INT,
  ADD COLUMN fw_patch INT;

-- Índice para queries de auditoría
CREATE INDEX idx_firmware_version ON datos_sensores(firmware_version);
```

---

## 🐛 Lecciones de Intentos Anteriores

### Problema en JAMR_3:
- Versión hardcoded en listener: `if (buffer.length === 46) { version = 'v3.0.10' }`
- Cuando se actualizó firmware, listener no se actualizó
- Resultado: versión incorrecta en database

### Solución en JAMR_4:
- Versión viaja **en el payload**
- Listener es agnóstico: lee de bytes, no asume
- Garantiza sincronización firmware ↔ database

---

## 📚 Referencias Técnicas

- Semantic Versioning 2.0.0: https://semver.org/
- Firmware Versioning Best Practices: https://embeddedartistry.com/blog/2016/12/21/creating-a-version-header/
- Git Tagging for Releases: https://git-scm.com/book/en/v2/Git-Basics-Tagging

---

## 🎯 Beneficios Esperados

1. **Trazabilidad Completa**
   - Saber exactamente qué código está corriendo en cada device
   - Correlacionar bugs con versiones específicas

2. **Auditoría Simplificada**
   - Query rápido para inventario de versiones
   - Identificar devices desactualizados

3. **Troubleshooting Más Rápido**
   - Eliminar "¿qué versión tiene?" como primera pregunta
   - Reproducir bugs en versión exacta

4. **Gestión de Despliegues**
   - Validar que flashing fue exitoso
   - Planificar rollouts graduales

5. **Compliance y Documentación**
   - Logs auditables para certificaciones
   - Evidencia de actualizaciones de seguridad

---

**Documento creado:** 2025-10-29  
**Responsable:** Por definir  
**Revisión siguiente:** Tras implementación inicial
