# Backlog de FIXs y FEATs - JAMR_4.5

**Proyecto:** JAMR_4.5 (sensores_rv03)  
**Versión Actual:** v2.5.0 (periodic-restart)  
**Última Actualización:** 2026-01-28  
**Origen:** Auditoría de Trazabilidad + Desarrollo Continuo + Diagnóstico EMI

---

## 🔬 HALLAZGOS DIAGNÓSTICO EMI (2026-01-28)

### Prueba Realizada
- **Duración:** ~5.5 horas (22.9% → 23.6% del ciclo 24h)
- **Ciclos observados:** 2+ ciclos completos con comunicación real
- **Firmware:** v2.5.0 con DEBUG_EMI_DIAGNOSTIC_ENABLED=1

### Resultados

| Métrica | Valor | Estado |
|---------|-------|--------|
| Bytes analizados | ~200+ | - |
| Bytes corruptos (0xFF/0x00) | **0** | ✅ |
| Caracteres inválidos | **0** | ✅ |
| Corrupción total | **0.0%** | ✅ |
| Timeouts (power off) | ~15 | ⚪ Normal |

### Hex Dumps Verificados
```
AT+CNMP=38   → 41 54 2B 43...4F 4B ✅
AT+CMNB=1    → 41 54 2B 43...4F 4B ✅
AT+CGDCONT   → 41 54 2B 43...4F 4B ✅
AT+CNACT    → 41 54 2B 43...4F 4B ✅
AT+CACLOSE   → 41 54 2B 43...4F 4B ✅
```
Todos los bytes en rango ASCII válido (0x20-0x7E, 0x0D, 0x0A).

### Veredicto
```
╔══════════════════════════════════════╗
║  EMI STATUS: PCB OK ✅               ║
╠══════════════════════════════════════╣
║  No se detectó ruido/EMI en UART     ║
║  Comunicación modem estable          ║
║  Diseño PCB 2 capas: ACEPTABLE       ║
╚══════════════════════════════════════╝
```

### Bug Detectado y Corregido
- **Problema:** Contador `g_emiDiagCycleCount` se reseteaba en cada deep sleep
- **Causa:** Variable `static` en lugar de `RTC_DATA_ATTR`
- **Fix:** Cambiado a `RTC_DATA_ATTR static uint32_t g_emiDiagCycleCount`
- **Commit:** feat-v7/production-diagnostics

---

## ✅ IMPLEMENTADOS (v2.5.0)

### FIX-V1: Skip Reset PDP Redundante
- **Estado:** ✅ **IMPLEMENTADO** (v2.0.2, 2026-01-07)
- **Requisito:** Optimización conexión
- **Archivo Doc:** `fixs-feats/fixs/FIX_V1_PDP_REDUNDANTE.md`
- **Descripción:** Evita reset innecesario cuando hay operadora guardada

---

### FIX-V2: Fallback a Escaneo de Operadoras
- **Estado:** ✅ **IMPLEMENTADO** (v2.2.0, 2026-01-13)
- **Requisito:** RF-12
- **Archivo Doc:** `fixs-feats/fixs/FIX_V2_FALLBACK_OPERADORA.md`
- **Descripción:** Si `configureOperator()` falla, escanea alternativas automáticamente

---

### FIX-V3: Modo Solo-Adquisición por Baja Batería
- **Estado:** ✅ **IMPLEMENTADO** (v2.3.0, 2026-01-15)
- **Requisito:** RF-06, RF-09
- **Archivo Doc:** `fixs-feats/fixs/FIX_V3_MODO_BATERIA_BAJA.md`
- **Descripción:** Desactiva modem cuando batería < UTS (3.3V), mantiene adquisición

---

### FIX-V4: Apagado Robusto de Modem
- **Estado:** ✅ **IMPLEMENTADO** (v2.4.0, 2026-01-26)
- **Requisito:** Consumo energía
- **Descripción:** Espera URC "NORMAL POWER DOWN" antes de deep sleep

---

### FEAT-V2: Timing de Ciclo FSM
- **Estado:** ✅ **IMPLEMENTADO** (v2.1.0, 2026-01-07)
- **Descripción:** Medición de tiempos de cada fase del ciclo

---

### FEAT-V3: Diagnósticos de Crash
- **Estado:** ✅ **IMPLEMENTADO** (v2.3.0)
- **Requisito:** RF-05 (parcial)
- **Descripción:** RTC_DATA_ATTR para persistir estado entre reinicios

---

### FEAT-V4: Reinicio Periódico Preventivo
- **Estado:** ✅ **IMPLEMENTADO** (v2.5.0, 2026-01-28)
- **Descripción:** Reinicio automático cada 24h para prevenir degradación

---

## ⏳ PENDIENTES

### 🟠 Prioridad Alta

