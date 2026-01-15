# FEAT-V3: Sistema de Diagnóstico Post-Mortem para Crashes

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V3 |
| **Tipo** | Feature (Infraestructura de Diagnóstico) |
| **Sistema** | Diagnóstico / LTE / Módem |
| **Archivo Principal** | `src/data_diagnostics/CrashDiagnostics.h` |
| **Estado** | 📋 Propuesto (Revisado v4 - Solo Post-Mortem Local) |
| **Fecha** | 2026-01-15 |
| **Versión Target** | v2.3.0 |
| **Depende de** | FEAT-V1 (Feature Flags) |

---

## ⚠️ NOTAS DE REVISIÓN v4 (2026-01-15)

### Alcance Definido:
> **FEAT-V3 es EXCLUSIVAMENTE para análisis post-mortem LOCAL (vía Serial).**
> NO incluye cambios a la trama ni envío de datos al servidor.
> El diagnóstico remoto será un feature separado (FEAT-V4 futuro).

### Mejoras v2:
1. **Persistencia dual RTC + NVS**: RTC_DATA_ATTR no sobrevive brownout, se agrega NVS
2. **Contadores de boot/crash**: Para tracking de estabilidad a largo plazo
3. ~~**Diagnóstico remoto vía payload**~~: EXCLUIDO - será FEAT-V4
4. **Ubicación de archivos corregida**: `data_diagnostics/` en lugar de `data_info/`

### Mejoras v3 (revisión crítica):
5. **Semántica de contadores**: Tabla "Evento → Campos Afectados" con reglas precisas
6. ~~**Spec binaria del payload**~~: EXCLUIDO - será FEAT-V4
7. **Matriz de riesgos**: Modelo de degradación por fuente (RTC/NVS/LittleFS)
8. **Mapeo FSM → Checkpoints**: Alineación explícita con estados del AppController
9. **Límites de logs**: Truncado de AT commands
10. **Definition of Done**: Criterios de aceptación medibles

### Cambios v4 (simplificación de alcance):
11. **Excluido**: Cambios a FORMATModule.cpp
12. **Excluido**: Payload de diagnóstico en trama
13. **Enfoque**: Solo diagnóstico local vía Serial

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
Cuando el dispositivo está en campo y ocurre un crash durante la conexión del módem a la antena, no hay forma de determinar:
1. En qué punto exacto del código ocurrió el crash
2. Qué tipo de crash fue (WDT, panic, brownout, etc.)
3. Qué comando AT se estaba ejecutando
4. Historial de eventos previos al crash

### Síntomas
1. Dispositivo se reinicia inesperadamente durante operación LTE
2. No hay logs disponibles post-mortem
3. Imposible reproducir en laboratorio (solo ocurre con conexión real)
4. Pérdida de contexto al recolectar dispositivo en campo

### Causa Raíz
- Falta de sistema de persistencia de estado pre-crash
- Logs Serial se pierden al reiniciar
- No hay mecanismo de checkpoints en operaciones críticas del módem

---

## 📊 EVALUACIÓN

### Impacto
| Aspecto | Evaluación |
|---------|------------|
| Criticidad | **Alta** - Bloquea diagnóstico de problemas de campo |
| Riesgo de no implementar | **Alto** - Ciclos de desarrollo extendidos |
| Esfuerzo | **Medio** - ~3 archivos nuevos, modificaciones menores |
| Beneficio | **Alto** - Reduce tiempo de diagnóstico de días a minutos |

### Justificación
Sin este sistema, cada crash en campo requiere:
1. Viajar a recoger dispositivo
2. Intentar reproducir en laboratorio (frecuentemente imposible)
3. Agregar logs especulativos
4. Redesplegar y esperar otro crash
5. Repetir ciclo

Con FEAT-V3:
1. Recoger dispositivo
2. Conectar a Serial y ejecutar comando de diagnóstico
3. Obtener: checkpoint exacto, tipo de crash, último AT command, historial
4. Corregir con información precisa

---

## 🔧 IMPLEMENTACIÓN

### Archivos a Crear

| Archivo | Propósito |
|---------|-----------|
| `src/data_diagnostics/CrashDiagnostics.h` | Definiciones, estructuras RTC, funciones inline |
| `src/data_diagnostics/CrashDiagnostics.cpp` | Implementación de logging a NVS y LittleFS |
| `src/data_diagnostics/config_crash_diagnostics.h` | Configuración (tamaños, paths, límites) |

### Archivos a Modificar

| Archivo | Cambio | Sección |
|---------|--------|---------|
| `src/FeatureFlags.h` | Agregar `ENABLE_FEAT_V3_CRASH_DIAGNOSTICS` | Sección FEAT FLAGS |
| `src/data_lte/LTEModule.cpp` | Agregar checkpoints en operaciones críticas | Funciones AT |
| `AppController.cpp` | Agregar análisis de reset en AppInit + imprimir reporte | Inicio de FSM |

> **NOTA**: `src/data_format/FORMATModule.cpp` NO se modifica.
> El diagnóstico remoto (payload) será un feature separado (FEAT-V4).

---

## 📐 DISEÑO TÉCNICO

### Arquitectura de Persistencia (Dual Layer)

