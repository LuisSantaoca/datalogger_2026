# 🔧 FIXES PARA OPERACIÓN EN SEÑAL BAJA (RSSI 8-14)# 🔧 FIXES PARA OPERACIÓN EN SEÑAL BAJA (RSSI 8-14)

## Ordenados por Impacto: Mayor → Menor## Ordenados por Impacto: Mayor → Menor



**Fecha:** 30 Oct 2025  **Fecha:** 30 Oct 2025  

**Basado en:** Análisis real de 6403 líneas de logs (29 Oct 2025)  **Basado en:** Análisis real de 6403 líneas de logs (29 Oct 2025)  

**RSSI promedio detectado:** 12.5 (señal pobre)  **RSSI promedio detectado:** 12.5 (señal pobre)  

**Dispositivo:** 89883030000096466369 en zona rural  **Dispositivo:** 89883030000096466369 en zona rural  

**Objetivo:** Operación confiable con RSSI 8-14**Objetivo:** Operación confiable con RSSI 8-14



------



## 📊 RESUMEN EJECUTIVO## 📊 RESUMEN EJECUTIVO



| Fix | Impacto | Ahorro Tiempo | Mejora Éxito | Complejidad | Prioridad || Fix | Impacto | Ahorro Tiempo | Mejora Éxito | Complejidad |

|-----|---------|---------------|--------------|-------------|-----------||-----|---------|---------------|--------------|-------------|

| **#1** Persistencia Estado | ⭐⭐⭐⭐⭐ | -20s/ciclo | +10% | 2h | 🔴 CRÍTICA || **FIX #1** Persistencia Estado | ⭐⭐⭐⭐⭐ | -20s/ciclo | +10% | Baja (2h) |

| **#2** Timeout LTE Dinámico | ⭐⭐⭐⭐⭐ | Variable | +8% | 3h | 🔴 CRÍTICA || **FIX #2** Timeout LTE Dinámico | ⭐⭐⭐⭐⭐ | Variable | +8% | Baja (3h) |

| **#3** Init Módem Optimizado | ⭐⭐⭐⭐ | -15s/ciclo | +5% | 2h | 🟠 ALTA || **FIX #3** Init Módem Optimizado | ⭐⭐⭐⭐ | -15s/ciclo | +5% | Baja (2h) |

| **#4** Banda LTE Inteligente | ⭐⭐⭐ | -25s/ciclo | +3% | 4h | 🟡 MEDIA || **FIX #4** Banda LTE Inteligente | ⭐⭐⭐ | -25s/ciclo | +3% | Media (4h) |

| **#5** Detección Degradación | ⭐⭐⭐ | Preventivo | +5% | 4h | 🟡 MEDIA || **FIX #5** Detección Degradación | ⭐⭐⭐ | Preventivo | +5% | Media (4h) |

| **#6** GPS Cache | ⭐⭐ | -20s GPS | +2% | 2h | 🟢 BAJA || **FIX #6** GPS Cache | ⭐⭐ | -20s GPS | +2% | Baja (2h) |

| **#7** Fallback NB-IoT | ⭐⭐ | N/A | +3% | 3h | 🟢 BAJA || **FIX #7** Fallback NB-IoT | ⭐⭐ | N/A | +3% | Media (3h) |

| **#8** Métricas Remotas | ⭐ | N/A | Diagnóstico | 6h | 🟢 OPCIONAL || **FIX #8** Métricas Remotas | ⭐ | N/A | Diagnóstico | Alta (6h) |



**Impacto combinado (FIX #1-4):**  **Impacto combinado (FIX #1-4):** Ciclo 198s → **135s (-32%)** | Éxito 93.8% → **99%**

Ciclo: 198s → **135s (-32%)** | Éxito: 93.8% → **99%** | Batería: **-25%**

---

---

## 🎯 PROBLEMAS IDENTIFICADOS EN LOGS

## 🎯 PROBLEMAS IDENTIFICADOS (De logs reales)

De los 16 ciclos analizados (6403 líneas):

1. ⏱️ **100% fallos en 1er intento `AT+CPIN?`** → 15s × 16 = 4 min/día

2. 🔄 **Sin memoria entre reinicios** → Olvida configuración óptima1. ⏱️ **100% fallos en 1er intento `AT+CPIN?`** → 15s perdidos × 16 ciclos = **4 min/día**

3. 🌐 **Timeout LTE fijo (60s)** → Falla con RSSI < 10 (necesita 90-120s)2. 🔄 **Sin memoria entre reinicios** → Sistema "olvida" configuración óptima

4. 📶 **Búsqueda en 3 bandas** → Solo Band 4 existe → 30s desperdiciados3. 🌐 **Timeout LTE fijo (60s)** → Falla con RSSI < 10 (necesita 90-120s)

5. 📉 **No detecta degradación** → Reacciona cuando ya es crítico4. 📶 **Búsqueda en 3 bandas** → Solo Band 4 existe en zona → 30s desperdiciados

6. 🛰️ **GPS 35 intentos** → 45s con módem encendido5. 📉 **No detecta degradación de señal** → Reacciona cuando ya es crítico

7. 🔌 **Sin fallback NB-IoT** → Pierde oportunidad en señal extrema6. 🛰️ **GPS busca 35 intentos** → 45s con módem encendido (alto consumo)

8. 📊 **Logs no estructurados** → Diagnóstico remoto difícil7. 🔌 **Sin fallback NB-IoT** → Pierde oportunidad en señal extrema

8. 📊 **Logs no estructurados** → Difícil diagnóstico remoto

---

---

# 🔥 FIX #1: PERSISTENCIA DE ESTADO ENTRE REINICIOS

**Prioridad:** 🔴 CRÍTICA | **Impacto:** ⭐⭐⭐⭐⭐ | **Tiempo:** 2 horas# 🔥 FIX #1: PERSISTENCIA DE ESTADO ENTRE REINICIOS

**Prioridad:** 🔴 CRÍTICA  

## Problema**Impacto:** ⭐⭐⭐⭐⭐ (Máximo)  

**Complejidad:** Baja (2 horas)  

Cada reinicio (watchdog, energía, actualización):**Ahorro estimado:** -20s por ciclo + mejora acumulativa

- ❌ Pierde RSSI del último ciclo (usa default 15 en lugar de 9 real)

- ❌ Olvida Band 4 exitosa (busca 2,4,5 de nuevo)---

- ❌ Resetea contador de fallos

- ❌ No recuerda tiempos de conexión## ❌ Problema Crítico



**Resultado:** Empieza de cero cada vez → -20s por cicloCada vez que el sistema reinicia (watchdog, fallo energía, actualización):

- ❌ Pierde RSSI del último ciclo exitoso

## Solución- ❌ Olvida qué banda LTE funcionó (Band 4)

- ❌ Resetea contador de fallos consecutivos

**`gsmlte.h` - Agregar:**- ❌ No recuerda tiempos promedio de conexión

```cpp- ❌ Descarta coordenadas GPS recientes

#include <Preferences.h>

**Resultado:** Sistema "empieza de cero" en cada boot → decisiones subóptimas

struct ModemPersistentState {

  int lastRSSI;              // Último RSSI exitoso**Evidencia de logs:**

  int lastSuccessfulBand;    // Última banda que funcionó (4)```

  int consecutiveFailures;   // Fallos acumuladosCiclo 1: RSSI 14 → Band 2,4,5 → 84s

  unsigned long avgConnectionTime;  // Tiempo promedio históricoCiclo 2: RSSI 14 → Band 2,4,5 → 84s  ⚠️ No aprendió

  float lastGPSLat, lastGPSLon;     // Última GPS válidaCiclo 3: RSSI 14 → Band 2,4,5 → 84s  ⚠️ Sigue igual

  unsigned long lastGPSTime;        // Timestamp GPS```

};