#### FEAT-V7: Diagnóstico de Producción (NUEVO)
- **Estado:** 📝 **DOCUMENTADO** (2026-01-28)
- **Archivo Doc:** `fixs-feats/feats/FEAT_V7_PRODUCTION_DIAGNOSTICS.md`
- **Descripción:** Sistema de diagnóstico ligero siempre activo
- **Incluye:**
  - Contadores persistentes (ciclos, LTE, batería, EMI)
  - Log de eventos críticos (LittleFS circular)
  - Detección EMI sin overhead (conteo, no hex dump)
  - Comandos Serial: STATS, LOG, CLEAR
- **Prioridad:** Implementar antes de despliegue largo en campo

---

#### FEAT-V6: Almacenamiento Reportes EMI (NUEVO)
- **Estado:** 📝 **DOCUMENTADO** (2026-01-28)
- **Archivo Doc:** `fixs-feats/feats/FEAT_V6_EMI_REPORT_STORAGE.md`
- **Descripción:** Guardar reportes EMI en LittleFS para revisión post-mortem
- **Prioridad:** Baja (FEAT-V7 lo reemplaza con mejor diseño)

---

#### FIX-V5: Protección Brown-out Activa
- **Estado:** 📋 Por evaluar en validación de 30 días
- **Requisito:** RNF-02
- **Descripción:** Modo seguro si voltaje < UMO (3.0V)
- **Nota:** FIX-V3 cubre parcialmente este caso. Evaluar si es necesario.

---

#### FEAT-V5: CLI de Mantenimiento Serial
- **Estado:** 📋 Pendiente
- **Requisito:** RF-16, RF-17
- **Descripción:** Comandos serial para diagnóstico remoto
- **Comandos:** STATUS, BATTERY, BUFFER, EXPORT, LOGS
- **Prioridad:** Útil para debugging en campo

---

### 🟡 Prioridad Media

#### FEAT-V6: Conexión TLS/SSL
- **Estado:** 📋 Pendiente
- **Requisito:** RNF-03
- **Descripción:** Migrar de TCP plano a TLS 1.2+
- **Complejidad:** Alta (certificados, memoria)
- **Nota:** Evaluar si el backend lo requiere

---

#### FEAT-V7: Validación de Éxito RS-485
- **Estado:** 📋 Pendiente
- **Requisito:** RF-01
- **Descripción:** Contador de tasa de éxito (≥98%)

---

#### FIX-V6: Límite de Escaneos Diarios
- **Estado:** 📋 Pendiente
- **Requisito:** RF-14
- **Descripción:** Máximo 3 escaneos completos por día
- **Nota:** FIX-V2 mitiga el problema. Evaluar necesidad real.

---

## 📊 Resumen de Backlog Actualizado

| ID | Tipo | Prioridad | Estado | Versión |
|----|------|-----------|--------|---------|
| FIX-V1 | Fix | - | ✅ Implementado | v2.0.2 |
| FIX-V2 | Fix | - | ✅ Implementado | v2.2.0 |
| FIX-V3 | Fix | - | ✅ Implementado | v2.3.0 |
| FIX-V4 | Fix | - | ✅ Implementado | v2.4.0 |
| FIX-V5 | Fix | � Media | 📋 Por evaluar | - |
| FIX-V6 | Fix | 🟡 Media | 📋 Pendiente | - |
| FEAT-V2 | Feat | - | ✅ Implementado | v2.1.0 |
| FEAT-V3 | Feat | - | ✅ Implementado | v2.3.0 |
| FEAT-V4 | Feat | - | ✅ Implementado | v2.5.0 |
| FEAT-V5 | Feat | 🟡 Media | 📝 Debug only | - |
| FEAT-V6 | Feat | ⚪ Baja | 📝 Documentado | - |
| **FEAT-V7** | **Feat** | **🟠 Alta** | **📝 Documentado** | **-** |

### Diagnóstico EMI
| Métrica | Resultado |
|---------|-----------|
| PCB 2 capas | ✅ Sin problemas detectados |
| UART corrupción | 0.0% |
| Recomendación | Continuar con diseño actual |

---

## 📝 Proceso para Nuevos Hallazgos

1. Identificar requisito incumplido o mejora necesaria
2. Agregar a este archivo con estado "Pendiente"
3. Crear archivo de especificación en `fixs-feats/fixs/` o `fixs-feats/feats/`
4. Implementar según metodología de FIXs
5. Actualizar estado a "Implementado" con versión

---

## 📁 Archivos Históricos

Documentos de auditorías anteriores archivados en `calidad/historico/`:
- `AUDITORIA_REQUISITOS_v2.0.2_2026-01-13.md`
- `REVISION_CODIGO_v2.2.0_2026-01-14.md`
