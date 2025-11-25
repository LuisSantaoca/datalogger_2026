# FIX v2: Conectividad en Señal Baja (v4.1.0 → v4.2.x)

## 📋 Resumen

**Versiones:** v4.1.0 → v4.2.0 → v4.2.1 → ... → v4.2.7  
**Fecha inicio:** 30 Oct 2025  
**Problema identificado:** Operación ineficiente con RSSI 8-14 en zona rural  
**Estado:** 📝 Documentado, pendiente implementación  

---

## 🎯 Problema Identificado

**Análisis de logs 29 Oct 2025** (6403 líneas, 16 ciclos):
- RSSI promedio: **12.5** (señal pobre)
- 71.4% del tiempo en zona crítica/débil
- 100% fallos en 1er intento `AT+CPIN?` → 15s perdidos × 16 = 4 min/día
- Sin memoria entre reinicios → configuración óptima olvidada
- Timeout LTE fijo 60s → falla con RSSI < 10
- Búsqueda en 3 bandas → solo Band 4 existe → 30s desperdiciados

**Resultado:** Sistema funciona al 100% pero en límite operativo constante.

---

## 📦 Estrategia de Versiones: Fixes Incrementales

### Esquema de Versionamiento

```
v4.1.0 (actual)
    ↓
v4.2.0 → FIX #1: Persistencia Estado
    ↓
v4.2.1 → FIX #2: Timeout LTE Dinámico
    ↓
v4.2.2 → FIX #3: Init Módem Optimizado
    ↓
v4.2.3 → FIX #4: Banda LTE Inteligente
    ↓
v4.2.4 → FIX #5: Detección Degradación
    ↓
v4.2.5 → FIX #6: GPS Cache
    ↓
v4.2.6 → FIX #7: Fallback NB-IoT
    ↓
v4.2.7 → FIX #8: Métricas Remotas
```

**Nomenclatura archivos:**
- `FIX-4.2.0_*.md` → Documentos de v4.2.0
- `FIX-4.2.1_*.md` → Documentos de v4.2.1
- etc.

---

## 🗺️ Roadmap de Implementación

| Versión | Fix | Impacto | Tiempo Est. | Estado | Fecha Target |
|---------|-----|---------|-------------|--------|--------------|
| **v4.2.0** | Persistencia Estado | ⭐⭐⭐⭐⭐ | 2h + 1h test | 📝 Planificado | 1 Nov 2025 |
| **v4.2.1** | Timeout Dinámico | ⭐⭐⭐⭐⭐ | 3h + 2h test | ⏸️ Pendiente | 4 Nov 2025 |
| **v4.2.2** | Init Módem | ⭐⭐⭐⭐ | 2h + 1h test | ⏸️ Pendiente | 6 Nov 2025 |
| **v4.2.3** | Banda Inteligente | ⭐⭐⭐ | 4h + 2h test | ⏸️ Pendiente | 8 Nov 2025 |
| **v4.2.4** | Degradación | ⭐⭐⭐ | 4h + 1h test | ⏸️ Pendiente | 11 Nov 2025 |
| **v4.2.5** | GPS Cache | ⭐⭐ | 2h + 1h test | ⏸️ Pendiente | 13 Nov 2025 |
| **v4.2.6** | NB-IoT Fallback | ⭐⭐ | 3h + 3h test | ⏸️ Pendiente | 15 Nov 2025 |
| **v4.2.7** | Métricas Remotas | ⭐ | 6h + 2h test | ⏸️ Pendiente | 18 Nov 2025 |

**Tiempo total estimado:** 26h implementación + 13h testing = **39 horas (~1 mes)**

---

## 📊 Impacto Esperado por Versión

### v4.2.0 - Persistencia Estado

**Cambio:** Guardar RSSI, banda exitosa, GPS, tiempos en NVS

| Métrica | v4.1.0 | v4.2.0 | Mejora |
|---------|--------|--------|--------|
| Tiempo ciclo | 198s | 178s | -20s (-10%) |
| Éxito conexión | 93.8% | 97% | +3.2% |
| Consumo batería | 50mA | 46mA | -8% |

**Archivos:**
- `FIX-4.2.0_PLAN_EJECUCION.md`
- `FIX-4.2.0_LOG_PASO1.md`
- `FIX-4.2.0_LOG_PASO2.md`
- `FIX-4.2.0_LOG_PASO3.md`
- `FIX-4.2.0_VALIDACION_HARDWARE.md`
- `FIX-4.2.0_REPORTE_FINAL.md`

---

### v4.2.1 - Timeout LTE Dinámico

**Cambio:** Ajustar timeout según RSSI (60-120s)

| Métrica | v4.2.0 | v4.2.1 | Mejora |
|---------|--------|--------|--------|
| Fallos timeout | ~10% | ~2% | -8% |
| Tiempo conexión | Variable | Óptimo | Adaptativo |

**Archivos:**
- `FIX-4.2.1_PLAN_EJECUCION.md`
- `FIX-4.2.1_VALIDACION_HARDWARE.md`
- `FIX-4.2.1_REPORTE_FINAL.md`

