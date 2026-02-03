# Checklist de Validación en Campo - JAMR_4.5

**Proyecto:** JAMR_4.5  
**Versión Inicial:** v2.5.0 (periodic-restart)  
**Fecha Inicio:** 2026-01-28  
**Fecha Fin Esperada:** 2026-02-27  
**Objetivo:** Validar estabilidad operativa antes de producción rural

---

## 📋 Resumen de Criterios de Aceptación

| Criterio | Mínimo Requerido | Estado |
|----------|------------------|--------|
| Días de operación continua | ≥ 30 días | ⬜ Pendiente |
| Ciclos exitosos totales | ≥ 500 ciclos | ⬜ Pendiente |
| Tasa de éxito transmisión | ≥ 95% | ⬜ Pendiente |
| Recuperaciones de falla | ≥ 1 documentada | ⬜ Pendiente |
| Prueba brown-out | 1 superada | ⬜ Pendiente |
| Prueba cobertura intermitente | 1 superada | ⬜ Pendiente |
| Reinicio preventivo 24h | ≥ 29 exitosos | ⬜ Pendiente |

---

## 🔧 Pre-Requisitos de Instalación

### Hardware
- [ ] ESP32-S3 con firmware v2.5.0 flasheado
- [ ] SIM7080G con SIM activa (datos)
- [ ] Batería cargada (medir voltaje inicial: ____V)
- [ ] Panel solar conectado (si aplica)
- [ ] Sensores RS-485 conectados y respondiendo
- [ ] Antenas LTE y GPS instaladas

### Software/Configuración
- [ ] `FEAT_V4_PERIODIC_RESTART` = 1 (activo)
- [ ] `FEAT_V4_PERIODIC_RESTART_HOURS` = 24
- [ ] `FIX_V3_LOW_BATTERY_MODE` = 1 (activo)
- [ ] `DEBUG_LEVEL` configurado para logs mínimos
- [ ] Servidor backend accesible y registrando

### Documentación Inicial
- [ ] Serial del dispositivo: ________________
- [ ] ICCID de SIM: ________________
- [ ] Ubicación de instalación: ________________
- [ ] Fecha/hora de encendido: ________________
- [ ] Voltaje batería inicial: ____V
- [ ] Señal LTE inicial (RSRP): ____dBm

---

## 📅 Registro Diario de Operación

### Semana 1 (Días 1-7)

| Día | Fecha | Ciclos OK | Ciclos Fail | Voltaje | RSRP | Restart 24h | Notas |
|-----|-------|-----------|-------------|---------|------|-------------|-------|
| 1 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 2 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 3 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 4 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 5 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 6 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 7 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |

**Subtotal Semana 1:** ___ ciclos OK / ___ ciclos totales = ___% éxito

---

### Semana 2 (Días 8-14)

| Día | Fecha | Ciclos OK | Ciclos Fail | Voltaje | RSRP | Restart 24h | Notas |
|-----|-------|-----------|-------------|---------|------|-------------|-------|
| 8 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 9 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 10 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 11 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 12 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 13 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 14 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |

**Subtotal Semana 2:** ___ ciclos OK / ___ ciclos totales = ___% éxito

---

### Semana 3 (Días 15-21)

| Día | Fecha | Ciclos OK | Ciclos Fail | Voltaje | RSRP | Restart 24h | Notas |
|-----|-------|-----------|-------------|---------|------|-------------|-------|
| 15 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 16 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 17 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 18 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 19 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 20 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 21 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |

**Subtotal Semana 3:** ___ ciclos OK / ___ ciclos totales = ___% éxito

---

### Semana 4 (Días 22-30)

| Día | Fecha | Ciclos OK | Ciclos Fail | Voltaje | RSRP | Restart 24h | Notas |
|-----|-------|-----------|-------------|---------|------|-------------|-------|
| 22 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 23 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 24 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 25 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 26 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 27 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 28 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 29 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |
| 30 | ______ | ___ | ___ | ___V | ___dBm | ⬜ | |

**Subtotal Semana 4:** ___ ciclos OK / ___ ciclos totales = ___% éxito

---

## 🧪 Pruebas Específicas Requeridas

### 1. Prueba de Reinicio Preventivo (FEAT-V4)
**Objetivo:** Verificar que el dispositivo reinicia automáticamente cada 24h

| # | Fecha/Hora Esperada | Fecha/Hora Real | ¿Exitoso? | Tramas en buffer post-reinicio |
|---|---------------------|-----------------|-----------|-------------------------------|
| 1 | __________________ | ________________ | ⬜ Sí / ⬜ No | ___ |
| 2 | __________________ | ________________ | ⬜ Sí / ⬜ No | ___ |
| 3 | __________________ | ________________ | ⬜ Sí / ⬜ No | ___ |

**Criterio:** ≥ 29/30 reinicios exitosos sin pérdida de datos

---

### 2. Prueba de Brown-out Controlado
**Objetivo:** Verificar comportamiento con voltaje bajo

**Procedimiento:**
1. Desconectar panel solar
2. Esperar descarga de batería hasta < 3.3V
3. Observar activación de FIX-V3 (modo batería baja)
4. Reconectar panel solar
5. Verificar recuperación automática

| Paso | Fecha/Hora | Voltaje | Observación |
|------|------------|---------|-------------|
| Inicio prueba | ________ | ___V | |
| FIX-V3 activado | ________ | ___V | ⬜ Modem desactivado |
| Voltaje mínimo | ________ | ___V | |
| Recuperación | ________ | ___V | ⬜ Transmisión reanudada |