```
┌─────────────────────────────────────────────────────────────────┐
│                    ESTRATEGIA DE PERSISTENCIA                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│   RTC_DATA_ATTR (8KB)              NVS (Preferences)            │
│   ├─ Rápido (~1µs)                 ├─ Lento (~1-5ms)            │
│   ├─ Sobrevive: reset, WDT         ├─ Sobrevive: TODO           │
│   ├─ NO sobrevive: brownout        ├─ Incluye brownout          │
│   └─ Uso: checkpoints en curso     └─ Uso: historial crítico    │
│                                                                  │
│   ┌──────────────┐                 ┌──────────────┐             │
│   │ checkpoint   │ ───sync───────► │ last_cp      │             │
│   │ timestamp    │   (antes de     │ boot_count   │             │
│   │ at_command   │    operación    │ crash_count  │             │
│   │ history[16]  │    crítica)     │ crash_reason │             │
│   └──────────────┘                 └──────────────┘             │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

**Regla de sincronización:**
- RTC se actualiza en CADA checkpoint (rápido, no bloquea)
- NVS se actualiza SOLO antes de operaciones de alto riesgo:
  - Antes de `AT+CAOPEN` (TCP connect)
  - Antes de `AT+CASEND` (TCP send)
  - Antes de entrar a deep sleep
  - Después de crash detectado (al boot)

---

### Componente 1: RTC Memory (Datos Volátiles Rápidos)

```cpp
// Estructura que sobrevive reset y WDT (NO brownout)
// Ubicación: RTC slow memory (8KB disponibles)
RTC_DATA_ATTR struct CrashContext {
    // Checkpoint actual
    uint8_t checkpoint;            // Enum del último punto alcanzado
    uint32_t timestamp_ms;         // millis() del checkpoint
    
    // Contexto del módem
    char last_at_command[48];      // Último comando AT enviado
    char last_at_response[48];     // Última respuesta recibida
    uint8_t modem_state;           // Estado del módem (0-255)
    uint8_t lte_attempts;          // Intentos de conexión LTE
    int8_t rssi;                   // Señal al momento del crash
    
    // Historial circular
    uint8_t history[16];           // Últimos 16 checkpoints (ring buffer)
    uint8_t history_idx;           // Índice del ring buffer
    
    // Validación
    uint32_t magic;                // 0xDEADBEEF = datos válidos
} g_crash_ctx;
```

---

### Componente 2: NVS (Datos Persistentes Críticos)

```cpp
// Namespace: "crashdiag"
// Estos datos sobreviven TODO tipo de reset incluyendo brownout

struct NVSCrashData {
    // Contadores de estabilidad
    uint16_t boot_count;           // Boots totales del dispositivo
    uint16_t crash_count;          // Crashes acumulados (resets anormales)
    uint8_t consecutive_crashes;   // Crashes seguidos sin ciclo exitoso
    
    // Último crash
    uint8_t last_crash_checkpoint; // Dónde estaba cuando crasheó
    uint8_t last_crash_reason;     // ESP_RST_xxx
    uint32_t last_crash_epoch;     // Timestamp del crash (epoch)
    
    // Último ciclo exitoso
    uint32_t last_success_epoch;   // Cuándo fue el último envío OK
    uint8_t cycles_since_success;  // Ciclos desde último éxito
};

// Keys NVS:
// "boot_cnt"     - uint16_t
// "crash_cnt"    - uint16_t
// "consec_crash" - uint8_t
// "last_cp"      - uint8_t
// "last_reason"  - uint8_t
// "last_epoch"   - uint32_t
// "success_ep"   - uint32_t
// "cycles_fail"  - uint8_t
```

---

### Componente 2.1: Semántica de Actualización de Contadores

> **CRÍTICO**: Esta tabla define EXACTAMENTE cuándo se modifica cada campo.
> Sin estas reglas, la implementación sería ambigua.

| Evento | boot_count | crash_count | consecutive_crashes | cycles_since_success | last_success_epoch |
|--------|------------|-------------|---------------------|----------------------|--------------------|
| **Boot normal** (ESP_RST_DEEPSLEEP, ESP_RST_SW) | +1 | = | = | = | = |
| **Boot post-crash** (ESP_RST_TASK_WDT, ESP_RST_PANIC, ESP_RST_BROWNOUT, ESP_RST_INT_WDT) | +1 | +1 | +1 | +1 | = |
| **Boot power-on** (ESP_RST_POWERON) | +1 | = | =0 (reset) | =0 (reset) | = |
| **Ciclo exitoso** (TCP send OK + buffer compactado) | = | = | =0 (reset) | =0 (reset) | =now |
| **Ciclo fallido** (no se llegó a CP_CYCLE_SUCCESS) | = | = | = | +1 | = |
| **Enter deep sleep** | = | = | = | = | = |

**Definiciones precisas:**
- **Ciclo exitoso**: Se alcanza `CP_CYCLE_SUCCESS` Y se recibe "SEND OK" del módem Y se ejecuta `compactBuffer()`
- **Boot normal**: `reset_reason` ∈ {ESP_RST_DEEPSLEEP, ESP_RST_SW}
- **Boot post-crash**: `reset_reason` ∈ {ESP_RST_TASK_WDT, ESP_RST_INT_WDT, ESP_RST_PANIC, ESP_RST_BROWNOUT, ESP_RST_UNKNOWN}
- **Boot power-on**: `reset_reason` == ESP_RST_POWERON (implica que se perdió energía, reset de contadores volátiles)

**Clasificación explícita de reset_reason:**
| ESP_RST_xxx | ¿Es Crash? | Acción en boot |
|-------------|------------|----------------|
| ESP_RST_POWERON | ❌ No | Reset `consecutive_crashes` y `cycles_since_success` a 0 |
| ESP_RST_DEEPSLEEP | ❌ No | Solo incrementar `boot_count` |
| ESP_RST_SW | ❌ No | Solo incrementar `boot_count` |
| ESP_RST_PANIC | ✅ Sí | Incrementar `crash_count` y `consecutive_crashes` |
| ESP_RST_TASK_WDT | ✅ Sí | Incrementar `crash_count` y `consecutive_crashes` |
| ESP_RST_INT_WDT | ✅ Sí | Incrementar `crash_count` y `consecutive_crashes` |
| ESP_RST_BROWNOUT | ✅ Sí | Incrementar `crash_count` y `consecutive_crashes` |
| ESP_RST_UNKNOWN | ⚠️ Tratado como crash | Incrementar `crash_count` (causa indeterminada, asumir problema) |

**Caso especial - ESP_RST_POWERON con RTC inválido:**
- Si `g_crash_ctx.magic != 0xDEADBEEF` → RTC corrupto (brownout destruyó datos)
- Mensaje: `"RTC context lost - likely brownout before power-on"`
- Usar datos de NVS como fuente primaria

**Notas:**
- `=` significa "sin cambio"
- `+1` significa "incrementar en 1"
- `=0` significa "resetear a 0"
- `=now` significa "asignar epoch actual"

---

### Componente 3: Checkpoints del Sistema

```cpp
enum CrashCheckpoint : uint8_t {
    CP_NONE = 0,
    
