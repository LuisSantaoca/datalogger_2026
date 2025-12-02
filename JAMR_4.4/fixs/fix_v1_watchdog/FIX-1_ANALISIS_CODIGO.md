# FIX-1: Análisis Exhaustivo de Código - Watchdog Timer

**Fecha:** 2025-10-29  
**Requisito:** REQ-002 (Watchdog Protection)  
**Estado Actual:** Código base tiene implementación parcial

---

## 📊 Estado Actual del Código

### ✅ Lo que YA EXISTE (No requiere cambios)

#### 1. **Configuración del Watchdog en JAMR_4.ino**

**Archivo:** `JAMR_4.ino`  
**Líneas:** 87-106  
**Estado:** ✅ **FUNCIONAL - NO MODIFICAR**

```cpp
// Watchdog ya configurado correctamente:
esp_task_wdt_deinit();  // Limpiar watchdog previo

esp_task_wdt_config_t wdt_config = {
  .timeout_ms = WATCHDOG_TIMEOUT_SEC * 1000,  // 120000 ms = 120s
  .idle_core_mask = 0,
  .trigger_panic = true
};

esp_task_wdt_init(&wdt_config);
esp_task_wdt_add(NULL);
```

**Evaluación:**
- ✅ Timeout: 120s (correcto según REQ-002)
- ✅ Trigger panic: true (reset automático)
- ✅ Compatible con ESP-IDF v5.3+
- ✅ Desinicialización previa (evita warnings)

---

#### 2. **Constante de Timeout en sleepdev.h**

**Archivo:** `sleepdev.h`  
**Línea:** 34  
**Estado:** ✅ **CORRECTO - NO MODIFICAR**

```cpp
#define WATCHDOG_TIMEOUT_SEC 120
```

**Evaluación:**
- ✅ Valor: 120 segundos
- ✅ Justificación documentada
- ✅ Permite operaciones LTE (hasta 60s) con margen

---

### ⚠️ Lo que NECESITA AJUSTES

#### 3. **Feeds Actuales en gsmlte.cpp**

**Total de feeds encontrados:** 16  
**Estado:** ⚠️ **PARCIAL - REQUIERE VALIDACIÓN Y AJUSTES**

##### **Feeds Existentes por Ubicación:**

| # | Ubicación | Línea | Contexto | Estado |
|---|-----------|-------|----------|--------|
| 1 | `setupModem()` | 178 | Después de `startGsm()` | ✅ OK |
| 2 | `setupModem()` | 186 | Después de `iniciarLittleFS()` | ✅ OK |
| 3 | `setupModem()` | 196 | Después de `startLTE()` | ✅ OK |
| 4 | `setupModem()` | 201 | Después de `enviarDatos()` | ✅ OK |
| 5 | `startLTE()` | 299 | Dentro de while esperando red | ✅ OK |
| 6 | `readResponse()` | 411 | Durante lectura de respuesta | ✅ OK |
| 7 | `sendATCommand()` | 450 | Durante comando AT largo | ✅ OK |
| 8 | `setupGpsSim()` | 481 | Al inicio de GPS | ✅ OK |
| 9 | `setupGpsSim()` | 487 | Después de `startGps()` | ✅ OK |
| 10 | `startGps()` | 555 | Dentro de while verificando AT | ⚠️ **REVISAR** |
| 11 | `getGpsSim()` | 614 | Durante intentos de GPS fix | ⚠️ **REVISAR** |
| 12 | `startGsm()` | 842 | Dentro de while verificando AT | ⚠️ **REVISAR** |
| 13 | `startGsm()` | 867 | Después de `+CFUN=1` | ✅ OK |
| 14 | `startGsm()` | 874 | Después de `+CFUN=1,1` | ✅ OK |
| 15 | `waitForToken()` | 1071 | Durante espera de token | ✅ OK |
| 16 | `waitForResponseFragment()` | 1114 | Durante espera de respuesta | ✅ OK |

---

### 🔴 Lo que FALTA o ESTÁ MAL

#### **Problema 1: Delays Largos Sin Feeds**

**Ubicaciones críticas encontradas:**

