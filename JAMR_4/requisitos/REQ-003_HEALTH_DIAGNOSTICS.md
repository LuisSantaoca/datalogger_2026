# REQ-003: Diagnóstico Postmortem de Fallos (Health Data)

**Versión:** 1.0  
**Fecha:** 2025-10-29  
**Prioridad:** ALTA  
**Estado:** PENDIENTE

---

## 🎯 Objetivo (QUÉ)

El sistema **DEBE** preservar información crítica sobre su estado antes de cualquier reset/crash, permitiendo diagnóstico remoto de problemas sin acceso físico al dispositivo.

---

## 📋 Requisitos Funcionales

### RF-001: Persistencia Durante Resets
El sistema **DEBE** mantener datos de diagnóstico a través de resets de watchdog, panics y brownouts.

**Criterio de aceptación:**
- Datos persisten en memoria que sobrevive resets (RTC memory, NVRAM, etc.)
- Información disponible inmediatamente después del reset
- No se corrompe durante el proceso de reset

### RF-002: Identificación de Causa de Reset
El sistema **DEBE** determinar y registrar qué causó el último reset.

**Criterio de aceptación:**
- Detecta al menos: Power-on, Watchdog, Software reset, Brownout, Panic
- Información disponible antes de primera transmisión
- Causa se incluye en telemetría hacia backend

### RF-003: Tracking de Checkpoints
El sistema **DEBE** registrar en qué punto del código estaba ejecutando antes del reset.

**Criterio de aceptación:**
- Checkpoints en todas las fases críticas del ciclo
- Último checkpoint alcanzado se preserva
- Granularidad suficiente para identificar dónde ocurrió el fallo

### RF-004: Contador de Boot
El sistema **DEBE** contar cuántas veces ha reiniciado, distinguiendo entre resets esperados e inesperados.

**Criterio de aceptación:**
- Contador incrementa en cada boot
- Persiste a través de deep sleep sin incrementar
- Se resetea solo en power-on real (desconexión de batería)

### RF-005: Timestamp de Crashes
El sistema **DEBE** registrar cuándo ocurrió el último crash.

**Criterio de aceptación:**
- Timestamp relativo (segundos desde boot) o absoluto (epoch)
- Precisión: 1 segundo
- Persiste hasta siguiente transmisión exitosa

### RF-006: Transmisión a Backend
Los datos de diagnóstico **DEBEN** incluirse en el payload normal de telemetría.

**Criterio de aceptación:**
- Formato compatible con estructura de datos existente
- Tamaño adicional: ≤ 10 bytes
- Decodificable por servicios de ingesta actuales

---

## 🚫 Anti-Requisitos (QUÉ NO HACER)

### ANR-001: NO Implementar Storage Complejo
**PROHIBIDO:** Usar filesystem, database o estructuras complejas para health data.

**Razón:**
- Filesystem puede corromperse durante crash
- Overhead de I/O agrega puntos de fallo
- RTC memory es más simple y confiable

### ANR-002: NO Exceder Espacio Disponible
**PROHIBIDO:** Usar más de 64 bytes de RTC memory para health data.

**Razón:**
- RTC memory es limitada (ej: ESP32-S3 tiene 8KB, pero compartida)
- Otros sistemas pueden necesitar RTC memory
- Simplicidad > Cantidad de información

### ANR-003: NO Asumir Orden de Inicialización
**PROHIBIDO:** Asumir que sistema de logging o filesystem están disponibles para health data.

**Razón:**
- Health data debe funcionar **antes** de cualquier inicialización compleja
- Si crash ocurre temprano, otros sistemas pueden no estar listos
- Health data debe ser standalone

---

## 📊 Datos Mínimos Requeridos

### Estructura de Health Data

| Campo | Tipo | Bytes | Descripción |
|-------|------|-------|-------------|
| `checkpoint` | uint8 | 1 | Último checkpoint alcanzado (0-255) |
| `crash_reason` | uint8 | 1 | Causa del último reset (enum) |
| `boot_count` | uint16 | 2 | Contador de reinicios |
| `crash_timestamp` | uint32 | 4 | Segundos desde boot del crash |
| **Total** | | **8 bytes** | |

