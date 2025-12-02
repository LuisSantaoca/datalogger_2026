# Requisitos del Sistema JAMR_4

**Versión:** 1.0  
**Fecha:** 2025-10-29  
**Estado:** Definición Completa

---

## 📚 Índice de Requisitos

### Requisitos Críticos (MUST HAVE)

| ID | Título | Prioridad | Estado | Documento |
|----|--------|-----------|--------|-----------|
| REQ-001 | Gestión de Estado del Módem entre Ciclos | CRÍTICA | Pendiente | [REQ-001_MODEM_STATE_MANAGEMENT.md](./REQ-001_MODEM_STATE_MANAGEMENT.md) |
| REQ-002 | Protección contra Cuelgues (Watchdog) | CRÍTICA | Pendiente | [REQ-002_WATCHDOG_PROTECTION.md](./REQ-002_WATCHDOG_PROTECTION.md) |

### Requisitos Alta Prioridad (SHOULD HAVE)

| ID | Título | Prioridad | Estado | Documento |
|----|--------|-----------|--------|-----------|
| REQ-003 | Diagnóstico Postmortem (Health Data) | ALTA | Pendiente | [REQ-003_HEALTH_DIAGNOSTICS.md](./REQ-003_HEALTH_DIAGNOSTICS.md) |

### Requisitos Media Prioridad (NICE TO HAVE)

| ID | Título | Prioridad | Estado | Documento |
|----|--------|-----------|--------|-----------|
| REQ-004 | Versionamiento Dinámico del Firmware | MEDIA | Pendiente | [REQ-004_FIRMWARE_VERSIONING.md](./REQ-004_FIRMWARE_VERSIONING.md) |

---

## 🎯 Resumen Ejecutivo

### Problema a Resolver

El firmware JAMR_3 (basado en `sensores_elathia_fix_gps`) presenta un **problema crítico**: el módem SIM7080 no mantiene su estado correctamente entre ciclos de deep sleep, causando:

1. **Estado "zombi":** Módem no responde después de despertar
2. **Delays largos:** 5-10s para re-inicializar módem en cada ciclo
3. **Inestabilidad:** Cuelgues impredecibles que requieren reset de watchdog
4. **Falta de diagnóstico:** Difícil identificar causas de fallos en campo

### Objetivo de JAMR_4

Crear un firmware **robusto, diagnosticable y eficiente** que:

- ✅ **Responde inmediatamente** tras despertar (< 1s a primer AT command)
- ✅ **Se recupera automáticamente** de cualquier cuelgue
- ✅ **Reporta su salud** para diagnóstico remoto
- ✅ **Es auditable** con versionamiento en cada transmisión

---

## 📋 Filosofía de Diseño

### Principios Guía

1. **Simplicidad > Complejidad**
   - Evitar lógica de recuperación multi-nivel
   - Código debe ser fácil de entender y debuggear

2. **Explícito > Implícito**
   - Estado del sistema siempre claro
   - No asumir, verificar y loguear

3. **Fail Fast, Recover Clean**
   - Detectar errores temprano
   - Reset limpio mejor que workarounds complejos

4. **Diagnosticable por Default**
   - Todo evento crítico logueado
   - Telemetría incluye health data

5. **Evidence-Based Implementation**
   - Cada decisión respaldada por documentación técnica
   - Referencias a manuales incluidas en requisitos

---

## 🔄 Relación entre Requisitos

