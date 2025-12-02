# 📑 REPORTE DE CALIDAD – Firmware JAMR_4.4.7

**Sistema:** Datalogger LTE/GPS ESP32-S3 (JAMR_4.4.7)  
**Versión firmware:** `FIRMWARE_VERSION_TAG = "v4.1.1-JAMR4-TIMEOUT"` (árbol de código `JAMR_4.4.7/`)  
**Fecha del reporte:** 2025-12-01  
**Responsable del reporte:** Ingeniería de Calidad (rol)

---

## 1. Alcance del reporte

Este reporte evalúa la **calidad del diseño e implementación** del firmware `JAMR_4.4.7` con foco en los requisitos definidos en `requisitos/`:

- `REQ-001_MODEM_STATE_MANAGEMENT` – Gestión de estado del módem entre ciclos de sleep.
- `REQ-002_WATCHDOG_PROTECTION` – Protección contra cuelgues mediante watchdog.
- `REQ-003_HEALTH_DIAGNOSTICS` – Diagnóstico postmortem (health data).
- `REQ-004_FIRMWARE_VERSIONING` – Versionado de firmware en payload y logs.

La revisión se basa en el código fuente actual (`JAMR_4.4.7/*.ino`, `gsmlte.*`, `sleepdev.*`, `sensores.*`, `timedata.*`, `cryptoaes.*`, `type_def.h`) y en los documentos de calidad previos en `calidad/`.

---

## 2. Resumen ejecutivo

- **Estado general:** El firmware `JAMR_4.4.7` muestra una **mejora clara** frente a iteraciones JAMR_3, con foco en robustez, telemetría de salud y control de tiempo de los ciclos de comunicación.
- **REQ-002 / REQ-003 / REQ-004:** Se observan **implementaciones directas y alineadas** con los requisitos (watchdog configurado explícitamente, checkpoints en RTC, inclusión de health data y versión en `sensordata_type`).
- **REQ-001:** No se encontraron aún mecanismos completos de **preservación de estado del módem entre ciclos**; el diseño actual sigue más cerca de un modelo "boot por ciclo" con foco en límites de tiempo y fiabilidad, aún no en un módem persistente multi‑ciclo.
- **Riesgo principal:** Dependencia fuerte de la calidad de señal y del comportamiento del módem SIM7080. Aunque hay presupuestos de tiempo y defensas de watchdog, el patrón sigue siendo de **reconexión por ciclo** más que de módem semi‑persistente.
- **Recomendación:** Considerar `REQ-001` como **parcialmente implementado** y planificar una fase específica para la gestión de estado del módem (CFUN/DTR/sleep modes).

---

## 3. Trazabilidad de requisitos vs implementación

### 3.1 REQ-001 – Gestión de Estado del Módem

**Expectativa (resumen):**
- El módem debe responder a AT inmediatamente tras despertar del deep sleep, sin secuencias de power cycle innecesarias.
- Solo se hace power‑on completo en el primer boot.
- Uso de modos de bajo consumo del módem (CFUN/DTR) en lugar de apagado completo.

**Evidencia encontrada:**
- Definiciones en `gsmlte.h` para control de módem: pines `PWRKEY`, `DTR`, `SerialAT`, `APN`, `MODEM_NETWORK_MODE`, perfiles de operador (`OperatorProfile`), y lógica avanzada de conexión (`LTE_CONNECT_BUDGET_MS`, `COMM_CYCLE_BUDGET_MS`).
- Funciones como `modemPwrKeyPulse()`, `modemRestart()`, `startLTE()`, `setupModem()` que gestionan la inicialización completa del módem en cada ciclo.
- No se identifican, en la revisión actual, flags persistentes de "módem inicializado una sola vez" ni uso de un modo CFUN mínimo / DTR‑sleep entre ciclos.

