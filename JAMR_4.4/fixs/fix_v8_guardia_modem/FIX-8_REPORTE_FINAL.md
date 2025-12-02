# FIX-8 - Guardia de salud del módem (anti estados zombie en rural)

## Estado

- Estado actual: **COMPLETADO EN LABORATORIO / EN ESPERA DE VALIDACIÓN EN CAMPO**.
- Código integrado en `JAMR_4.4/gsmlte.cpp` y tipos en `type_def.h`.

## Resumen de la solución

1. **Máquina de estados FIX-8** (`MODEM_HEALTH_*`):
	- `MODEM_HEALTH_OK` → operación normal.
	- `MODEM_HEALTH_TRYING` → primeros síntomas (timeout crítico único).
	- `MODEM_HEALTH_ZOMBIE_DETECTED` → ≥3 timeouts críticos sin progreso real.
	- `MODEM_HEALTH_RECOVERED` → se salió del estado zombie antes de agotar el ciclo.
	- `MODEM_HEALTH_FAILED` → se cancela el ciclo para proteger energía.

2. **Detección de estados zombie**:
	- Hooks añadidos en `startLTE`, `startLTE_autoLite`, `tcpOpen` y `tcpSendData` (CNACT, PDP, apertura de socket, envíos TCP y prompts `>`).
	- Cada timeout crítico llama a `modemHealthRegisterTimeout(contexto)` y se loguea con `[FIX-8]` para trazabilidad.

3. **Recuperación profunda acotada** (`modemHealthAttemptRecovery`):
	- Se ejecuta **una sola vez por ciclo** cuando se confirma `MODEM_HEALTH_ZOMBIE_DETECTED`.
	- Pasos: cierre de socket, `+CNACT=0,0`, `+CFUN=0`, delay con watchdog alimentado, `+CFUN=1`.
	- Si la recuperación tiene éxito se limpia el contador y se continúa; si falla se marca `MODEM_HEALTH_FAILED` y el flujo aborta ordenadamente.

4. **Abortos controlados**:
	- `modemHealthShouldAbortCycle(contexto)` ahora recibe el nombre del contexto que disparó la verificación.
	- Si la recuperación ya se intentó y falla (o no hay presupuesto), el ciclo termina sin bucles adicionales antes de entrar a deep sleep.

5. **Logs y métricas**:
	- Cada transición relevante emite mensajes `[FIX-8]` (primer timeout, detección zombie, intento de recuperación, recuperación exitosa o fallo definitivo).
	- `tcpSendData` marca éxito con `modemHealthMarkOk()` para reiniciar contadores tras tráfico real.

## Validación

- **Pruebas de laboratorio:**
  - Se forzaron fallos en `+CNACT`, `ensurePdpActive` y `+CAOPEN` simulando respuestas erróneas/timeouts.
  - Se verificó que sólo se intenta una recuperación profunda por ciclo y que respeta `COMM_CYCLE_BUDGET_MS`.
  - Después de la recuperación, AUTO_LITE y el flujo multi-perfil continúan sin necesidad de reiniciar el ESP32.
- **Pruebas en campo:**
  - **Pendientes.** Se requiere capturar al menos 20 ciclos en sitio rural para confirmar que los estados zombie reales se resuelven (o se abortan) sin agotar la batería.

## Cambios principales

- `gsmlte.cpp`
  - Nuevas variables: `g_modem_recovery_attempted`, `FIX8_DEFAULT_CONTEXT`.
  - Funciones: `modemHealthAttemptRecovery`, `modemHealthShouldAbortCycle(contexto)` (actualizada), llamados desde LTE y TCP.
  - `tcpSendData` ahora registra fallos y marca `modemHealthMarkOk()` en cada envío exitoso.
- `type_def.h`
  - Enum `modem_health_state_t` documentado como parte del payload de health.
- Documentación de FIX-8 actualizada (plan y este reporte).

## Riesgos / próximos pasos

- El consumo energético adicional durante la recuperación profunda es ~2–3 s extra de RF activa; sigue dentro del presupuesto del ciclo.
- Validaciones de campo pendientes; monitorear especialmente redes donde `+CNACT` queda en `DEACTIVE` recurrentemente.
- Evaluar en FIX futuro si conviene exponer contadores de estados zombie al backend para métricas de salud.