    // Boot y sistema
    CP_BOOT_START = 10,
    CP_BOOT_SERIAL_OK = 11,
    CP_BOOT_GPIO_OK = 12,
    CP_BOOT_LITTLEFS_OK = 13,
    
    // Módem power
    CP_MODEM_POWER_ON_START = 100,
    CP_MODEM_POWER_ON_PWRKEY = 101,
    CP_MODEM_POWER_ON_WAIT = 102,
    CP_MODEM_POWER_ON_OK = 103,
    
    // Módem AT commands
    CP_MODEM_AT_SEND = 110,
    CP_MODEM_AT_WAIT_RESPONSE = 111,
    CP_MODEM_AT_RESPONSE_OK = 112,
    CP_MODEM_AT_RESPONSE_ERROR = 113,
    CP_MODEM_AT_TIMEOUT = 114,
    
    // Network
    CP_MODEM_NETWORK_ATTACH_START = 120,
    CP_MODEM_NETWORK_ATTACH_WAIT = 121,
    CP_MODEM_NETWORK_ATTACH_OK = 122,
    CP_MODEM_NETWORK_ATTACH_FAIL = 123,
    
    // PDP Context
    CP_MODEM_PDP_ACTIVATE_START = 130,
    CP_MODEM_PDP_ACTIVATE_WAIT = 131,
    CP_MODEM_PDP_ACTIVATE_OK = 132,
    CP_MODEM_PDP_ACTIVATE_FAIL = 133,
    
    // TCP
    CP_MODEM_TCP_CONNECT_START = 140,
    CP_MODEM_TCP_CONNECT_WAIT = 141,
    CP_MODEM_TCP_CONNECT_OK = 142,
    CP_MODEM_TCP_CONNECT_FAIL = 143,
    CP_MODEM_TCP_SEND_START = 144,
    CP_MODEM_TCP_SEND_WAIT = 145,
    CP_MODEM_TCP_SEND_OK = 146,
    CP_MODEM_TCP_CLOSE = 147,
    
    // Power off
    CP_MODEM_POWER_OFF_START = 150,
    CP_MODEM_POWER_OFF_OK = 151,
    
    // Sleep
    CP_SLEEP_ENTER = 200,
    
    // Ciclo exitoso
    CP_CYCLE_SUCCESS = 250,
};
```

### Principio de Obligatoriedad de Checkpoints

> ⚠️ **CRÍTICO**: Si los checkpoints obligatorios no están instrumentados,
> el reporte post-mortem NO garantiza localizar la fase del crash.

**Checkpoints OBLIGATORIOS (mínimo viable):**

| Checkpoint | Ubicación | Justificación |
|------------|-----------|---------------|
| `CP_BOOT_START` | Inicio de `setup()` | Confirma que el boot inició |
| `CP_MODEM_POWER_ON_START` | Antes de `powerOn()` | Punto más problemático históricamente |
| `CP_MODEM_TCP_CONNECT_START` | Antes de `AT+CAOPEN` | Operación de alto riesgo |
| `CP_MODEM_TCP_SEND_START` | Antes de `AT+CASEND` | Operación de alto riesgo |
| `CP_SLEEP_ENTER` | Antes de `esp_deep_sleep_start()` | Marca fin exitoso de ciclo |

**Checkpoints OPCIONALES (para diagnóstico más granular):**
- Todos los demás checkpoints son opcionales
- Agregar según necesidad de diagnóstico en áreas problemáticas
- Más checkpoints = más precisión, pero más código

**Consecuencia de no instrumentar:**
- Si falta `CP_MODEM_TCP_CONNECT_START`, un crash en TCP mostrará último checkpoint = `CP_BOOT_*`
- Esto impide saber si el crash fue en boot, sensores, módem, o TCP
- El reporte será **técnicamente correcto pero inútil para diagnóstico**

---

### ~~Componente 4: Diagnóstico Remoto (Payload)~~ - EXCLUIDO

> **NOTA**: El diagnóstico remoto vía payload ha sido EXCLUIDO de FEAT-V3.
> Será implementado como **FEAT-V4: Diagnóstico Remoto** en el futuro.
> 
> FEAT-V3 se enfoca exclusivamente en análisis post-mortem LOCAL vía Serial.

---

### Componente 4: API Principal

```cpp
// Macros para uso con feature flag (cero overhead si deshabilitado)
#if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
    #define CRASH_CHECKPOINT(cp) CrashDiag::setCheckpoint(cp)
    #define CRASH_LOG_AT(cmd) CrashDiag::logATCommand(cmd)
    #define CRASH_LOG_RESPONSE(resp) CrashDiag::logATResponse(resp)
    #define CRASH_ANALYZE() CrashDiag::analyzeLastCrash()
    #define CRASH_PRINT_REPORT() CrashDiag::printReport()
    #define CRASH_SYNC_NVS() CrashDiag::syncToNVS()
    #define CRASH_MARK_SUCCESS() CrashDiag::markCycleSuccess()
#else
    #define CRASH_CHECKPOINT(cp)
    #define CRASH_LOG_AT(cmd)
    #define CRASH_LOG_RESPONSE(resp)
    #define CRASH_ANALYZE()
    #define CRASH_PRINT_REPORT()
    #define CRASH_SYNC_NVS()
    #define CRASH_MARK_SUCCESS()
#endif

namespace CrashDiag {
    // Inicialización
    void init();                              // Inicializar sistema (llamar en setup)
    
    // Checkpoints (muy rápido, solo RTC)
    void setCheckpoint(CrashCheckpoint cp);   // Marcar checkpoint
    void logATCommand(const char* cmd);       // Guardar último AT
    void logATResponse(const char* resp);     // Guardar última respuesta
    
    // Persistencia NVS (llamar antes de operaciones críticas)
    void syncToNVS();                         // Sincronizar RTC → NVS
    
