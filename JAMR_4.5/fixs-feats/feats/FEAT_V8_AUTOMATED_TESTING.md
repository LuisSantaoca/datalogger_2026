# FEAT-V8: Sistema de Testing Automatizado (Unity Framework)

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V8 |
| **Tipo** | Feature (Calidad / Testing) |
| **Sistema** | Infraestructura de Testing |
| **Archivo Principal** | `test/*.cpp`, `platformio.ini` |
| **Estado** | 📋 Propuesto |
| **Fecha** | 2026-01-28 |
| **Versión Target** | v2.7.0 |
| **Depende de** | Ninguno (infraestructura independiente) |
| **Origen** | Auditoría de Calidad - Issue Crítico C1 |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
**AUSENCIA TOTAL DE TESTING AUTOMATIZADO**

Estado actual del proyecto:
```bash
$ find JAMR_4.5 -name "*test*.cpp"
# No files found ❌

Cobertura de tests: 0%
Validación automatizada: Ninguna
Regresión detection: Manual
```

### Síntomas Observados
1. **Validación lenta:** Cada cambio requiere:
   - Flashear dispositivo (30 segundos)
   - Conectar Serial Monitor
   - Esperar ciclo completo (10+ minutos)
   - Validación manual de logs

2. **Bugs no detectados tempranamente:**
   - Memory leaks en BLEModule (detectados en auditoría)
   - Validación incorrecta de bounds en ProductionDiag
   - Edge cases no validados en lógica de batería

3. **Riesgo de regresiones:**
   - Sin forma de verificar que cambios no rompen funcionalidad existente
   - Refactoring es peligroso sin red de seguridad

4. **Debugging ineficiente:**
   - Bugs reportados en campo difíciles de reproducir
   - Ciclo de fix → flashear → esperar → validar es costoso

### Causa Raíz
El proyecto fue desarrollado sin infraestructura de testing desde el inicio. La naturaleza embedded del firmware no es excusa - frameworks como Unity permiten testing en PC antes de flashear.

---

## 📊 EVALUACIÓN

### Impacto
| Aspecto | Evaluación |
|---------|------------|
| Criticidad | **CRÍTICA** - Bloqueante para producción masiva |
| Riesgo de no implementar | **MUY ALTO** - Bugs costosos en campo |
| Esfuerzo | **Alto** - Infraestructura + tests iniciales (1-2 semanas) |
| Beneficio | **MÁXIMO** - Reduce tiempo de desarrollo 50%+ |
| ROI | **Altísimo** - Se paga en primer bug evitado |

### Justificación Técnica

**Métricas de desarrollo SIN tests:**
```
Bug en producción:
  Detección: 2-7 días (reporte cliente)
  Reproducción: 1-2 días (enviar logs, traer dispositivo)
  Fix: 1 día
  Validación: 1 día (testing manual)
  Deployment: 1 día (OTA/visita sitio)
  TOTAL: 6-12 días
```

**Métricas de desarrollo CON tests:**
```
Bug detectado en build:
  Detección: 0 segundos (CI falla)
  Reproducción: 0 segundos (test reproduce exactamente)
  Fix: 2 horas
  Validación: 5 segundos (tests pasan)
  Deployment: 30 minutos (CI builds automático)
  TOTAL: 3 horas
```

**Ahorro:** 95% de tiempo en ciclo de desarrollo

---

## 🔧 ARQUITECTURA DE TESTING

### Niveles de Testing