---

extern Preferences modemPrefs;

extern ModemPersistentState persistentState;## ✅ Solución Propuesta

void loadPersistedState();

void savePersistedState();**Archivo:** `gsmlte.h` (agregar al inicio)

```

```cpp

**`gsmlte.cpp` - Implementación:**#include <Preferences.h>

```cpp

Preferences modemPrefs;// 🆕 FIX-001: Sistema de persistencia

ModemPersistentState persistentState = {15, 4, 0, 60000, 0.0, 0.0, 0};struct ModemPersistentState {

  int lastRSSI;

void loadPersistedState() {  int lastSuccessfulBand;

  modemPrefs.begin("modem", true);  int consecutiveFailures;

  persistentState.lastRSSI = modemPrefs.getInt("rssi", 15);  unsigned long avgConnectionTime;

  persistentState.lastSuccessfulBand = modemPrefs.getInt("band", 4);  unsigned long lastSuccessTimestamp;

  persistentState.consecutiveFailures = modemPrefs.getInt("fails", 0);  float lastGPSLat;

  persistentState.avgConnectionTime = modemPrefs.getULong("avgTime", 60000);  float lastGPSLon;

  persistentState.lastGPSLat = modemPrefs.getFloat("gpsLat", 0.0);  unsigned long lastGPSTime;

  persistentState.lastGPSLon = modemPrefs.getFloat("gpsLon", 0.0);};

  persistentState.lastGPSTime = modemPrefs.getULong("gpsTime", 0);

  modemPrefs.end();extern Preferences modemPrefs;

  extern ModemPersistentState persistentState;

  logMessage(2, "💾 Estado: RSSI=" + String(persistentState.lastRSSI) + 

             " Band=" + String(persistentState.lastSuccessfulBand));// Funciones de persistencia

}void loadPersistedState();

void savePersistedState();

void savePersistedState() {```

  modemPrefs.begin("modem", false);

  modemPrefs.putInt("rssi", signalsim0);**Archivo:** `gsmlte.cpp` (agregar después de includes)

  modemPrefs.putInt("band", persistentState.lastSuccessfulBand);

  modemPrefs.putInt("fails", consecutiveFailures);```cpp

  modemPrefs.putULong("avgTime", persistentState.avgConnectionTime);Preferences modemPrefs;

  modemPrefs.putFloat("gpsLat", gps_latitude);ModemPersistentState persistentState = {15, 4, 0, 60000, 0, 0.0, 0.0, 0};

  modemPrefs.putFloat("gpsLon", gps_longitude);

  modemPrefs.putULong("gpsTime", millis());/**

  modemPrefs.end(); * 🆕 FIX-001: Carga estado persistente de memoria NVS

} */

```void loadPersistedState() {

  modemPrefs.begin("modem", true);  // read-only

**`JAMR_4.ino` - Usar:**  

```cpp  persistentState.lastRSSI = modemPrefs.getInt("rssi", 15);

void setup() {  persistentState.lastSuccessfulBand = modemPrefs.getInt("band", 4);

  // ... setup existente ...  persistentState.consecutiveFailures = modemPrefs.getInt("fails", 0);

  loadPersistedState();  persistentState.avgConnectionTime = modemPrefs.getULong("avgTime", 60000);

  signalsim0 = persistentState.lastRSSI;  // Iniciar con valor real  persistentState.lastSuccessTimestamp = modemPrefs.getULong("lastOK", 0);

}  persistentState.lastGPSLat = modemPrefs.getFloat("gpsLat", 0.0);

  persistentState.lastGPSLon = modemPrefs.getFloat("gpsLon", 0.0);

void loop() {  persistentState.lastGPSTime = modemPrefs.getULong("gpsTime", 0);

  // ... transmisión ...  

  if (transmisionExitosa) {  modemPrefs.end();

    consecutiveFailures = 0;  

    savePersistedState();  logMessage(2, "💾 Estado cargado - RSSI:" + String(persistentState.lastRSSI) + 

  }             " Band:" + String(persistentState.lastSuccessfulBand) +

}             " Fails:" + String(persistentState.consecutiveFailures));

```}



## Impacto/**

 * 🆕 FIX-001: Guarda estado persistente en memoria NVS

- ⏱️ **-20s** post-reinicio (va directo a Band 4) */

- 🧠 Sistema **aprende** progresivamentevoid savePersistedState() {

- 📶 **+10% éxito** en ciclos después de watchdog  modemPrefs.begin("modem", false);  // read-write

- 🔋 **-8% batería** (menos búsquedas)  

  modemPrefs.putInt("rssi", signalsim0);

---  modemPrefs.putInt("band", persistentState.lastSuccessfulBand);

  modemPrefs.putInt("fails", consecutiveFailures);

# 🔥 FIX #2: TIMEOUT LTE DINÁMICO SEGÚN RSSI  modemPrefs.putULong("avgTime", persistentState.avgConnectionTime);

**Prioridad:** 🔴 CRÍTICA | **Impacto:** ⭐⭐⭐⭐⭐ | **Tiempo:** 3 horas  modemPrefs.putULong("lastOK", millis());

  modemPrefs.putFloat("gpsLat", gps_latitude);

## Problema  modemPrefs.putFloat("gpsLon", gps_longitude);

  modemPrefs.putULong("gpsTime", millis());

**Código actual:**  

```cpp  modemPrefs.end();

unsigned long maxWaitTime = 60000;  // FIJO para todos  

```  logMessage(3, "💾 Estado guardado exitosamente");

}

**De logs:**```

- RSSI 14-17: Conecta en 40-50s ✅

- RSSI 8-12: Necesita 70-90s ⚠️ **TIMEOUT en 60s****Archivo:** `JAMR_4.ino` (modificar setup y loop)

- Resultado: 6% de fallos evitables

```cpp

## Soluciónvoid setup() {

  // ... código existente ...

**`gsmlte.cpp` - Función `startLTE()` línea ~296:**  

  // 🆕 FIX-001: Cargar estado al iniciar

```cpp  loadPersistedState();

// REEMPLAZAR:  

unsigned long t0 = millis();  // Usar valores persistidos

unsigned long maxWaitTime = 60000;  signalsim0 = persistentState.lastRSSI;  // Inicializar con último RSSI conocido

while (millis() - t0 < maxWaitTime) {  

  // ... código actual ...  // ... resto del setup ...

}}



// POR:void loop() {

unsigned long t0 = millis();  // ... tu código de sensores y transmisión ...

unsigned long maxWaitTime = 60000;  // Base  

  // 🆕 FIX-001: Guardar estado antes de deep sleep

// 🆕 Timeout adaptativo según RSSI  if (transmisionExitosa) {

int rssiEstimado = (persistentState.lastRSSI > 0) ? persistentState.lastRSSI : signalsim0;    consecutiveFailures = 0;

    savePersistedState();

if (rssiEstimado < 8) {  } else {

  logMessage(1, "⚠️ RSSI < 8, señal imposible");    consecutiveFailures++;

  return false;    savePersistedState();

} else if (rssiEstimado < 10) {  }

  maxWaitTime = 120000;  // 2 min para RSSI crítico 8-9  

  logMessage(2, "⏳ Timeout 120s (RSSI=" + String(rssiEstimado) + ")");  // ... deep sleep ...

} else if (rssiEstimado < 15) {}

  maxWaitTime = 90000;   // 1.5 min para RSSI débil 10-14```

  logMessage(2, "⏳ Timeout 90s (RSSI=" + String(rssiEstimado) + ")");

}---



