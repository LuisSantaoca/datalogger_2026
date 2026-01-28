# Auditoría de Trazabilidad Requisitos-Código (ATRC)

**Proyecto:** sensores_rv03  
**Versión Firmware:** v2.0.2  
**Fecha Auditoría:** 2026-01-13  
**Auditor:** Sistema de Calidad

---

## 📊 Resumen Ejecutivo

| Estado | Cantidad | Porcentaje |
|--------|----------|------------|
| ✅ Cumple | 10 | 40% |
| ⚠️ Parcial | 6 | 24% |
| ❌ No Cumple | 9 | 36% |
| **Total Requisitos** | **25** | 100% |

### Prioridad de Hallazgos

| Criticidad | Cantidad | IDs |
|------------|----------|-----|
| 🔴 Crítica | 3 | RF-06, RF-12, RF-14 |
| 🟠 Alta | 4 | RF-05, RF-15, RF-16, RF-17 |
| 🟡 Media | 2 | RNF-02, RNF-03 |

---

## 📋 Matriz de Trazabilidad Completa

### 1. PRINCIPIOS RECTORES

| ID | Requisito | Estado | Archivo | Evidencia |
|----|-----------|--------|---------|-----------|
| PRINC-01 | Prioridad almacenamiento sobre transmisión | ✅ Cumple | AppController.cpp | L752-773: `Cycle_BufferWrite` → `Cycle_SendLTE`. Primero guarda, luego transmite |
| PRINC-02 | Continuidad en fallos críticos | ⚠️ Parcial | AppController.cpp | Transmisión falla → continúa ciclo. PERO: No hay logs críticos persistentes |
| PRINC-03 | Diseño modular y escalable | ✅ Cumple | src/ | Estructura modular: data_sensors/, data_lte/, data_buffer/, etc. |

---

### 2. REQUISITOS FUNCIONALES - ADQUISICIÓN

| ID | Requisito | Estado | Archivo | Evidencia/Gap |
|----|-----------|--------|---------|---------------|
| RF-01 | Lectura RS-485 (98% éxito, 3 intentos) | ⚠️ Parcial | RS485Module.cpp, AppController.cpp:L298-314 | Implementa descarte+promedio (10 muestras) pero NO tiene contador de éxito ni reintentos configurables |
| RF-02 | Lectura sensores locales (ADC, I2C) independiente de modem | ✅ Cumple | AppController.cpp:L619-648 | `Cycle_ReadSensors` ejecuta ANTES de LTE. Max blocking ~50ms por sensor |
| RF-03 | Interfaz abstracta de sensores | ✅ Cumple | ADCSensorModule.h, I2CSensorModule.h, RS485Module.h | Cada sensor: `begin()`, `readSensor()`, `getValue()`. Agregar sensor = nuevo archivo |

---

### 3. REQUISITOS FUNCIONALES - ALMACENAMIENTO

| ID | Requisito | Estado | Archivo | Evidencia/Gap |
|----|-----------|--------|---------|---------------|
| RF-04 | Almacenamiento transaccional (atómico) | ✅ Cumple | BUFFERModule.cpp | `appendLine()` usa LittleFS con flush. No corrompe registros previos |
| RF-05 | Logs críticos persistentes | ❌ No Cumple | - | **NO IMPLEMENTADO.** No existe sistema de logs para watchdog, brown-out, fallos modem |
| RF-06 | Modo solo-adquisición por baja batería | ❌ No Cumple | - | **NO IMPLEMENTADO.** No hay UTS (Umbral Transmisión Segura), no desactiva modem por batería |

---

### 4. REQUISITOS FUNCIONALES - TRANSMISIÓN

| ID | Requisito | Estado | Archivo | Evidencia/Gap |
|----|-----------|--------|---------|---------------|
| RF-07 | Transmisión no bloqueante | ✅ Cumple | AppController.cpp:L752-773 | Adquisición completa ANTES de LTE. Fallo LTE no impide almacenamiento |
| RF-08 | Borrado solo tras ACK | ✅ Cumple | AppController.cpp:L426-432, BUFFERModule.cpp | `markLineAsProcessed()` solo si `sendTCPData()` retorna true |
| RF-09 | Reanudación tras recuperación batería | ❌ No Cumple | - | **NO IMPLEMENTADO.** No hay lógica de histéresis de batería para modem |

---

### 5. REQUISITOS FUNCIONALES - SELECCIÓN DE OPERADOR

| ID | Requisito | Estado | Archivo | Evidencia/Gap |
|----|-----------|--------|---------|---------------|
| RF-10 | Descubrimiento operadores (<60s) | ⚠️ Parcial | LTEModule.cpp:L617-667 | `testOperator()` toma ~120s por operadora (5 ops × 120s = 10 min) |
| RF-11 | Conexión prioritaria operador guardado | ✅ Cumple | AppController.cpp:L366-371 | Lee `lastOperator` de NVS, usa directamente si existe |
| RF-12 | Fallback a mejor operador si falla | ❌ No Cumple | AppController.cpp:L386-390 | **BUG DOCUMENTADO FIX-V2.** Si `configureOperator()` falla, retorna sin escanear |
| RF-13 | Memorización persistente operador exitoso | ✅ Cumple | AppController.cpp:L452-455 | `preferences.putUChar("lastOperator")` tras envío exitoso |
| RF-14 | Limitación escaneos (máx 3/día) | ❌ No Cumple | - | **NO IMPLEMENTADO.** No hay contador de escaneos ni límite diario |

---

### 6. REQUISITOS FUNCIONALES - RECUPERACIÓN Y RESILIENCIA

