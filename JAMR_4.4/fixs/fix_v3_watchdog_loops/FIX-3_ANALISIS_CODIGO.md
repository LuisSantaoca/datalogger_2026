# FIX-3: Análisis de Código – Watchdog en Loops Bloqueantes

**Firmware base:** JAMR_4 v4.1.1-JAMR4-TIMEOUT  
**Objetivo del análisis:** identificar todos los bucles (loops) que pueden
dejar de alimentar el watchdog durante operaciones largas, con foco en
interacciones con el módem (LTE/GPS) y librerías externas.

Este documento **no define aún los cambios**, solo marca el mapa de riesgo
para que el plan de ejecución sea mínimo, localizado y alineado con
`PREMISAS_DE_FIXS.md`.

---

## 1. Alcance de FIX-3

- **Incluye:**
	- Código de módem / LTE / TCP en `gsmlte.cpp` y `gsmlte.h`.
	- Código de GPS SIM7080 en `gsmlte.cpp` (`setupGpsSim`, `getGpsSim`, etc.).
	- Interacciones con `esp_task_wdt_*` y loops con timeouts largos.
- **Explora pero NO toca en primera iteración (solo documenta):**
	- Loops de sensores en `sensores.cpp`.
	- Loops de RTC en `timedata.cpp`.
	- Loops de cifrado en `cryptoaes.cpp`.
- **Explícitamente fuera de alcance de FIX-3:**
	- Cambios en estructura del payload.
	- Cambios de arquitectura de watchdog (tiempo, configuración básica).

Motivo: el problema reportado en campo es **cuelgue con buena energía**,
cuando el firmware debería recuperarse por watchdog. Los síntomas apuntan a
loops donde el feed **no ocurre porque el flujo nunca vuelve desde una
llamada bloqueante a librería externa** (TinyGSM / GPS).

---

## 2. Resumen de Watchdog Actual

En `JAMR_4.ino`:

- Watchdog inicializado en `setup()`:
	- `WATCHDOG_TIMEOUT_SEC = 120` (definido en `sleepdev.h`).
	- `esp_task_wdt_init()` + `esp_task_wdt_add(NULL)`.
	- Feeds explícitos en los puntos principales del ciclo:
		- Después de `setupGPIO()` inicial.
		- Después de lectura de sensores.
		- Después de `setupModem(&sensordata)`.
		- Antes de entrar a deep sleep.

En `gsmlte.cpp` ya existen feeds en:

- `startLTE()` → dentro de `while (millis() - t0 < maxWaitTime)`.
- `readResponse()` → feed por iteración de bucle externo.
- `sendATCommand()` → feed por iteración de bucle externo.
- `setupGpsSim()` → feeds alrededor de `startGps()` / `getGpsSim()`.

**Conclusión:** el diseño asume que el flujo sale periódicamente de las
llamadas al módem para poder ejecutar los feeds. FIX-3 se centra en los
casos donde esa suposición **no se cumple**.

---

## 3. Mapa de Bucles Críticos en `gsmlte.cpp`

### 3.1. `startLTE()` – Espera de registro LTE

**Ubicación:** `gsmlte.cpp`, `bool startLTE()`.

Fragmento relevante:

- Bucle principal de espera:
	- `unsigned long maxWaitTime = 120000;`  (FIX-4.1.1: 120s)
	- `while (millis() - t0 < maxWaitTime) { ... }`
- Dentro del bucle:
	- `esp_task_wdt_reset();` **(feed presente)**.
	- `int signalQuality = modem.getSignalQuality();`
	- `sendATCommand("+CNACT?", ...)`.
	- `if (modem.isNetworkConnected()) { ... return true; }`
	- `delay(1000);`

**Riesgo:**

- `modem.getSignalQuality()` y `modem.isNetworkConnected()` dependen de TinyGSM,
	pero la llamada es corta y el feed ocurre **antes** de ellas, una vez por
	iteración.
- Si TinyGSM internamente se cuelga, el feed puede no ejecutarse, pero la
	probabilidad es similar a otros usos; ya está mitigado por el reset de
	watchdog.

**Clasificación FIX-3:**

- Riesgo: **MEDIO** (120s de ventana, pero con feed explícito).
- Acción en FIX-3: **Sólo documentar**; no es el principal sospechoso de
	cuelgues actuales.

