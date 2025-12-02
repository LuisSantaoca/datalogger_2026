# FIX-3: Plan de Ejecución – Watchdog en Loops Bloqueantes

**Firmware base:** JAMR_4 v4.1.1-JAMR4-TIMEOUT  
**Objetivo:** eliminar cuelgues causados por loops bloqueantes relacionados con
el módem/GPS (SIM7080 + TinyGSM) que impiden que el watchdog se alimente de
forma correcta.

Este plan asume un programador senior con conocimiento profundo de:
- **ESP32-S3** (watchdog, FreeRTOS, timings, RTC memory).
- **SIM7080G** (estados de módem, comandos AT, GPS integrado).
- Arquitectura actual de JAMR_4 y premisas en `PREMISAS_DE_FIXS.md`.

---

## 1. Principios de Diseño y Buenas Prácticas

Antes de tocar código, TODO cambio de FIX-3 debe respetar estos principios:

1. **Simplicidad estructural (KISS realista):**
	 - Preferir pequeños helpers bien nombrados (por ejemplo,
		 `runWithTimeoutAndWdtFeed(...)`) antes que lógica compleja inline.
	 - Evitar anidamientos profundos de `while`/`for` + `if`; si un bloque se
		 vuelve difícil de leer, extraerlo a función.

2. **Separación de responsabilidades:**
	 - El watchdog **protege**; no debe mezclarse con la lógica de negocio.
	 - La lógica de negocio (GPS, LTE, TCP) **indica sus puntos críticos**, pero
		 la mecánica de timeout/feeds se centraliza en pequeños utilitarios.

3. **Timeouts explícitos y razonados:**
	 - Cada operación potencialmente bloqueante **debe tener un timeout explícito
		 a nivel de firmware**, incluso si la librería ya implementa uno.
	 - Los valores de timeout deben justificarse con:
		 - Especificaciones del SIM7080G.
		 - Métricas reales de campo (logs previos).

4. **Watchdog como red de seguridad, no como timer de control:**
	 - El flujo normal **nunca** debe depender del disparo del watchdog.
	 - El objetivo de FIX-3 es que, incluso en presencia de bugs de terceros
		 (librería, módem), el sistema salga de loops inaceptables **antes** de
		 llegar al timeout del WDT cuando sea posible.

5. **Feature flags y reversibilidad:**
	 - Introducir un flag de compilación, por ejemplo
		 `#define ENABLE_WDT_DEFENSIVE_LOOPS true`, que permita desactivar todo el
		 comportamiento nuevo sin borrar código.
	 - Esto sigue la premisa de rollback en < 5 minutos.

6. **Respeto estricto a la lógica actual (no romper lo que funciona):**
	 - No cambiar la semántica de éxito/fracaso de funciones (`true`/`false`).
	 - Los nuevos timeouts internos deben, en caso de alcance, comportarse como
		 “no hay datos / fallo controlado”, no como estados intermedios extraños.

7. **Uso correcto del SIM7080G:**
	 - No encadenar comandos AT de forma que violen tiempos mínimos del módem
		 (por ejemplo, respetar delays recomendados tras `CFUN`, `CAOPEN`, GPS).
	 - Evitar mandar nuevos comandos mientras el módem está claramente ocupado
		 procesando otros (reutilizar patrones ya probados en FIX-1/FIX-2).

8. **Uso correcto del ESP32-S3:**
	 - Evitar loops CPU-intensivos sin `delay`/`vTaskDelay` cortos que permitan
		 al scheduler de FreeRTOS ejecutar otras tareas del sistema.
	 - Mantener `delay(1)` o `vTaskDelay(1)` en bucles de espera con I/O para
		 no monopolizar el core.

9. **Trazabilidad y observabilidad:**
	 - Cada nueva rama de timeout o salida anticipada debe loguearse con contexto:
		 - Operación (GPS/AT/stream).
		 - Tiempo invertido.
		 - Índice de intento si aplica.
	 - Seguir la convención de prefijos `[GPS]`, `[LTE]`, `[MODEM]`, etc.

10. **Testing incremental obligatorio:**
		- Validar primero en laboratorio (1 ciclo, luego 10 ciclos) antes de campo.
		- Comparar métricas con v4.1.1: tiempo de ciclo, resets de watchdog,
			éxito de transmisión, comportamiento de GPS.

---

## 2. Definiciones Técnicas Previas