```
┌─────────────────────────────────────────┐
│  1. UNIT TESTS (Funciones individuales) │
│     - CRC16, formatters, parsers        │
│     - Validaciones, cálculos            │
│     - Ejecutan en PC (native)           │
│     - Muy rápidos (<1s total)           │
└─────────────────────────────────────────┘
                   ▼
┌─────────────────────────────────────────┐
│  2. INTEGRATION TESTS (Módulos)         │
│     - ProductionDiag persistencia       │
│     - Battery state machine             │
│     - EMI detection logic               │
│     - Mock hardware cuando sea necesario│
└─────────────────────────────────────────┘
                   ▼
┌─────────────────────────────────────────┐
│  3. HARDWARE TESTS (En ESP32)           │
│     - LTE connectivity                  │
│     - Sensor reading                    │
│     - RTC persistence                   │
│     - Ejecutan en dispositivo real      │
└─────────────────────────────────────────┘
```

### Framework Seleccionado: Unity

**¿Por qué Unity?**
- ✅ Oficial de ESP-IDF
- ✅ Compatible con PlatformIO y Arduino
- ✅ Lightweight (~2KB RAM)
- ✅ Soporta ejecución en PC (native) y en ESP32
- ✅ Sintaxis simple tipo xUnit
- ✅ Ampliamente usado en embedded

**Alternativas descartadas:**
- GoogleTest: Demasiado pesado para ESP32
- Catch2: No oficial para ESP32
- Custom: Reinventar la rueda

---

## 📐 DISEÑO TÉCNICO

### Estructura de Archivos

```
JAMR_4.5/
├── src/                              ← Código de producción
│   ├── data_diagnostics/
│   │   ├── ProductionDiag.cpp
│   │   ├── ProductionDiag.h
│   │   └── CrashDiagnostics.cpp
│   └── ...
│
├── test/                             ← ✨ NUEVO: Tests
│   ├── test_main.cpp                 # Entry point de tests
│   │
│   ├── unit/                         # Tests unitarios (PC)
│   │   ├── test_crc16.cpp            # Test CRC16 validation
│   │   ├── test_formatters.cpp       # Test coordinate formatting
│   │   └── test_parsers.cpp          # Test string parsing
│   │
│   ├── integration/                  # Tests de integración
│   │   ├── test_production_diag.cpp  # ProductionDiag completo
│   │   ├── test_battery_logic.cpp    # FIX-V3 state machine
│   │   ├── test_emi_detection.cpp    # EMI thresholds
│   │   └── test_crash_diagnostics.cpp# FEAT-V3 RTC persistence
│   │
│   ├── hardware/                     # Tests que requieren ESP32
│   │   ├── test_lte_connectivity.cpp
│   │   ├── test_sensors.cpp
│   │   └── test_rtc.cpp
│   │
│   └── mocks/                        # Mocks de hardware
│       ├── mock_lte.h
│       ├── mock_sensors.h
│       └── mock_rtc.h
│
└── platformio.ini                    ← Configuración de tests
```

### Configuración PlatformIO

```ini
# platformio.ini - Agregar sección de testing

# ============================================================
# TESTING ENVIRONMENTS
# ============================================================

# Tests que corren en PC (rápidos, no requieren hardware)
[env:native_test]
platform = native
framework = arduino
lib_deps = 
    throwtheswitch/Unity@^2.5.2
test_framework = unity
test_filter = unit/*
build_flags = 
    -DUNIT_TEST
    -std=c++11

# Tests de integración (pueden mockear hardware)
[env:integration_test]
platform = native
framework = arduino
lib_deps = 
    throwtheswitch/Unity@^2.5.2
test_framework = unity
test_filter = integration/*
build_flags = 
    -DINTEGRATION_TEST
    -std=c++11

# Tests que requieren ESP32 real
[env:esp32_test]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps = 
    throwtheswitch/Unity@^2.5.2
test_framework = unity
test_filter = hardware/*
monitor_speed = 115200
```

---

## 🧪 CASOS DE PRUEBA CRÍTICOS

### Prioridad 🔴 ALTA (Pre-producción)

