# Revisión de Calidad – Firmware JAMR_4 v4.1.1

## 1. Alcance de la revisión

- **Firmware:** JAMR_4
- **Versión analizada:** v4.1.1-JAMR4-TIMEOUT
- **Módulos clave revisados (sin modificar código):**
  - `JAMR_4.ino` (flujo principal, watchdog, checkpoints, deep sleep)
  - `gsmlte.cpp/.h` (GSM/LTE, multi-operador, FIX-3/4/5/6/7, LittleFS)
  - `sleepdev.*` (gestión de sueño)
  - `sensores.*` (lectura de sensores)
  - `timedata.*` (RTC/tiempo)
  - `type_def.h` (estructura de datos principal)
  - Carpeta `fixs/` (planes, análisis y reportes de FIX-3..7)

Objetivo: evaluar robustez, riesgos y preparación para liberación a producción, sin cambios de código.

---

## 2. Robustez y estabilidad

### 2.1 Watchdog y bucles largos (FIX-003)

**Observaciones:**
- Se utiliza `esp_task_wdt_reset()` de forma consistente en:
  - Bucle de espera de respuesta del módem en `startGsm()`.
  - Bucle de comandos AT largos (`sendATCommand`, `readResponse`, `waitForToken`, `waitForAnyToken`).
  - Bucle de GPS (`getGpsSim`) y RTC (`timedata.cpp`).
- El watchdog se inicializa explícitamente en `JAMR_4.ino` con `WATCHDOG_TIMEOUT_SEC = 120` y se desinicializa/reinicializa para evitar conflictos con Arduino.

**Riesgos:**
- La lógica de watchdog depende de muchos feeds manuales dispersos; futuras modificaciones podrían introducir bucles largos sin `esp_task_wdt_reset()`.

**Recomendación:**
- Definir una política explícita en documentación interna:
  - “Cualquier bucle que pueda superar 500 ms debe:
    1) Tener condición clara de salida.
    2) Alimentar watchdog al menos cada 150–500 ms.”
- Considerar el uso sistemático del macro `ENABLE_WDT_DEFENSIVE_LOOPS` para nuevas secciones críticas.

### 2.2 Presupuesto global de ciclo de comunicación (FIX-006)

**Observaciones:**
- Se introduce `COMM_CYCLE_BUDGET_MS` y funciones:
  - `resetCommunicationCycleBudget()`
  - `remainingCommunicationCycleBudget()`
  - `ensureCommunicationBudget(const char* contextTag)`
- Estas funciones se emplean en:
  - `startLTE()`
  - `startLTE_multiOperator()` y `tryConnectOperator()`
  - `enviarDatos()`
  - `tcpOpenSafe()` / `tcpOpen()` / `tcpSendData()`

**Riesgos:**
- Nuevos flujos de red (futuros) podrían omitir `ensureCommunicationBudget()` y romper el límite de tiempo por ciclo.

**Recomendación:**
- Documentar en `README` y en los requisitos de calidad que:
  - “Toda función que realice operaciones de comunicación prolongadas debe llamar a `ensureCommunicationBudget()` al inicio y en bucles internos si aplica.”

### 2.3 Manejo de errores de módem y reinicios

**Observaciones:**
- `startGsm()` tiene límite de intentos totales (`maxTotalAttempts`) y reintentos controlados con `PWRKEY`.
- `modemRestart()` encapsula un reinicio completo del módem.

**Riesgos:**
- No existe todavía una política de “N ciclos fallidos consecutivos ⇒ marcar dispositivo en estado de fallo persistente” (solo aumenta `consecutiveFailures`).

**Recomendación:**
- Para producción, definir umbral de `consecutiveFailures` a partir del cual se marque una condición de degradación grave en health data (ej. `health_crash_reason` o un nuevo campo) para facilitar alarmado en backend.

---

## 3. Comunicación LTE, multi-operador y PDP (FIX-4, FIX-5, FIX-7)

### 3.1 Multi-operador eficiente (FIX-4)

**Observaciones:**
- `OperatorProfile OP_PROFILES[]` define al menos dos perfiles: `DEFAULT_CATM` y `NB_IOT_FALLBACK`.
- `startLTE_multiOperator()`:
  - Calcula orden dinámico de prueba con `buildOperatorOrder()`.
  - Asigna un presupuesto total `LTE_CONNECT_BUDGET_MS` (120s).
  - Reparte tiempo por perfil usando `maxTimeMs` y recorte con `g_lte_max_wait_ms`.