**Opcional (si espacio disponible):**
- `last_error_code`: último código de error antes del crash (2 bytes)
- `uptime_seconds`: tiempo de ejecución antes del crash (4 bytes)

### Definición de Checkpoints (Ejemplo)

| Valor | Nombre | Descripción |
|-------|--------|-------------|
| 0 | `CP_BOOT` | Sistema iniciando |
| 1 | `CP_GPIO_INIT` | GPIO inicializado |
| 2 | `CP_WATCHDOG_SET` | Watchdog configurado |
| 3 | `CP_SENSORS_READ` | Sensores leídos |
| 4 | `CP_GPS_START` | GPS iniciando |
| 5 | `CP_GPS_FIX` | GPS fix obtenido |
| 6 | `CP_GSM_OK` | Módem respondiendo |
| 7 | `CP_LTE_CONNECT` | Conectando a LTE |
| 8 | `CP_LTE_OK` | Conexión LTE establecida |
| 9 | `CP_TCP_OPEN` | Socket TCP abierto |
| 10 | `CP_DATA_SENT` | Datos enviados |
| 11 | `CP_SLEEP_PREP` | Preparando deep sleep |
| 255 | `CP_UNKNOWN` | Estado desconocido |

### Definición de Crash Reasons

| Valor | Nombre | Descripción |
|-------|--------|-------------|
| 0 | `POWERON` | Encendido normal |
| 1 | `TASK_WDT` | Watchdog timeout |
| 2 | `SW_RESET` | Reset por software |
| 3 | `PANIC` | Exception/panic |
| 4 | `INT_WDT` | Interrupt watchdog |
| 5 | `BROWNOUT` | Caída de voltaje |
| 6 | `SDIO` | Reset SDIO |
| 7 | `DEEPSLEEP` | Wake from deep sleep |
| 255 | `UNKNOWN` | Causa desconocida |

---

## 📊 Métricas de Éxito

### Métricas Primarias
| Métrica | Objetivo | Método de Medición |
|---------|----------|-------------------|
| Datos preservados post-reset | 100% | Test: forzar reset, leer datos |
| Identificación correcta de causa | 100% | Test: cada tipo de reset |
| Transmisión de health data | 100% | Backend: verificar campo presente |

### Métricas Secundarias
| Métrica | Objetivo | Método de Medición |
|---------|----------|-------------------|
| Overhead en payload | ≤ 10 bytes | Análisis de estructura |
| Latencia en boot | < 10 ms | Timing de inicialización |
| Falsos positivos | 0 | Validación de crash_reason en operación normal |

---

## 🔍 Casos de Uso

### CU-001: Reset por Watchdog
**Precondición:** Sistema colgado en conexión LTE

**Flujo:**
1. Sistema conectando a LTE (checkpoint = `CP_LTE_CONNECT`)
2. Módem no responde
3. Watchdog dispara reset a los 120s
4. Durante reset: health data se preserva en RTC memory
5. Sistema reinicia
6. **Primera acción:** Leer health data de RTC memory
7. Detecta: `crash_reason = TASK_WDT`, `checkpoint = CP_LTE_CONNECT`
8. Sistema continúa operación normal
9. En siguiente transmisión: health data incluido en payload
10. Backend recibe: "Device tuvo watchdog reset en LTE connection"

**Postcondición:** 
- Problema identificado remotamente
- No se requiere acceso físico al dispositivo
- Se puede correlacionar con logs de red celular

### CU-002: Brownout por Batería Baja
**Precondición:** Batería en nivel crítico

**Flujo:**
1. Sistema transmitiendo datos (checkpoint = `CP_DATA_SENT`)
2. TX del módem consume corriente alta
3. Voltaje cae por debajo de threshold
4. ESP32 detecta brownout y resetea
5. Health data preservado: `crash_reason = BROWNOUT`
6. Sistema reinicia (si batería se recuperó)
7. Lee health data
8. Transmite en siguiente ciclo
9. Backend alerta: "Device con brownouts - revisar batería"

**Postcondición:**
- Mantenimiento preventivo puede programarse
- Se evita fallo total del dispositivo