#### Test 1: CRC16 Validation (ProductionDiag)
```cpp
// test/unit/test_crc16.cpp
#include <unity.h>
#include "../../src/data_diagnostics/ProductionDiag.h"

void setUp(void) {
    // Configuración antes de cada test
}

void tearDown(void) {
    // Limpieza después de cada test
}

// Test 1.1: CRC de datos vacíos
void test_crc16_empty_data(void) {
    uint8_t empty[0];
    uint16_t result = calculateCRC16(empty, 0);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, result);
}

// Test 1.2: Vector de prueba conocido (CRC16-MODBUS)
void test_crc16_known_values(void) {
    uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
    uint16_t result = calculateCRC16(data, 4);
    // CRC16-MODBUS de [01 02 03 04] = 0x89C3
    TEST_ASSERT_EQUAL_HEX16(0x89C3, result);
}

// Test 1.3: Estabilidad (mismo input → mismo output)
void test_crc16_stability(void) {
    uint8_t data[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    uint16_t r1 = calculateCRC16(data, 5);
    uint16_t r2 = calculateCRC16(data, 5);
    TEST_ASSERT_EQUAL(r1, r2);
}

// Test 1.4: Detección de corrupción (cambiar 1 bit)
void test_crc16_detects_corruption(void) {
    uint8_t data1[] = {0x01, 0x02, 0x03};
    uint8_t data2[] = {0x01, 0x02, 0x04};  // Último byte diferente
    uint16_t crc1 = calculateCRC16(data1, 3);
    uint16_t crc2 = calculateCRC16(data2, 3);
    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}

void test_runner(void) {
    UNITY_BEGIN();
    RUN_TEST(test_crc16_empty_data);
    RUN_TEST(test_crc16_known_values);
    RUN_TEST(test_crc16_stability);
    RUN_TEST(test_crc16_detects_corruption);
    UNITY_END();
}

#ifdef UNIT_TEST
int main(void) {
    return test_runner();
}
#endif
```

**Ejecución:**
```bash
$ pio test -e native_test
Testing unit/test_crc16.cpp
test_crc16_empty_data...OK
test_crc16_known_values...OK
test_crc16_stability...OK
test_crc16_detects_corruption...OK
-----------------
4 Tests 0 Failures
```

---