    // Análisis post-mortem (SOLO LOCAL - vía Serial)
    bool hadCrash();                          // ¿Hubo crash previo?
    void analyzeLastCrash();                  // Analizar causa de reset
    void printReport();                       // Imprimir reporte completo a Serial
    void clearHistory();                      // Limpiar historial
    
    // Ciclo exitoso
    void markCycleSuccess();                  // Marcar ciclo exitoso (reset contadores)
    
    // Getters para debug
    uint16_t getBootCount();
    uint8_t getCrashCount();
    uint8_t getLastCheckpoint();
    uint8_t getLastResetReason();
}
```

### Contrato de Captura de Comandos AT

> **IMPORTANTE**: Esta sección define EXACTAMENTE cómo y cuándo se capturan los comandos AT.

**¿Cuándo llamar `CRASH_LOG_AT(cmd)`?**
- INMEDIATAMENTE antes de `Serial1.println(cmd)` o equivalente
- NUNCA después del envío (si hay crash, ya se perdió la oportunidad)
- Solo para comandos críticos (TCP, PDP, network), no para `AT` simple

**¿Qué se guarda como respuesta?**
- `CRASH_LOG_RESPONSE(resp)` se llama cuando se recibe respuesta
- Si hay crash antes de la respuesta, `last_at_response` quedará vacío o con respuesta anterior
- Esto indica: "último comando enviado, sin respuesta recibida"

**Truncamiento de comandos AT:**
- RTC: 48 caracteres máximo
- LittleFS log: 32 caracteres máximo
- **Se mantiene PREFIJO, se descarta COLA**
  - `AT+CAOPEN=0,0,"TCP","serv...` (truncado a 48)
  - Racional: el tipo de comando está en el prefijo, los parámetros son menos críticos

**Ofuscación de datos sensibles:**
- Host y puerto son ofuscados antes de guardar
- Formato guardado: `AT+CAOPEN=0,0,"TCP",***,***`
- Esto aplica solo a log en LittleFS (el RTC tiene el comando real para debug local)

**Ejemplo de captura correcta:**
```cpp
// En LTEModule::tcpConnect()
CRASH_CHECKPOINT(CP_MODEM_TCP_CONNECT_START);
CRASH_LOG_AT("AT+CAOPEN=0,0,\"TCP\",\"host\",80");
CRASH_SYNC_NVS();  // Guardar antes de operación crítica
sendAT(cmd);       // Ahora sí enviar

// Respuesta
CRASH_CHECKPOINT(CP_MODEM_TCP_CONNECT_WAIT);
String response = readATResponse();
CRASH_LOG_RESPONSE(response.c_str());
if (response.indexOf("OK") >= 0) {
    CRASH_CHECKPOINT(CP_MODEM_TCP_CONNECT_OK);
}
```

### Componente 5: Logging a LittleFS (Historial Extendido)

```cpp
// Para análisis profundo cuando se recupera el dispositivo
// Archivo: /crash_log.bin (ring buffer de 32 eventos, ~2KB)
// Formato binario para eficiencia

struct CrashLogEntry {
    uint32_t epoch;                // Timestamp del evento
    uint8_t checkpoint;            // Checkpoint alcanzado
    uint8_t event_type;            // 0=checkpoint, 1=crash, 2=success
    uint8_t reset_reason;          // Solo si event_type=1
    int8_t rssi;                   // Señal al momento
    char at_command[32];           // Comando AT (truncado)
};
// Tamaño por entrada: ~40 bytes
// 32 entradas = 1.3KB

// Rotación automática: cuando llega a 32 entradas, sobrescribe la más antigua
```

**Límites de campos AT (privacidad y estabilidad):**
- `last_at_command`: Truncado a 32 chars en log, 48 chars en RTC
- Formato: Se guarda el comando COMPLETO pero SIN el host/puerto
  - Ejemplo: `AT+CAOPEN=0,0,"TCP",***,***` (host/puerto ofuscados)
- El host/puerto NUNCA viaja en payload remoto (solo local vía Serial)
- Esto evita exposición de infraestructura y reduce tamaño

---

### Componente 6: Prioridad de Fuentes para Reporte Serial

> **CRÍTICO**: El reporte Serial usa una jerarquía de fuentes.
> Esta prioridad es fija y hace el análisis reproducible.

**Orden de prioridad (mayor a menor):**

```
1. RTC Memory (si magic == 0xDEADBEEF)
   └─ checkpoint, at_command, at_response, history[], rssi
   
2. NVS (siempre disponible si no hay corrupción)
   └─ boot_count, crash_count, consecutive_crashes, last_crash_reason
   
3. LittleFS (solo para historial extendido)
   └─ crash_log.bin con últimos 32 eventos
```

**Reglas de uso:**
- Si RTC `magic` es válido → usar RTC para contexto del crash
- Si RTC `magic` es inválido → imprimir `"[RTC lost]"` y usar solo NVS
- NVS es fuente primaria para contadores (siempre válida)
- LittleFS se lee solo si se solicita historial extendido

**Formato de reporte cuando RTC está corrupto:**
```
=== CRASH DIAGNOSTICS REPORT ===
Boot count: 45
Crash count: 3
Last reset reason: ESP_RST_BROWNOUT
RTC Context: [LOST - magic invalid]
Using NVS fallback data...
Last checkpoint (NVS): CP_MODEM_TCP_SEND_WAIT (145)
```

---

### Componente 6.1: Guía de Interpretación Mecánica

> **PROPÓSITO**: Patrones comunes para diagnóstico rápido en campo.
> Esta guía permite operar sin conocer el código.

**Patrones de checkpoints y su significado:**

| Patrón | Diagnóstico | Acción sugerida |
|--------|-------------|-----------------|
| `*_WAIT` + respuesta vacía | Timeout en operación | Revisar timeouts, señal, antena |
| `*_START` repetido sin `*_OK` | Crash temprano en operación | Operación nunca completa, revisar hardware |
| `*_START` + WDT | Bloqueo en operación | Código se quedó esperando, revisar loops |
| `CP_BOOT_*` + WDT | Crash en inicialización | Problema de hardware o config |
| `CP_MODEM_AT_TIMEOUT` repetido | Módem no responde | Verificar alimentación, PWRKEY, baudrate |
| `CP_MODEM_TCP_CONNECT_*` + crash | Problema de red | Verificar SIM, APN, cobertura |
| `consecutive_crashes` > 3 | Problema recurrente | Posible bug de firmware, escalar |
| `cycles_since_success` > 10 | Dispositivo no transmite | Problema persistente, intervención necesaria |