**Conclusión de calidad:**
- **Grado de cumplimiento:** `PARCIAL`.
- La implementación actual prioriza la **robustez y el acotamiento de tiempo** de reconexión (multi‑perfil y presupuestos de tiempo), pero **no cumple todavía** el modelo de módem persistente descrito en REQ‑001 (CU‑001, CU‑002).

**Riesgos asociados:**
- Aumento de tiempo y consumo en cada ciclo por reconexiones completas.
- Mayor dependencia de estabilidad de red y respuesta del módem en cada boot.

**Recomendaciones:**
1. Diseñar una **máquina de estados explícita para el módem** con flags en RTC o NVS indicando:
   - Primer boot vs. re‑wake de deep sleep.
   - Último estado conocido de conexión (registrado / no registrado / fallo crítico).
2. Evaluar e implementar uno de los enfoques sugeridos en REQ‑001:
   - **CFUN mínimo** (`AT+CFUN=0`) en fases de sleep cortas.
   - **CSCLK + DTR** para sleep profundo del módem con wake‑up rápido.
3. Registrar en health data si la reconexión fue por:
   - Modo persistente (wake‑up rápido) o
   - Boot completo (power cycle), para comparativas de campo.

---

### 3.2 REQ-002 – Watchdog Protection

**Expectativa (resumen):**
- Watchdog configurado con timeout razonable (ej. 120 s).
- Feeds en puntos estratégicos: operaciones largas, loops y esperas.
- El watchdog es mecanismo de protección, no de flujo normal.

**Evidencia encontrada:**
- En `sleepdev.h`:
  - `#define WATCHDOG_TIMEOUT_SEC 120`.
  - Inclusión de `esp_task_wdt.h` y comentarios alineados con el requisito (rol de watchdog, tiempos, etc.).
- En `JAMR_4.4.7.ino` (setup):
  - Desinicialización del watchdog por defecto: `esp_task_wdt_deinit()`.
  - Configuración explícita de `esp_task_wdt_config_t` con `timeout_ms = WATCHDOG_TIMEOUT_SEC * 1000` y `trigger_panic = true`.
  - Llamadas recurrentes a `esp_task_wdt_reset()` tras:
    - Configuración de GPIO (`setupGPIO()`).
    - Lectura de sensores.
    - Envío de datos a través del módem.
    - Estadísticas finales y preparación para sleep.
- En `gsmlte.h`:
  - `ENABLE_WDT_DEFENSIVE_LOOPS` documentado para alimentar watchdog y acotar timeouts en loops de interacción con módem/GPS.

**Conclusión de calidad:**
- **Grado de cumplimiento:** `ALTO` (alineado con REQ‑002, sujeto a revisar detalles en `gsmlte.cpp`).
- Diseño coherente: watchdog centralizado, timeout definido en una sola macro, feeds en puntos críticos del flujo principal y en módulos de comunicación.

**Riesgos/observaciones:**
- Falta de documentación consolidada estilo "mapa de feeds" (qué feeds existen y por qué).
- Posible riesgo de feeds excesivos en loops que puedan enmascarar bloqueos parciales (hay que validar en campo).

**Recomendaciones:**
1. Añadir un documento corto `calidad/MAPA_FEEDS_WATCHDOG.md` con:
   - Lista de funciones de alto nivel (GPS, LTE, envío TCP, lectura sensores).
   - Dónde se alimenta el watchdog y cada cuánto.
2. Hacer pruebas controladas desconectando antena/módem para **forzar timeouts** y verificar que:
   - El sistema resetea en ≤ 120 s cuando el módem no responde.
   - Tras el reset, el dispositivo retorna a operación normal.

---

### 3.3 REQ-003 – Health Diagnostics

**Expectativa (resumen):**
- Uso de RTC memory para preservar: checkpoint, crash_reason, boot_count, timestamp de crash.
- Integración de estos campos en el payload de telemetría.

