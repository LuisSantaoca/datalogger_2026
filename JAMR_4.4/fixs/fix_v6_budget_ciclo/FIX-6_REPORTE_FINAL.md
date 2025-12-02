# FIX-6 - Reporte Final (pendiente)

> Este documento se completará una vez implementado el fix en código y ejecutadas las pruebas en campo (urbano y rural).

## Resumen ejecutivo

- **Objetivo:** Limitar el tiempo total del ciclo de comunicación LTE/TCP mediante un presupuesto global `COMM_CYCLE_BUDGET_MS`.
- **Estado:** Pendiente de implementación y pruebas.

## Evidencias previstas

- Logs urbanos donde el ciclo termina con éxito sin alcanzar el límite.
- Logs rurales donde el límite se alcanza y el sistema corta el ciclo de forma ordenada.

## Conclusiones preliminares

- Se espera una mejora significativa en control de consumo energético y previsibilidad del comportamiento del datalogger en escenarios adversos.
