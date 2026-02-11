# Requirements - Módulo SIM7080G (código Rolando)

> Documento generado a partir del análisis de [codigo_ejemplo.md](codigo_ejemplo.md)  
> Fecha: 2026-02-10

---

## 1. Arquitectura General

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| REQ-ARCH-01 | El módulo SIM7080G se comunica vía UART2 del ESP32-S3 a 115200 baud, configuración 8N1. | Alta |
| REQ-ARCH-02 | Los pines de comunicación son: RX=GPIO11, TX=GPIO10, RESET=GPIO9. | Alta |
| REQ-ARCH-03 | La clase `SIM7080G` encapsula toda la lógica de comunicación con el módem. | Alta |

---

## 2. Inicialización y Reset del Módem

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| REQ-INIT-01 | Al construir el objeto `SIM7080G`, se debe inicializar UART2, configurar el pin RESET como salida, y ejecutar un ciclo de reset hardware (HIGH 1s → LOW 3s). | Alta |
| REQ-INIT-02 | El método `inicio()` debe ejecutar un reset hardware adicional antes de configurar la red. | Alta |
| REQ-INIT-03 | Tras el reset, se debe activar la funcionalidad completa del módem con `AT+CFUN=1` y esperar 3 segundos. | Alta |
| REQ-INIT-04 | Se debe verificar el estado de attach a la red (`AT+CGATT?`) y la calidad de señal (`AT+CSQ`). | Media |

---

## 3. Configuración de Red

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| REQ-NET-01 | El modo de red debe configurarse como **solo LTE** (`AT+CNMP=38`). | Alta |
| REQ-NET-02 | El modo NB-IoT/Cat-M debe configurarse como **solo Cat-M1** (`AT+CMNB=1`). | Alta |
| REQ-NET-03 | La banda NB-IoT debe configurarse en **banda 12** (`AT+CBANDCFG="NB-IoT",12`). | Alta |
| REQ-NET-04 | Se debe configurar el scrambling NB-IoT (`AT+CNBS=2`). | Media |
| REQ-NET-05 | El APN debe configurarse como `"internet.itelcel.com"` (Telcel) en el contexto PDP 1 con tipo IP. | Alta |
| REQ-NET-06 | El perfil de conexión 0 debe asociarse al APN `"internet.itelcel.com"`. | Alta |

---

## 4. Activación de Contexto PDP (Obtención de IP)

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| REQ-PDP-01 | Se debe activar el contexto PDP en slot 0 (`AT+CNACT=0,1`). | Alta |
| REQ-PDP-02 | Se debe verificar que la IP asignada **no sea `0.0.0.0`**; de ser inválida, se debe reintentar. | Alta |
| REQ-PDP-03 | El ciclo de reintento debe desactivar el contexto (`AT+CNACT=0,0`), esperar 2s y reactivar. | Alta |
| REQ-PDP-04 | El ciclo de reintento **no tiene límite de intentos** (bucle infinito hasta éxito). | Alta |

> **⚠️ Observación crítica (REQ-PDP-04):** El bucle infinito sin límite de reintentos ni timeout global es un **riesgo de bloqueo total del dispositivo**. Se recomienda agregar un máximo de reintentos (ej. 10) y un timeout global (ej. 120s).

---

## 5. Obtención de Fecha/Hora

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| REQ-TIME-01 | El método principal `getFechaHora()` debe intentar primero obtener la fecha vía **HTTP GET** al servidor. | Alta |
| REQ-TIME-02 | Si el GET falla o retorna fecha inválida (vacía, año "1969", o contiene "00-00"), debe usar **AT+CCLK?** como respaldo. | Alta |
| REQ-TIME-03 | El formato de fecha retornado debe ser `"YYYY-MM-DD HH:MM:SS"`. | Alta |

