# 📘 CONTEXTO DEL PROYECTO 2026
## Guía Estratégica para el Desarrollo del Firmware Datalogger

**Documento:** CONTEXTO_2026.md  
**Fecha de Creación:** 15 de Enero de 2026  
**Proyecto:** Datalogger ESP32-S3 + SIM7080 para Agricultura Inteligente  
**Versión Actual:** v2.2.0 (JAMR_4.5)

---

## 🎯 OBJETIVO DEL PROYECTO

Desarrollar un **firmware robusto y confiable** para un sistema embebido de monitoreo agrícola que garantice:

1. **Adquisición continua de datos** sin interrupciones
2. **Almacenamiento seguro** ante cualquier fallo
3. **Transmisión eficiente** con mínimo consumo energético
4. **Resiliencia operativa** en condiciones rurales extremas
5. **Modularidad** para integrar nuevos sensores sin refactorizar

---

## 📜 HISTORIAL DEL DESARROLLO

### Era JAMR_3 (Pre-2025)
- Base original: `sensores_elathia_fix_gps`
- Primeros fixes de watchdog y health data
- Lecciones documentadas en `LECCIONES_APRENDIDAS_JARM3.md`

### JAMR_4.4 - Desarrollo 2025
- **Versión final:** 4.4.16 (default-budget-pdp)
- **16 fixes implementados** en producción
- **Arquitectura:** Archivos monolíticos (.cpp/.h)
- **Logros principales:**
  - FIX-001 a FIX-007: Watchdog, Health, PDP, Presupuesto, Perfil LTE
  - FIX-008: Guardia de salud del módem
  - FIX-009: Perfil AUTO_LITE de bajo consumo
  - FIX-010 a FIX-016: Refactor módem, timeout coordinado, batería, etc.
- **Documentación:** Excelente (premisas, logs, reportes de calidad)
- **Limitación:** Complejidad acumulada por fixes superpuestos

### JAMR_4.5 - Desarrollo 2026 (Actual)
- **Versión actual:** v2.2.0 (fallback-operadora)
- **Arquitectura:** Modular FSM con `AppController`
- **Estructura:** src/ con módulos independientes
- **Mejoras implementadas:**
  - FEAT-V1: Sistema de Feature Flags
  - FEAT-V2: Cycle Timing (instrumentación de tiempos)
  - FIX-V1: Skip reset en configureOperator
  - FIX-V2: Fallback a escaneo de operadoras
- **Estado:** En desarrollo activo con backlog definido

---

## 🔧 HARDWARE DE LA TARJETA (datasheets)

### Componentes Principales

| Componente | Modelo | Función | Notas |
|------------|--------|---------|-------|
| **MCU** | ESP32-S3-WROOM-1-N8 | Procesador principal | WiFi + BLE integrado |
| **Módem** | SIM7080G | LTE Cat-M/NB-IoT + GPS | Comunicación celular |
| **RTC** | DS3231MZ+ | Tiempo real de precisión | I²C, batería CR1220 |
| **Batería** | CR1220-2 | Backup RTC | Slot SMD |

### Reguladores de Energía

| Componente | Función | Entrada | Salida |
|------------|---------|---------|--------|
| LM2596S-5.0RG | Regulador principal | 7-40V | 5V/3A |
| TLV62569DBVR (x2) | Buck DC-DC | 5V | 3.3V eficiente |
| MIC2288YD5-TR | Boost DC-DC | 2.5-5V | Salida ajustable |

### Interfaz de Sensores

| Conector | Tipo | Función |
|----------|------|---------|
| RS-485 | XH-4A | Modbus RTU (sensores industriales) |
| E/S1 | XH-4A | Entradas/salidas digitales |
| DHT-ADC1 | XH-4A | Sensor temperatura/humedad + ADC |
| DEBUG | XH-4A | Puerto serial para diagnóstico |
| BATERIA1 | XH-4A | Entrada de alimentación |
| GNDC1 | XH-4A | Conexión a tierra común |

### Transceptor RS-485
- Módulo: XY-017-CB (RS485-TTL)
- Control: DE/RE vía IO16/IO15
- Protección: ESD con PESD0603MS03-MS

### Conectividad RF
- Antenas: BWU.FL-IPEX1 (x2) para LTE + GPS
- Protección ESD: ESDA6V1W5 para líneas de RF

### Indicadores
- LEDs: 3x LED 0603 rojo para estado

---

## 📋 REQUISITOS PRINCIPALES (Documento Integrado v2.0)

### Principios Rectores

