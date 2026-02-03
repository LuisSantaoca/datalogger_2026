# FEAT-V5: Sistema de Pruebas de Diagnóstico

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V5 |
| **Tipo** | Feature (Desarrollo / Pruebas / Diagnóstico) |
| **Sistema** | Core / FeatureFlags / LTEModule |
| **Archivos** | `src/FeatureFlags.h`, `src/data_lte/LTEModule.cpp`, `AppController.cpp` |
| **Estado** | ✅ Implementado |
| **Fecha** | 2026-01-28 |
| **Versión** | v2.5.2 |
| **Depende de** | FEAT-V4 (Periodic Restart) |

---

## 🎯 OBJETIVO

Proporcionar herramientas de diagnóstico para:
1. **Stress Test con Mocks** - Validar FSM, memoria, buffer (sin hardware real)
2. **Diagnóstico EMI** - Detectar problemas de ruido/integridad en comunicación UART con modem real

---

## 🔧 MODOS DE OPERACIÓN

### Modo 1: Stress Test con Mocks (sin red)

```cpp
#define DEBUG_STRESS_TEST_ENABLED    1   // ON
#define DEBUG_MOCK_LTE               1   // Simula LTE
#define DEBUG_MOCK_GPS               1   // Simula GPS
#define DEBUG_MOCK_ICCID             1   // Simula ICCID
#define DEBUG_EMI_DIAGNOSTIC_ENABLED 0   // OFF
```

**Valida:**
- ✅ FSM completa sin bloqueos
- ✅ Memory leaks (heap estable)
- ✅ Buffer LittleFS (escritura/lectura)
- ✅ Restart periódico FEAT-V4

**NO valida:**
- ❌ Comunicación UART real
- ❌ Problemas de EMI
- ❌ Comportamiento del modem

---

### Modo 2: Diagnóstico EMI (comunicación real) ← ACTIVO

```cpp
#define DEBUG_STRESS_TEST_ENABLED    0   // OFF
#define DEBUG_MOCK_LTE               0   // LTE real
#define DEBUG_MOCK_GPS               0   // GPS real
#define DEBUG_MOCK_ICCID             0   // ICCID real
#define DEBUG_EMI_DIAGNOSTIC_ENABLED 1   // ON
#define DEBUG_EMI_DIAGNOSTIC_CYCLES  3   // Reporte cada 3 ciclos
#define DEBUG_EMI_LOG_RAW_HEX        1   // Hex dump habilitado
```

**Valida:**
- ✅ Integridad de comunicación UART ESP32 ↔ SIM7080G
- ✅ Detección de bytes corruptos por ruido EMI
- ✅ Tasa de éxito/error de comandos AT
- ✅ Timeouts anómalos
- ✅ Comportamiento real del sistema completo
- ✅ Envío TCP real al servidor

**Características:**
- ⚠️ Usa SIM real (consume datos)
- ⚠️ Envía TCP al servidor
- ⚠️ Registra en red celular
- ✅ Diagnóstico de EMI en condiciones reales

---

## 📊 DIAGNÓSTICO EMI - DETALLE

### Estadísticas Recolectadas

| Métrica | Descripción | Umbral de alerta |
|---------|-------------|------------------|
| `totalATCommands` | Comandos AT enviados | - |
| `successfulResponses` | Respuestas OK | >90% esperado |
| `errorResponses` | Respuestas ERROR | <5% aceptable |
| `timeouts` | Sin respuesta | <5% aceptable |
| `corruptedResponses` | Bytes inválidos detectados | **>0% = problema** |
| `invalidCharsDetected` | Total caracteres corruptos | **>0 = EMI** |

### Detección de Corrupción

Caracteres válidos en respuesta AT:
- `0x0D` (CR), `0x0A` (LF)
- `0x20` - `0x7E` (ASCII imprimible)

Caracteres **inválidos** (indican ruido EMI):
- `0x00`, `0xFF` - Típicos de ruido
- `0x01` - `0x1F` (excepto CR/LF) - Control chars espurios
- `0x7F` - `0xFE` - Fuera de rango ASCII

### Output de Ejemplo

Comunicación limpia:
```
[EMI-CMD] AT+CSQ
[EMI-RAW] 18 bytes: 0D 0A 2B 43 53 51 3A 20 31 38 2C 30 0D 0A 4F 4B 0D 0A
```