### 5.1 Fecha vía HTTP GET

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| REQ-TIME-GET-01 | Se debe abrir una sesión HTTP al host configurado (`HOST`) con bodylen=1024, headerlen=350. | Alta |
| REQ-TIME-GET-02 | Se debe realizar un GET a la ruta `/api/SAT/Evento/FechaActual`. | Alta |
| REQ-TIME-GET-03 | Se debe esperar la respuesta `+SHREQ: "GET",200,...` con timeout de **5 segundos**. | Alta |
| REQ-TIME-GET-04 | Se debe leer el cuerpo de la respuesta (35 bytes) y extraer el campo `"localTime"` del JSON. | Alta |
| REQ-TIME-GET-05 | El separador `"T"` entre fecha y hora debe reemplazarse por espacio. | Media |
| REQ-TIME-GET-06 | La sesión HTTP debe cerrarse con `AT+SHDISC` al finalizar (éxito o fallo). | Alta |

### 5.2 Fecha vía AT+CCLK?

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| REQ-TIME-CCLK-01 | Se debe enviar `AT+CCLK?` y parsear la respuesta con formato `"YY/MM/DD,HH:MM:SS±ZZ"`. | Alta |
| REQ-TIME-CCLK-02 | El año debe expandirse a 4 dígitos (prefijo "20"). | Alta |
| REQ-TIME-CCLK-03 | Se debe eliminar el sufijo de zona horaria (ej. `-2`, `-6`) si está presente. | Alta |

---

## 6. Envío de Datos (HTTP POST)

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| REQ-POST-01 | Antes de iniciar un POST, se debe cerrar cualquier sesión HTTP previa (`AT+SHDISC`). | Alta |
| REQ-POST-02 | Se debe configurar la sesión HTTP con URL del host, bodylen=1024, headerlen=350. | Alta |
| REQ-POST-03 | La conexión HTTP (`AT+SHCONN`) debe reintentarse hasta **3 veces** con espera de 5s por intento. | Alta |
| REQ-POST-04 | Si los 3 intentos de conexión fallan, se debe abortar y retornar `"ERROR: SHCONN"`. | Alta |
| REQ-POST-05 | Se deben configurar los headers HTTP: `Content-Type: application/json`, `Cache-control: no-cache`, `Connection: keep-alive`, `Accept: */*`. | Alta |
| REQ-POST-06 | El body se envía como stream plano tras `AT+SHBOD=<longitud>,10000`. | Alta |
| REQ-POST-07 | Se debe ejecutar `AT+SHREQ="<ruta>",3` para realizar el POST. | Alta |
| REQ-POST-08 | Se debe esperar la respuesta `+SHREQ: "POST",<código>,...` con timeout de **6 segundos**. | Alta |
| REQ-POST-09 | Si el código HTTP es **200**, se retorna `"1"` (éxito). | Alta |
| REQ-POST-10 | Si el código HTTP es **500**, se retorna `"0"` y se lee el cuerpo de respuesta (512 bytes) para diagnóstico. | Alta |
| REQ-POST-11 | Si no se recibe respuesta en el timeout, el resultado queda indeterminado (se retorna el string de respuesta crudo). | Media |
| REQ-POST-12 | La sesión HTTP debe cerrarse con `AT+SHDISC` siempre al finalizar. | Alta |

---

## 7. Utilidad de Envío de Comandos AT

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| REQ-CMD-01 | El método `enviarComando()` debe enviar un comando AT por UART y esperar un tiempo configurable (default 1000ms). | Alta |
| REQ-CMD-02 | Si el parámetro `leer` es `true` (default), se debe imprimir la respuesta por Serial. | Media |

---

## 8. Observaciones y Riesgos Identificados

### 8.1 Riesgos Críticos

