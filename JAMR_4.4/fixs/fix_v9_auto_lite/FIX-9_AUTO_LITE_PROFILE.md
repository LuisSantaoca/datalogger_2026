# FIX-9: Perfil AUTO_LITE para conexión rápida en JAMR_4.4.7

## 1. Contexto

Este fix introduce un nuevo perfil de conexión LTE "AUTO_LITE" en `JAMR_4.4.7`, inspirado en el comportamiento rápido de conexión observado en el firmware `agroMod01_jorge_trino`.

Objetivo: intentar primero un camino de conexión **simple y rápido**, y sólo si falla, caer a la lógica multi-perfil completa ya existente (perfiles más pesados y con validaciones profundas), manteniendo watchdog, health data y presupuestos de tiempo.

## 2. Problema que atiende

En algunos ciclos de `JAMR_4.4.7`, se observa lo siguiente:

- Señal LTE adecuada (RSSI aceptable), pero PDP en estado `DEACTIVE`.
- El primer perfil `DEFAULT_CATM` consume casi todo el presupuesto LTE (`LTE_CONNECT_BUDGET_MS` ~120 s).
- El segundo perfil o alternativas tienen poco o ningún tiempo para probarse.
- El equipo no se cuelga (gracias a watchdog y health), pero pierde oportunidades de transmisión y alarga el ciclo.

## 3. Objetivo del FIX-9

- **Perfil AUTO_LITE**: un primer intento de conexión con una secuencia AT mínima (similar a `agroMod01_jorge_trino`):
  - Mantener `CFUN=1`, `CGDCONT` y `CNACT=0,1` con APN `"em"`.
  - No reconfigurar bandas extensivamente en este primer intento, salvo lo estrictamente necesario.
  - Usar un tiempo máximo corto (por ejemplo, 20–30 s) para decidir éxito o fallo.
- **Fallback robusto**: si AUTO_LITE falla o el tiempo se agota, pasar a la lógica actual de `startLTE_multiOperator()` con perfiles completos y validaciones (PDP, bandas, etc.).
- **No romper defensas ya existentes**:
  - Mantener watchdog de tarea (`esp_task_wdt`) alimentado en los bucles de conexión.
  - Respetar `COMM_CYCLE_BUDGET_MS` para todo el ciclo de comunicación.
  - Seguir registrando eventos en health data / RTC.

## 4. Alcance

En este fix se plantea:

1. Definir una nueva función de conexión rápida, tentativamente `startLTE_autoLite()`, dentro de `gsmlte.cpp`.
2. Integrar este intento rápido al inicio del flujo de `setupModem` / `startLTE_multiOperator` sin alterar el comportamiento de los perfiles ya existentes cuando AUTO_LITE falla.
3. Ajustar tiempos máximos por perfil para asegurar que:
   - AUTO_LITE tenga una ventana corta y controlada.
   - El resto de perfiles sigan respetando el presupuesto LTE y el presupuesto global de comunicación.
4. Documentar criterios de éxito y fallos para facilitar pruebas de campo.

## 5. Criterios de éxito

- En redes buenas (como las observadas con `agroMod01_jorge_trino`):
  - Tiempo desde wake-up hasta red LTE registrada + PDP activo dentro de una ventana corta (por ejemplo, < 30–40 s, sujeto a validación en campo).
  - El equipo envía datos sin necesidad de pasar a perfiles más pesados.
- En redes problemáticas o con PDP `DEACTIVE` persistente:
  - AUTO_LITE agota su tiempo máximo y entrega el control a la lógica multi-perfil actual sin quedarse bloqueado.
  - Se respeta el watchdog y el equipo entra a deep sleep de forma ordenada si el ciclo de comunicación falla.
- No introducción de estados zombie (sin bucles infinitos ni bloqueos alrededor del módem).

## 6. Lineamientos de implementación (borrador)

1. **Nueva función rápida** en `gsmlte.cpp`:
   - Basarse en la secuencia de `startLTE()` de `agroMod01_jorge_trino`:
     - `CFUN=1`.
     - `testAT()` hasta que el módem responda (con timeout corto y alimentando el watchdog).
     - `CGDCONT` con APN `"em"`.
     - `CNACT=0,1` para activar PDP.
     - Bucle de `isNetworkConnected()` con tiempo acotado.
   - Registrar RSSI y estado de red al final.
2. **Integración con el flujo actual**:
   - Llamar primero a `startLTE_autoLite()` desde `setupModem` o como primer paso de `startLTE_multiOperator()`.
   - Si devuelve éxito, continuar con el envío de datos sin reconfigurar bandas ni cambiar de perfil.
   - Si devuelve fallo o timeout, continuar con el flujo actual de `startLTE_multiOperator()` (perfiles `DEFAULT_CATM`, `NB_IOT_FALLBACK`, etc.).
3. **Presupuestos de tiempo**:
   - Incluir controles explícitos de tiempo para:
     - Tiempo total de AUTO_LITE (parámetro configurable, p.ej. `AUTO_LITE_BUDGET_MS`).
     - Respeto de `COMM_CYCLE_BUDGET_MS` para que el ciclo completo no se extienda de forma no controlada.
4. **Watchdog y health data**:
   - Alimentar el watchdog en cada bucle de espera (`testAT`, `isNetworkConnected`, esperas de PDP).
   - Registrar en health data si:
     - AUTO_LITE fue exitoso.
     - AUTO_LITE falló y se tuvo que recurrir a perfiles pesados.

## 7. Pruebas sugeridas

1. **Pruebas de laboratorio**:
   - Red estable con buena señal (simulando el entorno donde `agroMod01_jorge_trino` funciona bien):
     - Verificar que AUTO_LITE conecta dentro de la ventana objetivo y el ciclo no pasa a perfiles pesados.
   - Red limitada o con APN incorrecta:
     - Confirmar que AUTO_LITE falla sin colgarse, y el flujo cae al multi-perfil existente.
2. **Pruebas de campo**:
   - Ejecutar varias decenas de ciclos completos en diferentes condiciones de señal.
   - Medir tiempos wake-up → envío de datos.
   - Revisar logs para confirmar que no aparecen nuevos estados zombie.

## 8. Trazabilidad

- Relacionado con los requisitos:
  - **REQ-001_MODEM_STATE_MANAGEMENT**: mejora la gestión de estado del módem al introducir un camino feliz rápido más alineado con el entorno real del cliente, sin perder la capacidad de fallback.
  - **REQ-002_WATCHDOG_PROTECTION**: se asegura que AUTO_LITE respete el watchdog.
  - **REQ-003_HEALTH_DIAGNOSTICS**: se sugiere registrar métricas específicas de AUTO_LITE.

---

Este documento es el punto de partida del FIX-9. Los siguientes pasos serán ajustar el diseño de detalle y aplicar los cambios en `gsmlte.cpp` / `gsmlte.h`, junto con las pruebas correspondientes.