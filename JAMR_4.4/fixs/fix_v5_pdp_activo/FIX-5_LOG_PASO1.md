# FIX-5 — LOG PASO 1

## Objetivo de este paso
- Capturar el comportamiento actual (antes del cambio de código) en situaciones donde:
  - LTE se registra correctamente (CPSI OK).
  - `+CNACT=0,1` devuelve `OK` pero `+CNACT?` muestra IP `0.0.0.0` en todos los contextos.
  - `+CAOPEN` falla con `+CAOPEN: 0,1`.

## Evidencia usada
- Log `JAMR_4/fixs/fix_v4_operador_eficiente/logs/2025-11-24 19-45-54-215.txt`.
- Log `JAMR_4/fixs/fix_v2_conexion/logs/89883030000096466336_2025-10-31 10-43-50-763.txt`.

## Observaciones clave
- FIX-2 (v4.1.0, v4.2.1):
  - Siempre hay PDP activo (`+APP PDP: 0,ACTIVE` y `+CNACT: 0,1,"IP_REAL"`) antes de abrir sockets.
  - `+CAOPEN` responde `+CAOPEN: 0,0` y los envíos TCP son exitosos.
- FIX-4 (v4.1.1-JAMR4-TIMEOUT, log 2025-11-24):
  - LTE y RF correctas (`+CPSI` OK, señal 99).
  - `+CNACT=0,1` devuelve `OK` pero con `+APP PDP: 0,DEACTIVE`.
  - `+CNACT?` sólo muestra IP `0.0.0.0`.
  - Aun así, se intenta abrir socket 5 veces y `+CAOPEN` falla siempre (`+CAOPEN: 0,1`).

## Resultado del PASO 1
- Confirmado que existe una **ventana de fallo** en la que el firmware da por buena la fase de datos sin que el PDP esté realmente activo.
- Confirmado que la evidencia histórica (FIX-2) respalda el uso de `+CNACT?` como condición necesaria para abrir sockets.

## Próximo paso
- Implementar en código (FIX-5) una verificación explícita del estado PDP antes de `+CAOPEN`, integrada con el presupuesto de tiempo del FIX-4.
