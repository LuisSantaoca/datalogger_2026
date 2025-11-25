# FIX-4: Plan de Ejecución – Operador Eficiente

**Objetivo:** introducir una lógica de selección de operador LTE/NB-IoT
energéticamente eficiente, que **no toque** el orden actual

```text
lectura sensores → preparar/encriptar → guardar en buffer → intentar LTE/TCP → enviar si se puede
```

y que sólo modifique **cómo** se intenta conectar, no **cuándo** se guardan
los datos.

---

## 1. Principios de diseño

1. **Prioridad absoluta: no perder datos**
	 - Mantener `setupModem` con la secuencia actual:
		 - `dataSend` → `guardarDato` → `startLTE` → `enviarDatos`.
	 - FIX-4 sólo puede tocar la parte `startLTE()` / “intento de conexión”.

2. **Presupuesto de tiempo/energía por ciclo**
	 - Definir un tiempo máximo dedicado a conexión de red en cada ciclo,
		 `LTE_CONNECT_BUDGET_MS` (ejemplo inicial: 120000 ms).
	 - Dentro de ese presupuesto, repartir tiempo entre diferentes “perfiles de
		 operador” sin superar nunca el total.

3. **Operador como “perfil de red”**
	 - No se pretende controlar PLMN fino con `+COPS` en esta fase, sino
		 **perfiles de configuración** (modo/bandas/APN) que favorezcan a uno u
		 otro operador según la SIM y la zona.
	 - En versiones posteriores se podría extender a selección explícita de PLMN
		 si es necesario.

4. **Memoria de último operador exitoso**
	 - Mantener un registro del `operator_id` que funcionó en el último ciclo
		 exitoso y priorizarlo en el siguiente.
	 - Guardar también un pequeño contador de fallos por operador para ajustar
		 el orden dinámicamente.

5. **Cambios mínimos en código de producción**
	 - La lógica actual de `startLTE()` debe reutilizarse, envolviéndola en una
		 capa superior que gestiona perfiles y presupuesto.
	 - Evitar duplicar código de comandos AT; sólo parametrizarlo vía
		 `modemConfig` y, si hace falta, uno o dos comandos adicionales antes de
		 llamar a `startLTE()`.

6. **Respeto a FIX-3 y watchdog**
	 - Cualquier bucle nuevo debe tener:
		 - Contadores de reintentos.
		 - Timeouts claros.
		 - Feeds de watchdog en los puntos ya establecidos.

---

## 2. Nuevas definiciones (propuestas)

### 2.1 Feature flag global

En `gsmlte.h`:

```cpp
// FIX-4: operador eficiente multi-perfil
#define ENABLE_MULTI_OPERATOR_EFFICIENT true
```

Permite desactivar toda la lógica de multi-operador y volver al comportamiento
único actual si fuese necesario.

### 2.2 Presupuesto de conexión por ciclo

En `gsmlte.h` o `sleepdev.h` (según convenga):

```cpp
// Presupuesto total de tiempo para conexión LTE por ciclo (ms)
static const uint32_t LTE_CONNECT_BUDGET_MS = 120000; // 120s, alineado con FIX-4.1.1
```

### 2.3 Estructura `OperatorProfile`

En `gsmlte.h`:

```cpp
struct OperatorProfile {
	const char* name;          // Nombre lógico ("DEFAULT", "RURAL_ALT", etc.)
	int networkMode;           // p.ej. MODEM_NETWORK_MODE (38) o variante
	int bandMode;              // CAT_M, NB_IOT, etc.
	const char* apn;           // APN a usar (puede ser el mismo para todos)
	uint32_t maxTimeMs;        // Tiempo máximo a dedicar a este perfil en un ciclo
	uint8_t id;                // Identificador para tracking/memoria
};
```

Inicializar una tabla estática básica (`gsmlte.cpp`):

```cpp
static const OperatorProfile OP_PROFILES[] = {
	{ "DEFAULT_CATM", MODEM_NETWORK_MODE, CAT_M,  "em", 90000, 0 },
	{ "NB_IOT_FALLBACK", MODEM_NETWORK_MODE, NB_IOT, "em", 30000, 1 },
};
static const size_t OP_PROFILES_COUNT = sizeof(OP_PROFILES) / sizeof(OP_PROFILES[0]);
```

> Nota: el objetivo inicial es tener pocos perfiles, por ejemplo uno CAT-M
> “normal” y otro NB-IoT de fallback en zonas rurales.

### 2.4 Memoria de último operador exitoso

Definir variables globales en `gsmlte.cpp`:

```cpp
static int8_t g_last_success_operator_id = -1;  // -1 = ninguno
static uint8_t g_operator_failures[OP_PROFILES_COUNT] = {0};
```

> Versión futura: persistir esto en NVS o en un pequeño archivo en LittleFS
> para que sobreviva a resets, pero en FIX-4 podemos empezar con memoria volátil.

---

## 3. Algoritmo de selección por ciclo

### 3.1 Construir orden de prueba

Función auxiliar (pseudo-código):

```cpp
void buildOperatorOrder(uint8_t order[], size_t& count) {
	// Inicialmente, usar el orden tal cual OP_PROFILES
	for (size_t i = 0; i < OP_PROFILES_COUNT; ++i) {
		order[i] = i;
	}
	count = OP_PROFILES_COUNT;

	// Si hay un último operador exitoso, colócalo primero
	if (g_last_success_operator_id >= 0) {
		// Buscarlo en order[] y moverlo a la posición 0
	}

	// (Opcional) Podríamos reordenar según g_operator_failures
}
```

Objetivo: probar primero lo que **suele funcionar** en esta zona.

