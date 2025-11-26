# FIX-7: Perfil LTE persistente por sitio

Este fix introduce una memoria ligera para recordar **qué perfil de operador LTE** funcionó por última vez, de forma que en el siguiente ciclo se pruebe **primero** ese perfil dentro del mismo presupuesto de tiempo y energía (FIX-4/FIX-6).

## Objetivo

- Reducir el **tiempo promedio de conexión LTE** en sitios donde siempre gana el mismo perfil de operador.
- Aprovechar mejor los ciclos de comunicación en zonas rurales sin aumentar el tiempo máximo ni el consumo energético.
- Mantener intactas las protecciones existentes:
  - FIX-3: watchdog y loops defensivos.
  - FIX-4: presupuesto LTE multi-perfil.
  - FIX-5: validación de PDP activo.
  - FIX-6: presupuesto global de ciclo de comunicación.

## Idea general

- Añadir un pequeño archivo en LittleFS (por ejemplo `"/lte_profile.cfg"`).
- Guardar en ese archivo el `id` del último `OperatorProfile` que logró LTE+PDP activo.
- Al inicio de `startLTE_multiOperator`, leer ese valor y **reordenar la lista de perfiles** para que ese id se pruebe primero.
- Si el archivo no existe, está corrupto o el id no es válido, se usa el orden por defecto actual.

## Estado actual

- [x] Carpeta de fix creada.
- [x] Análisis detallado de código (`FIX-7_ANALISIS_CODIGO.md`).
- [x] Plan de ejecución (`FIX-7_PLAN_EJECUCION.md`).
- [x] Implementación en código (`gsmlte.cpp` / `gsmlte.h`).
- [ ] Logs de validación (ciclos consecutivos en el mismo sitio, cambio de sitio).
- [ ] Reporte final (`FIX-7_REPORTE_FINAL.md`).