**Ejemplo de lectura de reporte:**
```
=== CRASH DIAGNOSTICS REPORT ===
Boot count: 47
Crash count: 5
Consecutive crashes: 2
Last reset reason: ESP_RST_TASK_WDT
Last checkpoint: CP_MODEM_TCP_SEND_WAIT (145)
Last AT command: AT+CASEND=0,256
Last AT response: [empty]
```

**Interpretación mecánica:**
1. `ESP_RST_TASK_WDT` → El watchdog mató el proceso (timeout)
2. `CP_MODEM_TCP_SEND_WAIT` → Estaba esperando respuesta del envío TCP
3. `AT+CASEND` + respuesta vacía → El módem no respondió al envío
4. **Diagnóstico**: Timeout en envío TCP, posible pérdida de conexión durante transmisión
5. **Acción**: Verificar estabilidad de conexión, considerar reducir tamaño de payload

---

### Componente 6.1: Matriz de Riesgos y Degradación

> **IMPORTANTE**: Esta matriz documenta qué puede fallar y cómo se degrada el diagnóstico.
> La frase "no tiene riesgo" está prohibida - todo tiene riesgos.

| Componente | Sobrevive a | NO sobrevive a | Falla típica | Detección | Degradación |
|------------|-------------|----------------|--------------|-----------|-------------|
| **RTC Memory** | Reset SW, WDT, Panic | Brownout, Power-off | Corrupción por escritura parcial | Campo `magic` ≠ 0xDEADBEEF | Usar solo datos de NVS |
| **NVS** | Todo (incluso brownout) | Wear-out (~100K escrituras) | Partición llena, corrupción | `nvs_get` retorna error | Log warning, continuar sin persistencia |
| **LittleFS** | Todo | Corrupción por brownout durante escritura | Archivo corrupto, filesystem dañado | `LittleFS.begin()` falla | Recrear archivo, perder historial |

**Estrategia de degradación graceful:**
```
Nivel 0 (óptimo):   RTC ✓ + NVS ✓ + LittleFS ✓ → Diagnóstico completo
Nivel 1:            RTC ✓ + NVS ✓ + LittleFS ✗ → Sin historial extendido
Nivel 2:            RTC ✗ + NVS ✓ + LittleFS ✗ → Solo contadores (sin contexto AT)
Nivel 3 (mínimo):   RTC ✗ + NVS ✗ + LittleFS ✗ → Solo reset_reason de hardware
```

**Mitigaciones implementadas:**
- RTC: Campo `magic` para detectar datos inválidos
- NVS: Verificar retorno de cada operación, no asumir éxito
- LittleFS: Escribir en archivo temporal + rename (atómico)
- General: Feature flag permite deshabilitar todo si causa problemas

**Ciclos de escritura (wear-out):**
- NVS: ~100,000 escrituras por key (flash)
- LittleFS: ~100,000 escrituras por sector
- Con 1 sync NVS por ciclo y ciclos de 10 min → ~16 años de vida útil
- Riesgo real: Casi nulo en operación normal

---

### Componente 7: Flujo de Operación

