# FIX-6: Límite global de tiempo de ciclo de comunicación

Este fix introduce un **presupuesto global de tiempo** para todo el ciclo de comunicación LTE/TCP del datalogger JAMR_4.

## Objetivo

- Garantizar que cada ciclo de medición + envío tenga un **tiempo máximo acotado** (batería bajo control).
- Evitar que en zonas rurales con red muy mala el firmware pase **minutos** intentando conectar antes de dormir.
- Integrarse con FIX-3/FIX-4/FIX-5 como "cinturón de seguridad" final: si pese a watchdog, multi-operador y PDP activo no se logra enviar, se corta el ciclo de forma ordenada.

## Idea general

- Definir una constante de configuración (por ejemplo `COMM_CYCLE_BUDGET_MS`, 120-180s típicos).
- Medir el tiempo al inicio del ciclo de comunicación.
- En los bucles críticos de LTE (registro, multi-operador) y de TCP (CAOPEN/envío/reintentos), verificar que el tiempo consumido no exceda ese presupuesto.
- Si se agota el presupuesto:
  - Registrar un log claro (ej. `"[FIX-6] Abortando ciclo LTE/TCP por presupuesto agotado"`).
  - Salir de las funciones de conexión/envío con fallo controlado.
  - Permitir que la lógica superior decida ir a deep sleep.

## Estado actual

- [x] Carpeta de fix creada y documentación base.
- [x] Análisis detallado de código (`FIX-6_ANALISIS_CODIGO.md`).
- [x] Plan de ejecución (`FIX-6_PLAN_EJECUCION.md`).
- [x] Implementación inicial en código (`gsmlte.cpp` / `gsmlte.h`).
- [ ] Logs de validación (zona urbana y zona rural).
- [ ] Reporte final (`FIX-6_REPORTE_FINAL.md`).