### CU-003: Operación Normal (Sin Crashes)
**Precondición:** Sistema operando correctamente

**Flujo:**
1. Sistema despierta de deep sleep
2. Lee health data: `crash_reason = DEEPSLEEP` (normal)
3. `checkpoint` avanza: BOOT → GPIO → SENSORS → GPS → GSM → LTE → TCP → SENT
4. Transmisión exitosa
5. Health data transmitido: `crash_reason = DEEPSLEEP`, `checkpoint = CP_DATA_SENT`
6. Backend ve: operación normal, último checkpoint fue transmisión exitosa

**Postcondición:**
- Health data confirma operación correcta
- Baseline para comparar con dispositivos con problemas

---

## 🔗 Dependencias

### Hardware
- RTC memory disponible y funcional
- Reset reason registers del microcontrolador accesibles

### Software
- Ninguna dependencia externa (debe ser standalone)
- Compatible con deep sleep (RTC memory persiste)
- Inicialización antes de cualquier otro sistema

### Backend
- Parser de payload actualizado para incluir health data
- Tabla en database con campos para diagnóstico
- Dashboard o alertas basadas en crash patterns

---

## ✅ Criterios de Validación

### Validación en Desarrollo
- [ ] Health data estructura definida y documentada
- [ ] Checkpoints identificados y mapeados
- [ ] Código de lectura/escritura RTC memory funcional
- [ ] Tests unitarios para cada tipo de reset

### Validación en Pruebas
- [ ] Test: Watchdog reset preserva datos
- [ ] Test: Brownout preserva datos (simulado)
- [ ] Test: Deep sleep no corrompe datos
- [ ] Test: Payload incluye health data correctamente codificado

### Validación en Campo
- [ ] Health data presente en 100% de transmisiones
- [ ] Crash reasons correlacionan con síntomas observados
- [ ] Checkpoints permiten identificar punto de fallo
- [ ] Boot count incrementa solo en resets reales

---

## 📝 Notas de Implementación

### Acceso a RTC Memory (ESP32-S3)

```cpp
// Ejemplo conceptual - NO código final
RTC_DATA_ATTR uint8_t health_checkpoint = 0;
RTC_DATA_ATTR uint8_t health_crash_reason = 0;
RTC_DATA_ATTR uint16_t health_boot_count = 0;
RTC_DATA_ATTR uint32_t health_crash_ts = 0;
```

### Detección de Reset Reason

```cpp
// Ejemplo conceptual
esp_reset_reason_t reason = esp_reset_reason();

switch (reason) {
  case ESP_RST_POWERON: return POWERON;
  case ESP_RST_TASK_WDT: return TASK_WDT;
  case ESP_RST_BROWNOUT: return BROWNOUT;
  // etc.
}
```

### Actualización de Checkpoints

```cpp
// En cada fase crítica
updateCheckpoint(CP_GPS_START);
// ... código de GPS ...
updateCheckpoint(CP_GPS_FIX);
```

### Inclusión en Payload

```cpp
// Agregar al final del payload existente
payload[40] = health_checkpoint;
payload[41] = health_crash_reason;
payload[42] = (health_boot_count >> 8) & 0xFF;
payload[43] = health_boot_count & 0xFF;
// ...
```

---

## 🐛 Lecciones de Intentos Anteriores

### Lo que SÍ funcionó en JAMR_3:
1. **RTC memory para persistencia:** Simple y confiable
2. **12 checkpoints:** Granularidad adecuada para diagnóstico
3. **Integración con payload:** Mínimo overhead, máximo beneficio

### Mejoras Necesarias:
1. **Documentación de checkpoints:** No estaba centralizada
2. **Dashboard de health data:** Backend no lo utilizaba efectivamente
3. **Alertas automáticas:** No había alerts basadas en crash patterns

---

## 📚 Referencias Técnicas

- ESP-IDF Reset Reason API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#reset-reason
- RTC Memory Usage: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/deep-sleep-stub.html
- Postmortem Debugging Best Practices: https://interrupt.memfault.com/blog/

---

**Documento creado:** 2025-10-29  
**Responsable:** Por definir  
**Revisión siguiente:** Tras implementación inicial