---

### v4.2.2 - Init Módem Optimizado

**Cambio:** Delay post-PWRKEY 1s→5s, timeout AT+CPIN? 5s→20s

| Métrica | v4.2.1 | v4.2.2 | Mejora |
|---------|--------|--------|--------|
| Tiempo init | Variable | Consistente | -15s promedio |
| Fallos CPIN | 100% 1er intento | 5% | -95% |

**Archivos:**
- `FIX-4.2.2_PLAN_EJECUCION.md`
- `FIX-4.2.2_VALIDACION_HARDWARE.md`
- `FIX-4.2.2_REPORTE_FINAL.md`

---

### v4.2.3 - Banda LTE Inteligente

**Cambio:** Intentar banda guardada primero, luego búsqueda estándar

| Métrica | v4.2.2 | v4.2.3 | Mejora |
|---------|--------|--------|--------|
| Tiempo búsqueda banda | 30s | 5s | -25s (-83%) |
| Éxito 1er intento | 33% | 95% | +62% |

**Archivos:**
- `FIX-4.2.3_PLAN_EJECUCION.md`
- `FIX-4.2.3_VALIDACION_HARDWARE.md`
- `FIX-4.2.3_REPORTE_FINAL.md`

---

### v4.2.4 - Detección Degradación

**Cambio:** Monitoreo RSSI en ventana deslizante, alertas preventivas

| Métrica | v4.2.3 | v4.2.4 | Mejora |
|---------|--------|--------|--------|
| Detección temprana | No | Sí | Preventivo |
| Alertas generadas | 0 | Según umbral | Diagnóstico |

**Archivos:**
- `FIX-4.2.4_PLAN_EJECUCION.md`
- `FIX-4.2.4_VALIDACION_HARDWARE.md`
- `FIX-4.2.4_REPORTE_FINAL.md`

---

### v4.2.5 - GPS Cache

**Cambio:** Reutilizar última posición si < 24h (device estático)

| Métrica | v4.2.4 | v4.2.5 | Mejora |
|---------|--------|--------|--------|
| Tiempo GPS | 45s | 25s | -20s (-44%) |
| Intentos GPS | Variable | 1 (con cache) | Optimizado |

**Archivos:**
- `FIX-4.2.5_PLAN_EJECUCION.md`
- `FIX-4.2.5_VALIDACION_HARDWARE.md`
- `FIX-4.2.5_REPORTE_FINAL.md`

---

### v4.2.6 - Fallback NB-IoT

**Cambio:** Intentar NB-IoT si LTE Cat-M falla 3 veces

| Métrica | v4.2.5 | v4.2.6 | Mejora |
|---------|--------|--------|--------|
| Éxito en señal extrema | 93% | 96% | +3% |
| Opciones conectividad | 1 (Cat-M) | 2 (Cat-M + NB) | +100% |

**Archivos:**
- `FIX-4.2.6_PLAN_EJECUCION.md`
- `FIX-4.2.6_VALIDACION_HARDWARE.md`
- `FIX-4.2.6_REPORTE_FINAL.md`

---

### v4.2.7 - Métricas Remotas

**Cambio:** Agregar RSSI, tiempos LTE/GPS a payload

| Métrica | v4.2.6 | v4.2.7 | Mejora |
|---------|--------|--------|--------|
| Diagnóstico remoto | Limitado | Completo | +100% |
| Visibilidad Grafana | Básica | Avanzada | Mejorada |

**Archivos:**
- `FIX-4.2.7_PLAN_EJECUCION.md`
- `FIX-4.2.7_VALIDACION_HARDWARE.md`
- `FIX-4.2.7_REPORTE_FINAL.md`

---

## 📈 Impacto Acumulado

### Evolución de Métricas Clave

| Métrica | v4.1.0 | v4.2.0 | v4.2.1 | v4.2.2 | v4.2.3 | v4.2.7 Final |
|---------|--------|--------|--------|--------|--------|--------------|
| Tiempo ciclo | 198s | 178s | 175s | 160s | 135s | **130s (-34%)** |
| Éxito TX | 93.8% | 97% | 98.5% | 98.8% | 99% | **99.5% (+5.7%)** |
| Consumo | 50mA | 46mA | 45mA | 43mA | 41mA | **40mA (-20%)** |
| GPS tiempo | 45s | 40s | 40s | 40s | 40s | **25s (-44%)** |
| LTE tiempo | 90s | 70s | 65s | 50s | 45s | **40s (-56%)** |

**Mejora combinada v4.1.0 → v4.2.7:**
- ⏱️ Tiempo: 198s → 130s = **-68s (-34%)**
- ✅ Éxito: 93.8% → 99.5% = **+5.7%**
- 🔋 Batería: 50mA → 40mA = **-20%**

---

## 🔄 Proceso de Implementación por Versión

### Ciclo Estándar (Cada Fix)

