# Changelog — Fenix 1.0

## v1.0.0 — 2026-02-15

### Fork de jarm_4.5_autoswich v2.3.0

Variante de firmware para dataloggers con el mismo PCB pero **SIN circuito autoswitch** poblado. GPIO 2 no tiene efecto en estas tarjetas.

### Cambios vs jarm_4.5_autoswich v2.3.0

| Cambio | Detalle |
|--------|---------|
| REMOVIDO | Logica autoswitch GPIO 2 (.ino) |
| REMOVIDO | Constante `AUTOSWITCH_CYCLES` |
| REMOVIDO | Countdown autoswitch en boot banner y cycle summary |
| NUEVO | Deteccion de modem zombie por contador de fallos consecutivos |
| NUEVO | `esp_restart()` como recovery tras 6 fallos consecutivos |
| NUEVO | Backoff progresivo: skip modem 3 ciclos tras restart |
| NUEVO | NVS namespace "fenix" para persistir estado de recovery |
| NUEVO | Gate `g_modemAllowed` en GPS, ICCID y LTE |
| CAMBIADO | `FW_VERSION`: `"2.3.0"` → `"1.0.0"` |
| CAMBIADO | Banner: `"FENIX v1.0.0"` |
| CAMBIADO | Ciclo: libre sin reset (diagnostico) |

### Jerarquia de recuperacion del modem

| Nivel | Mecanismo | Origen |
|-------|-----------|--------|
| 1 | PWRKEY toggle 1200ms x 3 | FIX-V3 (heredado) |
| 2 | PWRKEY forzado 13s x 1 | FIX-V4 (heredado) |
| 3 | esp_restart() | NUEVO fenix |
| 4 | Backoff 3 ciclos | NUEVO fenix |
| 5 | Buffering pasivo | FIX-V7/V12 (heredado) |

### Fixes heredados de jarm_4.5_autoswich v2.3.0

| Fix | Descripcion | Estado |
|-----|-------------|--------|
| FIX-V1 | Eliminar manipulacion directa GPIO 9 + powerOff antes de sleep | Heredado |
| FIX-V2 | Esperar URC NORMAL POWER DOWN en GPSModule::powerOff() | Heredado |
| FIX-V3 | Verificar isAlive antes de toggle PWRKEY | Heredado |
| FIX-V4 | Reset forzado PWRKEY 13s como ultimo recurso | Heredado |
| FIX-V5 | Deshabilitar PSM (AT+CPSMS=0) tras powerOn | Heredado |
| FIX-V6 | Deshabilitar BLE | Heredado |
| FIX-V7 | Gate de voltaje: verificar > 3.80V antes de modem | Heredado |
| FIX-V8 | GPS basado en NVS (primer ciclo solamente) | Heredado |
| FIX-V9 | N/A — era intervalo del autoswitch, eliminado | Removido |
| FIX-V10 | DISCARD_SAMPLES: 5 (sensor phase ~12s) | Heredado |
| FIX-V11 | ADC calibrado con analogReadMilliVolts (eFuse) | Heredado |
| FIX-V12 | buildFrame fallo → Sleep (no Error state) | Heredado |
| FIX-V13 | Buffer trim CR (.trim() en 5 ubicaciones) | Heredado |
| FIX-V14 | ICCID NVS cache con fallback | Heredado |
| FEAT-V2 | Logging diagnostico: banner, timing, resumen | Heredado |

### Limitacion conocida

Sin autoswitch, un modem en verdadero latch-up hardware (zombie) NO se puede recuperar por firmware. `esp_restart()` limpia el estado GPIO/UART del ESP32 y permite reintentar FIX-V4, pero no corta VBAT al SIM7080G. En caso de zombie persistente, los datos se bufferean en LittleFS hasta intervencion manual.

### Archivos modificados

- `fenix_1.0.ino` — Quitar autoswitch, agregar modemConsecFails
- `AppController.h` — Constantes zombie, version, AppConfig
- `AppController.cpp` — Banner, zombie recovery, modem gate