| ID | Principio | Descripción |
|----|-----------|-------------|
| PRINC-01 | Prioridad almacenamiento | El almacenamiento local SIEMPRE tiene prioridad sobre transmisión |
| PRINC-02 | Continuidad operativa | El sistema debe seguir funcionando aun en fallos severos |
| PRINC-03 | Modularidad | Integrar nuevos sensores sin modificar el núcleo |

### Requisitos Funcionales Clave

#### Adquisición (RF-01 a RF-03)
- ✅ Lectura continua RS-485: Tasa éxito ≥98% en 3 intentos
- ✅ Lectura sensores locales: No bloqueante (max 50ms)
- ⚠️ Interfaz abstracta de sensor: Parcialmente implementada

#### Almacenamiento (RF-04 a RF-06)
- ✅ Almacenamiento transaccional: Buffer persistente en LittleFS
- ⚠️ Logs críticos persistentes: Por implementar (FEAT-V3)
- ⚠️ Modo solo-adquisición por baja batería: Por implementar (FIX-V3)

#### Transmisión (RF-07 a RF-09)
- ✅ Transmisión no bloqueante: Implementado con FSM
- ✅ Borrado solo tras ACK: Buffer marca procesados, no borra
- ⚠️ Reanudación tras recuperación de carga: Por implementar

#### Selección de Operador (RF-10 a RF-14)
- ✅ Descubrimiento de operadores: Escaneo completo
- ✅ Conexión con operador preferido: NVS persistente
- ✅ Fallback a mejor operador: FIX-V2 implementado
- ✅ Memorización persistente: NVS funcional
- ⚠️ Limitación de escaneos (3/día): Por implementar (FIX-V4)

#### Recuperación (RF-15 a RF-17)
- ⚠️ Autorestablecimiento controlado: Parcial (FIX-V6 pendiente)
- ⚠️ Extracción por puerto serie: Por implementar (FEAT-V4)
- ⚠️ Consulta interactiva: Por implementar (FEAT-V4)

### Requisitos No Funcionales

| ID | Requisito | Estado | Notas |
|----|-----------|--------|-------|
| RNF-01 | Deep sleep <300µA | ✅ | ~10µA logrado |
| RNF-02 | Protección brown-out | ⚠️ | FIX-V5 pendiente |
| RNF-03 | Cifrado TLS 1.2+ | ⚠️ | FEAT-V5 pendiente |

### Requisitos de Interfaz Hardware

| ID | Requisito | Estado |
|----|-----------|--------|
| RI-01 | UART ESP32↔SIM7080 (115200 8N1) | ✅ |
| RI-02 | Control DE/RE RS-485 (<10µs) | ✅ |

### Requisitos Ambientales

| ID | Condición | Rango | Estado |
|----|-----------|-------|--------|
| REA-01 | Temperatura | -10°C a 60°C | ✅ Hardware soporta |
| REA-01 | Humedad | 5-95% | ✅ |
| REA-01 | Operación sin intervención | ≥30 días | 🔄 En validación |

---

## 🏗️ ARQUITECTURA JAMR_4.5

### Máquina de Estados (FSM)

```
Boot → BleOnly (solo primer encendido)
       ↓
    Cycle_ReadSensors
       ↓
    Cycle_Gps (solo primer ciclo)
       ↓
    Cycle_GetICCID
       ↓
    Cycle_BuildFrame
       ↓
    Cycle_BufferWrite
       ↓
    Cycle_SendLTE
       ↓
    Cycle_CompactBuffer
       ↓
    Cycle_Sleep → (despertar) → Cycle_ReadSensors
```

### Estructura de Módulos

```
JAMR_4.5/
├── JAMR_4.5.ino              # Entry point
├── AppController.cpp/.h       # Orquestador FSM
├── src/
│   ├── FeatureFlags.h         # Feature flags centralizados
│   ├── version_info.h         # Versionamiento centralizado
│   ├── CycleTiming.h          # Instrumentación de tiempos
│   ├── DebugConfig.h          # Sistema debug homogéneo
│   ├── data_buffer/           # Buffer + BLE
│   ├── data_format/           # Construcción de tramas
│   ├── data_gps/              # GPS/GNSS
│   ├── data_lte/              # Comunicación LTE
│   ├── data_sensors/          # ADC, I2C, RS485
│   ├── data_sleepwakeup/      # Deep sleep
│   └── data_time/             # RTC
└── data/
    └── buffer.txt             # Almacenamiento persistente
```

### Formato de Trama

```
$,<iccid>,<epoch>,<lat>,<lng>,<alt>,<var1>,<var2>,<var3>,<var4>,<var5>,<var6>,<var7>,#
```

- Variables 1-4: Registros RS485 (Modbus)
- Variables 5-6: Temperatura/Humedad (I2C)
- Variable 7: Voltaje batería (ADC)
- Codificación: Base64 antes de enviar