1. **Feature flag global (en `gsmlte.h`):**

	 ```cpp
	 // Control de FIX-3: protección extra en loops con módem
	 #define ENABLE_WDT_DEFENSIVE_LOOPS true
	 ```

2. **Helper genérico propuesto (conceptual):**

	 ```cpp
	 // Ejecuta una operación repetitiva con timeout total y feeds WDT regulares.
	 // La operación se pasa como lambda o función que puede fallar/éxito.
	 // NO se implementa aquí, solo se documenta para el paso de código.
	 ```

	 La implementación concreta se decidirá en el paso de código, pero el
	 principio es **no copiar-pegar** patrones de “while con timeout + feed” en
	 muchos sitios.

---

## 3. Pasos de Ejecución – Nivel Alto

1. **Introducir feature flag y pequeños helpers en `gsmlte.h/.cpp`.**
2. **Proteger `getGpsSim()` con timeout interno por intento y feeds WDT.**
3. **Fortalecer `sendATCommand()` y `readResponse()` para streams largos.**
4. **Revisar y, si aplica, acotar `while (!modem.testAT(1000))`.**
5. **Agregar logging detallado para nuevos paths de timeout.**
6. **Compilar, probar en hardware (1 ciclo → 10 ciclos → 24h).**
7. **Actualizar documentación de validación y reporte final.**

---

## 4. Paso a Paso Detallado

### Paso 1 – Feature flag y utilitarios base

**Archivos:** `gsmlte.h`, `gsmlte.cpp`.

1.1. **Agregar flag en `gsmlte.h`:**

- Definir `ENABLE_WDT_DEFENSIVE_LOOPS` cerca de otras configuraciones FIX.
- Comentario claro indicando que es parte de FIX-3.

1.2. **(Opcional) Declarar helper genérico:**

- Prototipo de una función inline/`static` que reciba:
	- Un timeout máximo en ms.
	- Un puntero a función/lambda que realice la operación y devuelva `bool`.
- Este helper podría usarse tanto en `getGpsSim()` como en otros patrones.

> Buenas prácticas:
> - Mantener helpers `static` en `gsmlte.cpp` si sólo se usan ahí.
> - Evitar plantillas complicadas; priorizar claridad sobre genericidad extrema.

### Paso 2 – Proteger `getGpsSim()` (GPS SIM7080)

**Archivo:** `gsmlte.cpp`.

2.1. **Analizar patrón actual (50 intentos × 2s).**

- Confirmar ubicación exacta del bucle `for (int i = 0; i < 50; ++i)`.
- Medir tiempo máximo teórico actual (~100s) vs WDT (120s).

2.2. **Diseñar timeout por intento acorde al SIM7080:**

- Basado en experiencia previa y manual AT:
	- Tiempo razonable para `AT+CGNSINF` típico: << 5s.
- Definir, por ejemplo:
	- `GPS_SINGLE_ATTEMPT_TIMEOUT_MS = 5000`.
	- Tiempo total máximo para GPS configurable (ej. 60–80s).

2.3. **Envolver `modem.getGPS(...)` en mini-loop con timeout:**

- Dentro de cada iteración del `for (i < 50)`:
	- Medir `unsigned long startAttempt = millis();`.
	- Ejecutar un bucle:
		- Mientras `millis() - startAttempt < GPS_SINGLE_ATTEMPT_TIMEOUT_MS`:
			- Llamar a `modem.getGPS(...)`.
			- Si devuelve `true`, salir con éxito.
			- Alimentar watchdog en cada vuelta (`esp_task_wdt_reset()`).
			- Incluir `delay(100)` o similar para no bloquear el core.
	- Si se alcanza el timeout de intento sin éxito, loguear WARN y pasar al
		siguiente intento del `for`.

2.4. **Imponer límite de tiempo total para GPS:**

- Además del timeout por intento, definir un tiempo máximo global, por
	ejemplo `GPS_TOTAL_TIMEOUT_MS` (ej. 60000–80000ms).
- Antes de iniciar un nuevo intento en el `for`, comprobar si el tiempo total
	excede el límite; si sí, romper el bucle y reportar fallo de GPS.

2.5. **Logging detallado:**

- Para cada intento, loguear al menos nivel DEBUG (`level 3`):
	- Índice de intento.
	- Tiempo invertido en el intento.
	- Motivo de salida: éxito / timeout intento / timeout total.

### Paso 3 – Fortalecer `sendATCommand()` y `readResponse()`

