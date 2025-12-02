# FIX-4: Análisis de Código – Operador Eficiente

**Objetivo del análisis:** entender cómo JAMR_4 conecta hoy a la red LTE,
qué decisiones toma (o no toma) respecto al operador, y dónde se concentran
el tiempo y el consumo, manteniendo clara la prioridad de **no perder datos**
(buffer primero, envío después).

---

## 1. Flujo actual de alto nivel

En `JAMR_4.ino`, el flujo del ciclo es:

1. `setupTimeData(&sensordata);` – RTC/tiempo.
2. `setupGpsSim(&sensordata);` – GPS vía módem SIM (opcional según `gps_sim_enabled`).
3. `setupSensores(&sensordata);` – lectura de sensores.
4. `setupModem(&sensordata);` – **preparar payload, guardar en buffer y conectar/enviar**.
5. Estadísticas y `sleepIOT();` – deep sleep.

El orden clave para este fix está en `setupModem` (en `gsmlte.cpp`):

```cpp
// 1) Inicializar config
initModemConfig();

// 2) Configurar serial y arrancar GSM
SerialMon.begin(115200);
SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
startGsm();
updateCheckpoint(CP_GSM_OK);
esp_task_wdt_reset();
getIccid();

// 3) Inicializar LittleFS
if (!iniciarLittleFS(...)) { ESP.restart(); }
esp_task_wdt_reset();

// 4) Preparar y GUARDAR datos SIEMPRE
String cadenaEncriptada = dataSend(data);
guardarDato(cadenaEncriptada);

// 5) Intentar LTE + TCP + envío
if (startLTE() == true) {
	updateCheckpoint(CP_LTE_OK);
	esp_task_wdt_reset();
	enviarDatos();
	updateCheckpoint(CP_DATA_SENT);
	limpiarEnviados();
	esp_task_wdt_reset();
	consecutiveFailures = 0;
} else {
	consecutiveFailures++;
}

// 6) Apagar módem y terminar
modemPwrKeyPulse();
modemInitialized = true;
```

Punto importante: **los datos ya están en el buffer antes de intentar
conectarse**. Si `startLTE()` falla o no hay operador, el dato queda
pendiente para ciclos futuros. Esto cumple la prioridad de no perder datos y
es innegociable para FIX-4.

---

## 2. Cómo se conecta hoy: `startGsm` + `startLTE`

### 2.1 `startGsm()` – Asegurar que el módem responde

Código relevante (resumen):

```cpp
void startGsm() {
	int retry = 0;
	const int maxRetries = 3;
	const int maxTotalAttempts = 15;
	int totalAttempts = 0;

	while (!modem.testAT(1000)) {
		esp_task_wdt_reset();
		flushPortSerial();
		logMessage(3, "Esperando respuesta del módem...");

		if (retry++ > maxRetries) {
			modemPwrKeyPulse();
			sendATCommand("+CPIN?", "READY", 15000);
			retry = 0;
			logMessage(2, "Reintentando inicio del módem");
		}

		if (++totalAttempts > maxTotalAttempts) {
			logMessage(0, "Módem no responde después de 15 intentos");
			return;
		}
	}

	logMessage(2, "Comunicación GSM establecida");

	// Encender RF con +CFUN=1 (y fallback +CFUN=1,1)
	// Esperar estabilización de RF (delays fragmentados + WDT)
	// Verificar +CFUN? == 1
}
```

Este bloque **no decide operador**; solo garantiza que el SIM7080 responde y
que la RF está encendida. Tiene límites de reintentos para no colgarse, y ya
consume una parte no trivial del tiempo y energía del ciclo.

### 2.2 `startLTE()` – Configuración de red única

Código relevante (resumido):