---

### 3.2. `sendATCommand()` – Bucle de lectura de respuesta AT

**Ubicación:** `gsmlte.cpp`, `bool sendATCommand(const String& command, ...)`.

Patrón actual:

- Bucle externo con timeout:
	- `while (millis() - start < finalTimeout) { ... }`
- Bucle interno de lectura:
	- `while (SerialAT.available()) { ... }`
- Feed watchdog:
	- `esp_task_wdt_reset();` **después** del inner loop.

**Riesgo:**

- Si el módem o la línea serie entran en un estado donde `SerialAT.available()`
	devuelve datos continuamente (eco, ruido, stream muy largo), el inner loop
	puede ejecutarse **durante periodos largos** sin salir al feed.
- Aunque hay `delay(1)` después del feed, este delay sólo se ejecuta **una vez
	por iteración externa**, no por cada ráfaga de datos.
- En la práctica, si la cantidad de datos es muy alta o la librería no drena
	el buffer, se podría acumular tiempo significativo sin `esp_task_wdt_reset()`.

**Clasificación FIX-3:**

- Riesgo: **ALTO** (afecta a casi todos los comandos AT largos).
- Acción en FIX-3: **Modificar** patrón de lectura para garantizar feeds
	regulares incluso con streams largos.

---

### 3.3. `readResponse()` – Bucle genérico de lectura con timeout

**Ubicación:** `gsmlte.cpp`, `String readResponse(unsigned long timeout)`.

Patrón actual:

- Bucle externo:
	- `while (millis() - start < finalTimeout) { ... }`
- Inner loop:
	- `while (SerialAT.available()) { ... }`
- Feed watchdog:
	- `esp_task_wdt_reset();` al final de cada iteración del bucle externo.

**Riesgo:**

- Igual patrón que `sendATCommand()`; si el módem envía muchos datos seguidos,
	el inner loop puede consumir mucho tiempo sin devolver el control.

**Clasificación FIX-3:**

- Riesgo: **ALTO**.
- Acción en FIX-3: **Modificar** de forma similar a `sendATCommand()`.

---

### 3.4. `setupGpsSim()` / `getGpsSim()` – Búsqueda de GPS Fix

**Ubicación principal:** `gsmlte.cpp`, funciones `setupGpsSim(...)` y
`getGpsSim()`.

Patrón relevante en `getGpsSim()` (simplificado):

```cpp
for (int i = 0; i < 50; ++i) {
	esp_task_wdt_reset();
	if (modem.getGPS(...)) {
		// usar coordenadas y salir
	}
	delay(2000);  // 2s × 50 = hasta ~100s
}
```

**Riesgo:**

- `modem.getGPS(...)` es una llamada a TinyGSM que internamente ejecuta
	comandos AT y parsing.
- Si `modem.getGPS()` **se bloquea internamente** (por bug, respuesta parcial,
	estado extraño del módem), **no se vuelve al for** y por tanto **no se
	ejecuta más `esp_task_wdt_reset()`**.
- Con `WATCHDOG_TIMEOUT_SEC = 120`, es muy plausible que:
	- Se entre a `getGpsSim()`.
	- En uno de los 50 intentos, `modem.getGPS()` se queda colgado.
	- Nunca se vuelve al feed → watchdog dispara reset o, peor, el sistema queda
		en un estado no recuperable si el WDT no llega a activarse por alguna
		configuración.
- Esto encaja con el síntoma reportado: **cuelgues con buena batería**, en
	operaciones donde se usa GPS/módem.

**Clasificación FIX-3:**

- Riesgo: **MUY ALTO**.
- Acción en FIX-3: **Modificar** `getGpsSim()` para:
	- Envolver `modem.getGPS()` en un timeout propio por intento.
	- Alimentar el watchdog dentro de ese timeout interno.
	- Limitar tiempo total invertido en GPS.

---

### 3.5. `while (!modem.testAT(1000))` – Esperas activas de módem

**Ubicación:** varias funciones de inicialización del módem (secciones de
`startGsm()` / `modemRestart()` y similares).

Patrón típico:

```cpp
while (!modem.testAT(1000)) {
	// reintentos sin contador explícito en este fragmento
}
```

**Riesgo:**

