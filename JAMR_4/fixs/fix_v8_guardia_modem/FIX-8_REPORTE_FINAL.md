# FIX-8 - Guardia de salud del módem (anti estados zombie en rural)

## Estado

- Estado actual: **EN DISEÑO / PENDIENTE DE IMPLEMENTACIÓN**.
- Este documento se completará una vez implementado y probado FIX-8.

## Resumen

FIX-8 busca añadir una capa explícita de "guardia de salud" sobre el módem SIM7080G para:

- Detectar estados zombie (responde AT pero no progresa en señal/PDP/TCP).
- Intentar una recuperación profunda **acotada** (una vez por ciclo).
- Cerrar el ciclo de forma ordenada cuando no hay recuperación, evitando bucles de reintento costosos en zonas rurales.

## Tareas pendientes

- Implementar la máquina de estados de salud del módem.
- Integrar los hooks de detección de "no progreso" en `gsmlte.cpp`.
- Añadir logs `[FIX-8]` en puntos clave (detección, recuperación, fallo).
- Ejecutar pruebas en laboratorio y en campo rural.
- Documentar resultados, métricas de mejora y evidencias de no aparición de estados zombie.

## Notas

- FIX-8 debe convivir con FIX-3/4/5/6/7, reutilizando los presupuestos de tiempo ya existentes y sin romper la lógica de perfiles LTE persistentes.