```
┌─────────────────────────────────────────────────────────────────┐
│                        FLUJO NORMAL                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Boot                                                            │
│   ├─► CrashDiag::init()                                         │
│   │    ├─ boot_count++ en NVS                                   │
│   │    ├─ Verificar reset_reason                                │
│   │    │   └─ Si anormal: crash_count++, guardar contexto       │
│   │    └─ Imprimir reporte si hubo crash                        │
│   │                                                              │
│   ├─► CRASH_CHECKPOINT(CP_BOOT_START)                           │
│   ├─► CRASH_CHECKPOINT(CP_BOOT_LITTLEFS_OK)                     │
│   ├─► ... sensores ...                                           │
│   │                                                              │
│   ├─► CRASH_CHECKPOINT(CP_MODEM_TCP_CONNECT_START)              │
│   ├─► CRASH_SYNC_NVS()  ◄── Punto crítico, guardar en NVS       │
│   ├─► sendATCommand("AT+CAOPEN...")                             │
│   │    └─ CRASH_LOG_AT(cmd)                                     │
│   │                                                              │
│   ├─► Si éxito:                                                  │
│   │    └─ CRASH_MARK_SUCCESS()  ◄── Reset consecutive_crashes   │
│   │                                                              │
│   └─► CRASH_CHECKPOINT(CP_SLEEP_ENTER)                          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                        FLUJO CRASH                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Operación normal...                                             │
│   ├─► CRASH_CHECKPOINT(CP_MODEM_TCP_SEND_WAIT)                  │
│   │                                                              │
│   ├─► ⚡ CRASH (WDT/Panic/Brownout)                              │
│   │                                                              │
│   └─► [Hardware reset]                                           │
│                                                                  │
│  Boot (post-crash)                                               │
│   ├─► CrashDiag::init()                                         │
│   │    ├─ Detecta reset_reason = ESP_RST_TASK_WDT               │
│   │    ├─ crash_count++                                          │
│   │    ├─ consecutive_crashes++                                  │
│   │    ├─ Lee RTC: checkpoint=144, at_cmd="AT+CASEND"           │
│   │    ├─ Guarda en NVS: last_crash_checkpoint, last_crash_epoch│
│   │    └─ Escribe entrada en /crash_log.bin                      │
│   │                                                              │
│   └─► Serial: "⚠️ CRASH DETECTED! See report below..."          │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 🧪 PLAN DE TESTING

> **ALCANCE**: Solo diagnóstico post-mortem LOCAL vía Serial.
> No hay pruebas de payload, servidor ni trama.

### Fase 1: Validación Local (Oficina)
| Test | Método | Criterio de Éxito |
|------|--------|-------------------|
| Compilación | Build con flag=1 y flag=0 | Sin errores ni warnings |
| Sin overhead | Medir tiempo ciclo con/sin | Diferencia < 10ms |
| RTC persistencia | Forzar reset por WDT, leer datos | Datos en RTC sobreviven |
| NVS persistencia | Forzar brownout, leer datos | Datos en NVS sobreviven |
| Reset reason | Provocar cada tipo de reset | Clasificación correcta (crash vs normal) |
| Contadores | 10 boots, verificar incremento | boot_count = 10 |
| Reporte Serial | Conectar post-crash, leer reporte | Todos los campos presentes y legibles |
| Último AT | Crash durante TCP, verificar log | AT command capturado correctamente |

### Tipos de Reset a Probar
| Tipo | Cómo Provocar | ESP_RST_xxx | ¿Es Crash? |
|------|---------------|-------------|------------|
| Power cycle | Desconectar alimentación | ESP_RST_POWERON | ❌ No (reset contadores) |
| WDT Task | `while(1){}` sin feed | ESP_RST_TASK_WDT | ✅ Sí |
| WDT Int | Deshabilitar interrupts | ESP_RST_INT_WDT | ✅ Sí |
| Brownout | Bajar voltaje a <2.8V | ESP_RST_BROWNOUT | ✅ Sí |
| Panic | Acceso a memoria inválida | ESP_RST_PANIC | ✅ Sí |
| Reset SW | `ESP.restart()` | ESP_RST_SW | ❌ No |
| Deep sleep | Wakeup normal | ESP_RST_DEEPSLEEP | ❌ No |
| Unknown | Causa indeterminada | ESP_RST_UNKNOWN | ⚠️ Tratado como crash |

### Fase 2: Validación Campo Controlado
| Test | Método | Criterio de Éxito |
|------|--------|-------------------|
| Operación normal | 24h con feature activo | Sin degradación |
| Crash simulado | Quitar antena durante TX | Captura checkpoint correcto |
| Reporte Serial | Conectar y leer post-crash | Información completa y clara |
| Log LittleFS | Leer `/crash_log.bin` vía Serial | Historial correcto |
| Interpretación | Seguir guía mecánica | Fase identificable sin ambigüedad |

### Fase 3: Despliegue Limitado
- 1-2 dispositivos en ubicación accesible
- Monitoreo por 1 semana
- Recolección física y análisis vía Serial
- Validar que el reporte permite identificar fase del crash

---

## 🔧 PROCEDIMIENTO DE REVISIÓN POST-MORTEM

> **PROPÓSITO**: Cómo revisar la información de diagnóstico cuando recoges un dispositivo del campo.

### Flujo de Revisión

```
┌─────────────────────────────────────────────────────────────────┐
│                  PROCEDIMIENTO DE CAMPO                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  1. Recoger dispositivo del campo                               │
│      └─► Traer a oficina/laboratorio                            │
│                                                                  │
│  2. Conectar USB al ESP32-S3                                    │
│      └─► Abrir terminal Serial (115200 baud)                    │
│                                                                  │
│  3. Resetear dispositivo (botón EN o power cycle)               │
│      └─► Si hubo crash: reporte se imprime AUTOMÁTICAMENTE      │
│      └─► Si no hubo crash: boot normal                          │
│                                                                  │
│  4. Comandos manuales disponibles:                              │
│      └─► "DIAG"    → Reimprimir último reporte                  │
│      └─► "HISTORY" → Ver últimos 32 eventos                     │
│      └─► "CLEAR"   → Limpiar historial (después de analizar)    │
│                                                                  │
│  5. Copiar/fotografiar reporte para análisis                    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Implementación del Procedimiento

**En `setup()` - Reporte automático al boot:**
```cpp
void setup() {
    Serial.begin(115200);
    delay(100);  // Dar tiempo a terminal para conectar
    
    CrashDiag::init();
    
    // AUTO: Si hubo crash, imprimir inmediatamente
    if (CrashDiag::hadCrash()) {
        Serial.println("\n⚠️ CRASH DETECTADO EN CICLO ANTERIOR");
        CrashDiag::printReport();
        Serial.println("Escribe 'HISTORY' para ver historial completo\n");
    }
}
```

**En `loop()` - Comandos manuales:**
```cpp
void loop() {
    // Comandos de diagnóstico (solo si hay datos en Serial)
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        cmd.toUpperCase();
        
        if (cmd == "DIAG") {
            CrashDiag::printReport();
        } else if (cmd == "HISTORY") {
            CrashDiag::printHistory();  // 32 eventos de /crash_log.bin
        } else if (cmd == "CLEAR") {
            CrashDiag::clearHistory();
            Serial.println("✓ Historial borrado");
        }
    }
    
    // ... resto del loop normal
}
```

### Comandos Disponibles

| Comando | Función | Cuándo Usar |
|---------|---------|-------------|
| `DIAG` | Reimprimir último reporte de crash | Cuando necesitas ver de nuevo el reporte |
| `HISTORY` | Mostrar últimos 32 eventos (crashes + éxitos) | Análisis profundo de patrones |
| `CLEAR` | Borrar historial de LittleFS | Después de analizar, antes de redesplegar |

### Herramientas Recomendadas

| Herramienta | Plataforma | Notas |
|-------------|------------|-------|
| Arduino IDE Serial Monitor | Windows/Mac/Linux | Incluido con Arduino |
| PlatformIO Serial Monitor | VS Code | `pio device monitor` |
| PuTTY | Windows | Permite logging a archivo |
| minicom | Linux/Mac | Terminal de línea de comandos |
| CoolTerm | Windows/Mac | Permite captura de pantalla |

**Configuración Serial:**
- Baudrate: 115200
- Data bits: 8
- Parity: None
- Stop bits: 1
- Flow control: None

### Checklist de Revisión en Campo

```
□ Conectar USB
□ Abrir terminal Serial (115200)
□ Resetear dispositivo
□ ¿Apareció reporte automático?
  □ Sí → Copiar/fotografiar reporte
  □ No → Dispositivo no crasheó (verificar con "DIAG")
□ Escribir "HISTORY" para ver patrones
□ Analizar usando Guía de Interpretación Mecánica (Componente 6.1)
□ Escribir "CLEAR" si el análisis está completo
□ Redesplegar o reparar según diagnóstico
```