---

## 🔄 EVOLUCIÓN DE COMMITS 2026

| Hash | Descripción | Cambios Clave |
|------|-------------|---------------|
| `cc223a4` | FIX-V2 v2.2.0 completado | Fallback operadora implementado |
| `d67b877` | FIX-V2: Fallback a escaneo | AppController.cpp, FeatureFlags.h |
| `c312211` | FEAT-V2: Cycle Timing | CycleTiming.h nuevo |
| `fb18b71` | FEAT-V1 + FIX-V1 merge | Feature flags + skip reset |
| `503fe04` | FEAT-V1: Feature Flags | FeatureFlags.h nuevo |
| `f7630ca` | Aprendizajes JAMR_4.4 | Documentación técnica |
| `741695f` | Sistema versiones | version_info.h nuevo |

---

## 📊 COMPARATIVA JAMR_4.4 vs JAMR_4.5

| Característica | JAMR_4.4 | JAMR_4.5 | Ventaja |
|----------------|----------|----------|---------|
| Arquitectura | Monolítica | FSM Modular | JAMR_4.5 ✅ |
| Feature Flags | ✅ | ✅ | Igual |
| Versión en payload | ✅ | ⚠️ Pendiente | JAMR_4.4 |
| Health checkpoints | ✅ | ⚠️ Pendiente | JAMR_4.4 |
| Presupuesto ciclo | ✅ | ⚠️ Pendiente | JAMR_4.4 |
| Detección zombie | ✅ | ⚠️ Pendiente | JAMR_4.4 |
| Operadora persistente | LittleFS | NVS | JAMR_4.5 ✅ |
| Buffer resiliente | ⚠️ | ✅ | JAMR_4.5 ✅ |
| GPS persistente | ⚠️ | ✅ NVS | JAMR_4.5 ✅ |
| Modularidad código | ⚠️ | ✅ | JAMR_4.5 ✅ |
| Testing documentado | ✅ | 🔄 | JAMR_4.4 |

---

## 📋 BACKLOG DE MEJORAS 2026

### 🔴 Prioridad Crítica

| ID | Tipo | Descripción | Requisito |
|----|------|-------------|-----------|
| FIX-V2 | Fix | ✅ Fallback a escaneo operadoras | RF-12 |
| FIX-V3 | Fix | Modo solo-adquisición por baja batería | RF-06, RF-09 |
| FIX-V4 | Fix | Limitación escaneos (3/día) | RF-14 |

### 🟠 Prioridad Alta

| ID | Tipo | Descripción | Requisito |
|----|------|-------------|-----------|
| FIX-V5 | Fix | Protección brown-out | RNF-02 |
| FIX-V6 | Fix | Recuperación escalonada | RF-15 |
| FEAT-V3 | Feat | Sistema logs críticos (10 eventos) | RF-05 |
| FEAT-V4 | Feat | CLI mantenimiento serial | RF-16, RF-17 |

### 🟡 Prioridad Media

| ID | Tipo | Descripción | Requisito |
|----|------|-------------|-----------|
| FEAT-V5 | Feat | Conexión TLS/SSL | RNF-03 |
| FEAT-V6 | Feat | Validación éxito RS-485 (≥98%) | RF-01 |
| FEAT-V7 | Feat | Versión firmware en payload | REQ-004 (JAMR_4.4) |
| FEAT-V8 | Feat | Health checkpoints básicos | FIX-4 (JAMR_4.4) |

---

## 🎓 PREMISAS DE DESARROLLO

### Filosofía Central
> **"Si no lo toco, no lo rompo. Si lo toco, lo valido. Si falla, lo deshabilito."**

### 10 Premisas Fundamentales

1. **Aislamiento Total**: Un branch por fix, merge solo tras validación
2. **Cambios Mínimos**: Menor superficie de cambio = menor riesgo
3. **Defaults Seguros**: Si el fix falla, comportamiento original
4. **Feature Flags**: Todo fix deshabilitado por defecto vía compilación
5. **Sin Cambiar Lógica Existente**: Agregar, no reemplazar
6. **Testing Gradual**: Compilación → Unitario → 1 ciclo → 24h → 7d campo
7. **Documentación Obligatoria**: README, Plan, Log, Reporte Final
8. **Rollback en 5 min**: Cambiar flag y recompilar
9. **Observabilidad**: Logs suficientes para diagnóstico remoto
10. **Trazabilidad**: Cada fix vinculado a requisito específico

### Pirámide de Testing