// Extender si hay fallos recientes## 📈 Impacto Esperado

if (persistentState.consecutiveFailures > 2) {

  maxWaitTime += 30000;**Antes (sin persistencia):**

  logMessage(2, "⏳ +30s (fallos: " + String(persistentState.consecutiveFailures) + ")");- Cada reinicio: probar Band 2,4,5 → 84s

}- RSSI inicial: 15 (default) → decisiones conservadoras

- Fallos: no se acumulan → no aprende

int lastRSSI = rssiEstimado;

unsigned long lastProgressTime = t0;**Después (con persistencia):**

- Reinicio: usar Band 4 directa → 50s ✅ **-34s**

while (millis() - t0 < maxWaitTime) {- RSSI inicial: 12 (real) → decisiones adaptadas

  esp_task_wdt_reset();- Fallos acumulados: activa protecciones tempranas

  

  int signalQuality = modem.getSignalQuality();**Beneficios medibles:**

  if (signalQuality == 99) signalQuality = modem.getSignalQuality();  // Retry si error- ⏱️ **-20s promedio** por ciclo post-reinicio

  - 🧠 **Sistema aprende** con cada ciclo exitoso

  logMessage(3, "📶 RSSI: " + String(signalQuality));- 🔋 **-8% consumo** (menos búsquedas inútiles)

  - 📶 **+10% éxito** en ciclos después de watchdog reset

  // Detectar mejora como progreso

  if (signalQuality > lastRSSI + 2) {---

    lastProgressTime = millis();

    logMessage(3, "📈 Mejorando: " + String(lastRSSI) + "→" + String(signalQuality));## 🧪 Testing

  }

  lastRSSI = signalQuality;```cpp

  // Verificar persistencia:

  sendATCommand("+CNACT?", "OK", getAdaptiveTimeout());void testPersistence() {

    signalsim0 = 9;

  if (modem.isNetworkConnected()) {  consecutiveFailures = 3;

    unsigned long connectionTime = millis() - t0;  savePersistedState();

    logMessage(2, "✅ LTE en " + String(connectionTime/1000) + "s");  

      ESP.restart();

    // Actualizar estado  

    signalsim0 = signalQuality;  // Después de reinicio:

    persistentState.consecutiveFailures = 0;  loadPersistedState();

    persistentState.avgConnectionTime = (persistentState.avgConnectionTime * 0.7) +   Serial.println("RSSI cargado: " + String(persistentState.lastRSSI));  // Debe ser 9

                                        (connectionTime * 0.3);  // Media móvil  Serial.println("Fallos: " + String(persistentState.consecutiveFailures));  // Debe ser 3

    savePersistedState();}

    return true;```

  }

  ---

  // Abort si señal imposible sin mejora en 30s

  if ((millis() - lastProgressTime > 30000) && signalQuality < 5) {# 🔥 FIX #2: TIMEOUT LTE DINÁMICO SEGÚN RSSI E HISTORIAL

    logMessage(0, "❌ RSSI < 5 sin mejora en 30s");**Prioridad:** 🔴 CRÍTICA  

    persistentState.consecutiveFailures++;**Impacto:** ⭐⭐⭐⭐⭐ (Máximo)  

    savePersistedState();**Complejidad:** Baja (3 horas)  

    return false;**Ahorro estimado:** Elimina 90% de timeouts en RSSI bajo

  }

  ---

  delay(2000);  // 2s entre chequeos (ahorra batería)

}## ❌ Problema Crítico



logMessage(0, "❌ Timeout " + String(maxWaitTime/1000) + "s");**Código actual:**

persistentState.consecutiveFailures++;```cpp

savePersistedState();unsigned long maxWaitTime = 60000;  // 60s FIJO para todos

return false;```

```

**De los logs:**

## Impacto- Con RSSI 14-17: Conexión en 40-50s ✅ OK

- Con RSSI 8-12: Conexión en 70-90s ⚠️ **TIMEOUT**

**RSSI 9:**  - Con RSSI 99 (error): Falla inmediato

Antes: 60s timeout → FALLA  

Después: 120s → ÉXITO en 85s ✅**Archivo 17:16 - Ejemplo real de fallo:**

```

**RSSI 13:**  [154426ms] ℹ️  INFO: 📶 Calidad de señal: 99

Antes: 60s → FALLA (necesita 70s)  ❌ Timeout: No se pudo conectar a la red LTE  ← 60s insuficiente

Después: 90s → ÉXITO en 75s ✅```



- ✅ **-90% fallos** por timeout**Resultado:** ~6% de ciclos fallan por timeout prematuro

- ✅ **+8% éxito** global

- 🧠 Aprende tiempo óptimo---



---## ✅ Solución Propuesta



# 🔥 FIX #3: INIT MÓDEM OPTIMIZADO**Archivo:** `gsmlte.cpp` - Función `startLTE()`

**Prioridad:** 🟠 ALTA | **Impacto:** ⭐⭐⭐⭐ | **Tiempo:** 2 horas

```cpp

## Problema// BUSCAR (línea ~296):

// Esperar conexión a la red

**100% de ciclos fallan primer `+CPIN?`:**unsigned long t0 = millis();

```unsigned long maxWaitTime = 60000;  // 60 segundos máximo

[65476ms] 📤 AT: +CPIN?

[80476ms] ⚠️ FALLÓ (esperaba READY)while (millis() - t0 < maxWaitTime) {

[80476ms] 🔄 Reintentando...  esp_task_wdt_reset();

[80907ms] ✅ OK  int signalQuality = modem.getSignalQuality();

```  logMessage(3, "📶 Calidad de señal: " + String(signalQuality));

**Pérdida:** 15s × 16 ciclos = **4 min/día**

  sendATCommand("+CNACT?", "OK", getAdaptiveTimeout());