| Archivo | Línea | Código | Problema | Prioridad |
|---------|-------|--------|----------|-----------|
| `gsmlte.cpp` | 531 | `delay(1000);` | En `setupGpsSim()` - 1s bloqueante | 🟡 MEDIA |
| `gsmlte.cpp` | 577 | `delay(3000);` | En `startGps()` - 3s bloqueante | 🔴 ALTA |
| `gsmlte.cpp` | 881 | `delay(2000);` | En `startGsm()` - 2s bloqueante | 🟡 MEDIA |
| `JAMR_4.ino` | 118 | `delay(1000);` | En `setup()` inicial | 🟢 BAJA |
| `JAMR_4.ino` | 339 | `delay(2000);` | Antes de sleep | 🟢 BAJA |
| `JAMR_4.ino` | 132 | `delay(2000);` | Después de GPIO | 🟢 BAJA |

**Solución requerida:** Fragmentar delays >1s con feeds intermedios

---

#### **Problema 2: Loops Sin Límite de Intentos**

**Encontrados:**

##### **Loop 1: startLTE() - Línea 298**
```cpp
while (millis() - t0 < maxWaitTime) {  // maxWaitTime = 60000ms
  esp_task_wdt_reset(); // ✅ YA TIENE FEED
  // ...
  if (modem.isNetworkConnected()) {
    return true;
  }
  delay(1000);
}
```
**Estado:** ✅ **OK - Ya protegido con timeout (60s) y feeds**

##### **Loop 2: startGps() - Línea 554**
```cpp
while (!modem.testAT(1000)) {
  esp_task_wdt_reset(); // ✅ YA TIENE FEED
  // ...
  if (retry++ > maxRetries) {  // maxRetries = 3
    return false;
  }
  // ...
}
```
**Estado:** ⚠️ **REVISAR - Tiene feed pero sin límite total de tiempo**

**Riesgo:** Si `modem.testAT()` nunca retorna, el loop podría ejecutarse indefinidamente

**Solución:** Agregar timeout absoluto basado en tiempo (no solo en intentos)

##### **Loop 3: startGsm() - Línea 841**
```cpp
while (!modem.testAT(1000)) {
  esp_task_wdt_reset(); // ✅ YA TIENE FEED
  // ...
  if (retry++ > maxRetries) {  // maxRetries = 3
    return false;
  }
  // ...
}
```
**Estado:** ⚠️ **MISMO PROBLEMA QUE LOOP 2**

##### **Loop 4: getGpsSim() - Línea 610+**
```cpp
delay(1000);  // Estabilización inicial
esp_task_wdt_reset();

for (int intento = 0; intento < intentosMaximos; intento++) {
  esp_task_wdt_reset(); // ✅ YA TIENE FEED
  // ...
  delay(1000);  // Entre intentos
}
```
**Estado:** ✅ **OK - For loop con límite fijo (intentosMaximos)**

---

#### **Problema 3: Funciones de Archivo Sin Feeds**

##### **Función: guardarDato() - Línea ~890+**
```cpp
void guardarDato(String data) {
  // ... lógica de lectura de archivo ...
  while (f.available()) {  // ❌ SIN FEED
    String linea = f.readStringUntil('\n');
    // ...
  }
  // ... lógica de escritura ...
}
```
**Riesgo:** Si archivo corrupto o muy grande, loop sin límite  
**Prioridad:** 🟡 MEDIA (poco probable pero posible)

##### **Función: enviarDatos() - Línea ~970+**
```cpp
void enviarDatos() {
  // ...
  while (fin.available()) {  // ❌ SIN FEED
    String linea = fin.readStringUntil('\n');
    // ...
  }
  // ...
}
```
**Riesgo:** Similar a `guardarDato()`  
**Prioridad:** 🟡 MEDIA

##### **Función: limpiarEnviados() - Línea ~1025+**
```cpp
void limpiarEnviados() {
  // ...
  while (f.available()) {  // ❌ SIN FEED
    String linea = f.readStringUntil('\n');
    // ...
  }
  // ...
}
```
**Riesgo:** Similar a anteriores  
**Prioridad:** 🟡 MEDIA

---

#### **Problema 4: Operaciones Serial Sin Timeout Absoluto**

##### **readResponse() - Línea 396+**
```cpp
String readResponse(unsigned long timeout) {
  unsigned long start = millis();
  // ...
  while (millis() - start < finalTimeout) {  // ✅ Tiene timeout
    while (SerialAT.available()) {  // ✅ Inner loop OK
      char c = SerialAT.read();
      response += c;
    }
    esp_task_wdt_reset(); // ✅ YA TIENE FEED
    delay(1);
  }
  return response;
}
```
**Estado:** ✅ **OK - Bien protegido**