```
┌─────────────────────────────────────────────────────────┐
│                  JAMR_4 Sistema                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌─────────────────────────────────────────────────┐   │
│  │  REQ-002: Watchdog                              │   │
│  │  (Protección contra cuelgues)                   │   │
│  └─────────────────┬───────────────────────────────┘   │
│                    │ Detecta y resetea                 │
│                    │ si hay problemas                  │
│                    ▼                                   │
│  ┌─────────────────────────────────────────────────┐   │
│  │  REQ-001: Gestión Estado Módem                  │   │
│  │  (Core del sistema - wake-up rápido)            │   │
│  └─────────────────┬───────────────────────────────┘   │
│                    │ Genera eventos                    │
│                    │ de diagnóstico                    │
│                    ▼                                   │
│  ┌─────────────────────────────────────────────────┐   │
│  │  REQ-003: Health Data                           │   │
│  │  (Diagnóstico postmortem)                       │   │
│  └─────────────────┬───────────────────────────────┘   │
│                    │ Incluye versión                   │
│                    │ en telemetría                     │
│                    ▼                                   │
│  ┌─────────────────────────────────────────────────┐   │
│  │  REQ-004: Versionamiento                        │   │
│  │  (Trazabilidad)                                 │   │
│  └─────────────────────────────────────────────────┘   │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

### Dependencias

- **REQ-002 → REQ-001**: Watchdog protege si gestión de módem falla
- **REQ-003 → REQ-001**: Health data registra checkpoints de módem
- **REQ-003 → REQ-002**: Health data registra resets de watchdog
- **REQ-004 → REQ-003**: Versión incluida en payload de health data

---

## 🚀 Plan de Implementación

### Fase 1: Fundamentos (Semana 1)
**Objetivo:** Sistema básico estable con watchdog

1. Implementar **REQ-002** (Watchdog)
   - Configurar watchdog 120s
   - Agregar feeds en operaciones críticas
   - Validar que no hay resets espurios

2. Implementar **REQ-003** (Health Data) - Básico
   - RTC memory para checkpoints
   - Detección de reset reason
   - Logging de diagnóstico

**Entregable:** Firmware que se recupera de cuelgues y loguea causas

**Criterio de éxito:** 24h sin resets de watchdog en operación normal

---

### Fase 2: Gestión de Módem (Semana 2)
**Objetivo:** Wake-up rápido del módem

3. Implementar **REQ-001** (Gestión Estado Módem)
   - Detección de primer boot
   - Modo de bajo consumo (CFUN=0 o DTR)
   - Verificación de estado al despertar

**Entregable:** Firmware con wake-to-transmit < 30s

**Criterio de éxito:** 
- Primer AT command exitoso en < 1s tras despertar
- Sin uso de power cycles entre ciclos normales

---

### Fase 3: Observabilidad (Semana 3)
**Objetivo:** Telemetría completa para diagnóstico

4. Completar **REQ-003** (Health Data) - Completo
   - Integrar checkpoints de módem
   - Incluir en payload de transmisión
   - Actualizar backend para recibir health data

5. Implementar **REQ-004** (Versionamiento)
   - Definir constantes de versión
   - Incluir en payload
   - Actualizar parser en backend

**Entregable:** Sistema completamente observable

**Criterio de éxito:**
- 100% de transmisiones incluyen health data
- Versión visible en dashboard para cada device

---

### Fase 4: Validación en Campo (Semana 4+)
**Objetivo:** Confirmar estabilidad en producción

6. Testing Extensivo
   - Desplegar en 3-5 devices
   - Monitorear 7 días continuos
   - Recolectar métricas

7. Ajustes Finales
   - Optimizar timeouts basado en telemetría real
   - Refinar checkpoints si necesario
   - Documentar lecciones aprendidas

**Entregable:** Firmware listo para producción

**Criterio de éxito:**
- 0 gaps > 15 minutos en 7 días
- 0 resets de watchdog
- Consumo < 2 mA promedio

---

## 📊 Métricas de Éxito del Proyecto

### Métricas Técnicas

| Métrica | Baseline (JAMR_3) | Target (JAMR_4) |
|---------|------------------|-----------------|
| Wake-to-transmit time | ~30-60s | < 30s |
| Primer AT response | 5-10s (con power cycle) | < 1s |
| Resets de watchdog | Frecuentes | 0 en 24h |
| Gaps en telemetría | Múltiples por día | 0 en 24h |
| Consumo promedio | ~10 mA (estimado) | < 2 mA |
| Diagnóstico remoto | Imposible | Completo |

### Métricas de Negocio

| Métrica | Baseline | Target |
|---------|---------|--------|
| Tiempo de troubleshooting | Horas (requiere acceso físico) | Minutos (remoto) |
| Uptime del device | ~80-90% | > 99% |
| Mantenimientos no planificados | Frecuentes | Raros |
| Visibilidad de flota | Limitada | Completa |

---

## 🔍 Testing y Validación

### Testing en Desarrollo

```bash
# Checklist por requisito
[ ] REQ-001: Mock de modem responde en < 1s
[ ] REQ-002: Forzar timeout dispara watchdog
[ ] REQ-003: RTC memory persiste a través de reset
[ ] REQ-004: Payload incluye versión correcta
```

### Testing en Lab

```bash
# Escenarios de stress
[ ] Desconectar antena → watchdog reset → recovery
[ ] Desconectar batería → power-on → inicialización
[ ] 100 ciclos sleep/wake → sin degradación
[ ] Brownout simulado → health data registra causa
```

### Testing en Campo

```bash
# Validación en condiciones reales
[ ] 7 días continuos sin intervención
[ ] Telemetría consistente cada 10 minutos
[ ] Health data muestra operación normal
[ ] Versión correcta en 100% de transmisiones
```

---

## 📝 Proceso de Cambios a Requisitos

### Solicitud de Cambio

1. **Identificar necesidad**
   - ¿Por qué el requisito actual no es suficiente?
   - ¿Qué evidencia respalda el cambio?

2. **Proponer modificación**
   - Qué se debe cambiar (QUÉ, no CÓMO)
   - Impacto en otros requisitos
   - Impacto en cronograma

3. **Revisión y aprobación**
   - Evaluar con equipo técnico
   - Actualizar documentos afectados
   - Incrementar versión del requisito

### Control de Versiones

- Requisitos usan versionamiento semántico
- Cambios mayores → nueva versión MAJOR
- Clarificaciones → nueva versión MINOR
- Correcciones tipográficas → nueva versión PATCH

---

## 🎓 Lecciones de JAMR_3

### Lo que Aprendimos

1. **Iteración sin requisitos claros = degradación**
   - JAMR_3 intentó múltiples fixes sin QUÉs claros
   - Resultado: código cada vez más complejo sin mejoras

2. **Implementar CÓMOs antes de definir QUÉs = desperdicio**
   - Se implementaron soluciones sin validar que resolvían el problema correcto
   - Múltiples enfoques probados sin criterios de éxito claros

3. **Complejidad acumulativa es peor que simplicidad**
   - Intentar "arreglar" con más código empeoró las cosas
   - Reset a versión estable fue la mejor decisión

4. **Diagnóstico debe ser built-in, no agregado después**
   - Sin health data, imposible saber qué estaba pasando
   - REQ-003 debe implementarse desde el inicio

### Lo que Haremos Diferente en JAMR_4

✅ **Requisitos primero, implementación después**  
✅ **Criterios de éxito medibles para cada requisito**  
✅ **Simplicidad como principio de diseño**  
✅ **Diagnóstico desde el día 1**  
✅ **Testing exhaustivo antes de declarar "completo"**

---

## 📚 Referencias

### Documentación de Requisitos
- REQ-001: Gestión de Estado del Módem
- REQ-002: Protección contra Cuelgues
- REQ-003: Diagnóstico Postmortem
- REQ-004: Versionamiento Dinámico

### Documentación Técnica
- SIM7080 AT Command Manual V1.02
- SIM7080 Hardware Design Guide V1.05
- ESP32-S3 Technical Reference Manual
- ESP-IDF Programming Guide v5.3

### Código Base
- JAMR_4: Versión estable pre-FIX-004 (baseline)
- JAMR_3: Versión con iteraciones (referencia de lo que NO hacer)

---

**Documento creado:** 2025-10-29  
**Próxima revisión:** Inicio de Fase 1  
**Responsable:** Equipo de desarrollo JAMR_4