**Archivo:** `gsmlte.cpp`.

3.1. **Objetivo:** garantizar que, incluso si `SerialAT.available()` ofrece
datos de forma continua, se alimenta el watchdog a intervalos constantes.

3.2. **Estrategia:**

- Introducir un contador o marca de tiempo dentro del inner loop
	`while (SerialAT.available())` para:
	- Cada N bytes leídos **o** cada M ms, ejecutar:
		- `esp_task_wdt_reset()`.
		- Un `delay(0)` / `delay(1)` para ceder CPU.

3.3. **Implementación propuesta (a alto nivel):**

- Dentro del inner loop:
	- Llevar `unsigned long lastFeed = millis();` fuera del loop.
	- Cada vez que se lea un char, comprobar si `millis() - lastFeed` excede
		un umbral pequeño (ej. 100–200ms):
		- Si sí, hacer `esp_task_wdt_reset(); lastFeed = millis();`.
		- Opcional: `delay(1);` para no monopolizar CPU.

3.4. **Mantener semántica existente:**

- No cambiar la lógica que determina éxito/fracaso (`expectedResponse`).
- Asegurar que los logs actuales se mantienen (no romper parsers de logs).

### Paso 4 – Revisar `while (!modem.testAT(1000))`

**Archivo:** `gsmlte.cpp` (varias zonas, según uso actual).

4.1. **Localizar todos los `while (!modem.testAT(...))`.**

- Usar búsqueda para identificar cada patrón.
- Documentar en comentario sobre cada uno:
	- Si ya tiene contador de reintentos.
	- Si hay un timeout global alrededor.

4.2. **Para los que no tienen límite claro:**

- Introducir un contador máximo de reintentos o timeout global, por ejemplo:
	- `MAX_AT_TEST_RETRIES` o `MAX_AT_TEST_TIME_MS`.
- Alimentar watchdog en cada vuelta y romper con log de ERROR si se alcanza el
	límite.

4.3. **Respetar el comportamiento legacy:**

- Si actualmente el código espera indefinidamente y después funciona, no
	introducir cortes agresivos; usar límites amplios pero acotados.

### Paso 5 – Logging y Trazabilidad

5.1. **Añadir mensajes de log con prefijo `[FIX-3]` o similar** para:

- Timeouts internos por intento de GPS.
- Timeouts internos en lectura de AT/streams.
- Salidas anticipadas de `while (!modem.testAT)`.

5.2. **No saturar logs en producción:**

- Mantener la mayor parte de estos mensajes en nivel DEBUG (3).
- Para condiciones anómalas reales (timeout total de GPS, imposibilidad de
	conseguir AT), usar nivel ERROR (0) o WARNING (1).

### Paso 6 – Testing y Validación

6.1. **Compilación:**

- Confirmar que el proyecto compila sin warnings nuevos.

6.2. **Pruebas en laboratorio:**

- 1 ciclo completo (boot → GPS → LTE → envío → sleep).
- 10 ciclos consecutivos, observando:
	- Ausencia de resets de watchdog.
	- Tiempos de GPS y LTE dentro de rangos esperados.

6.3. **Pruebas en hardware con condiciones reales (24h):**

- De preferencia en escenario donde antes se observaban cuelgues.
- Verificar:
	- 0 cuelgues.
	- 0 resets no explicados del WDT.
	- Logs `[FIX-3]` mostrando comportamientos de timeout razonables.

6.4. **Actualizar `FIX-3_VALIDACION_HARDWARE.md` y `FIX-3_REPORTE_FINAL.md`.**

- Documentar métricas comparativas vs v4.1.1.
- Decidir si `ENABLE_WDT_DEFENSIVE_LOOPS` se deja en `true` para producción.

---

## 5. Criterios de Éxito de FIX-3

1. **Cuelgues por GPS/AT loops desaparecen** en escenarios donde antes
	 ocurrían.
2. **Resets de watchdog**:
	 - 0 en operación normal durante 24h.
	 - Si ocurre, health data permite identificar punto exacto.
3. **Tiempo de ciclo**:
	 - No aumenta de forma significativa vs v4.1.1.
	 - Idealmente igual o ligeramente mejor, gracias a timeouts explícitos.
4. **Sin regresiones** en conectividad, GPS ni envío de datos.

Con este plan, el siguiente paso es implementar los cambios descritos en
`gsmlte.cpp`/`gsmlte.h` respetando estas directrices.