---

## ⏪ ESTRATEGIA DE ROLLBACK

### Rollback Inmediato
```cpp
// En FeatureFlags.h cambiar:
#define ENABLE_FEAT_V3_CRASH_DIAGNOSTICS  0  // Deshabilitado
```
- Recompilar y flashear
- Tiempo: < 5 minutos
- El código nuevo se ignora completamente (macros vacías)

### Rollback Parcial
- Si solo LittleFS causa problemas, deshabilitar solo esa parte
- RTC memory no tiene riesgo (solo lectura/escritura de variables)

---

## 📦 ENTREGABLES

### Código
- [ ] `src/data_diagnostics/CrashDiagnostics.h`
- [ ] `src/data_diagnostics/CrashDiagnostics.cpp`
- [ ] `src/data_diagnostics/config_crash_diagnostics.h`
- [ ] Modificación `src/FeatureFlags.h`
- [ ] Modificación `src/data_lte/LTEModule.cpp` (checkpoints)
- [ ] Modificación `AppController.cpp` (análisis al boot + imprimir reporte)

> **EXCLUIDO de FEAT-V3:**
> - ~~`src/data_format/FORMATModule.cpp`~~ (será FEAT-V4)
> - ~~Payload de diagnóstico en trama~~ (será FEAT-V4)

### Documentación
- [x] FEAT_V3_CRASH_DIAGNOSTICS.md (este documento)
- [ ] Actualizar README.md con nueva funcionalidad

---

## 📝 EJEMPLO DE SALIDA ESPERADA

### Reporte Serial (Post-Crash)
```
=====================================
🔍 CRASH DIAGNOSTICS REPORT
=====================================
Reset Reason: ESP_RST_TASK_WDT (Task Watchdog)
Boot Count: 47
Crash Count: 3 (consecutive: 1)
Cycles Since Last Success: 1

Last Checkpoint: CP_MODEM_TCP_CONNECT_WAIT (141)
Checkpoint Time: 45230 ms after boot
Last AT Command: AT+CAOPEN=0,0,"TCP","d04.elathia.ai",13607
Last Response: (none - timeout)
Modem State: 3 (Connecting)
LTE Attempts: 2
RSSI: -89 dBm

Checkpoint History (newest first):
  [0] CP_MODEM_TCP_CONNECT_WAIT (141) @ 45230ms
  [1] CP_MODEM_TCP_CONNECT_START (140) @ 45100ms
  [2] CP_MODEM_PDP_ACTIVATE_OK (132) @ 43500ms
  [3] CP_MODEM_NETWORK_ATTACH_OK (122) @ 38200ms
  [4] CP_MODEM_POWER_ON_OK (103) @ 12500ms
  [5] CP_BOOT_LITTLEFS_OK (13) @ 850ms

Last Crash Log Entry:
  Epoch: 1736899200 (2026-01-15 10:00:00)
  Checkpoint: 141 (TCP_CONNECT_WAIT)
  AT Command: AT+CAOPEN=0,0,"TCP"...
=====================================
```

> **NOTA**: El diagnóstico es SOLO LOCAL vía Serial.
> Para diagnóstico remoto, ver FEAT-V4 (futuro).

### Interpretación Rápida de Checkpoints
| Rango | Subsistema | Acción si crash aquí |
|-------|------------|---------------------|
| 10-19 | Boot | Problema de hardware/flash |
| 100-109 | Modem Power | Revisar alimentación módem |
| 110-119 | AT Commands | Timeout de comandos |
| 120-129 | Network | Revisar SIM/cobertura |
| 130-139 | PDP | Revisar APN/operadora |
| 140-149 | TCP | Revisar servidor/puerto |
| 150-159 | Power Off | Problema al apagar módem |
| 200 | Sleep | Problema entrando a deep sleep |
| 250 | Success | No debería crashear aquí |

---

### Mapeo FSM → Checkpoints (Alineación con AppController)

> **CRÍTICO**: Esta tabla define qué checkpoints son OBLIGATORIOS en cada estado de la FSM.
> Evita checkpoints "inventados" y garantiza cobertura completa.

| Estado FSM (AppController) | Checkpoints OBLIGATORIOS | Checkpoints Opcionales | Propósito |
|----------------------------|--------------------------|------------------------|------------|
| `Boot` | CP_BOOT_START, CP_BOOT_LITTLEFS_OK | CP_BOOT_SERIAL_OK, CP_BOOT_GPIO_OK | Detectar fallos de inicialización |
| `BleOnly` | (ninguno) | - | BLE no es crítico para diagnóstico |
| `Cycle_ReadSensors` | (ninguno) | CP_SENSORS_OK* | Sensores rara vez causan crash |
| `Cycle_Gps` | (ninguno) | CP_GPS_OK* | GPS rara vez causa crash |
| `Cycle_GetICCID` | CP_MODEM_POWER_ON_START, CP_MODEM_POWER_ON_OK | CP_MODEM_POWER_ON_PWRKEY | Detectar fallos de encendido módem |
| `Cycle_BuildFrame` | (ninguno) | - | Operación en memoria, sin riesgo |
| `Cycle_BufferWrite` | (ninguno) | - | LittleFS raramente causa crash |
| `Cycle_SendLTE` | **TODOS los de red** | - | Fase más propensa a crashes |
| `Cycle_CompactBuffer` | (ninguno) | - | Post-éxito, bajo riesgo |
| `Cycle_Sleep` | CP_SLEEP_ENTER | - | Último checkpoint antes de dormir |

**Checkpoints OBLIGATORIOS en `Cycle_SendLTE`:**
```
CP_MODEM_NETWORK_ATTACH_START → CP_MODEM_NETWORK_ATTACH_OK/FAIL
CP_MODEM_PDP_ACTIVATE_START → CP_MODEM_PDP_ACTIVATE_OK/FAIL
CP_MODEM_TCP_CONNECT_START → [SYNC_NVS] → CP_MODEM_TCP_CONNECT_OK/FAIL
CP_MODEM_TCP_SEND_START → [SYNC_NVS] → CP_MODEM_TCP_SEND_OK
CP_MODEM_TCP_CLOSE
CP_MODEM_POWER_OFF_START → CP_MODEM_POWER_OFF_OK
CP_CYCLE_SUCCESS (solo si todo OK)
```