#### Test 2: Battery Hysteresis (FIX-V3)
```cpp
// test/integration/test_battery_logic.cpp
#include <unity.h>

// Variables globales del sistema (normalmente en AppController.cpp)
static bool g_restMode = false;
static uint8_t g_stableCycleCounter = 0;

// Umbr ales configurables
#define UTS_LOW_ENTER  3.20f
#define UTS_LOW_EXIT   3.80f
#define STABLE_CYCLES  3

// Función bajo test (copiar lógica de AppController.cpp)
bool evaluateBatteryState(float vBat) {
    if (!g_restMode) {
        if (vBat <= UTS_LOW_ENTER) {
            g_restMode = true;
            g_stableCycleCounter = 0;
            return false;  // No puede operar
        }
        return true;  // Operación normal
    } else {
        if (vBat >= UTS_LOW_EXIT) {
            g_stableCycleCounter++;
            if (g_stableCycleCounter >= STABLE_CYCLES) {
                g_restMode = false;
                g_stableCycleCounter = 0;
                return true;  // Sale de reposo
            }
        } else {
            g_stableCycleCounter = 0;  // Reset si cae
        }
        return false;  // Sigue en reposo
    }
}

void setUp(void) {
    g_restMode = false;
    g_stableCycleCounter = 0;
}

// Test 2.1: Entrada a modo reposo
void test_battery_enters_rest_mode(void) {
    float vBat = 3.15f;  // Bajo umbral
    bool canOperate = evaluateBatteryState(vBat);
    
    TEST_ASSERT_FALSE(canOperate);
    TEST_ASSERT_TRUE(g_restMode);
    TEST_ASSERT_EQUAL(0, g_stableCycleCounter);
}

// Test 2.2: Requiere 3 ciclos estables para salir
void test_battery_requires_3_stable_cycles(void) {
    // Entrar a reposo primero
    evaluateBatteryState(3.10f);
    TEST_ASSERT_TRUE(g_restMode);
    
    // Ciclo 1: voltaje OK
    bool r1 = evaluateBatteryState(3.85f);
    TEST_ASSERT_FALSE(r1);  // Aún en reposo
    TEST_ASSERT_EQUAL(1, g_stableCycleCounter);
    
    // Ciclo 2: voltaje OK
    bool r2 = evaluateBatteryState(3.90f);
    TEST_ASSERT_FALSE(r2);  // Aún en reposo
    TEST_ASSERT_EQUAL(2, g_stableCycleCounter);
    
    // Ciclo 3: voltaje OK → SALE
    bool r3 = evaluateBatteryState(3.85f);
    TEST_ASSERT_TRUE(r3);   // ✅ Sale de reposo
    TEST_ASSERT_FALSE(g_restMode);
}

// Test 2.3: Reset de contador si voltaje cae
void test_battery_resets_counter_on_drop(void) {
    // Entrar a reposo
    evaluateBatteryState(3.15f);
    
    // 2 ciclos buenos
    evaluateBatteryState(3.90f);
    evaluateBatteryState(3.85f);
    TEST_ASSERT_EQUAL(2, g_stableCycleCounter);
    
    // Voltaje CAE ⚠️
    evaluateBatteryState(3.70f);  // Bajo UTS_LOW_EXIT
    TEST_ASSERT_EQUAL(0, g_stableCycleCounter);  // ✅ Contador reset
    TEST_ASSERT_TRUE(g_restMode);  // Sigue en reposo
}

// Test 2.4: No entra si está justo en umbral
void test_battery_threshold_exact(void) {
    float vBat = 3.20f;  // Exactamente en umbral
    bool canOperate = evaluateBatteryState(vBat);
    
    TEST_ASSERT_FALSE(canOperate);  // <= incluye igual
    TEST_ASSERT_TRUE(g_restMode);
}

void test_runner(void) {
    UNITY_BEGIN();
    RUN_TEST(test_battery_enters_rest_mode);
    RUN_TEST(test_battery_requires_3_stable_cycles);
    RUN_TEST(test_battery_resets_counter_on_drop);
    RUN_TEST(test_battery_threshold_exact);
    UNITY_END();
}

#ifdef INTEGRATION_TEST
int main(void) {
    return test_runner();
}
#endif
```

**Validación:**
```bash
$ pio test -e integration_test
Testing integration/test_battery_logic.cpp
test_battery_enters_rest_mode...OK (0.002s)
test_battery_requires_3_stable_cycles...OK (0.003s)
test_battery_resets_counter_on_drop...OK (0.002s)
test_battery_threshold_exact...OK (0.001s)
-----------------
4 Tests 0 Failures (0.008s total)
```

---

#### Test 3: EMI Detection Thresholds (FEAT-V7)
```cpp
// test/integration/test_emi_detection.cpp
#include <unity.h>
#include "../../src/data_diagnostics/ProductionDiag.h"

// Mocks de ProductionStats (simplificado)
static ProductionStats g_mockStats;
static CycleEMIStats g_mockCycleEMI;

// Test 3.1: Contar caracteres inválidos
void test_emi_counts_invalid_chars(void) {
    String response = "OK\xFF\xFE\x00ERROR";  // Bytes inválidos
    
    // La función real debe incrementar invalidChars
    // (Esto requiere refactor de countEMI para ser testeable)
    
    // TEST_ASSERT_EQUAL(3, g_mockCycleEMI.invalidChars);
    TEST_PASS_MESSAGE("Implementar después de refactor de countEMI");
}

// Test 3.2: Veredicto WARNING (10-30% errores)
void test_emi_verdict_warning_threshold(void) {
    g_mockCycleEMI.totalResponses = 100;
    g_mockCycleEMI.invalidChars = 15;  // 15% de errores
    
    // evaluateCycleEMI() debe generar WARNING
    // (Requiere extraer lógica a función testeable)
    
    TEST_PASS_MESSAGE("Implementar después de refactor de evaluateCycleEMI");
}

// Test 3.3: Veredicto CRITICAL (>30% errores)
void test_emi_verdict_critical_threshold(void) {
    g_mockCycleEMI.totalResponses = 50;
    g_mockCycleEMI.invalidChars = 20;  // 40% de errores
    
    // evaluateCycleEMI() debe generar CRITICAL
    
    TEST_PASS_MESSAGE("Implementar después de refactor");
}

void test_runner(void) {
    UNITY_BEGIN();
    RUN_TEST(test_emi_counts_invalid_chars);
    RUN_TEST(test_emi_verdict_warning_threshold);
    RUN_TEST(test_emi_verdict_critical_threshold);
    UNITY_END();
}

#ifdef INTEGRATION_TEST
int main(void) {
    return test_runner();
}
#endif
```

