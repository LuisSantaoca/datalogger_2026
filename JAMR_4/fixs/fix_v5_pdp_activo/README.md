# FIX-5 — Asegurar PDP activo antes de abrir sockets

Este fix añade una capa de protección sobre el flujo LTE existente (incluyendo FIX-3 y FIX-4) para **no abrir sockets TCP si el contexto PDP no está realmente activo**.

## Idea central
- No basta con que `+CNACT=0,1` responda `OK`.
- Se requiere que `+CNACT?` muestre al menos un contexto `,1,"X.X.X.X"` (IP real) antes de permitir `+CAOPEN`.

## Motivación
- En FIX-2, todos los envíos exitosos muestran esa condición previa.
- En el log problemático de FIX-4, nunca se alcanza esa condición y, aun así, se reintentan sockets hasta agotar reintentos.

## Integración
- FIX-5 se apoya en el presupuesto de tiempo de FIX-4 (multi-operador eficiente) para decidir:
  - Cuántos intentos de activación PDP hacer.
  - Cuándo cambiar de operador o abortar el envío preservando energía y watchdog.

Revisar:
- `FIX-5_ANALISIS_CODIGO.md`
- `FIX-5_PLAN_EJECUCION.md`
- `FIX-5_LOG_PASO1.md`
- `FIX-5_LOG_PASO2.md`
para el detalle técnico y el plan completo.

## Estado actual (24/11/2025)
- La validación de PDP activo se ejecuta antes de declarar éxito en `startLTE()` y está integrada con el presupuesto de FIX-4.
- Se añadieron logs `[FIX-5]` para cada resultado (éxito, reintento, agotamiento) y se documentó la evidencia en `FIX-5_LOG_PASO2.md`.
- Pendiente: capturar logs de banco/campo (Paso 3) para cerrar el fix y validar en hardware.