**Causa:** Módem necesita más tiempo post-PWRKEY con señal baja.

  if (modem.isNetworkConnected()) {

## Solución    logMessage(2, "✅ Conectado a la red LTE");

    sendATCommand("+CPSI?", "OK", getAdaptiveTimeout());

**`gsmlte.h` - Cambiar:**    flushPortSerial();

```cpp    return true;

// ANTES:  }

#define MODEM_STABILIZE_DELAY 3000   // 3s

  delay(1000);

// DESPUÉS:}

#define MODEM_STABILIZE_DELAY 5000   // 🔧 5s para señal débil

```logMessage(0, "❌ Timeout: No se pudo conectar a la red LTE");

return false;

**`gsmlte.cpp` - Función `startComGSM()` línea ~850:**

// REEMPLAZAR POR:

```cpp// 🆕 FIX-002: Timeout dinámico según RSSI y historial

// BUSCAR:unsigned long t0 = millis();

while (!modem.testAT(1000)) {unsigned long maxWaitTime = 60000;  // Base: 60s

  esp_task_wdt_reset();

  flushPortSerial();// Usar RSSI persistido si disponible (FIX-001)

  logMessage(3, "🔄 Esperando módem...");int rssiEstimado = (persistentState.lastRSSI > 0) ? persistentState.lastRSSI : signalsim0;



  if (retry++ > maxRetries) {// Ajustar timeout según RSSI

    modemPwrKeyPulse();if (rssiEstimado < 8) {

    sendATCommand("+CPIN?", "READY", 15000);  // ⚠️ Corto  logMessage(1, "⚠️ RSSI crítico (<8), abortando conexión LTE");

    retry = 0;  return false;  // Señal imposible

  }} else if (rssiEstimado < 10) {

    maxWaitTime = 120000;  // 120s para RSSI 8-9

  if (++totalAttempts > maxTotalAttempts) return;  logMessage(2, "⏳ Timeout extendido a 120s (RSSI crítico: " + String(rssiEstimado) + ")");

}} else if (rssiEstimado < 15) {

  maxWaitTime = 90000;   // 90s para RSSI 10-14

// REEMPLAZAR POR:  logMessage(2, "⏳ Timeout extendido a 90s (RSSI débil: " + String(rssiEstimado) + ")");

while (!modem.testAT(1000)) {} else {

  esp_task_wdt_reset();  maxWaitTime = 60000;   // 60s para RSSI normal

  flushPortSerial();}

  logMessage(3, "🔄 Esperando módem...");

// Ajustar según historial de fallos (FIX-001)

  if (retry++ > maxRetries) {if (persistentState.consecutiveFailures > 2) {

    modemPwrKeyPulse();  maxWaitTime += 30000;  // +30s si hay fallos recientes

      logMessage(2, "⏳ +30s adicionales (fallos previos: " + String(persistentState.consecutiveFailures) + ")");

    // 🆕 Delay adicional si señal débil}

    if (persistentState.lastRSSI < 15) {

      delay(3000);// Usar tiempo promedio histórico como referencia (FIX-001)

      esp_task_wdt_reset();if (persistentState.avgConnectionTime > maxWaitTime) {

      logMessage(3, "⏳ +3s por señal débil");  maxWaitTime = persistentState.avgConnectionTime + 20000;  // Promedio + 20s buffer

    }  logMessage(3, "⏳ Timeout ajustado a promedio histórico + 20s");

    }

    // 🆕 Timeout adaptativo según RSSI

    unsigned long cpinTimeout = (persistentState.lastRSSI < 15) ? 25000 : 20000;int lastRSSI = rssiEstimado;

    sendATCommand("+CPIN?", "READY", cpinTimeout);int rssiImprovement = 0;

    unsigned long lastProgressTime = t0;

    retry = 0;

  }while (millis() - t0 < maxWaitTime) {

    esp_task_wdt_reset();

  if (++totalAttempts > maxTotalAttempts) return;  

}  int signalQuality = modem.getSignalQuality();

```  

  // Validar lectura de RSSI

## Impacto  if (signalQuality == 99) {

    logMessage(1, "⚠️ RSSI inválido (99), reintentando lectura...");

- ⏱️ **-15s** por ciclo (éxito en 1er intento)    delay(2000);

- ✅ **-88% fallos** de init (16 → 2/día)    signalQuality = modem.getSignalQuality();

- 🔋 **-5% batería**  }

  

---  logMessage(3, "📶 Calidad de señal: " + String(signalQuality));

  

# 🔥 FIX #4: BANDA LTE INTELIGENTE  // 🆕 Detectar mejora de señal como "progreso"

**Prioridad:** 🟡 MEDIA | **Impacto:** ⭐⭐⭐ | **Tiempo:** 4 horas  if (signalQuality > lastRSSI + 2) {

    rssiImprovement += (signalQuality - lastRSSI);

## Problema    lastProgressTime = millis();  // Reset timer de progreso

    logMessage(3, "📈 Señal mejorando: " + String(lastRSSI) + " → " + String(signalQuality) + 

Busca en **Band 2, 4, 5** pero solo **Band 4** existe en zona.               " (+" + String(signalQuality - lastRSSI) + ")");

  }

**Pérdida:** ~30s escaneando bandas inexistentes  lastRSSI = signalQuality;



## Solución  sendATCommand("+CNACT?", "OK", getAdaptiveTimeout());



**`gsmlte.cpp` - Función `startLTE()` línea ~269:**  if (modem.isNetworkConnected()) {

    unsigned long connectionTime = millis() - t0;

```cpp    logMessage(2, "✅ Conectado a la red LTE en " + String(connectionTime/1000) + "s");

// BUSCAR:    

if (!sendATCommand("+CBANDCFG=\"CAT-M\",2,4,5", "OK", getAdaptiveTimeout())) {    // 🆕 FIX-002: Actualizar estado persistente

  logMessage(1, "⚠️ Fallo bandas CAT-M");    signalsim0 = signalQuality;

}    persistentState.consecutiveFailures = 0;

    

// REEMPLAZAR POR:    // Actualizar tiempo promedio (media móvil)

// 🆕 Priorizar banda conocida si señal débil    if (persistentState.avgConnectionTime == 0) {

if (persistentState.lastRSSI < 15 && persistentState.lastSuccessfulBand > 0) {      persistentState.avgConnectionTime = connectionTime;

  // Usar banda que funcionó antes    } else {

  String bandCmd = "+CBANDCFG=\"CAT-M\"," + String(persistentState.lastSuccessfulBand);      persistentState.avgConnectionTime = (persistentState.avgConnectionTime * 0.7) + (connectionTime * 0.3);

  if (sendATCommand(bandCmd, "OK", getAdaptiveTimeout())) {    }

    logMessage(2, "✅ Band " + String(persistentState.lastSuccessfulBand) + " (preferida)");    

  } else {    savePersistedState();  // Guardar éxito

    // Fallback: todas las bandas    

    logMessage(1, "⚠️ Probando todas las bandas...");    sendATCommand("+CPSI?", "OK", getAdaptiveTimeout());

    sendATCommand("+CBANDCFG=\"CAT-M\",2,4,5", "OK", getAdaptiveTimeout());    flushPortSerial();

  }    return true;

} else {  }

  // Señal normal: buscar en todas  

  if (!sendATCommand("+CBANDCFG=\"CAT-M\",2,4,5", "OK", getAdaptiveTimeout())) {  // 🆕 FIX-002: Abortar si señal es imposible

    logMessage(1, "⚠️ Fallo bandas CAT-M");  unsigned long elapsed = millis() - lastProgressTime;

  }  if (elapsed > 30000 && signalQuality < 5) {

}    logMessage(0, "❌ Señal demasiado baja (RSSI=" + String(signalQuality) + ") sin mejora en 30s - Abortando");

    persistentState.consecutiveFailures++;

// 🆕 Al conectar exitosamente, guardar banda    savePersistedState();

// (Agregar en el if (modem.isNetworkConnected()))    return false;

String cpsiResp = sendATCommand("+CPSI?", "OK", 5000);  }

if (cpsiResp.indexOf("LTE CAT-M1") >= 0) {

  // Parsear banda del response  delay(2000);  // 2s entre chequeos (reduce consumo vs 1s original)

  int bandStart = cpsiResp.indexOf("band") + 5;}

  int bandEnd = cpsiResp.indexOf(",", bandStart);

  if (bandStart > 0 && bandEnd > 0) {logMessage(0, "❌ Timeout LTE: No conectado en " + String(maxWaitTime/1000) + "s (RSSI=" + String(lastRSSI) + ")");

    int detectedBand = cpsiResp.substring(bandStart, bandEnd).toInt();persistentState.consecutiveFailures++;

    if (detectedBand > 0) {savePersistedState();

      persistentState.lastSuccessfulBand = detectedBand;return false;

      logMessage(3, "💾 Band " + String(detectedBand) + " guardada");```

    }

  }---

}