```cpp
bool startLTE() {
	logMessage(2, "Iniciando conexión LTE");
	updateCheckpoint(CP_LTE_CONNECT);

	// 1) Configurar modo de red y banda (una sola vez)
	sendATCommand("+CNMP=" + modemConfig.networkMode, "OK", getAdaptiveTimeout());
	sendATCommand("+CMNB=" + modemConfig.bandMode, "OK", getAdaptiveTimeout());

	// 2) Configurar bandas completas CAT-M y NB-IOT (fijo)
	sendATCommand(catmBands, "OK", getAdaptiveTimeout());
	sendATCommand("+CBANDCFG=\"NB-IOT\",2,3,5,8,20,28", "OK", getAdaptiveTimeout());
	sendATCommand("+CBANDCFG?", "OK", getAdaptiveTimeout());
	delay(LONG_DELAY);

	// 3) Configurar PDP con APN único
	String pdpCommand = "+CGDCONT=1,\"IP\",\"" + modemConfig.apn + "\"";
	sendATCommand(pdpCommand, "OK", getAdaptiveTimeout());

	// 4) Activar PDP
	sendATCommand("+CNACT=0,1", "OK", 5000);

	// 5) Esperar conexión a la red con timeout global
	unsigned long t0 = millis();
	unsigned long maxWaitTime = 120000; // 120s (FIX-4.1.1)

	while (millis() - t0 < maxWaitTime) {
		esp_task_wdt_reset();
		int signalQuality = modem.getSignalQuality();
		logMessage(3, "Calidad de señal: " + String(signalQuality));

		sendATCommand("+CNACT?", "OK", getAdaptiveTimeout());

		if (modem.isNetworkConnected()) {
			logMessage(2, "Conectado a la red LTE");
			sendATCommand("+CPSI?", "OK", getAdaptiveTimeout());
			flushPortSerial();
			return true;
		}

		delay(1000);
	}

	logMessage(0, "Timeout: No se pudo conectar a la red LTE");
	return false;
}
```

Observaciones clave:

- `startLTE()` trabaja con **un solo conjunto de configuración** (modo, bandas,
	APN) definido en `modemConfig`:
	- `modemConfig.networkMode` (por defecto `MODEM_NETWORK_MODE = 38`).
	- `modemConfig.bandMode` (por defecto `CAT_M`).
	- `modemConfig.apn` (por defecto `"em"`).
- No hay noción de **múltiples operadores** ni de cambiar PLMN/operador
	explícitamente durante este proceso (el módem busca dentro de las bandas
	configuradas y se registra donde pueda).
- El tiempo máximo es **120 s por ciclo** para la conexión LTE, independientemente
	de cuántas veces el módem haya intentado internamente distintos operadores.

En otras palabras: hoy hay **un único “perfil de red”** y se le da al módem
hasta 120 s para que encuentre algo conectable dentro de ese perfil.

---

## 3. Relación con el buffer de datos y la prioridad “no perder datos”

La lógica de buffer está encapsulada en:

- `dataSend(sensordata_type* data)` – construye payload y lo encripta.
- `guardarDato(String data)` – escribe la línea en `buffer.txt`, con gestión
	de espacio y marca `#ENVIADO`.
- `enviarDatos()` – recorre el buffer, intenta enviar y marca como `#ENVIADO`.
- `limpiarEnviados()` – borra los ya enviados.

El detalle importante para FIX-4:

1. **El guardado en buffer se hace antes de cualquier intento de LTE.**
2. Si la conexión falla (`startLTE() == false`), no se llama a `enviarDatos()` y
	 por lo tanto **no se pierden datos**; simplemente se acumulan para futuros
	 ciclos.
3. La energía gastada en buscar red/operador no afecta la integridad del buffer,
	 pero sí afecta:
	 - Duración del ciclo.
	 - Consumo de batería.
	 - Riesgo de caer en “ciclos de intento eterno” en zonas de señal muy mala.

Por tanto, FIX-4 **no debe modificar** el orden:

```text
lectura sensores → preparar/encriptar → guardar en buffer → intentar LTE/TCP → enviar si se puede
```