Con corrupción por EMI:
```
[EMI-CMD] AT+CSQ
[EMI-RAW] 20 bytes: 0D 0A 2B 43 FF 51 3A 20 00 38 2C 30 0D 0A 4F 4B 0D 0A [!2 INVALID]
```

### Reporte Final (cada 3 ciclos)

```
╔════════════════════════════════════════════════════════════╗
║           REPORTE DIAGNÓSTICO EMI                          ║
╠════════════════════════════════════════════════════════════╣
║  Comandos AT enviados:           45                        ║
║  Respuestas exitosas (OK):       42 (93.3%)               ║
║  Respuestas ERROR:                1 ( 2.2%)               ║
║  Timeouts (sin respuesta):        2 ( 4.4%)               ║
╠════════════════════════════════════════════════════════════╣
║  Respuestas CORRUPTAS:            3 ( 6.7%)  ⚠️ EMI?      ║
║  Caracteres inválidos total:      5                        ║
╠════════════════════════════════════════════════════════════╣
║  Tiempo respuesta MIN:           12 ms                     ║
║  Tiempo respuesta MAX:          850 ms                     ║
║  Tiempo respuesta PROMEDIO:     125 ms                     ║
╠════════════════════════════════════════════════════════════╣
║  DIAGNÓSTICO: ⚠️  PROBLEMAS DE EMI/INTEGRIDAD DETECTADOS  ║
║  RECOMENDACIÓN: Revisar PCB, blindaje, desacoplo          ║
╚════════════════════════════════════════════════════════════╝
```

---

## 🔬 INTERPRETACIÓN DE RESULTADOS

### Comunicación Limpia ✅
```
Respuestas corruptas:  0%
Timeouts:              <3%
Caracteres inválidos:  0
```
**→ PCB OK, listo para producción**

### Ruido Menor ⚡
```
Respuestas corruptas:  1-5%
Timeouts:              3-10%
Caracteres inválidos:  1-10
```
**→ Monitorear en campo, considerar filtros de software**

### Problema de EMI Serio ⚠️
```
Respuestas corruptas:  >5%
Timeouts:              >10%
Caracteres inválidos:  >10
```
**→ Requiere revisión de PCB: planos de tierra, desacoplo, 4 capas**

---

## ⚠️ CONFIGURACIÓN PARA PRODUCCIÓN

**ANTES de desplegar en campo:**

```cpp
// TODOS los flags de debug en 0
#define DEBUG_STRESS_TEST_ENABLED    0
#define DEBUG_MOCK_LTE               0
#define DEBUG_MOCK_GPS               0
#define DEBUG_MOCK_ICCID             0
#define DEBUG_EMI_DIAGNOSTIC_ENABLED 0

// FEAT-V4 en modo producción
#define FEAT_V4_STRESS_TEST_MODE     0
#define FEAT_V4_RESTART_HOURS        24
```

---

## 📁 ARCHIVOS MODIFICADOS

| Archivo | Cambios |
|---------|---------|
| `src/FeatureFlags.h` | Nuevos flags DEBUG_EMI_* |
| `src/data_lte/LTEModule.cpp` | Struct EMIDiagStats, logRawHex(), printEMIDiagnosticReport() |
| `AppController.cpp` | Contador g_emiDiagCycleCount, llamada a reporte |

---

## 🧪 PRUEBA ACTUAL ACTIVA

**Configuración:**
```cpp
DEBUG_EMI_DIAGNOSTIC_ENABLED = 1
DEBUG_EMI_DIAGNOSTIC_CYCLES  = 3   // Reporte rápido
DEBUG_EMI_LOG_RAW_HEX        = 1
DEBUG_MOCK_* = 0                   // Comunicación REAL
```

**Qué obtendrás:**
- 3 ciclos completos con envío LTE real
- Hex dump de cada comando AT
- Estadísticas de integridad de comunicación
- Reporte de diagnóstico EMI al finalizar ciclo 3
- Tiempo estimado: ~30-40 minutos

---

## 📈 CHANGELOG

| Fecha | Versión | Cambio |
|-------|---------|--------|
| 2026-01-28 | v2.5.1 | Implementación inicial stress test con mocks |
| 2026-01-28 | v2.5.2 | Agregado diagnóstico EMI con comunicación real |