**Resultado:** ⬜ PASS / ⬜ FAIL  
**Notas:** ________________________________________________

---

### 3. Prueba de Cobertura Intermitente
**Objetivo:** Verificar fallback de operadora y buffering

**Procedimiento:**
1. Bloquear antena LTE parcialmente (simular cobertura débil)
2. Observar comportamiento durante 3-5 ciclos
3. Verificar:
   - Datos almacenados en buffer
   - Intento de fallback a otra operadora (FIX-V2)
   - Recuperación al restaurar antena

| Evento | Fecha/Hora | Observación |
|--------|------------|-------------|
| Antena bloqueada | ________ | |
| Falla transmisión detectada | ________ | Ciclo #___ |
| Fallback operadora activado | ________ | ⬜ Sí / ⬜ No |
| Antena restaurada | ________ | |
| Transmisión recuperada | ________ | Ciclo #___ |
| Buffer transmitido completo | ________ | ⬜ Sí / ⬜ No |

**Resultado:** ⬜ PASS / ⬜ FAIL  
**Notas:** ________________________________________________

---

### 4. Prueba de Recuperación de Falla
**Objetivo:** Documentar al menos 1 recuperación automática de cualquier falla

| # | Fecha/Hora | Tipo de Falla | Acción del Sistema | ¿Recuperó? | Ciclos hasta recuperar |
|---|------------|---------------|--------------------|-----------|-----------------------|
| 1 | __________ | _____________ | __________________ | ⬜ Sí / ⬜ No | ___ |
| 2 | __________ | _____________ | __________________ | ⬜ Sí / ⬜ No | ___ |
| 3 | __________ | _____________ | __________________ | ⬜ Sí / ⬜ No | ___ |

**Tipos de falla esperables:**
- Timeout GPS
- Falla conexión LTE
- Error PDP context
- Timeout servidor
- Falla sensor RS-485

---

## 📊 Métricas Finales (Día 30)

### Operación General
| Métrica | Valor | Criterio | ¿Cumple? |
|---------|-------|----------|----------|
| Días operativos | ___/30 | ≥ 30 | ⬜ |
| Total ciclos exitosos | ___ | ≥ 500 | ⬜ |
| Total ciclos fallidos | ___ | < 25 | ⬜ |
| Tasa de éxito | ___% | ≥ 95% | ⬜ |
| Reinicios 24h exitosos | ___/30 | ≥ 29 | ⬜ |

### Pruebas Específicas
| Prueba | Resultado | ¿Cumple? |
|--------|-----------|----------|
| Brown-out controlado | ⬜ PASS / ⬜ FAIL | ⬜ |
| Cobertura intermitente | ⬜ PASS / ⬜ FAIL | ⬜ |
| Recuperación documentada | ⬜ ≥1 / ⬜ 0 | ⬜ |

### Estabilidad de Recursos
| Recurso | Valor Inicial | Valor Final | Tendencia |
|---------|---------------|-------------|-----------|
| Voltaje batería | ___V | ___V | ⬜ Estable / ⬜ Degradando |
| RSRP señal | ___dBm | ___dBm | ⬜ Estable / ⬜ Degradando |
| Tamaño buffer promedio | ___ líneas | ___ líneas | ⬜ Estable / ⬜ Creciendo |

---

## ✅ Veredicto Final

### Checklist de Aprobación

- [ ] ≥ 30 días de operación continua
- [ ] ≥ 500 ciclos exitosos
- [ ] Tasa de éxito ≥ 95%
- [ ] ≥ 1 recuperación de falla documentada
- [ ] Prueba brown-out PASS
- [ ] Prueba cobertura intermitente PASS
- [ ] ≥ 29 reinicios preventivos exitosos
- [ ] Sin pérdida de datos por reinicio
- [ ] Buffer no crece indefinidamente

### Decisión

| Resultado | Acción |
|-----------|--------|
| ⬜ **TODOS los criterios cumplidos** | ✅ JAMR_4.5 aprobado para producción rural |
| ⬜ **1-2 criterios no cumplidos** | ⚠️ Requiere análisis y posible FIX específico |
| ⬜ **≥3 criterios no cumplidos** | ❌ JAMR_4.5 NO aprobado, revisar arquitectura |

---

## 📝 Registro de Incidentes

| # | Fecha/Hora | Descripción | Severidad | Acción Tomada | Resuelto |
|---|------------|-------------|-----------|---------------|----------|
| 1 | __________ | ___________ | 🔴/🟠/🟡 | _____________ | ⬜ |
| 2 | __________ | ___________ | 🔴/🟠/🟡 | _____________ | ⬜ |
| 3 | __________ | ___________ | 🔴/🟠/🟡 | _____________ | ⬜ |
| 4 | __________ | ___________ | 🔴/🟠/🟡 | _____________ | ⬜ |
| 5 | __________ | ___________ | 🔴/🟠/🟡 | _____________ | ⬜ |

---

## 🔗 Referencias

- [AUDITORIA_REQUISITOS.md](AUDITORIA_REQUISITOS.md) - Requisitos pendientes
- [HALLAZGOS_PENDIENTES.md](HALLAZGOS_PENDIENTES.md) - Backlog de FIXs
- [REVISION_CODIGO_v2.2.0_2026-01-14.md](REVISION_CODIGO_v2.2.0_2026-01-14.md) - Bugs conocidos

---

**Responsable de Validación:** ____________________  
**Firma:** ____________________  
**Fecha de Cierre:** ____________________