| ID | Requisito | Estado | Archivo | Evidencia/Gap |
|----|-----------|--------|---------|---------------|
| RF-15 | Autorestablecimiento controlado (<120s) | ⚠️ Parcial | LTEModule.cpp:L158-185 | `resetModem()` existe pero NO hay protocolo de recuperación escalonado (UART→modem→sistema) |
| RF-16 | Extracción datos/logs por serial | ❌ No Cumple | - | **NO IMPLEMENTADO.** No hay modo mantenimiento serial para exportar buffer/logs |
| RF-17 | Consulta interactiva estado por serial | ❌ No Cumple | - | **NO IMPLEMENTADO.** No hay comandos serial para consultar batería, RTC, memoria, modem |

---

### 7. REQUISITOS NO FUNCIONALES

| ID | Requisito | Estado | Archivo | Evidencia/Gap |
|----|-----------|--------|---------|---------------|
| RNF-01 | Consumo <300µA deep sleep | ⚠️ Parcial | SLEEPModule.cpp, config_data_sleepwakeup.h | Deep sleep implementado pero NO hay medición/validación de consumo |
| RNF-02 | Protección brown-out (modo seguro) | ❌ No Cumple | - | **NO IMPLEMENTADO.** No hay detección de UMO ni modo seguro |
| RNF-03 | Cifrado TLS 1.2+ (<8s handshake) | ❌ No Cumple | config_data_lte.h:L46-47 | Conexión TCP plana (sin TLS). `CAOPEN` usa TCP, no SSL |

---

### 8. REQUISITOS DE INTERFAZ HARDWARE

| ID | Requisito | Estado | Archivo | Evidencia/Gap |
|----|-----------|--------|---------|---------------|
| RI-01 | UART ESP32↔SIM7080 (115200 8N1) | ✅ Cumple | config_data_lte.h:L18, LTEModule.cpp:L31 | `_serial.begin(115200, SERIAL_8N1, RX, TX)` |
| RI-02 | Control DE/RE RS-485 (<10µs) | ⚠️ Parcial | RS485Module.cpp | Usa ModbusMaster con callback pero NO hay medición de timing |

---

### 9. REQUISITOS AMBIENTALES

| ID | Requisito | Estado | Archivo | Evidencia/Gap |
|----|-----------|--------|---------|---------------|
| REA-01 | Operación –10°C a 60°C, 30 días | ⚠️ Parcial | - | Depende de hardware. Firmware no tiene validaciones térmicas |

---

## 🔴 Hallazgos Críticos (Requieren FIX)

### HAL-001: Modo Baja Batería No Implementado (RF-06)
- **Severidad:** 🔴 Crítica
- **Descripción:** No existe UTS ni lógica para desactivar modem y mantener solo adquisición
- **Impacto:** Dispositivo puede morir completamente sin enviar datos acumulados
- **Acción:** Crear FIX-V3 para implementar gestión de energía

### HAL-002: Fallback Operadora Defectuoso (RF-12)
- **Severidad:** 🔴 Crítica
- **Descripción:** Si operadora guardada falla, no escanea alternativas
- **Impacto:** Dispositivo queda offline indefinidamente tras cambio de zona
- **Acción:** **FIX-V2 ya documentado**, pendiente implementación

### HAL-003: Sin Límite de Escaneos (RF-14)
- **Severidad:** 🔴 Crítica  
- **Descripción:** No hay contador ni límite de escaneos por día
- **Impacto:** Consumo excesivo de batería en zonas sin cobertura
- **Acción:** Crear FIX-V4 para contador de escaneos diario

---

## 🟠 Hallazgos Altos

### HAL-004: Sin Logs Críticos Persistentes (RF-05)
- **Descripción:** No se almacenan eventos de watchdog, brown-out, fallos
- **Acción:** FEAT para sistema de logging

### HAL-005: Sin Modo Mantenimiento Serial (RF-16, RF-17)
- **Descripción:** No hay interfaz serial para diagnóstico
- **Acción:** FEAT para CLI de mantenimiento

### HAL-006: Sin Protocolo de Recuperación Escalonado (RF-15)
- **Descripción:** Solo existe `resetModem()`, falta escalamiento
- **Acción:** FIX/FEAT para protocolo de recuperación

---

## 🟡 Hallazgos Medios

### HAL-007: Conexión Sin Cifrado (RNF-03)
- **Descripción:** TCP plano, sin TLS
- **Acción:** FEAT para migrar a SSL/TLS

### HAL-008: Sin Detección Brown-out (RNF-02)
- **Descripción:** No hay modo seguro ante caída de voltaje
- **Acción:** FIX para protección brown-out

---

## 📅 Plan de Acción Sugerido

| Prioridad | ID | Tipo | Descripción | Esfuerzo |
|-----------|-----|------|-------------|----------|
| 1 | FIX-V2 | Fix | Fallback operadora (RF-12) | Bajo |
| 2 | FIX-V3 | Fix | Modo baja batería (RF-06) | Medio |
| 3 | FIX-V4 | Fix | Límite escaneos/día (RF-14) | Bajo |
| 4 | FEAT-V3 | Feat | Logs críticos (RF-05) | Medio |
| 5 | FEAT-V4 | Feat | CLI mantenimiento (RF-16,17) | Alto |
| 6 | FEAT-V5 | Feat | TLS/SSL (RNF-03) | Alto |
| 7 | FIX-V5 | Fix | Brown-out (RNF-02) | Medio |

---

## 📝 Notas de Auditoría

1. Esta auditoría se basa en revisión estática del código
2. No incluye pruebas de campo ni mediciones de consumo
3. Los estados "Parcial" indican implementación incompleta o sin validación
4. Se recomienda re-auditar tras cada release mayor