**Riesgos:**
- La lógica de distribución de presupuesto es correcta en el código actual, pero puede degradarse si se añaden más perfiles sin ajustar `LTE_CONNECT_BUDGET_MS` ni los tiempos por perfil.

**Recomendación:**
- Documentar que cualquier nuevo `OperatorProfile` debe venir acompañado de:
  - Revisión de `maxTimeMs` total.
  - Casos de prueba de campo específicos para ese perfil.

### 3.2 PDP activo obligatorio (FIX-5)

**Observaciones:**
- `ensurePdpActive()` valida, via `+CNACT?`, que exista contexto con estado `1` y IP distinta de `0.0.0.0`.
- En los logs de prueba se observa:
  - `+APP PDP: 0,DEACTIVE` tras el primer `+CNACT=0,1`.
  - Reintentos y finalmente `PDP activo detectado en contexto 0 (IP 100.116.56.23)`.

**Riesgos:**
- Si el firmware se despliega en redes con comportamientos no estándar de PDP (p.ej. IPv6, contextos múltiples), la lógica de parseo actual podría necesitar ampliación.

**Recomendación:**
- Antes de producción masiva, validar en al menos 1 red adicional (otro operador / otro país) que `+CNACT?` muestre el mismo formato y que la lógica de parseo siga siendo válida.

### 3.3 Perfil LTE persistente (FIX-7)

**Observaciones:**
- `loadPersistedOperatorId()` y `persistOperatorId()` manejan `/lte_profile.cfg` con:
  - Manejo de archivo inexistente.
  - Validación de rango de ID (`0 <= id < OP_PROFILES_COUNT`).
  - Logs detallados `[FIX-7]` de carga y actualización.
- En los logs proporcionados se observa:
  - Primer ciclo: archivo inexistente → orden por defecto.
  - Conexión exitosa con `DEFAULT_CATM`.
  - Mensaje: `[FIX-7] Perfil LTE persistente actualizado a id=0`.

**Riesgos:**
- Si en futuras versiones se reordenan los perfiles o se cambian los IDs, el entero guardado podría apuntar a otro perfil diferente al original.

**Recomendación:**
- Documentar en `gsmlte.h` o en un doc interno que los `id` de `OperatorProfile` forman parte del contrato persistente (no cambiar sin migración de archivo).

---

## 4. Almacenamiento, buffer de datos y LittleFS

### 4.1 Gestión de buffer (`/buffer.txt`)

**Observaciones:**
- `guardarDato()`:
  - Limpia líneas marcadas con `#ENVIADO` cuando se sobrepasa `MAX_LINEAS` de no enviadas.
  - Reescribe el archivo entero tras cualquier cambio.
- `enviarDatos()`:
  - Usa archivo temporal `/buffer.tmp` y `rename` atómico.
  - Marca envíos exitosos con `#ENVIADO`.
  - Respeta presupuesto de comunicación (FIX-6).

**Riesgos:**
- Si `MAX_LINEAS` no se ajusta al tamaño real de payload (que podría crecer en el futuro), existe el riesgo de acercarse demasiado al límite de espacio en LittleFS.

**Recomendación:**
- Mantener una tabla simple en documentación técnica que relate:
  - Tamaño típico de payload encriptado.
  - Número máximo de líneas no enviadas (`MAX_LINEAS`).
  - Uso máximo esperado de espacio en `/buffer.txt`.

### 4.2 LittleFS init

**Observaciones:**
- `iniciarLittleFS()` realiza varios intentos con retardo y reporta espacio total, usado y libre.
- Si todos los intentos fallan, aborta la ejecución con log crítico.

**Riesgos:**
- En producción, un fallo persistente de montaje sin mecanismo de recuperación (formateo controlado, por ejemplo) dejaría el dispositivo en bucle de reinicios.

**Recomendación:**
- Evaluar, para producción, si se desea añadir una estrategia de “formateo seguro” tras N fallos consecutivos de montaje, o al menos registrar esta decisión en los requisitos.

---

## 5. Seguridad y consistencia de datos

### 5.1 Estructura `sensordata_type`

**Observaciones:**
- `type_def.h` define claramente los campos, con comentarios extensos.
- `dataSend()` fuerza un tamaño fijo `STRUCT_SIZE = 46` y compara con `sizeof(sensordata_type)`.
- En logs se observa discrepancia (43 vs 46) que se maneja con warning pero sin romper el flujo.

**Riesgos:**
- Cambios futuros en `sensordata_type` podrían desalinearse del protocolo de backend si no se actualiza `STRUCT_SIZE` y el formato esperado.