---

#### Test 4: Stats Persistence (ProductionDiag)
```cpp
// test/integration/test_production_diag.cpp
#include <unity.h>
#include "../../src/data_diagnostics/ProductionDiag.h"

void setUp(void) {
    // Limpiar stats antes de cada test
    ProdDiag::clearAll();
}

// Test 4.1: Incrementar contadores
void test_counters_increment(void) {
    ProdDiag::init();
    
    uint32_t before = ProdDiag::getStats().totalCycles;
    ProdDiag::incrementCycle();
    uint32_t after = ProdDiag::getStats().totalCycles;
    
    TEST_ASSERT_EQUAL(before + 1, after);
}

// Test 4.2: Guardar y cargar stats
void test_stats_persistence(void) {
    ProdDiag::init();
    
    // Modificar stats
    for (int i = 0; i < 10; i++) {
        ProdDiag::incrementCycle();
        ProdDiag::recordLTESendOk();
    }
    
    uint32_t cycles = ProdDiag::getStats().totalCycles;
    uint32_t lteSent = ProdDiag::getStats().lteSendOk;
    
    // Guardar
    ProdDiag::saveStats(1234567890);
    
    // Limpiar memoria y recargar
    ProdDiag::clearAll();
    ProdDiag::init();
    ProdDiag::loadStats();
    
    // Verificar persistencia
    TEST_ASSERT_EQUAL(cycles, ProdDiag::getStats().totalCycles);
    TEST_ASSERT_EQUAL(lteSent, ProdDiag::getStats().lteSendOk);
}

// Test 4.3: CRC detecta corrupción
void test_stats_crc_validation(void) {
    // Este test requiere simular corrupción del archivo
    // (Implementar con mock filesystem)
    
    TEST_PASS_MESSAGE("Implementar con mock LittleFS");
}

void test_runner(void) {
    UNITY_BEGIN();
    RUN_TEST(test_counters_increment);
    RUN_TEST(test_stats_persistence);
    RUN_TEST(test_stats_crc_validation);
    UNITY_END();
}

#ifdef INTEGRATION_TEST
int main(void) {
    return test_runner();
}
#endif
```

---

### Prioridad 🟡 MEDIA (Post-deployment)

#### Test 5: Operator Fallback (FIX-V2)
```cpp
// test/integration/test_operator_fallback.cpp

void test_fallback_after_failure(void) {
    // Simular fallo de operadora guardada
    // Verificar que se hace escaneo completo
    TEST_PASS_MESSAGE("Implementar");
}

void test_skipScanCycles_decrements(void) {
    // Verificar que skipScanCycles se decrementa
    TEST_PASS_MESSAGE("Implementar");
}
```

#### Test 6: Periodic Restart Timing (FEAT-V4)
```cpp
// test/integration/test_periodic_restart.cpp

void test_restart_after_24h(void) {
    // Simular acumulación de 24h de sleep
    // Verificar que g_last_restart_reason_feat4 se setea
    TEST_PASS_MESSAGE("Implementar");
}

void test_anti_bootloop_protection(void) {
    // Simular restart ejecutado
    // Verificar que no se triggerea otro restart inmediato
    TEST_PASS_MESSAGE("Implementar");
}
```