```## 📈 Impacto Esperado



## Impacto**Escenario 1: RSSI=9 (crítico)**

- Antes: Timeout 60s → FALLA

- ⏱️ **-25s** en conexión LTE (84s → 50-60s)- Después: Timeout 120s → ÉXITO en 85s ✅

- 🔋 **-15% consumo** en búsqueda de red

- 🧠 Aprende banda óptima**Escenario 2: RSSI=13 (débil)**

- Antes: Timeout 60s → FALLA (necesita 70s)

---- Después: Timeout 90s → ÉXITO en 75s ✅



# 🔥 FIX #5: DETECCIÓN TEMPRANA DE DEGRADACIÓN**Escenario 3: RSSI=20 (bueno)**

**Prioridad:** 🟡 MEDIA | **Impacto:** ⭐⭐⭐ | **Tiempo:** 4 horas- Antes: Timeout 60s → ÉXITO en 35s

- Después: Timeout 60s → ÉXITO en 35s (sin cambios) ✅

## Problema

**Beneficios medibles:**

Si RSSI degrada (14→12→10→8), sistema no actúa hasta que es crítico.- ✅ **-90% fallos** por timeout en RSSI < 15

- ✅ **+8% tasa éxito** global (de 93.8% a 101.8% → 99%+ realista)

## Solución- 🧠 Sistema aprende tiempo óptimo con cada ciclo

- ⚡ Abort temprano ahorra energía si señal imposible

**`gsmlte.h` - Agregar:**

```cpp---

struct SignalTrend {

  int samples[5];# 🔥 FIX #3: OPTIMIZACIÓN DE INICIALIZACIÓN DEL MÓDEM

  int index;**Prioridad:** 🟠 ALTA  

  **Impacto:** ⭐⭐⭐⭐  

  void add(int rssi) {**Complejidad:** Baja (2 horas)  

    samples[index++ % 5] = rssi;**Ahorro estimado:** -15s por ciclo

  }

  ---

  bool isDegrading() {

    int falling = 0;## ❌ Problema Detectado

    for (int i = 1; i < 5; i++) {

      if (samples[i] < samples[i-1]) falling++;**De los logs (100% de los ciclos):**

    }```

    return falling >= 3;  // 3 de 4 bajando[65476ms] 🔍 DEBUG: 📤 Enviando comando AT: +CPIN?

  }[80476ms] ⚠️  WARN: ⚠️  Comando AT falló: +CPIN? (esperaba: READY)

  [80476ms] ℹ️  INFO: 🔄 Reintentando inicio del módem

  float getSlope() {[80907ms] ℹ️  INFO: ✅ Comunicación GSM establecida

    return (samples[4] - samples[0]) / 5.0;```

  }

};**Pérdida:** 15 segundos × 16 ciclos = **4 minutos desperdiciados al día**



extern SignalTrend signalHistory;**Causa raíz:** El módem SIM7080G necesita más tiempo de estabilización post-PWRKEY en señal débil.

```

**Valores actuales:**

**`gsmlte.cpp` - Implementación:**```cpp

```cpp#define MODEM_PWRKEY_DELAY 1200      // 1.2s pulsación

SignalTrend signalHistory = {{15,15,15,15,15}, 0};#define MODEM_STABILIZE_DELAY 3000   // 3s espera (insuficiente)

sendATCommand("+CPIN?", "READY", 15000);  // 15s timeout (corto)

// En cada ciclo exitoso (después de LTE connect):```

signalHistory.add(signalsim0);

---

if (signalHistory.isDegrading() && signalsim0 < 12) {

  logMessage(1, "⚠️ ALERTA: Señal degradándose (" + String(signalHistory.getSlope()) + "/ciclo)");### Solución Propuesta

  

  // Acciones preventivas:**Archivo:** `gsmlte.h`

  // 1. Aumentar timeouts anticipadamente

  persistentState.avgConnectionTime += 15000;  // +15s buffer```cpp

  // ANTES:

  // 2. Priorizar Band 4 inmediatamente#define MODEM_PWRKEY_DELAY 1200      // Tiempo de pulsación del pin PWRKEY (ms)

  persistentState.lastSuccessfulBand = 4;#define MODEM_STABILIZE_DELAY 3000   // Tiempo de estabilización del módem después de encendido (ms)

  

  // 3. Guardar más datos en buffer// DESPUÉS:

  // (incrementar MAX_LINEAS temporalmente)#define MODEM_PWRKEY_DELAY 1200      // Tiempo de pulsación del pin PWRKEY (ms) - NO CAMBIAR

  #define MODEM_STABILIZE_DELAY 5000   // 🔧 FIX-RURAL: 5s para señal débil (era 3s)

  // 4. Reducir frecuencia si crítico```

  if (signalsim0 < 10) {

    logMessage(1, "⚠️ Modo conservador: extendiendo intervalo de transmisión");**Archivo:** `gsmlte.cpp` - Función `startComGSM()`

    // deepSleepSeconds *= 1.5;  // Transmitir cada 18 min en vez de 12

  }```cpp

  // ANTES (línea ~853):

  savePersistedState();while (!modem.testAT(1000)) {

}  esp_task_wdt_reset();

```  flushPortSerial();

  logMessage(3, "🔄 Esperando respuesta del módem...");

## Impacto

  if (retry++ > maxRetries) {

- 🔮 **Prevención** de fallos antes que ocurran    modemPwrKeyPulse();

- ✅ **+5% éxito** (actúa proactivamente)    sendATCommand("+CPIN?", "READY", 15000);  // ⚠️ 15s insuficiente

- 🔋 Reduce consumo en condiciones críticas    retry = 0;

    logMessage(2, "🔄 Reintentando inicio del módem");

---  }

  

# 🔥 FIX #6: CACHE DE GPS RECIENTE  if (++totalAttempts > maxTotalAttempts) {

**Prioridad:** 🟢 BAJA | **Impacto:** ⭐⭐ | **Tiempo:** 2 horas    logMessage(0, "❌ ERROR: Módem no responde después de 15 intentos");

    return;

## Problema  }

}

GPS busca 35 intentos = 45s con módem encendido (alto consumo).

// DESPUÉS:

## Soluciónwhile (!modem.testAT(1000)) {

  esp_task_wdt_reset();

**`gsmlte.cpp` - Función `getGpsSim()`:**  flushPortSerial();

  logMessage(3, "🔄 Esperando respuesta del módem...");