**Checkpoints marcados con * son opcionales pero recomendados para futura expansión.**

**Regla de implementación:**
- Los checkpoints OBLIGATORIOS deben estar en el código desde v1
- Los opcionales pueden agregarse en versiones futuras
- NUNCA agregar checkpoints que no estén en esta tabla sin actualizar el documento

---

## ✅ DEFINITION OF DONE (Criterios de Aceptación)

> **OBLIGATORIO**: El feature NO está completo hasta cumplir TODOS estos criterios.
> **ALCANCE**: Solo diagnóstico post-mortem LOCAL (vía Serial).
> **EXCLUIDO**: Payload, trama, backend, servidor, transmisión remota (será FEAT-V4).

### Criterios Funcionales

| # | Criterio | Métrica | Cómo Verificar |
|---|----------|---------|----------------|
| 1 | Identificar fase del crash | ≥95% de crashes tienen checkpoint identificable | Provocar 20 crashes, verificar que 19+ tienen CP correcto |
| 2 | Distinguir crash vs reset normal | 100% de resets clasificados según tabla | Provocar cada tipo de reset, verificar clasificación |
| 3 | Distinguir crash aislado vs repetitivo | consecutive_crashes refleja realidad | Provocar 3 crashes seguidos, verificar contador=3 |
| 4 | Contexto AT disponible | ≥90% de crashes en LTE tienen comando AT | Crashes en TCP deben mostrar último AT |
| 5 | Reporte Serial completo y legible | Todos los campos del ejemplo presentes | Conectar post-crash, ejecutar CRASH_PRINT_REPORT() |
| 6 | Guía de interpretación usable | Técnico puede diagnosticar sin leer código | Dar reporte a persona ajena, pedir diagnóstico |

### Criterios de Calidad

| # | Criterio | Métrica | Cómo Verificar |
|---|----------|---------|----------------|
| 7 | Zero overhead cuando deshabilitado | Diferencia <1ms en tiempo de ciclo | Medir con flag=0 vs sin código |
| 8 | Overhead aceptable cuando habilitado | <10ms adicionales por ciclo | Medir con FEAT_V2 Cycle Timing |
| 9 | No degrada estabilidad | 0 crashes causados por el feature | 48h de operación sin crashes nuevos |
| 10 | Rollback funcional | <5min para deshabilitar | Cambiar flag, compilar, flashear |

### Criterios de Documentación

| # | Criterio | Entregable |
|---|----------|------------|
| 11 | Semántica de contadores clara | Este documento, sección 2.1 |
| 12 | Mapeo FSM completo | Este documento, sección "Mapeo FSM" |
| 13 | Guía de interpretación mecánica | Componente 6.1 de este documento |
| 14 | Prioridad de fuentes documentada | Componente 6 de este documento |

### Criterios EXCLUIDOS (serán FEAT-V4)

| # | Criterio | Por qué excluido |
|---|----------|------------------|
| ❌ | Datos llegan al servidor | Requiere cambio de payload |
| ❌ | Dashboard de salud | Requiere backend |
| ❌ | Parser actualizado | Requiere coordinación servidor |

### Conexión con Objetivos del Proyecto

| Objetivo (CONTEXTO_2026) | Cómo FEAT-V3 Contribuye |
|--------------------------|-------------------------|
| Robustez y confiabilidad | Permite identificar y corregir causas de crash |
| Operación sin intervención 30+ días | Diagnóstico local acelera corrección cuando se recoge dispositivo |
| Resiliencia energética | Detecta brownouts y permite ajustar umbrales |

---

## ✅ CHECKLIST PRE-IMPLEMENTACIÓN

- [x] Documento FEAT creado
- [ ] Revisión y aprobación del diseño
- [ ] Crear branch `feat-v3-crash-diagnostics`
- [ ] Implementar archivos nuevos
- [ ] Modificar archivos existentes
- [ ] Testing Fase 1
- [ ] Testing Fase 2
- [ ] Merge a main

---

## 📚 REFERENCIAS

- ESP-IDF Reset Reasons: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html
- RTC Memory: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/deep-sleep-stub.html
- NVS (Preferences): https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html
- Proyecto JAMR_4.4: Implementación similar con `getCrashInfo()` y checkpoints

---

## 📊 RESUMEN DE CONSUMO DE RECURSOS

| Recurso | Uso | Límite | % |
|---------|-----|--------|---|
| RTC Memory | ~150 bytes | 8KB | 1.8% |
| NVS | ~50 bytes | 16KB | 0.3% |
| LittleFS | ~2KB | Variable | Mínimo |
| RAM (runtime) | ~200 bytes | - | Despreciable |
| Overhead por checkpoint | ~5µs | - | Despreciable |
| Overhead sync NVS | ~2ms | - | Solo en puntos críticos |

---

## � FEATURE FUTURO: FEAT-V4 Diagnóstico Remoto

> **NOTA**: El diagnóstico remoto vía payload NO está incluido en FEAT-V3.
> Será implementado como un feature separado.

### Alcance de FEAT-V4 (futuro)
- Modificación de `FORMATModule.cpp` para agregar 8 bytes de diagnóstico
- Especificación binaria del payload para backend
- Cambios en parser del servidor
- Dashboard de salud de dispositivos

### Por qué está separado
1. **Independencia**: FEAT-V3 es útil por sí solo (diagnóstico local)
2. **Riesgo**: Cambiar la trama afecta backend, requiere coordinación
3. **Validación**: Primero validar checkpoints localmente, luego enviar remoto
4. **Rollback**: Si falla el payload, FEAT-V3 sigue funcionando

---

**Autor:** GitHub Copilot  
**Fecha creación:** 2026-01-15  
**Última actualización:** 2026-01-15 (Revisión v5 - Refinamiento quirúrgico para post-mortem local)
