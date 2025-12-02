# FIX-3: Log Paso 1

## 2025-11-24 – Implementación inicial en código

1. **Feature flag defensivo** (`gsmlte.h`)
	- Se agregó `#define ENABLE_WDT_DEFENSIVE_LOOPS true` con comentarios que describen su propósito.
	- Permite activar/desactivar toda la lógica de protección sin eliminar el código legado.

2. **Lecturas largas de AT** (`gsmlte.cpp` → `readResponse` y `sendATCommand`)
	- Se añadió cronómetro `lastFeed` y, cada ~150 ms de lectura continua desde `SerialAT`, se ejecuta `esp_task_wdt_reset()` y `delay(1)`.
	- Este comportamiento se encapsuló con `#if ENABLE_WDT_DEFENSIVE_LOOPS` para mantener la trazabilidad del fix.

3. **Timeouts internos en GPS** (`gsmlte.cpp` → `getGpsSim`)
	- Se introdujo timeout por intento (`GPS_SINGLE_ATTEMPT_TIMEOUT_MS = 5000`) y timeout global (`GPS_TOTAL_TIMEOUT_MS = 80000`).
	- Cada intento ahora ejecuta un mini-loop que alimenta el watchdog y cede CPU mientras reintenta `modem.getGPS`.
	- Se añadió log `[FIX-3]` cuando se alcanza el timeout total antes de completar los 50 intentos.

4. **Documentación**
	- Este archivo actualizado para reflejar los cambios del Paso 1 (feature flag, AT defensivo y GPS protegido).
