# FIX-6 - Análisis de Código

## Contexto

En JAMR_4 se han introducido varios fixes importantes:

- FIX-3: loops defensivos y watchdog en puntos críticos.
- FIX-4: gestión multi-operador eficiente con presupuesto `LTE_CONNECT_BUDGET_MS`.
- FIX-5: validación de PDP activo antes de abrir sockets TCP.

Estos fixes mejoran mucho la robustez, pero aún existe un riesgo en **zonas rurales con mala red**:

- El dispositivo puede pasar demasiado tiempo en el ciclo de comunicación (LTE + TCP + reintentos), aunque cada componente individual tenga timeouts razonables.
- Esto implica **consumo energético excesivo** y tiempos de ciclo muy variables.

## Problema detectado

- No existe un **límite global explícito** para el tiempo total invertido en:
  - Búsqueda y selección de operador (`startLTE_multiOperator`, `tryConnectOperator`).
  - Registro LTE (`startLTE`).
  - Activación y validación de PDP (`ensurePdpActive`).
  - Apertura de socket y reintentos de envío (`tcpOpenSafe`, `enviarDatos`, etc.).
- En escenarios límite (red intermitente, servidor lento, congestión), se pueden encadenar varios timeouts parciales que sumen un tiempo muy alto en un solo ciclo.

## Objetivo del FIX-6

- Introducir un **presupuesto global de tiempo por ciclo de comunicación** (ej. `COMM_CYCLE_BUDGET_MS`).
- Garantizar que, pase lo que pase a nivel de red o servidor, el datalogger:
  - Nunca gaste más de `COMM_CYCLE_BUDGET_MS` milisegundos intentando comunicar.
  - Registre un log claro cuando se agote ese presupuesto.
  - Termine el ciclo de forma ordenada (cerrar recursos y permitir deep sleep).

## Hipótesis de impacto

- En zonas urbanas / buena señal: el impacto es mínimo, casi todo ciclo terminará **mucho antes** del límite.
- En zonas rurales / muy mala señal: el fix protegerá la **batería y la previsibilidad** del comportamiento, forzando a abandonar el intento cuando ya no tiene sentido seguir.

## Siguientes pasos

- Definir ubicación y semántica de `COMM_CYCLE_BUDGET_MS` en `gsmlte.h`.
- Identificar los puntos de entrada del ciclo de comunicación (probablemente `setupModem` / `startLTE_multiOperator` / `enviarDatos`).
- Diseñar una pequeña API interna para consultar si queda presupuesto (ej. `bool cycleBudgetRemaining()` o `bool checkCycleBudget(const char* contextTag)`).
- Planear casos de prueba (logs) en:
  - Escenario normal (ciudad) donde el presupuesto no se consume.
  - Escenario adverso (rural) donde se agota el presupuesto y se fuerza el corte.

## Implementación realizada (24/11/2025)

- `COMM_CYCLE_BUDGET_MS` definido en `gsmlte.h` junto con helpers públicos (`resetCommunicationCycleBudget`, `remainingCommunicationCycleBudget`, `ensureCommunicationBudget`).
- `gsmlte.cpp` ahora registra el inicio del ciclo en `setupModem()` y utiliza el helper para monitorear:
  - `startLTE` (incluyendo `startLTE_multiOperator` y `tryConnectOperator`).
  - `tcpOpenSafe`, `tcpOpen` y `tcpSendData`.
  - `enviarDatos` (inicio y por línea).
- Cuando el presupuesto se agota, se emite log `[FIX-6] Presupuesto de ciclo agotado en <contexto>` y la función retorna fallo controlado.