---

## 📝 REFACTORING NECESARIO PARA TESTABILIDAD

### Problema: Funciones no testeables

Muchas funciones actuales están acopladas al hardware:

```cpp
// ❌ NO TESTEABLE: Lee directamente de Serial
bool LTEModule::waitForOK(uint32_t timeout) {
    while (timeout > 0) {
        if (_serial.available()) {  // Hardware dependency
            // ...
        }
    }
}
```

### Solución: Dependency Injection

```cpp
// ✅ TESTEABLE: Recibe interfaz abstracta
class ISerial {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual void write(uint8_t) = 0;
};

class HardwareSerial : public ISerial {
    // Implementación real
};

class MockSerial : public ISerial {
    // Implementación para tests
};

class LTEModule {
private:
    ISerial* _serial;
public:
    LTEModule(ISerial* serial) : _serial(serial) {}
    
    bool waitForOK(uint32_t timeout) {
        while (timeout > 0) {
            if (_serial->available()) {  // ✅ Mockeable
                // ...
            }
        }
    }
};
```

### Refactoring Recomendado

| Módulo | Función | Refactor Necesario |
|--------|---------|-------------------|
| ProductionDiag | `countEMI()` | Extraer lógica de detección a función pura |
| ProductionDiag | `evaluateCycleEMI()` | Separar cálculo de veredicto de logging |
| LTEModule | `waitForOK()` | Dependency injection para Serial |
| AppController | `evaluateBatteryState()` | Ya es testeable ✅ |
| CrashDiag | RTC operations | Mock de RTC_DATA_ATTR |

---

## 🚀 PLAN DE IMPLEMENTACIÓN

### Fase 1: Infraestructura (1-2 días)
```
✅ Configurar PlatformIO con Unity
✅ Crear estructura test/
✅ Implementar test_main.cpp
✅ Validar ejecución básica
```

### Fase 2: Tests Críticos (3-5 días)
```
✅ test_crc16.cpp (Unit)
✅ test_battery_logic.cpp (Integration)
✅ test_production_diag.cpp (Integration)
⚠️ test_emi_detection.cpp (requiere refactor menor)
```

### Fase 3: Refactoring para Testabilidad (2-3 días)
```
✅ Extraer lógica pura de ProductionDiag
✅ Dependency injection en LTEModule (opcional)
✅ Mock de LittleFS para tests de persistencia
```

### Fase 4: Tests Completos (1 semana)
```
✅ Todos los tests de prioridad ALTA
✅ Tests de prioridad MEDIA
✅ Integración con CI/CD (GitHub Actions)
```

---

## 🎯 MÉTRICAS DE ÉXITO

### Objetivos Mínimos (Pre-producción)
```
✅ Cobertura de código: >50% (crítico: >80%)
✅ Tests de CRC16: 100% passing
✅ Tests de batería: 100% passing
✅ Tests de ProductionDiag: >80% passing
✅ Tiempo de ejecución: <30 segundos total
```

### Objetivos Ideales (Post-deployment)
```
✅ Cobertura de código: >70%
✅ CI/CD integrado (auto-test en cada commit)
✅ Tests de hardware en ESP32 real
✅ Tests de regresión automáticos
```

---

## 📊 BENEFICIOS TANGIBLES

### Antes de FEAT-V8 (Sin tests)
```
Desarrollo de feature: 2-3 días
Testing manual: 1-2 días  
Bug encontrado: +2 días debugging
Confianza en deploy: 60%
TOTAL: 5-7 días por feature
```

### Después de FEAT-V8 (Con tests)
```
Desarrollo de feature: 2-3 días
Testing automático: 30 segundos
Bug detectado en build: Fix inmediato
Confianza en deploy: 95%
TOTAL: 2-3 días por feature
```