**Evidencia encontrada:**
- En `sleepdev.h`:
  - Definición de checkpoints `CP_*` cubriendo todo el flujo (desde `CP_BOOT` hasta `CP_SLEEP_READY`).
  - Declaraciones `RTC_DATA_ATTR` para:
    - `rtc_last_checkpoint` (uint8_t).
    - `rtc_timestamp_ms` (uint32_t).
    - `rtc_boot_count` (uint16_t).
    - `rtc_crash_reason` (uint8_t).
    - `rtc_last_success_time` (uint32_t).
  - Funciones declaradas: `updateCheckpoint(uint8_t)`, `getCrashInfo()`, `markCycleSuccess()`.
- En `JAMR_4.4.7.ino`:
  - Uso de `getCrashInfo()` al inicio para determinar si hubo crash previo y loguear:
    - `Boot count`, `Último checkpoint`, `Timestamp`.
  - Llamadas sistemáticas a `updateCheckpoint()` en puntos clave (GPIO, GPS, sensores, envío de datos).
  - Carga de health data en `sensordata` antes de llamar a `setupModem()`:
    - `sensordata.health_checkpoint = rtc_last_checkpoint;`
    - `sensordata.health_crash_reason = rtc_crash_reason;`
    - `H/L_health_boot_count`, `H/L_health_crash_ts`.
- Integración esperada en capa de comunicación (`dataSend()` + `cryptoaes`) para enviar estos campos al backend.

**Conclusión de calidad:**
- **Grado de cumplimiento:** `MUY ALTO`.
- Diseño coherente con el REQ‑003, con una implementación clara de health data en RTC y exposición vía payload.

**Riesgos/observaciones:**
- Es clave que `rtc_boot_count` **no se incremente en cada wake de deep sleep**, sino solo en resets reales; esto depende de la implementación en `sleepdev.cpp` (no revisada en detalle aquí, pero es crítico).

**Recomendaciones:**
1. Validar experimentalmente, mediante pruebas de campo, que:
   - Deep sleep + wakeup **no** incrementa el boot_count.
   - Watchdog/panic sí incrementan el contador y actualizan crash_reason.
2. En backend, crear dashboards/alertas que usen estos campos para detectar patrones de fallos (p.ej. repetidos `TASK_WDT` en `CP_LTE_CONNECT`).

---

### 3.4 REQ-004 – Firmware Versioning

**Expectativa (resumen):**
- Incluir versión de firmware en logs, payload y health data.
- Permitir trazabilidad clara entre binario desplegado y requisitos/fixes activos.

**Evidencia encontrada:**
- En `JAMR_4.4.7.ino`:
  - `const char* FIRMWARE_VERSION_TAG = "v4.1.1-JAMR4-TIMEOUT";`
  - Campos semánticos:
    - `FIRMWARE_VERSION_MAJOR = 4;`
    - `FIRMWARE_VERSION_MINOR = 1;`
    - `FIRMWARE_VERSION_PATCH = 1;`
  - Logs del setup:
    - Imprime la etiqueta de firmware activa.
  - Se cargan en `sensordata`:
    - `sensordata.fw_major`, `fw_minor`, `fw_patch`.

**Conclusión de calidad:**
- **Grado de cumplimiento:** `ALTO`.
- La firmware version se expone correctamente para backend y logging.

**Recomendaciones:**
1. Alinearse con `CHANGELOG.md` y `VERSION_INFO` para que los nombres/etiquetas sean consistentes (ej. JAMR_4.4.7 vs `v4.1.1-JAMR4-TIMEOUT`).
2. Añadir a la documentación de despliegue el proceso de actualización de `FIRMWARE_VERSION_TAG` en cada release.

---

## 4. Evaluación de riesgos de calidad

### 4.1 Riesgos Técnicos

1. **Estado del módem no persistente (REQ‑001 parcial):**
   - Impacto: Mayor tiempo de conexión y consumo por ciclo; más puntos de fallo al reconectar.
   - Probabilidad: Media (depende de calidad de red y comportamiento del módulo).

