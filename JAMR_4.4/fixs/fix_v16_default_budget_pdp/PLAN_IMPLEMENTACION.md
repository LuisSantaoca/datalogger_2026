# FIX-16: Plan de Implementación (DEFAULT_CATM budget + PDP activo)

Resumen
- Reducir presupuesto de DEFAULT_CATM a 65s.
- Aplicar validación activa de PDP durante la fase DEFAULT, similar a AUTO_LITE.

Cambios a realizar
1) Presupuesto
- Variable/punto donde se define el presupuesto de DEFAULT_CATM → ajustar a 65000ms.
- Mantener presupuesto LTE total y coherencia con watchdog.

2) Validación PDP en DEFAULT
- Reutilizar `checkPDPContextActive()` y `waitForPDPActivation(timeoutMs)`.
- Tras `+CNACT=0,1`, ejecutar `waitForPDPActivation(20000)` con checks periódicos (RSSI opcional, logs consistentes [FIX-14]).
- Si timeout: log `DEFAULT_CATM_pdp_timeout`, abortar DEFAULT y salir.

3) Logging y trazabilidad
- Añadir logs `[FIX-16]` en entrada/salida del perfil y en detección de PDP.
- Mantener `AUTO_LITE OK/FAIL` y `DEFAULT_CATM OK/FAIL` con tiempos medidos.

4) Versión
- Actualizar etiqueta en `JAMR_4.4/JAMR_4.4.ino` a `v4.4.16 default-budget-pdp`.

Pruebas
- Caso A: AUTO_LITE falla → DEFAULT activa PDP < 65s; TCP envía OK.
- Caso B: AUTO_LITE falla → DEFAULT sin PDP → timeout < 65s; ciclo acortado.
- Caso C: AUTO_LITE OK → DEFAULT no se usa; sin regresiones.

Riesgos
- Entornos con asignación IP lenta pueden necesitar más tiempo; se confía en próximos ciclos.
- Monitorizar con métricas de causa para ajuste fino si es necesario.