```
┌─────────────────────────────────────────┐
│ 1. PLANIFICACIÓN (30 min)              │
│    - FIX-4.2.X_PLAN_EJECUCION.md       │
│    - Definir alcance, archivos, pasos  │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 2. IMPLEMENTACIÓN (2-6h según fix)     │
│    - Branch: fix-4.2.X-nombre           │
│    - Logs por paso                      │
│    - FIX-4.2.X_LOG_PASO1/2/3.md        │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 3. TESTING GRADUAL (1-3h)              │
│    - Compilación (2 min)                │
│    - Test unitario (5 min)              │
│    - Hardware 1 ciclo (20 min)          │
│    - Hardware 24h (1 día)               │
│    - FIX-4.2.X_VALIDACION_HARDWARE.md  │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 4. VALIDACIÓN Y REPORTE (30 min)       │
│    - Comparación con baseline           │
│    - Métricas objetivas                 │
│    - FIX-4.2.X_REPORTE_FINAL.md        │
└─────────────────────────────────────────┘
              ↓
┌─────────────────────────────────────────┐
│ 5. MERGE Y TAG (10 min)                │
│    - Merge a main                       │
│    - Tag: v4.2.X                        │
│    - Push a remoto                      │
└─────────────────────────────────────────┘
              ↓
        Siguiente versión
```

---

## 📂 Estructura de Archivos

```
fix_v2_conexion/
├── README.md (este archivo)
│
├── FIX_SEÑAL_BAJA_RURAL.md (8 fixes con código completo)
│
├── v4.2.0_persistencia/
│   ├── FIX-4.2.0_PLAN_EJECUCION.md
│   ├── FIX-4.2.0_LOG_PASO1.md
│   ├── FIX-4.2.0_LOG_PASO2.md
│   ├── FIX-4.2.0_LOG_PASO3.md
│   ├── FIX-4.2.0_VALIDACION_HARDWARE.md
│   └── FIX-4.2.0_REPORTE_FINAL.md
│
├── v4.2.1_timeout_dinamico/
│   ├── FIX-4.2.1_PLAN_EJECUCION.md
│   ├── FIX-4.2.1_VALIDACION_HARDWARE.md
│   └── FIX-4.2.1_REPORTE_FINAL.md
│
├── v4.2.2_init_modem/
│   └── ...
│
├── v4.2.3_banda_inteligente/
│   └── ...
│
├── v4.2.4_degradacion/
│   └── ...
│
├── v4.2.5_gps_cache/
│   └── ...
│
├── v4.2.6_nbiot_fallback/
│   └── ...
│
└── v4.2.7_metricas_remotas/
    └── ...
```

---

## 🎯 Estado Actual

### Completado
- ✅ Análisis exhaustivo logs (29 Oct 2025)
- ✅ Identificación 8 fixes prioritarios
- ✅ Código completo para cada fix
- ✅ Análisis de riesgos detallado
- ✅ Premisas estratégicas definidas
- ✅ Plan v4.2.0 creado

### En Progreso
- 🔄 Preparación implementación v4.2.0

### Pendiente
- ⏸️ Implementar v4.2.0 (Persistencia)
- ⏸️ Implementar v4.2.1-v4.2.7
- ⏸️ Validación campo 7 días cada versión

---

## 🎓 Lecciones de FIX v1 (Watchdog)

### Aplicadas en v2
✅ **Branch dedicado** - Un branch por versión  
✅ **Feature flags** - Rollback instantáneo  
✅ **Defaults seguros** - Funciona sin cache si falla  
✅ **Testing gradual** - 5 capas de validación  
✅ **Métricas baseline** - Comparación objetiva  
✅ **Documentación exhaustiva** - Logs de cada paso  

---

## 🔗 Relación con Otros Componentes

**Depende de:**
- ✅ FIX-1 (Watchdog) - v4.1.0 estable
- ✅ REQ-004 (Versionamiento) - 3 bytes implementado

**Requiere:**
- Backend compatible (ya validado)
- NVS disponible (ESP32-S3 tiene 512KB)
- Testing en zona rural con RSSI 8-14

**Habilita:**
- Operación confiable en zonas marginales
- Diagnóstico remoto avanzado
- Reducción consumo batería 20%

---

## 📞 Contacto y Referencias

**Responsable:** Luis Santaoca  
**Fecha última actualización:** 30 Oct 2025  
**Versión firmware actual:** v4.1.0-JAMR4-VERSION  
**Próxima versión:** v4.2.0 (Persistencia Estado)  
**Fecha target v4.2.0:** 1 Nov 2025  

**Documentos relacionados:**
- `../PREMISAS_DE_FIXS.md` - Estrategia general
- `FIX_SEÑAL_BAJA_RURAL.md` - Código completo 8 fixes
- `../../hallazgos/Analisis/ANALISIS_CONSOLIDADO_20251029.md` - Análisis logs
- `../SNAPSHOT_20251029.md` - Estado v4.1.0

**Repositorio:**
- GitHub: `LuisSantaoca/JAMR_4`
- Branch actual: `main` (v4.1.0)
- Branch siguiente: `fix-4.2.0-persistencia-estado`