```
         ┌─────────────┐
         │  Campo 7d   │  ← Validación final
         └─────────────┘
        ┌───────────────┐
        │  Hardware 24h │  ← Ciclos reales
        └───────────────┘
      ┌───────────────────┐
      │  Hardware 1 ciclo │  ← Funcionalidad completa
      └───────────────────┘
    ┌─────────────────────────┐
    │  Test unitario (5 min)  │  ← Función específica
    └─────────────────────────┘
  ┌───────────────────────────────┐
  │  Compilación (2 min)          │  ← Sin errores
  └───────────────────────────────┘
```

---

## 🔑 LECCIONES APRENDIDAS DE JAMR_4.4

### Lo Que Funcionó Bien
1. **Premisas de desarrollo** - Metodología sólida y probada
2. **Presupuesto de ciclo** - Evita ciclos infinitos de reintentos
3. **Health del módem** - Detecta estados zombie
4. **Perfil LTE persistente** - Ahorra energía en reconexiones
5. **Documentación exhaustiva** - Trazabilidad completa

### Lo Que Puede Mejorar en JAMR_4.5
1. **Versión en payload** - El servidor no sabe qué versión tiene cada dispositivo
2. **Health checkpoints** - No hay diagnóstico remoto de fallos
3. **Confirmación backend** - "SEND OK" no garantiza que llegó al servidor
4. **Power-cycle del módem** - Debería mantenerse en bajo consumo, no apagar

### Sistemas a Portar de JAMR_4.4
1. `gsm_comm_budget.h` → Presupuesto global de ciclo
2. `gsm_health.h` → Detección de estados zombie
3. Health checkpoints en payload
4. Versión de firmware en trama

---

## 🎯 ROADMAP 2026

### Q1 2026 (Enero - Marzo)
- [x] ~~FIX-V1: Skip reset PDP~~
- [x] ~~FIX-V2: Fallback operadoras~~
- [x] ~~FEAT-V1: Feature Flags~~
- [x] ~~FEAT-V2: Cycle Timing~~
- [ ] FIX-V3: Modo baja batería
- [ ] FIX-V4: Límite escaneos

### Q2 2026 (Abril - Junio)
- [ ] FIX-V5: Protección brown-out
- [ ] FIX-V6: Recuperación escalonada
- [ ] FEAT-V3: Logs críticos
- [ ] FEAT-V4: CLI serial

### Q3 2026 (Julio - Septiembre)
- [ ] FEAT-V5: TLS/SSL
- [ ] FEAT-V7: Versión en payload
- [ ] FEAT-V8: Health checkpoints
- [ ] Validación 30 días campo

### Q4 2026 (Octubre - Diciembre)
- [ ] Optimización consumo energético
- [ ] Documentación final
- [ ] Release v3.0.0 estable

---

## 📁 ARCHIVOS DE REFERENCIA

### Requisitos y Reglas
- `requisitos y reglas/requisitos.txt` - Documento integrado de requisitos v2.0

### Memoria de Proyecto
- `memoria_de_proyecto/APRENDIZAJES_JAMR44.md` - Lecciones de JAMR_4.4
- `memoria_de_proyecto/MEMORIA_SENSORES_RV03.md` - Memoria técnica completa

### Calidad JAMR_4.5
- `JAMR_4.5/calidad/HALLAZGOS_PENDIENTES.md` - Backlog de fixes/features
- `JAMR_4.5/calidad/AUDITORIA_REQUISITOS.md` - Trazabilidad código-requisitos

### Metodología
- `JAMR_4.5/fixs-feats/PREMISAS_DE_FIXS.md` - Guía de desarrollo seguro
- `JAMR_4.5/fixs-feats/PLANTILLA.md` - Template para documentar fixes

### Código Clave
- `JAMR_4.5/src/FeatureFlags.h` - Control de features
- `JAMR_4.5/src/version_info.h` - Versionamiento centralizado
- `JAMR_4.5/AppController.cpp` - FSM principal

---

## ✅ CONCLUSIÓN

Este documento establece el **contexto estratégico** para el desarrollo 2026 del firmware Datalogger. Los puntos clave son:

1. **JAMR_4.4 fue exitoso** pero acumuló complejidad - las lecciones están documentadas
2. **JAMR_4.5 tiene mejor arquitectura** (modular, FSM) pero necesita portar sistemas de robustez
3. **Los requisitos están claros** en el documento integrado v2.0
4. **El hardware está bien documentado** en los datasheets
5. **La metodología de fixes es sólida** y debe seguirse estrictamente
6. **El backlog está priorizado** - ejecutar en orden

**Próximo paso inmediato:** Implementar FIX-V3 (modo baja batería) siguiendo las premisas establecidas.

---

*Documento generado: 15 de Enero de 2026*  
*Última actualización: 15 de Enero de 2026*
