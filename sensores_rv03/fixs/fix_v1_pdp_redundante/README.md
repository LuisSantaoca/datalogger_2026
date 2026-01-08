# FIX-V1: Reducir Eventos PDP Redundantes

**Versión objetivo:** v2.1  
**Fecha de identificación:** 2026-01-07  
**Estado:** Pendiente  
**Prioridad:** Media-Alta  
**Impacto:** Consumo de datos, batería, tiempo de ciclo  

---

## 📋 Resumen Ejecutivo

El firmware genera múltiples eventos PDP (Create/Delete) en cada ciclo de transmisión, incluso cuando ya tiene una operadora guardada en NVS. Se observan 3+ Create/Delete cuando debería ser solo 1+1.

---

## 🔴 Problema Identificado

### Descripción
El método `configureOperator()` siempre ejecuta `resetModem()` al inicio, independientemente de si el modem acaba de encenderse o si ya tiene una operadora guardada.

### Evidencia
En el dashboard de la operadora se observan múltiples eventos PDP en el mismo minuto para un solo ciclo de transmisión:

```
19:38 - Create PDP Context - AT&T
19:38 - Delete PDP Context - AT&T
19:38 - Create PDP Context - AT&T
19:38 - Delete PDP Context - AT&T
19:38 - Create PDP Context - AT&T
19:38 - Delete PDP Context - AT&T
```

### Ubicación del Bug
**Archivo:** `src/data_lte/LTEModule.cpp`  
**Línea:** 327  

```cpp
bool LTEModule::configureOperator(Operadora operadora) {
    resetModem();  // ← PROBLEMA: Siempre ejecuta AT+CFUN=1,1
    ...
}
```

---

## 🔍 Causa Raíz

El comando `AT+CFUN=1,1` dentro de `resetModem()`:

1. Reinicia completamente la radio del modem
2. Puede cerrar sesiones PDP existentes (genera Delete)
3. Fuerza re-registro en la red (genera Create al reconectar)

Esto ocurre **siempre**, incluso cuando:
- El modem acaba de encenderse con `powerOn()` y está limpio
- Ya tiene una operadora guardada en NVS (no necesita re-escanear)

---

## ⚡ Impacto

| Aspecto | Descripción |
|---------|-------------|
| **Consumo de datos** | Eventos PDP adicionales consumen presupuesto del SIM |
| **Tiempo de ciclo** | +5-10 segundos por reset innecesario |
| **Batería** | Mayor consumo por operaciones de radio redundantes |
| **Estabilidad** | Riesgo de perder conexión durante el reset |
| **Costos** | Algunos planes M2M cobran por eventos de señalización |

### Métricas de Impacto

| Métrica | Actual | Esperado | Diferencia/Mes |
|---------|--------|----------|----------------|
| Eventos PDP/ciclo | 3+ | 1 | -66% |
| Eventos PDP/día (10min) | 432+ | 144 | -288 eventos |
| Eventos PDP/mes | 12,960+ | 4,320 | **-8,640 eventos** |
| Tiempo extra/ciclo | 5-10s | 0s | **-12+ horas/mes** |

---

## ✅ Solución Propuesta

### Estrategia
Agregar parámetro `skipReset` a `configureOperator()` para omitir el reset cuando el modem ya está en estado limpio (recién encendido con operadora guardada).

### Cambios Requeridos

Ver archivo `IMPLEMENTACION.md` para detalles técnicos.

---

## 🧪 Criterios de Aceptación

1. ✅ Con operadora guardada: **1 Create + 1 Delete PDP** por ciclo
2. ✅ Sin operadora guardada (escaneo): Múltiples eventos (aceptable)
3. ✅ Log debe mostrar: `[INFO][APP] Usando operadora guardada: ...`
4. ✅ Tiempo de ciclo reducido ~5-10 segundos
5. ✅ Sin errores de conexión TCP
6. ✅ Compilación sin warnings

---

## 📁 Archivos Afectados

| Archivo | Tipo de Cambio |
|---------|----------------|
| `src/data_lte/LTEModule.h` | Modificar firma de función |
| `src/data_lte/LTEModule.cpp` | Agregar lógica condicional |
| `AppController.cpp` | Pasar parámetro skipReset |

---

## 🔬 Plan de Pruebas

1. **Boot frío** → Verificar escaneo completo funciona
2. **Wakeup desde sleep** → Verificar solo 1 PDP Create/Delete
3. **Fallo de transmisión** → Verificar que borra operadora y re-escanea
4. **Monitorear plataforma** → Confirmar reducción de eventos PDP
5. **Prueba de regresión** → Verificar que no rompe funcionalidad existente

---

## 📊 Trazabilidad

| Campo | Valor |
|-------|-------|
| Reportado por | Análisis de logs y dashboard operadora |
| Asignado a | Pendiente |
| Rama | `fix/v1-pdp-redundante` |
| Issue | - |
| PR | - |

---

## 📝 Notas Adicionales

### Comparación con otros firmwares
Se revisaron **JAMR_4.4** y **3.4_trinof** del mismo workspace. Ninguno hace reset en cada `configureOperator()`, lo que confirma que el comportamiento actual no es estándar.

### Referencia del fabricante
El datasheet del SIM7080G no recomienda ejecutar `AT+CFUN=1,1` antes de cada operación de configuración. El comando está diseñado para recuperación de errores, no para uso rutinario.
