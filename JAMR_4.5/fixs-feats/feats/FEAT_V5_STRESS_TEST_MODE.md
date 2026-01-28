# FEAT-V5: Modo Stress Test para Reinicio Periódico

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V5 |
| **Tipo** | Feature (Desarrollo / Pruebas) |
| **Sistema** | Core / FeatureFlags |
| **Archivo Principal** | `src/FeatureFlags.h` |
| **Estado** | ✅ Implementado |
| **Fecha** | 2026-01-28 |
| **Versión** | v2.5.1 |
| **Depende de** | FEAT-V4 (Periodic Restart) |

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
FEAT-V4 implementa reinicio periódico cada 24 horas, pero para validar su funcionamiento:
- Esperar 24 horas para ver UN reinicio es ineficiente
- Se necesitan múltiples reinicios para validar estabilidad
- Las pruebas de campo requieren ciclos más rápidos

### Necesidad
Un modo de prueba que permita:
1. Reducir el intervalo de reinicio de horas a minutos
2. Acumular más reinicios en menos tiempo
3. Validar que el sistema no pierde datos entre reinicios
4. Estresar el mecanismo de restart sin afectar producción

---

## 📊 EVALUACIÓN

### Impacto
| Aspecto | Evaluación |
|---------|------------|
| Criticidad | **Baja** - Solo para pruebas |
| Riesgo | **Bajo** - No afecta producción si se desactiva |
| Esfuerzo | **Bajo** - Solo parámetros de configuración |
| Beneficio | **Alto** - Acelera validación de FEAT-V4 |

### Comparación de Tiempos

| Modo | Intervalo | Reinicios/24h | Tiempo para 48 reinicios |
|------|-----------|---------------|--------------------------|
| Producción | 24 horas | 1 | 48 días |
| **Stress 30min** | 30 min | 48 | **24 horas** |
| Stress 10min | 10 min | 144 | 8 horas |

---

## 🔧 IMPLEMENTACIÓN

### Ubicación
Archivo: `src/FeatureFlags.h`, sección FEAT-V4

### Código Implementado

```cpp
// ============================================================
// FEAT-V4: PARÁMETROS DE REINICIO PERIÓDICO
// ============================================================

/** 
 * @brief Modo de prueba para reinicio periódico
 * 0 = Producción (usa FEAT_V4_RESTART_HOURS en horas)
 * 1 = Stress test (usa FEAT_V4_RESTART_MINUTES en minutos)
 */
#define FEAT_V4_STRESS_TEST_MODE              1   // ← CAMBIAR A 0 PARA PRODUCCIÓN

/** @brief Horas entre reinicios preventivos (producción) */
#define FEAT_V4_RESTART_HOURS                 24

/** @brief Minutos entre reinicios (solo para stress test) */
#define FEAT_V4_RESTART_MINUTES               30

/** @brief Threshold calculado en microsegundos */
#if FEAT_V4_STRESS_TEST_MODE
    #define FEAT_V4_THRESHOLD_US  ((uint64_t)FEAT_V4_RESTART_MINUTES * 60ULL * 1000000ULL)
#else
    #define FEAT_V4_THRESHOLD_US  ((uint64_t)FEAT_V4_RESTART_HOURS * 3600ULL * 1000000ULL)
#endif
```

---

## 📐 DISEÑO TÉCNICO

### Parámetros Configurables

| Parámetro | Descripción | Producción | Stress Test |
|-----------|-------------|------------|-------------|
| `FEAT_V4_STRESS_TEST_MODE` | Activa modo pruebas | 0 | **1** |
| `FEAT_V4_RESTART_HOURS` | Intervalo en horas | 24 | (ignorado) |
| `FEAT_V4_RESTART_MINUTES` | Intervalo en minutos | (ignorado) | **30** |

### Cálculo del Threshold

```
Producción:     24h × 3600s × 1,000,000µs = 86,400,000,000 µs
Stress 30min:   30min × 60s × 1,000,000µs = 1,800,000,000 µs
```

### Comportamiento

El sistema acumula el tiempo real de deep sleep. Cuando el acumulador supera el threshold:
1. Imprime banner de reinicio periódico
2. Ejecuta `esp_restart()`
3. En el siguiente boot, resetea el acumulador

---

## ⚠️ ADVERTENCIAS

### 1. NO usar en producción
```cpp
#define FEAT_V4_STRESS_TEST_MODE   0   // ← OBLIGATORIO para producción
```

### 2. Consumo de recursos
- Más reinicios = más escrituras a NVS
- Más reinicios = más ciclos de conexión LTE
- **Riesgo**: Operadora puede detectar comportamiento anómalo si ciclos muy cortos

### 3. Valores recomendados para stress test

| Escenario | Minutos | Justificación |
|-----------|---------|---------------|
| Stress normal | 30 | ~48 reinicios/día, seguro para LTE |
| Stress intenso | 10 | ~144 reinicios/día, usar con MOCK_LTE |
| Validación rápida | 5 | Solo para pruebas de FSM sin red |

---

## ✅ CHECKLIST ANTES DE PRODUCCIÓN

- [ ] `FEAT_V4_STRESS_TEST_MODE` = 0
- [ ] Verificar que `FEAT_V4_RESTART_HOURS` = 24
- [ ] Recompilar firmware
- [ ] Verificar log de boot muestra "Producción"

---

## 🧪 PLAN DE PRUEBAS

### Prueba 1: Validación de Reinicios (30 min)
1. Configurar `STRESS_TEST_MODE = 1`, `RESTART_MINUTES = 30`
2. Flashear y dejar correr 3 horas
3. **Esperado**: ~6 reinicios periódicos
4. **Verificar**: Buffer no pierde datos entre reinicios

### Prueba 2: Estabilidad de Memoria
1. Monitorear heap libre cada ciclo
2. Verificar que no hay memory leak acumulativo
3. Comparar heap antes y después de cada reinicio

### Prueba 3: Integridad de NVS
1. Verificar `lastOperator` persiste entre reinicios
2. Verificar coordenadas GPS persisten
3. Verificar contadores de ciclo se resetean correctamente

---

## 📊 MÉTRICAS DE VALIDACIÓN

| Métrica | Valor Aceptable | Crítico |
|---------|-----------------|---------|
| Reinicios exitosos consecutivos | ≥ 48 | < 10 |
| Pérdida de datos en buffer | 0 | > 0 |
| Crashes no planificados | 0 | > 0 |
| Heap leak por reinicio | 0 bytes | > 100 bytes |

---

## 🔗 REFERENCIAS

- **Depende de**: [FEAT_V4_PERIODIC_RESTART.md](FEAT_V4_PERIODIC_RESTART.md)
- **Archivo**: `src/FeatureFlags.h` líneas 186-207
- **Fecha implementación**: 2026-01-28