##### **sendATCommand() - Línea 430+**
```cpp
bool sendATCommand(...) {
  // ... similar a readResponse ...
  while (millis() - start < finalTimeout) {  // ✅ Tiene timeout
    while (SerialAT.available()) {  // ✅ Inner loop OK
      // ...
    }
    esp_task_wdt_reset(); // ✅ YA TIENE FEED
    delay(1);
  }
  // ...
}
```
**Estado:** ✅ **OK - Bien protegido**

---

### 📋 Archivos a Modificar para FIX-1

#### **Archivo 1: gsmlte.cpp** 🔴 **PRIORIDAD ALTA**

**Modificaciones requeridas:**

1. **Línea 577 - startGps()** 🔴 **CRÍTICO**
   ```cpp
   // ANTES:
   delay(3000);
   
   // DESPUÉS:
   for (int i = 0; i < 6; i++) {
     delay(500);
     esp_task_wdt_reset();
   }
   ```

2. **Línea 881 - startGsm()** 🟡 **IMPORTANTE**
   ```cpp
   // ANTES:
   delay(2000); // Esperar estabilización de la RF
   
   // DESPUÉS:
   for (int i = 0; i < 4; i++) {
     delay(500);
     esp_task_wdt_reset();
   }
   ```

3. **Línea 554 - startGps() while loop** 🟡 **IMPORTANTE**
   ```cpp
   // AGREGAR timeout absoluto:
   unsigned long startTime = millis();
   const unsigned long maxWaitTime = 15000; // 15s máximo
   
   while (!modem.testAT(1000)) {
     esp_task_wdt_reset();
     // ...
     
     // 🆕 AGREGAR:
     if (millis() - startTime > maxWaitTime) {
       logMessage(0, "❌ Timeout esperando respuesta del módem");
       return false;
     }
     // ...
   }
   ```

4. **Línea 841 - startGsm() while loop** 🟡 **IMPORTANTE**
   ```cpp
   // Mismo patrón que startGps()
   ```

5. **Líneas ~900, ~980, ~1030 - Funciones de archivo** 🟢 **OPCIONAL**
   ```cpp
   // Agregar contadores:
   int lineCount = 0;
   const int maxLines = 1000; // Límite de seguridad
   
   while (f.available() && lineCount++ < maxLines) {
     esp_task_wdt_reset();  // 🆕 Cada N líneas
     // ...
     if (lineCount % 10 == 0) {  // Feed cada 10 líneas
       esp_task_wdt_reset();
     }
   }
   ```

---

#### **Archivo 2: JAMR_4.ino** 🟢 **PRIORIDAD BAJA**

**Modificaciones opcionales:**

1. **Línea 118** - Delay inicial
   ```cpp
   // ANTES:
   delay(1000);
   
   // DESPUÉS (opcional):
   for (int i = 0; i < 2; i++) {
     delay(500);
     esp_task_wdt_reset();
   }
   ```

2. **Línea 132** - Delay después de GPIO
   ```cpp
   // Similar al anterior (opcional)
   ```

3. **Línea 339** - Delay antes de sleep
   ```cpp
   // Similar (opcional, ya estamos por entrar a sleep)
   ```

**Nota:** Estos delays están al inicio/fin del ciclo, bajo riesgo. Prioridad baja.

---

#### **Archivo 3: sleepdev.cpp** (No visible en análisis)

**Acción:** Verificar si hay operaciones largas antes de sleep  
**Prioridad:** 🟢 BAJA (no se detectaron problemas en referencias)

---

#### **Archivo 4: sensores.cpp** (No visible en análisis)

**Acción:** Verificar lectura de sensores RS485 (puede ser lenta)  
**Prioridad:** 🟡 MEDIA (sensores I2C suelen ser rápidos)

---

## 📊 Resumen de Cambios para FIX-1

### Cambios Obligatorios (MUST)

| Archivo | Línea(s) | Cambio | Justificación |
|---------|----------|--------|---------------|
| `gsmlte.cpp` | 577 | Fragmentar `delay(3000)` | 🔴 Delay crítico en startGps |
| `gsmlte.cpp` | 554 | Agregar timeout absoluto a while | 🟡 Prevenir loop infinito |
| `gsmlte.cpp` | 841 | Agregar timeout absoluto a while | 🟡 Prevenir loop infinito |

### Cambios Recomendados (SHOULD)

| Archivo | Línea(s) | Cambio | Justificación |
|---------|----------|--------|---------------|
| `gsmlte.cpp` | 881 | Fragmentar `delay(2000)` | 🟡 Delay en startGsm |
| `gsmlte.cpp` | ~900, ~980, ~1030 | Feeds en loops de archivo | 🟡 Protección adicional |