| ID | Riesgo | Impacto | Recomendación |
|----|--------|---------|---------------|
| RISK-01 | **Bucle infinito en activación PDP** (REQ-PDP-04). No hay límite de reintentos ni watchdog. | Bloqueo total del dispositivo en campo. | Agregar máx. reintentos + timeout global + watchdog. |
| RISK-02 | **Sin gestión de errores en configuración de red.** Los comandos AT de configuración no verifican respuesta OK/ERROR. | Configuración parcial silenciosa. | Verificar respuesta de cada comando AT crítico. |
| RISK-03 | **APN hardcodeado a Telcel.** No hay selección de operadora ni fallback. | Inoperabilidad con otras SIMs/operadoras. | Implementar selección dinámica de operadora (como JAMR_4.5). |
| RISK-04 | **Banda fija (12).** Sin escaneo multi-banda. | Cobertura limitada. | Permitir configuración de múltiples bandas. |
| RISK-05 | **HOST como `#define` con valor censurado.** Sin validación de conectividad al host. | Falla silenciosa si host inalcanzable. | Agregar validación de conectividad post-PDP. |

### 8.2 Riesgos Moderados

| ID | Riesgo | Impacto | Recomendación |
|----|--------|---------|---------------|
| RISK-06 | **Uso extensivo de `String` (heap fragmentation).** Concatenación en bucles de lectura UART. | Memory leaks y crashes en ejecución prolongada. | Usar buffers `char[]` de tamaño fijo. |
| RISK-07 | **Delays bloqueantes.** Múltiples `delay()` de 1-30 segundos bloquean el CPU. | No hay posibilidad de watchdog ni procesamiento concurrente. | Migrar a máquina de estados con `millis()`. |
| RISK-08 | **Sin lectura de ICCID.** No se identifica la SIM activa. | No se puede trazar el dispositivo ni seleccionar APN dinámicamente. | Agregar `AT+CCID` en inicialización. |
| RISK-09 | **Lectura UART byte-a-byte** sin buffer ni delimitador de fin de respuesta. | Respuestas incompletas o corruptas. | Implementar lectura hasta `OK`/`ERROR` o timeout. |
| RISK-10 | **SHREAD con tamaño fijo (35 bytes)** para la fecha, independiente del tamaño real de respuesta. | Datos truncados si el servidor cambia el formato. | Usar el tamaño reportado por `+SHREQ`. |

### 8.3 Diferencias vs JAMR_4.5

| Aspecto | Código Rolando | JAMR_4.5 |
|---------|---------------|----------|
| Arquitectura | Clase monolítica | Mediador FSM + módulos separados |
| Operadora | Hardcoded Telcel | Selección dinámica por score SINR/RSRP/RSRQ |
| Reset módem | Solo hardware | Hardware + software + watchdog |
| Gestión de errores | Mínima | Feature flags + rollback + logging estructurado |
| Formato trama | POST JSON directo | `$,...#` → Base64 → TCP |
| Persistencia estado | Ninguna | `RTC_DATA_ATTR` + NVS + LittleFS |
| Deep sleep | No implementado | Integrado en FSM |
| Logging | `Serial.println` con emojis | `[NIVEL][MODULO] Mensaje` |

---

## 9. Requisitos Funcionales Extraídos (Resumen)

```
Total requisitos identificados: 32
├── Arquitectura:          3  (REQ-ARCH-*)
├── Inicialización:        4  (REQ-INIT-*)
├── Red:                   6  (REQ-NET-*)
├── PDP:                   4  (REQ-PDP-*)
├── Fecha/Hora:           10  (REQ-TIME-*)
├── HTTP POST:            12  (REQ-POST-*)
└── Comandos AT:           2  (REQ-CMD-*)

Riesgos identificados: 10
├── Críticos:    5  (RISK-01 a RISK-05)
└── Moderados:   5  (RISK-06 a RISK-10)
```

---

## 10. Recomendación General

Este código funciona como **prototipo o prueba de concepto** para la comunicación con el SIM7080G. Para integración en producción (JAMR_4.5), se requiere:

1. **Refactorizar** a la arquitectura de módulos (`LTEModule`) con patrón `begin()/execute()/end()`
2. **Agregar timeouts y límites de reintento** en todos los bucles
3. **Implementar selección de operadora** dinámica
4. **Reemplazar `String`** por buffers estáticos `char[]`
5. **Migrar a delays no bloqueantes** (`millis()`)
6. **Agregar Feature Flags** para cada cambio según protocolo JAMR_4.5
7. **Logging estructurado** con formato `[NIVEL][MODULO]`