2. **Dependencia de terceros (TinyGSM/SIM7080):**
   - Impacto: Cambios o bugs en la librería pueden afectar la lógica defensiva actual.
   - Mitigación: Mantener versión fijada de la librería y pruebas de regresión.

3. **Límites de tiempo fijos:**
   - Impacto: Configuraciones estáticas (`LTE_CONNECT_BUDGET_MS`, `COMM_CYCLE_BUDGET_MS`) podrían no ser óptimas para todas las redes.
   - Mitigación: Telemetría de tiempos y ajustes basados en datos.

### 4.2 Riesgos de Proceso

1. **Desalineación requisito ↔ implementación:**
   - REQ‑001 describe un modelo de módem persistente; el código actual implementa principalmente protección y límites de tiempo.

2. **Falta de pruebas sistemáticas documentadas:**
   - No se encontró (en esta vista) un documento resumen de batería de pruebas específicas para JAMR_4.4.7.

---

## 5. Recomendaciones de acciones

### 5.1 Corto plazo (1–2 semanas)

1. **Documentar mapa de watchdog y health data**
   - Crear en `calidad/`:
     - `MAPA_FEEDS_WATCHDOG.md`.
     - `ESCENARIOS_HEALTH_DATA.md` (ejemplos de patrones esperados de crash_reason/checkpoints).

2. **Validación dirigida de REQ‑002 y REQ‑003**
   - Pruebas con módem desconectado, sin señal, y con brownouts simulados.
   - Confirmar en logs + backend que:
     - Watchdog actúa según diseño.
     - Health data refleja exactamente el flujo de fallo.

### 5.2 Mediano plazo (2–4 semanas)

1. **Diseño detallado de REQ‑001**
   - Definir estrategia seleccionada (CFUN mínimo, DTR sleep, híbrido) para SIM7080.
   - Actualizar `REQ-001` con la decisión arquitectónica concreta y criterios de éxito revisados.

2. **Implementación incremental de módem persistente**
   - Paso 1: Añadir flags en RTC/NVS para indicar estado del módem.
   - Paso 2: Evitar power‑cycle total si no es estrictamente necesario.
   - Paso 3: Medir mejora en tiempo wake‑to‑AT y consumo durante sleep.

3. **Plan de pruebas de regresión**
   - Definir conjunto mínimo de pruebas por release (con y sin señal, distintos operadores, distintos perfiles LTE).

---

## 6. Conclusión

El firmware `JAMR_4.4.7` representa un **salto cualitativo** frente a las versiones JAMR_3, especialmente en lo referente a:

- Protección robusta mediante watchdog (REQ‑002).
- Diagnóstico postmortem y telemetría de salud (REQ‑003).
- Versionado explícito y trazable de firmware (REQ‑004).

Sin embargo, desde la perspectiva de un **ingeniero de calidad**, el requisito `REQ‑001` (gestión de estado del módem entre ciclos de sleep) debe considerarse **parcialmente satisfecho** y requiere una fase específica de diseño e implementación para lograr un módem verdaderamente persistente y eficiente en energía.

Se recomienda **planificar un hito JAMR_4.5.x** o similar, enfocado en terminar REQ‑001 usando la base sólida de watchdog + health data ya implementada, manteniendo el enfoque de simplicidad, trazabilidad y validación en campo que se observa en esta versión.

---

**Estado global de calidad JAMR_4.4.7:**  
- REQ‑002, REQ‑003, REQ‑004: **Alineados / Alta confianza**.  
- REQ‑001: **Implementación parcial – Requiere iteración planificada.**  

**Recomendación:** Apto para **pilotos controlados** y recolección de métricas, con seguimiento específico sobre tiempos de conexión y estabilidad del módem, antes de etiquetar la línea JAMR_4.4.7 como candidata a producción definitiva.