### Cambios Opcionales (MAY)

| Archivo | Línea(s) | Cambio | Justificación |
|---------|----------|--------|---------------|
| `JAMR_4.ino` | 118, 132, 339 | Fragmentar delays | 🟢 Consistencia |
| `sensores.cpp` | TBD | Revisar lectura RS485 | 🟢 Validación |

---

## ✅ Validación del Estado Actual

### Lo que ya está BIEN implementado:

1. ✅ **Configuración del watchdog** (JAMR_4.ino línea 87-106)
   - Timeout correcto: 120s
   - Reset automático habilitado
   - Compatible con ESP-IDF v5.3+

2. ✅ **16 feeds estratégicos** ya distribuidos:
   - Después de operaciones mayores (GSM, LTE, GPS)
   - Dentro de loops largos (readResponse, sendATCommand)
   - En waitFor* functions

3. ✅ **Timeouts en funciones serial**:
   - readResponse() con timeout
   - sendATCommand() con timeout
   - Ambos con feeds internos

4. ✅ **Loop principal protegido**:
   - startLTE() con maxWaitTime (60s) y feeds

### Lo que FALTA para cumplir REQ-002 completo:

1. ❌ **3 delays largos sin fragmentar** (prioridad alta)
2. ❌ **2 while loops sin timeout absoluto** (prioridad media)
3. ❌ **3 funciones de archivo sin feeds** (prioridad media-baja)

---

## 📝 Plan de Implementación de FIX-1

### Fase 1: Cambios Críticos (1-2 horas)

```bash
1. Modificar gsmlte.cpp línea 577 (delay 3s en startGps)
   - Fragmentar en 6 × 500ms con feeds
   
2. Agregar timeout absoluto en línea 554 (startGps while)
   - Variable startTime = millis()
   - Check: millis() - startTime > 15000
   
3. Agregar timeout absoluto en línea 841 (startGsm while)
   - Mismo patrón que anterior
```

### Fase 2: Cambios Recomendados (1 hora)

```bash
4. Modificar gsmlte.cpp línea 881 (delay 2s en startGsm)
   - Fragmentar en 4 × 500ms con feeds

5. Agregar feeds en funciones de archivo
   - guardarDato()
   - enviarDatos()
   - limpiarEnviados()
   - Feed cada 10 líneas leídas
```

### Fase 3: Validación (2-4 horas)

```bash
6. Compilar y verificar
   - 0 warnings
   - Tamaño de firmware

7. Flash en device de desarrollo
   - Capturar logs de 5 ciclos completos
   - Verificar que todos los feeds se ejecutan

8. Testing de stress
   - Desconectar antena (simular módem no responde)
   - Verificar que watchdog resetea después de 120s
   - Confirmar que sistema se recupera

9. Testing 24h
   - Sin intervención
   - Confirmar 0 resets de watchdog
   - Validar que tiempo máximo sin feed < 60s
```

---

## 🎯 Criterios de Éxito para FIX-1

### Criterios Funcionales

- ✅ Código compila sin warnings
- ✅ Todos los delays >1s fragmentados con feeds
- ✅ Todos los while loops tienen timeout absoluto o feeds
- ✅ Funciones de archivo con protección

### Criterios de Testing

- ✅ 5 ciclos normales sin resets
- ✅ Módem desconectado → watchdog reset en ~120s
- ✅ Sistema se recupera post-reset
- ✅ 24h continuas sin resets espurios

### Métricas

| Métrica | Target | Método de Validación |
|---------|--------|---------------------|
| Resets de watchdog (24h) | 0 | Logs + health data |
| Tiempo max sin feed | < 60s | Instrumentación temporal |
| Recovery post-timeout | 100% | Test forzado |

---

## 📚 Referencias

### Código Relacionado

- `JAMR_4.ino` líneas 87-106: Configuración watchdog
- `sleepdev.h` línea 34: Constante WATCHDOG_TIMEOUT_SEC
- `gsmlte.cpp`: 16 feeds existentes
- `type_def.h`: Estructura de health data (para validación post-fix)

### Documentación

- REQ-002_WATCHDOG_PROTECTION.md: Requisito completo
- ESP-IDF Watchdog Timer API
- Lecciones de JAMR_3 FIX-001 (exitoso)

---

**Documento creado:** 2025-10-29  
**Próximo paso:** Implementar cambios de Fase 1  
**Responsable:** Por asignar