- Si el módem no responde nunca a `AT`, este loop puede extenderse mucho.
- No siempre hay un contador de reintentos o timeout explícito alrededor.
- Depende de detalles de implementación que aún no se han recorrido línea a
	línea para FIX-3.

**Clasificación FIX-3:**

- Riesgo: **MEDIO-ALTO** (depende de contexto completo).
- Acción en FIX-3:
	- **Documentar ahora como candidato**.
	- Decidir en el plan si se incluye una envoltura genérica tipo
		"operación con timeout + feed" también para este patrón.

---

### 3.6. `waitForToken` / `waitForAnyToken` – Lectura de Streams Genéricos

**Ubicación:** `gsmlte.cpp`, helpers estáticos al final del archivo.

Patrón:

- Bucle externo con timeout (`while (millis() - start < timeout_ms)`).
- Inner `while (s.available())` leyendo caracteres.
- `esp_task_wdt_reset()` presente en el bucle externo.

**Riesgo:**

- Similar patrón a `sendATCommand/readResponse`, pero estos helpers se usan en
	menos sitios y con timeouts más controlados.

**Clasificación FIX-3:**

- Riesgo: **MEDIO**.
- Acción en FIX-3: **No tocar en primera iteración**, pero mantener
	consistencia si cambiamos el patrón general de lectura.

---

## 4. Otros Módulos (Sensores, Tiempo, Cifrado)

### 4.1. `sensores.cpp`

- Loops detectados:
	- `while (sonda.available()) { ... }` (lectura RS485).
	- `while (millis() < timeout && bytesRead < responseLength) { ... }`.
	- Distintos `for` con tamaño acotado (`numLecturas`, longitud de frame, etc.).
- Características:
	- Timeouts típicamente cortos.
	- No interactúan con librerías que puedan bloquear el core (simple
		`Stream`/`HardwareSerial` local).

**Clasificación FIX-3:** Riesgo **BAJO**, **NO objetivo** del fix, pero se
documenta para que no se mezcle lógica de sensores con lógica de módem.

### 4.2. `timedata.cpp`

- Loops:
	- `for (int attempt = 1; attempt <= RTC_INIT_RETRIES; attempt++)`.
	- `for (int attempt = 1; attempt <= RTC_READ_RETRIES; attempt++)`.
- Todos con contadores y sin llamadas bloqueantes a librerías externas pesadas.

**Clasificación FIX-3:** Riesgo **BAJO**, **NO objetivo**.

### 4.3. `cryptoaes.cpp`

- Loops:
	- Varios `for (int attempt = 1; attempt <= ENCRYPTION_RETRIES && ...)`.
- Operaciones puramente en RAM/CPU, sin I/O externo.

**Clasificación FIX-3:** Riesgo **BAJO**, **NO objetivo**.

---

## 5. Conclusiones para el Plan de Ejecución (FIX-3)

1. **Puntos CRÍTICOS a intervenir sí o sí:**
	 - `getGpsSim()`:
		 - Envolver `modem.getGPS()` en un timeout interno por intento.
		 - Alimentar watchdog dentro de ese mini-loop.
		 - Limitar tiempo total máximo de GPS.
	 - `sendATCommand()` y `readResponse()`:
		 - Garantizar que, aunque `SerialAT.available()` tenga muchos datos,
			 haya feeds regulares del watchdog.

2. **Puntos a evaluar/incluir según complejidad:**
	 - `while (!modem.testAT(1000))`:
		 - Posible helper genérico tipo `bool runWithTimeout(fn, max_ms)` que
			 integre feed de watchdog.

3. **Puntos que se dejan sólo documentados en FIX-3:**
	 - Loops de sensores, RTC, cifrado (bajo riesgo actual).
	 - Helpers `waitForToken` / `waitForAnyToken`, salvo que en pruebas de campo
		 aparezca evidencia de bloqueo ahí.

4. **Enlace con premisas de FIXs:**
	 - Cambios deben ser:
		 - **Mínimos y localizados** (`gsmlte.cpp` únicamente para este fix).
		 - **Aditivos**: no eliminar lógica existente, sino envolver/proteger.
		 - **Reversibles**: idealmente bajo un feature flag
			 `ENABLE_WDT_DEFENSIVE_LOOPS`.

Este análisis sirve como entrada directa para `FIX-3_PLAN_EJECUCION.md`, donde
se definirán los pasos concretos de implementación y validación.
