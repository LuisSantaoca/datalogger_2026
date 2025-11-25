# FIX v4 – Operador Eficiente

Objetivo: implementar una lógica de selección de operador LTE/NB-IoT energéticamente eficiente,
que respete la prioridad absoluta de no perder datos:

1. Primero se guardan siempre los datos en buffer local.
2. Después se intenta la conexión usando un presupuesto de tiempo/energía por ciclo.
3. Se prueban operadores en un orden inteligente (último exitoso primero), sin quedarse
   pegados en uno solo cuando la señal es mala.

La implementación seguirá las `PREMISAS_DE_FIXS.md` y reutilizará toda la infraestructura
existente de watchdog, timeouts y buffer de datos.
