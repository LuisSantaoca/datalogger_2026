# FIX-5 — LOG PASO 2

## Objetivo del paso
Implementar la verificación de PDP activo directamente en `gsmlte.cpp`, integrándola con el presupuesto de FIX-4 y dejando trazas `[FIX-5]` que faciliten el análisis de logs posteriores.

## Cambios realizados
- Archivo: `JAMR_4/gsmlte.cpp`.
  - Nuevas utilidades: `captureCNACTResponse`, `extractActivePdpContext` y `ensurePdpActive`.
  - Se restringe el uso de `+CNACT?` a un bloque controlado que parsea las IPs devueltas y valida que no sean `0.0.0.0`.
  - `startLTE()` ahora invoca `ensurePdpActive()` antes de declarar exitosa la conexión y sólo continúa cuando existe un contexto `,1,"IP_REAL"`.
  - Se añadieron logs `[FIX-5]` para: respuesta incompleta, intentos sin PDP, detección de IP válida y agotamiento de presupuesto.

## Evidencia
- No se cuenta aún con logs de hardware con el nuevo firmware; la verificación se realizó por inspección del código y revisión de la salida esperada.
- Se espera observar en campo mensajes como:
  - `[FIX-5] Validando PDP activo (presupuesto XXXXms)`.
  - `[FIX-5] PDP activo detectado en contexto N (IP aaa.bbb.ccc.ddd)`.
  - `[FIX-5] Registro LTE sin PDP activo, reintentando dentro del presupuesto` (cuando el módem se registra pero el PDP no levanta).

## Próximos pasos
1. Generar un log de banco donde se vea la secuencia completa (`ensurePdpActive` → `+CAOPEN`) para confirmar que no hay regresiones.
2. Capturar al menos un log en campo rural donde se reproduzca el fallo original y confirmar que ahora se evita abrir sockets cuando el PDP sigue en 0.0.0.0.
3. Documentar los resultados en `FIX-5_LOG_PASO3.md` una vez obtenida la evidencia.