**Recomendación:**
- Tratar la discrepancia de `sizeof(sensordata_type)` como un **bloqueante de QA**:
  - Antes de producción, alinear `STRUCT_SIZE` y el tamaño real o documentar explícitamente el padding previsto.
  - Verificar que el backend conoce exactamente cuántos bytes se envían y en qué orden.

### 5.2 Encriptación y CRC

**Observaciones:**
- Se aplica CRC16 antes de encriptar, con funciones bien delimitadas.
- El módulo de encriptación (`cryptoaes`) se invoca con logs propios controlados.

**Riesgos:**
- Ninguno evidente desde el punto de vista de overflow o corrupción interna; el principal riesgo es la desincronización de protocolo con el backend.

**Recomendación:**
- Incluir en la documentación de requisitos una breve especificación de formato:
  - `ICCID (N bytes) + payload sensores (STRUCT_SIZE bytes) + CRC16 (2 bytes)` antes de encriptar.

---

## 6. Logging, salud y observabilidad

### 6.1 Niveles de log

**Observaciones:**
- Uso de niveles (0=ERROR, 1=WARN, 2=INFO, 3=DEBUG) con timestamps en ms.
- Existe `modemConfig.enableDebug` para filtrar logs de debug.

**Riesgos:**
- En compilación actual, se imprimen muchos logs de nivel DEBUG (AT completos, hexdumps) que pueden:
  - Aumentar consumo energético.
  - Añadir ruido en producción.

**Recomendación:**
- Definir una configuración de **build de producción** con:
  - `enableDebug = false` por defecto.
  - Posibilidad de reactivar debug solo en firmware de diagnóstico.

### 6.2 Health data (FIX-004)

**Observaciones:**
- Se incluyen en el payload: checkpoint, crash_reason, boot_count y timestamps.
- `markCycleSuccess()` limpia estados de crash tras ciclo exitoso.

**Riesgos:**
- Si el backend no consume todavía estos campos, se pierde una fuente importante de diagnóstico.

**Recomendación:**
- Acordar con backend al menos un uso mínimo:
  - Dashboard simple de:
    - número de resets por dispositivo,
    - porcentaje de ciclos que terminan en cada checkpoint,
    - tiempo medio hasta CP_LTE_OK y CP_DATA_SENT.

---

## 7. Pruebas recomendadas antes de liberación a producción

1. **Campaña multi-sitio:**
   - Sitio urbano con buena cobertura.
   - Sitio rural con RSSI bajo.
   - Sitio donde solo NB-IoT funcione (si es posible).
   - En cada sitio, al menos 10 ciclos consecutivos sin watchdog reset ni bloqueos de LTE.

2. **Validación de FIX-7 entre ciclos:**
   - Primer ciclo: verificar logs de creación y actualización de `/lte_profile.cfg`.
   - Segundo ciclo: verificar que se loguea `"Perfil LTE persistente cargado: id=X"` y que el perfil coincide con el esperado.

3. **Consumo y tiempo de ciclo:**
   - Comparar duración de ciclo (inicio→sleep) antes y después de aplicar FIX-4/5/6/7.
   - Asegurar que en sitios “fáciles” la duración no se incrementa de forma significativa.

4. **Pruebas de estrés de LittleFS:**
   - Ejecutar múltiples ciclos con buffer lleno (cerca de `MAX_LINEAS`).
   - Verificar que no se producen errores de montaje ni corrupción visible de archivos.

---

## 8. Conclusión de calidad

- Desde el punto de vista de diseño defensivo, manejo de errores y diagnósticos, el firmware JAMR_4 v4.1.1 se encuentra en un estado **avanzado y cercano a apto para producción**, con:
  - Watchdog bien integrado en los bucles críticos.
  - Control de tiempo de ciclo y de tiempo por perfil LTE.
  - Validación estricta de PDP activo antes de abrir TCP.
  - Persistencia de perfil LTE para mejorar tiempos de conexión en entornos estables.
  - Sistema de health data y logging estructurado listo para ser consumido por backend.

- Los principales puntos pendientes antes de liberación masiva son de **proceso y validación de campo**, no de arquitectura de código:
  - Formalizar campaña de pruebas multi-sitio.
  - Verificar uso de health data y formato de payload en backend.
  - Asegurar build de producción con nivel de log adecuado.
  - Alinear tamaño de estructura de datos con el protocolo acordado.

- Se recomienda NO modificar la arquitectura actual, sino consolidarla con:
  - Más evidencia de campo.
  - Documentación de contratos (IDs de perfiles, tamaños de payload, uso de watchdog y presupuestos de tiempo).