La mejora debe ocurrir exclusivamente en **cómo** se intenta LTE/TCP (qué
operadores, cuánto tiempo por operador, cuándo rendirse), no en el hecho de que
los datos se guarden primero.

---

## 4. Problemas actuales desde el punto de vista de operador y consumo

1. **Operador implícito y no controlado:**
	 - El firmware no maneja explícitamente “operadores” (AT&T/Telcel/Movistar);
		 se limita a configurar bandas y APN y deja que el módem decida.
	 - En zonas donde un operador es muy débil y otro aceptable, el módem puede
		 gastar tiempo intentando el peor primero sin que el firmware pueda
		 intervenir.

2. **Timeout global único de 120 s:**
	 - Si el módem tarda mucho intentando registrarse, **todo el ciclo puede
		 consumir hasta 120 s con la RF encendida**, incluso si la probabilidad de
		 éxito es muy baja en esa zona.
	 - No hay presupuesto de tiempo por “estrategia de operador”; el presupuesto
		 es monolítico.

3. **Sin memoria de operador “bueno” por zona:**
	 - Aunque se obtiene la celda y el operador en `+CPSI?`, no hay lógica que
		 diga: “la última vez funcionó con operador X, inténtalo primero”.
	 - Cada ciclo es más o menos “desde cero” respecto a elección de operador.

4. **Eficiencia energética limitada:**
	 - En la versión 4.3 (no JAMR_4) ya se observó que la búsqueda de operadores
		 podía agotar baterías; JAMR_4 usa una aproximación más robusta de
		 timeouts, pero **sigue sin optimizar la exploración de operador**.

5. **No se prioriza “salir rápido y volver luego” cuando la red está mala:**
	 - Si la red está muy mal, sería preferible:
		 - Intentar un tiempo acotado razonable.
		 - Registrar fallo y **volver a dormir** para conservar batería.
	 - Hoy, el límite de 120 s se aplica de forma uniforme, incluso si la señal es
		 mala o nula.

---

## 5. Requisitos específicos que debe cumplir FIX-4

1. **Mantener prioridad de no perder datos:**
	 - No modificar el orden de `setupModem`: buffer siempre antes de intentar
		 conectar.

2. **Introducir conciencia de “operador” a nivel de firmware:**
	 - Definir una abstracción de `OperatorProfile` (nombre/APN/bandas/tiempos).
	 - Poder decidir un orden de prueba por ciclo (último exitoso primero, luego
		 fallback).

3. **Presupuesto de tiempo/energía por ciclo:**
	 - Definir un `LTE_CONNECT_BUDGET_MS` total.
	 - Dividirlo entre operadores/estrategias, en lugar de un único bloque de
		 120 s ciego.

4. **Compatibilidad con `startLTE()`:**
	 - Reutilizar tanto como sea posible la lógica actual (evitar reescribirla).
	 - Encapsular la selección de operador en funciones que configuren
		 `modemConfig` y/o comandos AT extra, antes de llamar a `startLTE()` con un
		 timeout acotado.

5. **Medición y logging de operador/consumo:**
	 - Loguear claramente:
		 - Qué “perfil de operador” se está intentando.
		 - Cuánto tiempo se invirtió en cada intento.
		 - Resultado (éxito/fallo) y razón (sin señal, timeout, error AT).

6. **No romper FIX-3 ni el watchdog:**
	 - La nueva lógica debe apoyarse en las protecciones ya añadidas (loops con
		 feeds WDT, timeouts internos) y no introducir bucles nuevos sin límites.

---

Con este análisis, el siguiente documento a rellenar será
`FIX-4_PLAN_EJECUCION.md`, donde definiremos:

- La estructura de datos para perfiles de operador.
- El presupuesto de tiempo por ciclo y por operador.
- Cómo envolver `startLTE()` en una capa de “multi-operador eficiente” sin
	tocar la prioridad del buffer ni la robustez del watchdog.