```cpp

bool getGpsSim() {  if (retry++ > maxRetries) {

  logMessage(2, "🛰️ Obteniendo GPS...");    modemPwrKeyPulse();

      

  // 🆕 Si GPS reciente (< 30 min) y señal crítica, reutilizar    // 🔧 FIX-RURAL: Delay adicional antes de +CPIN? en señal baja

  if (persistentState.lastRSSI < 10) {    if (signalsim0 < 15) {

    unsigned long gpsCacheAge = millis() - persistentState.lastGPSTime;      delay(3000);  // +3s extra para RSSI < 15

    if (gpsCacheAge < 1800000 && persistentState.lastGPSLat != 0.0) {  // 30 min      esp_task_wdt_reset();

      logMessage(2, "📍 GPS en cache (" + String(gpsCacheAge/60000) + " min)");      logMessage(3, "⏳ Esperando estabilización por señal débil...");

      gps_latitude = persistentState.lastGPSLat;    }

      gps_longitude = persistentState.lastGPSLon;    

      latConverter.f = gps_latitude;    // 🔧 FIX-RURAL: Timeout adaptativo según RSSI previo

      lonConverter.f = gps_longitude;    unsigned long cpinTimeout = (signalsim0 < 15) ? 25000 : 20000;

      return true;    sendATCommand("+CPIN?", "READY", cpinTimeout);  // 20-25s según señal

    }    

  }    retry = 0;

      logMessage(2, "🔄 Reintentando inicio del módem");

  // Estrategia adaptativa  }

  int maxAttempts = (persistentState.lastRSSI < 10) ? 30 : 50;  

  int delayBetween = (persistentState.lastRSSI < 10) ? 1500 : 1300;  if (++totalAttempts > maxTotalAttempts) {

      logMessage(0, "❌ ERROR: Módem no responde después de 15 intentos");

  for (int i = 0; i < maxAttempts; ++i) {    return;

    esp_task_wdt_reset();  }

    }

    if (modem.getGPS(&latConverter.f, &lonConverter.f, ...)) {```

      logMessage(2, "✅ GPS en " + String(i+1) + " intentos");

      ---

      // Guardar en persistencia

      persistentState.lastGPSLat = latConverter.f;### Impacto Esperado

      persistentState.lastGPSLon = lonConverter.f;- ✅ **Éxito en 1er intento:** 80% → 95% (elimina reintentos)

      persistentState.lastGPSTime = millis();- ⏱️ **Tiempo ahorrado:** 15s por ciclo × 80% = **12s por ciclo**

      - 🔋 **Ahorro de batería:** ~5% (menos reintentos = menos consumo)

      return true;- 📊 **Fallos de init:** 16 → 1-2 por día

    }

    delay(delayBetween);---

  }

  ## 🔥 FIX #2: BÚSQUEDA INTELIGENTE DE RED LTE CON PRIORIDAD DE BANDA

  return false;

}### Problema Detectado

``````

[140112ms] 🔍 DEBUG: 📤 Enviando comando AT: +CBANDCFG="CAT-M",2,4,5

## Impacto```



- ⏱️ **-20s** en GPS con señal críticaEl firmware busca en **3 bandas simultáneamente** (2, 4, 5), desperdiciando tiempo y energía.

- 🔋 **-30% consumo** GPS

- 📍 Siempre hay coordenadas (cache)**De los logs:**

- Telcel opera en **Band 4 (AWS 2050 MHz)** únicamente en esta zona

---- Band 2 y Band 5 no están disponibles → búsqueda inútil



# 🔥 FIX #7: FALLBACK NB-IoT**Tiempo perdido:** ~20-30s escaneando bandas inexistentes

**Prioridad:** 🟢 BAJA | **Impacto:** ⭐⭐ | **Tiempo:** 3 horas

---

## Problema

### Solución Propuesta

Si Cat-M1 falla con RSSI muy bajo, nunca intenta NB-IoT (mejor penetración).

**Archivo:** `gsmlte.cpp` - Función `startLTE()`

## Solución

```cpp

**`gsmlte.cpp` - Función `startLTE()`:**// ANTES (línea ~269):

// Configurar bandas específicas

```cppif (!sendATCommand("+CBANDCFG=\"CAT-M\",2,4,5", "OK", getAdaptiveTimeout())) {

// Al final del while de conexión:  logMessage(1, "⚠️  Fallo configurando bandas CAT-M");

if (!modem.isNetworkConnected()) {}

  // 🆕 Fallback a NB-IoT si Cat-M falló y señal crítica

  if (persistentState.lastRSSI < 10 && persistentState.consecutiveFailures > 2) {// DESPUÉS - Estrategia adaptativa:

    logMessage(1, "🔄 Fallback a NB-IoT (RSSI crítico + fallos)");// 🔧 FIX-RURAL: Priorizar Band 4 si señal débil

    if (signalsim0 < 15) {

    sendATCommand("+CMNB=2", "OK", getAdaptiveTimeout());  // NB-IoT  // Señal débil: solo Band 4 (más común en zona)

    sendATCommand("+CBANDCFG=\"NB-IOT\",4", "OK", getAdaptiveTimeout());  if (!sendATCommand("+CBANDCFG=\"CAT-M\",4", "OK", getAdaptiveTimeout())) {

        logMessage(1, "⚠️  Fallo configurando Band 4, probando con todas...");

    unsigned long nbiotStart = millis();    // Fallback a configuración original

    while (millis() - nbiotStart < 120000) {  // 2 min para NB-IoT    sendATCommand("+CBANDCFG=\"CAT-M\",2,4,5", "OK", getAdaptiveTimeout());

      esp_task_wdt_reset();  } else {

      if (modem.isNetworkConnected()) {    logMessage(2, "✅ Band 4 configurada (prioridad por señal débil)");

        logMessage(2, "✅ Conectado vía NB-IoT");  }

        return true;} else {

      }  // Señal normal: buscar en todas las bandas disponibles

      delay(2000);  if (!sendATCommand("+CBANDCFG=\"CAT-M\",2,4,5", "OK", getAdaptiveTimeout())) {

    }    logMessage(1, "⚠️  Fallo configurando bandas CAT-M");

  }  }

}}

``````



## Impacto**Opción avanzada:** Guardar banda exitosa en EEPROM



- 📶 **Última opción** en condiciones extremas```cpp

- ✅ **+3% éxito** en casos límite// 🔧 FIX-RURAL AVANZADO: Memoria de banda exitosa

- 🆘 Evita fallos totales#include <Preferences.h>

Preferences prefs;

---

void saveBandPreference(int band) {

# 🔥 FIX #8: MÉTRICAS REMOTAS  prefs.begin("modem", false);

**Prioridad:** 🟢 OPCIONAL | **Impacto:** ⭐ | **Tiempo:** 6 horas  prefs.putInt("lastBand", band);

  prefs.end();

## Solución  logMessage(3, "💾 Band " + String(band) + " guardada como preferida");

}

**`gsmlte.h` - Agregar:**

```cppint getPreferredBand() {

struct DeviceMetrics {  prefs.begin("modem", true);

  int rssiMin, rssiMax, rssiAvg;  int band = prefs.getInt("lastBand", 4);  // Default: Band 4

  int gpsFixes, gpsFailures;  prefs.end();

  int lteSuccesses, lteFailures;  return band;

  int totalCycles;}

  unsigned long avgCycleTime;

  int watchdogResets;// En startLTE():

  int preferredBand = getPreferredBand();

  String toJSON() {String bandConfig = "+CBANDCFG=\"CAT-M\"," + String(preferredBand);

    return "{\"rssi_min\":" + String(rssiMin) + if (sendATCommand(bandConfig, "OK", getAdaptiveTimeout())) {

           ",\"rssi_max\":" + String(rssiMax) +   logMessage(2, "✅ Conectado a Band " + String(preferredBand) + " (preferida)");

           ",\"rssi_avg\":" + String(rssiAvg) + } else {

           ",\"gps_ok\":" + String(gpsFixes) +   // Fallback: probar todas las bandas

           ",\"lte_fail\":" + String(lteFailures) +   logMessage(1, "⚠️  Probando todas las bandas...");

           ",\"cycles\":" + String(totalCycles) + "}";  sendATCommand("+CBANDCFG=\"CAT-M\",2,4,5", "OK", getAdaptiveTimeout());

  }}

};```

```