### 3.2 Bucle de intento por operador

Función de alto nivel que reemplaza la llamada directa a `startLTE()` en
`setupModem` cuando `ENABLE_MULTI_OPERATOR_EFFICIENT` está activo:

```cpp
bool startLTE_multiOperator() {
	#if !ENABLE_MULTI_OPERATOR_EFFICIENT
		return startLTE();  // Comportamiento legacy
	#else
		uint32_t remainingBudget = LTE_CONNECT_BUDGET_MS;
		uint8_t order[OP_PROFILES_COUNT];
		size_t count = 0;
		buildOperatorOrder(order, count);

		for (size_t idx = 0; idx < count; ++idx) {
			const OperatorProfile& op = OP_PROFILES[order[idx]];

			if (remainingBudget < 5000) { // Presupuesto mínimo para que valga la pena
				logMessage(1, "[FIX-4] Presupuesto LTE agotado antes de probar más operadores");
				break;
			}

			uint32_t timeForOp = min(op.maxTimeMs, remainingBudget);

			bool ok = tryConnectOperator(op, timeForOp);

			if (ok) {
				g_last_success_operator_id = op.id;
				g_operator_failures[op.id] = 0;
				return true;
			} else {
				g_operator_failures[op.id]++;
				// Descontar tiempo consumido (medido por tryConnectOperator)
				remainingBudget -= timeForOp; // o elapsed real
			}
		}

		return false;
	#endif
}
```

### 3.3 Implementación de `tryConnectOperator`

Idea: **no duplicar `startLTE()`**, sino preparar `modemConfig` para el perfil y
ajustar el timeout interno.

Pseudo-código:

```cpp
bool tryConnectOperator(const OperatorProfile& op, uint32_t maxTimeMs) {
	logMessage(2, String("[FIX-4] Probando perfil de operador: ") + op.name);

	// Configurar modemConfig según el perfil
	modemConfig.networkMode = op.networkMode;
	modemConfig.bandMode = op.bandMode;
	modemConfig.apn = op.apn;

	uint32_t start = millis();

	// Aquí tenemos dos opciones:
	// Opción A: dejar startLTE tal cual y confiar en su timeout 120s,
	//           pero controlar externamente (no ideal).
	// Opción B (preferida): factorizar internamente para permitir maxTimeMs.

	// Para no reescribir demasiado, se puede:
	// - Introducir una variable global "g_lte_max_wait_ms" usada dentro de startLTE.
	// - Ajustarla aquí a min(120000, maxTimeMs).

	g_lte_max_wait_ms = maxTimeMs;  // variable a definir en gsmlte.cpp

	bool ok = startLTE();

	uint32_t elapsed = millis() - start;
	logMessage(2, String("[FIX-4] Resultado perfil ") + op.name +
								 ": " + (ok ? "OK" : "FAIL") +
								 ", tiempo=" + String(elapsed) + "ms");

	return ok;
}
```

Y en `startLTE()` reemplazar el literal `maxWaitTime = 120000` por
`maxWaitTime = min((unsigned long)120000, g_lte_max_wait_ms > 0 ? g_lte_max_wait_ms : 120000);`.

De este modo:

- Código existente casi intacto.
- `tryConnectOperator` controla cuánto tiempo máximo se permite por perfil.

---

## 4. Integración con `setupModem`

En `setupModem`, la parte final hoy es:

```cpp
if (startLTE() == true) {
	...
} else {
	consecutiveFailures++;
}
```

Cambiar a:

```cpp
bool lte_ok = false;

#if ENABLE_MULTI_OPERATOR_EFFICIENT
	lte_ok = startLTE_multiOperator();
#else
	lte_ok = startLTE();
#endif

if (lte_ok) {
	// Igual que antes: CP_LTE_OK, enviarDatos, limpiarEnviados, etc.
} else {
	consecutiveFailures++;
	logMessage(1, "[FIX-4] No se logró conexión LTE con ningún perfil en este ciclo");
}
```

Así se mantiene intacta la lógica de buffer y envío; sólo cambia cuántos y
cuáles intentos de conexión se hacen dentro del presupuesto.

---

## 5. Logging y métricas

Agregar logs específicos:

- Al iniciar `startLTE_multiOperator`: presupuesto inicial.
- Antes de cada perfil: nombre, presupuesto asignado.
- Después de cada intento: resultado y tiempo.
- Al terminar sin éxito: mensaje claro de presupuesto agotado.

Estos logs servirán para llenar:

- `FIX-4_LOG_PASO1.md` (cambios de código).
- `FIX-4_VALIDACION_HARDWARE.md` (mediciones de tiempo por ciclo y consumo).

---

## 6. Criterios de éxito de FIX-4

1. **Integridad de datos mantenida:**
	 - No se pierde ningún dato por cambio de lógica de conexión.

2. **Consumo mejor controlado en zonas de mala señal:**
	 - El tiempo máximo con RF encendida por ciclo está acotado por
		 `LTE_CONNECT_BUDGET_MS`, no por un timeout “ciego” de 120 s.

3. **Conexiones exitosas priorizan el mejor perfil:**
	 - En zonas con más de un operador disponible, el sistema tiende a usar el
		 mismo perfil que funcionó recientemente.

4. **Compatibilidad con FIX-3 y watchdog:**
	 - No se introducen nuevos cuelgues ni resets por watchdog relacionados con
		 la lógica multi-operador.

Con este plan, el siguiente paso será implementar las estructuras y funciones
descritas en `gsmlte.h/.cpp`, de forma incremental y documentando cada cambio
en `FIX-4_LOG_PASO1.md`.