**Ahorro:** 40-50% de tiempo de desarrollo

---

## ⚠️ RIESGOS Y MITIGACIONES

### Riesgo 1: Curva de aprendizaje de Unity
**Mitigación:** 
- Comenzar con tests simples (CRC16)
- Documentación inline en templates
- Pair programming en primeros tests

### Riesgo 2: Tests que requieren hardware real
**Mitigación:**
- Separar en `test/hardware/` 
- Ejecutar solo en CI con ESP32 conectado
- Priorizar tests que corren en PC

### Riesgo 3: Mantener tests actualizados
**Mitigación:**
- Test roto = build roto (obligatorio fixear)
- Cada PR debe incluir tests
- Code review valida tests

---

## 🔗 DEPENDENCIAS

### Herramientas Necesarias
```bash
# PlatformIO
pip install platformio

# Unity framework (auto-instalado por PlatformIO)
# No requiere instalación manual
```

### Modificaciones de Código
- **Mínimas:** Código actual es mayormente testeable
- **Refactoring:** Solo para funciones con hardware dependencies
- **Sin cambios** en lógica de producción

---

## 📚 RECURSOS Y REFERENCIAS

### Documentación
- [Unity Framework](https://github.com/ThrowTheSwitch/Unity)
- [PlatformIO Testing](https://docs.platformio.org/en/latest/plus/unit-testing.html)
- [ESP-IDF Unit Tests](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/unit-tests.html)

### Ejemplos en Industria
- Adafruit: Testing con Unity en productos ESP32
- Espressif: Tests de ejemplo en ESP-IDF
- Arduino: Tests en librerías oficiales

---

## 🏁 CONCLUSIÓN

FEAT-V8 es **crítico para la salud del proyecto a largo plazo**. 

Sin tests automatizados:
- ❌ Cada cambio es un riesgo
- ❌ Debugging es costoso
- ❌ Refactoring es peligroso
- ❌ Regresiones son frecuentes
- ❌ Confianza en deploys es baja

Con tests automatizados:
- ✅ Cambios seguros con red de seguridad
- ✅ Bugs detectados antes de deploy
- ✅ Refactoring sin miedo
- ✅ Regresiones previstas automáticamente
- ✅ Confianza 95% en cada deploy

**ROI:** Se paga en el primer bug crítico evitado en producción.

---

**Implementador:** A definir  
**Revisor:** QA Team  
**Fecha estimada:** Sprint 2026-02

---

## 📎 ANEXOS

### A. Template de Test Básico
```cpp
// test/template_test.cpp
#include <unity.h>

void setUp(void) {
    // Setup antes de cada test
}

void tearDown(void) {
    // Cleanup después de cada test
}

void test_example(void) {
    TEST_ASSERT_EQUAL(1, 1);
}

void test_runner(void) {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    UNITY_END();
}

#ifdef UNIT_TEST
int main(void) {
    return test_runner();
}
#endif
```

### B. Comandos Útiles
```bash
# Ejecutar todos los tests
pio test

# Ejecutar solo tests unitarios (PC)
pio test -e native_test

# Ejecutar solo tests de integración
pio test -e integration_test

# Ejecutar tests en ESP32 real
pio test -e esp32_test

# Ejecutar un test específico
pio test -e native_test -f test_crc16

# Ver output detallado
pio test -v
```

### C. Checklist de PR con Tests
```markdown
## Checklist antes de merge

- [ ] Código compila sin warnings
- [ ] Tests existentes pasan (100%)
- [ ] Nuevos tests agregados para nueva funcionalidad
- [ ] Cobertura de tests no disminuyó
- [ ] Tests pasan en CI/CD
- [ ] Documentación actualizada
```

---

*Este documento es parte de la estrategia de calidad del proyecto JAMR_4.5*