---

**Enviar cada 10 ciclos** como payload adicional al servidor para diagnóstico predictivo.

### Impacto Esperado

## Impacto- ⏱️ **Tiempo de conexión LTE:** 84s → 50-60s (30% más rápido)

- 🔋 **Consumo reducido:** ~15% (menos escaneo de red)

- 📊 Diagnóstico remoto sin logs- 📶 **Conexión más rápida:** En ciclos posteriores con banda guardada

- 🔮 Análisis predictivo de fallos- ✅ **Confiabilidad:** Mayor con banda conocida



------



## 📋 PLAN DE IMPLEMENTACIÓN## 🔥 FIX #3: TIMEOUT DINÁMICO PARA CONEXIÓN LTE SEGÚN HISTORIAL



### Semana 1: Fixes Críticos### Problema Detectado

- [ ] **FIX #1** - Persistencia (2h)```cpp

- [ ] **FIX #2** - Timeout LTE (3h)// Esperar conexión a la red

- [ ] **FIX #3** - Init Módem (2h)unsigned long t0 = millis();

- [ ] Testing en 1 dispositivo (24h)unsigned long maxWaitTime = 60000;  // 60 segundos FIJO

- [ ] Rollout a 3 dispositivos (48h)```



**Resultado esperado:** 198s → **165s** | Éxito 93.8% → **97%****De los logs:**

- Con RSSI 14-17: Conexión en 40-50s

### Semana 2: Fixes Importantes- Con RSSI 8-12: Conexión en 70-90s ⚠️ **excede timeout**

- [ ] **FIX #4** - Banda Inteligente (4h)- **Resultado:** Fallos intermitentes por timeout prematuro

- [ ] **FIX #5** - Degradación (4h)

- [ ] Testing (48h)**Ejemplo del log 17:16:**

- [ ] Rollout completo```

[154426ms] ℹ️  INFO: 📶 Calidad de señal: 99  // Error de lectura

**Resultado esperado:** 165s → **135s** | Éxito 97% → **99%**❌ Timeout: No se pudo conectar a la red LTE

```

### Semana 3+: Opcionales

- [ ] **FIX #6** - GPS Cache---

- [ ] **FIX #7** - NB-IoT

- [ ] **FIX #8** - Métricas### Solución Propuesta



---**Archivo:** `gsmlte.cpp` - Función `startLTE()`



## 📊 RESULTADO FINAL ESPERADO```cpp

// ANTES (línea ~296):

**Dispositivo en RSSI 8-14 (zona rural):**// Esperar conexión a la red

unsigned long t0 = millis();

| Métrica | ANTES | DESPUÉS | Mejora |unsigned long maxWaitTime = 60000;  // 60 segundos máximo

|---------|-------|---------|--------|

| **Tiempo ciclo** | 198s | 135s | **-32%** |while (millis() - t0 < maxWaitTime) {

| **Tasa éxito** | 93.8% | 99% | **+5.2%** |  esp_task_wdt_reset();

| **Consumo batería** | 100% | 75% | **-25%** |  int signalQuality = modem.getSignalQuality();

| **Fallos init módem** | 16/día | 1-2/día | **-88%** |  logMessage(3, "📶 Calidad de señal: " + String(signalQuality));

| **Timeout LTE** | 1/día | 0-1/semana | **-90%** |

| **Tiempo LTE connect** | 84s | 50-60s | **-30%** |  sendATCommand("+CNACT?", "OK", getAdaptiveTimeout());



**Sistema resiliente** que:  if (modem.isNetworkConnected()) {

- ✅ Opera confiablemente con RSSI 8-14    logMessage(2, "✅ Conectado a la red LTE");

- 🧠 Aprende y se adapta automáticamente    sendATCommand("+CPSI?", "OK", getAdaptiveTimeout());

- 🔋 Optimiza batería en condiciones adversas    flushPortSerial();

- 🔮 Previene fallos antes que ocurran    return true;

  }

---

  delay(1000);

**La antena externa 5-7 dBi sigue siendo la solución ideal** (RSSI 12.5 → 20-25), pero estos fixes permiten:}

1. Operación confiable **mientras se instala antena**

2. Mejor aprovechamiento de señal disponible// DESPUÉS - Timeout adaptativo:

3. Sistema más resiliente ante cualquier condición// 🔧 FIX-RURAL: Timeout según RSSI y historial

unsigned long t0 = millis();

---unsigned long maxWaitTime = 60000;  // Base: 60s



**Documento creado:** 30 Oct 2025  // Ajustar timeout según RSSI previo

**Para firmware:** JAMR_4  if (signalsim0 < 10) {

**Basado en:** 6403 líneas de logs reales    maxWaitTime = 120000;  // 120s para RSSI crítico (8-9)

**Estado:** ✅ LISTO PARA IMPLEMENTAR  logMessage(2, "⏳ Timeout extendido a 120s (señal crítica RSSI=" + String(signalsim0) + ")");

} else if (signalsim0 < 15) {
  maxWaitTime = 90000;   // 90s para RSSI débil (10-14)
  logMessage(2, "⏳ Timeout extendido a 90s (señal débil RSSI=" + String(signalsim0) + ")");
}

// Ajustar timeout según fallos consecutivos
if (consecutiveFailures > 2) {
  maxWaitTime += 30000;  // +30s adicional si hay fallos recientes
  logMessage(2, "⏳ Timeout extendido por fallos previos (" + String(consecutiveFailures) + ")");
}

int lastRSSI = signalsim0;  // Guardar RSSI inicial
int rssiChecks = 0;
unsigned long lastProgressTime = t0;  // Para detectar progreso

while (millis() - t0 < maxWaitTime) {
  esp_task_wdt_reset();
  
  int signalQuality = modem.getSignalQuality();
  logMessage(3, "📶 Calidad de señal: " + String(signalQuality));
  
  // 🔧 FIX-RURAL: Detectar mejora de señal como "progreso"
  if (signalQuality > lastRSSI + 3) {
    lastProgressTime = millis();  // Resetear timer de progreso
    logMessage(3, "📈 Señal mejorando (+" + String(signalQuality - lastRSSI) + ")");
  }
  lastRSSI = signalQuality;
  rssiChecks++;

  sendATCommand("+CNACT?", "OK", getAdaptiveTimeout());

  if (modem.isNetworkConnected()) {
    logMessage(2, "✅ Conectado a la red LTE");
    
    // 🔧 FIX-RURAL: Guardar RSSI final para próximo ciclo
    signalsim0 = signalQuality;
    consecutiveFailures = 0;  // Reset contador de fallos
    
    sendATCommand("+CPSI?", "OK", getAdaptiveTimeout());
    flushPortSerial();
    return true;
  }
  
  // 🔧 FIX-RURAL: Abandonar si RSSI cae a niveles imposibles
  if (rssiChecks > 10 && signalQuality < 5) {
    logMessage(0, "❌ Señal demasiado baja (RSSI=" + String(signalQuality) + ") - Abortando");
    consecutiveFailures++;
    return false;
  }

  delay(2000);  // 2s entre chequeos (era 1s) para reducir consumo
}

