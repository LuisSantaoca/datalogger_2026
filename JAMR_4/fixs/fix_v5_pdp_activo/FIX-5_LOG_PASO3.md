# FIX-5 — LOG PASO 3

## Objetivo del paso
Validar en hardware que la verificación de PDP activo se ejecuta antes de abrir sockets y que, una vez detectada una IP válida, el flujo de FIX-4 continúa enviando datos sin errores.

## Evidencia
- Archivo: `logs/2025-11-24_20-07-50_FIX5.txt`.
- Firmware: `v4.1.1-JAMR4-TIMEOUT` con FIX-3, FIX-4 y FIX-5 habilitados.

### Hallazgos relevantes del log
1. `+CNACT=0,1` → `+APP PDP: 0,ACTIVE`, seguido del bloque `[FIX-5] Validando PDP activo (presupuesto 20000ms)`.
2. Primer reforzamiento de `+CNACT=0,1` devuelve `ERROR`, pero `+CNACT?` muestra `+CNACT: 0,1,"100.116.56.23"`.
3. `[FIX-5] PDP activo detectado en contexto 0 (IP 100.116.56.23)` antes de declarar `✅ Conectado a la red LTE con PDP activo`.
4. `+CAOPEN` responde `+CAOPEN: 0,0` y se envían 5 paquetes de 108 bytes; buffer queda en 0 líneas.
5. Todo el ciclo termina en 146 s con watchdog alimentado y sin fallos.

## Conclusión
- FIX-5 cumple su objetivo: no se abre el socket hasta confirmar que el PDP tiene IP distinta de `0.0.0.0`.
- En escenarios con buena señal, el overhead es mínimo (unos segundos adicionales dentro del presupuesto). En caso de `+CNACT=0,1` errático, el sistema queda protegido y deja trazas `[FIX-5]` para diagnóstico.

## Pendientes
- Capturar un log adverso donde `+CNACT?` permanezca en `0.0.0.0` y verificar que el dispositivo evita `+CAOPEN` (sirve como prueba negativa).
- Integrar estas trazas en el reporte final de FIX-5 una vez completadas las pruebas de campo.