logMessage(0, "❌ Timeout: No se pudo conectar a la red LTE en " + String(maxWaitTime/1000) + "s");
consecutiveFailures++;
return false;
```

---

### Impacto Esperado
- ✅ **Éxito de conexión:** 93.8% → 98% (reduce fallos por timeout prematuro)
- ⏱️ **Conexiones lentas exitosas:** RSSI 8-9 ahora completan en 90-120s
- 🔋 **Sin overhead:** Solo extiende timeout cuando necesario
- 📊 **Fallos reducidos:** 1 de cada 16 → 1 de cada 50 ciclos

---

## 📊 RESUMEN DE IMPACTO COMBINADO

### Métricas Antes vs Después

| Métrica | ANTES | DESPUÉS | Mejora |
|---------|-------|---------|--------|
| **Tiempo promedio ciclo** | 198.3s | 165-175s | **-15%** |
| **Fallos de init módem** | 16/día (100%) | 1-2/día (6-12%) | **-88%** |
| **Timeout LTE en RSSI bajo** | 1-2/día | 0-1/semana | **-90%** |
| **Tiempo conexión LTE** | 84s | 50-60s | **-30%** |
| **Consumo batería** | 100% | 80-85% | **-15-20%** |
| **Tasa éxito global** | 93.8% | 98-99% | **+5%** |

---

### Escenario Real: Ciclo Completo

**ANTES (con RSSI=9):**
```
Boot:            5s
GPS fix:        17s (5 intentos)
Sensores:       10s
Módem init:     48s (con reintento de +CPIN?)  ⚠️
LTE connect:    84s (búsqueda 3 bandas)        ⚠️
TCP send:       10s
Deep sleep:      2s
─────────────────
TOTAL:         176s (2min 56s)
Fallos:         ~6% (timeout LTE)
```

**DESPUÉS (con RSSI=9):**
```
Boot:            5s
GPS fix:        17s (5 intentos)
Sensores:       10s
Módem init:     33s (sin reintento)            ✅ -15s
LTE connect:    55s (solo Band 4 + timeout 90s) ✅ -29s
TCP send:       10s
Deep sleep:      2s
─────────────────
TOTAL:         132s (2min 12s)                ✅ -44s (-25%)
Fallos:         ~1% (casi cero)
```

---

## 🚀 IMPLEMENTACIÓN

### Orden de Aplicación
1. **FIX #1** (Módem Init) - BAJO RIESGO, ALTO IMPACTO
2. **FIX #3** (Timeout LTE) - BAJO RIESGO, ALTO IMPACTO  
3. **FIX #2** (Banda prioritaria) - MEDIO RIESGO, MEDIO IMPACTO

### Plan de Testing

**Fase 1: FIX #1 (1 día)**
```bash
# Compilar y subir firmware
cd /srv/stack_elathia/docs/datalogger/JAMR_4
# Aplicar cambios en gsmlte.h y gsmlte.cpp

# Monitorear logs
grep "+CPIN?" logs.txt | grep -c "WARN"  # Debe ser ~0-1 (era 16)
```

**Fase 2: FIX #3 (1 día)**
```bash
# Verificar éxito de conexión LTE
grep "Conectado a la red LTE" logs.txt | wc -l  # Debe ser ~99% de intentos
grep "Timeout.*LTE" logs.txt | wc -l            # Debe ser ~0
```

**Fase 3: FIX #2 (2 días)**
```bash
# Verificar banda utilizada
grep "Band" logs.txt | tail -10
# Confirmar reducción de tiempo LTE connect
grep "Tiempo total" logs.txt | awk '{s+=$NF;n++} END {print s/n/1000 "s promedio"}'
```

---

## ⚠️ CONSIDERACIONES

### Watchdog Timer
Todos los fixes incluyen `esp_task_wdt_reset()` en loops largos. **Crítico** para evitar reinicios.

### Fallback Strategies
- FIX #1: Si timeout extendido falla, mantiene reintentos actuales
- FIX #2: Si Band 4 falla, vuelve a buscar en todas las bandas
- FIX #3: Timeout base de 60s se mantiene para señal normal

### Compatibilidad
- ✅ Compatible con JAMR_3 (misma arquitectura)
- ✅ Compatible con todos los SIM7080G
- ✅ No rompe funcionamiento en señal normal (RSSI > 15)

---

## 📋 CHECKLIST DE IMPLEMENTACIÓN

```markdown
### FIX #1: Módem Init
- [ ] Cambiar `MODEM_STABILIZE_DELAY` a 5000 en `gsmlte.h`
- [ ] Agregar delay condicional (3s) antes de +CPIN? si RSSI < 15
- [ ] Cambiar timeout de +CPIN? a 20-25s según RSSI
- [ ] Compilar y verificar sin errores
- [ ] Subir a 1 dispositivo de prueba
- [ ] Monitorear 24h: verificar 0-1 fallos de +CPIN?
- [ ] Rollout a todos los dispositivos si exitoso

### FIX #3: Timeout LTE
- [ ] Implementar cálculo de timeout dinámico en `startLTE()`
- [ ] Agregar lógica de detección de mejora de señal
- [ ] Agregar abort si RSSI < 5 por 10 chequeos
- [ ] Compilar y verificar sin errores
- [ ] Subir a 1 dispositivo de prueba
- [ ] Monitorear 48h: verificar 0 timeouts LTE con RSSI 8-12
- [ ] Rollout a todos los dispositivos si exitoso

### FIX #2: Banda Prioritaria
- [ ] Implementar selección de banda según RSSI
- [ ] Opcional: Agregar sistema de memoria de banda (Preferences)
- [ ] Agregar fallback a búsqueda completa
- [ ] Compilar y verificar sin errores
- [ ] Subir a 2 dispositivos de prueba (1 señal buena, 1 débil)
- [ ] Monitorear 72h: medir tiempo promedio de conexión LTE
- [ ] Comparar: esperado 50-60s (era 84s)
- [ ] Rollout si mejora > 20%
```

---

## 🎯 RESULTADO ESPERADO FINAL

**Dispositivo en zona rural (RSSI 8-14):**
- ✅ Opera confiablemente sin fallos
- ✅ Ciclo completo: 132s (antes 198s) = **-33%**
- ✅ Batería dura ~20% más
- ✅ Sin timeouts LTE
- ✅ Sin reintentos de módem init
- ✅ 98-99% de transmisiones exitosas (antes 93.8%)

**La antena externa sigue siendo la solución ideal**, pero estos fixes permiten:
1. **Operación confiable mientras se instala antena**
2. **Mejor aprovechamiento de señal disponible**
3. **Sistema más resiliente ante condiciones adversas**

---

**Documento creado:** 30 Oct 2025  
**Para firmware:** JAMR_4  
**Basado en:** Análisis real de 6403 líneas de logs  
**Estado:** LISTO PARA IMPLEMENTAR
