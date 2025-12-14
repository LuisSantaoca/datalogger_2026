# FIX-16: Reducir presupuesto DEFAULT_CATM y validar PDP activo

Objetivo
- Acortar la duración de ciclo en entornos de cobertura variable cuando AUTO_LITE falla.
- Evitar esperas inútiles en DEFAULT_CATM si no hay PDP activo (sesión IP).

Motivación basada en logs
- Envío 1 (v4.4.15): AUTO_LITE sin PDP → fallback DEFAULT_CATM ≈45s; ciclo total ≈185.5s.
- Envío 2 (v4.4.15): AUTO_LITE con PDP rápido (≈3.7s) → ciclo total ≈164.2s.
- Conclusión: El peor caso está dominado por DEFAULT_CATM. Acotar su presupuesto y validar PDP reduce tiempo y energía.

Alcance del cambio
- Limitar presupuesto de `DEFAULT_CATM` a 60–70s (target: 65s).
- Integrar validación activa de PDP (consulta `+CNACT?` y bucle de espera con checks periódicos) igual que en AUTO_LITE.
- Mantener retrocompatibilidad de perfiles y señales de logging.

Criterios de aceptación
- Cuando AUTO_LITE falle, DEFAULT_CATM:
  - Realiza attach y activa PDP dentro del nuevo presupuesto.
  - Si PDP no se activa en el tiempo límite, aborta y registra causa: `DEFAULT_CATM_pdp_timeout`.
- Ciclo total en peor caso se reduce ≥15–25s respecto a estado actual.
- Sin regresiones: TCP envía correctamente cuando PDP activo; watchdog sin disparos.

Riesgos y mitigaciones
- Cobertura marginal puede necesitar más de 70s → mitigar con reintento en siguiente ciclo y persistencia de perfil LTE.
- Operador lento en asignar IP → detectar explícitamente PDP no activo y salir (ahorro de energía).

Trazabilidad a requisitos
- REQ-001 Modem State Management: Validar estado útil (PDP) antes de gastar presupuesto.
- REQ-002 Watchdog Protection: Evitar loops largos que acerquen al límite.
- REQ-003 Health Diagnostics: Logging de causa y tiempos.
- REQ-004 Versioning: Incrementar etiqueta a `v4.4.16 default-budget-pdp`.

Plan resumido
- Ajustar constantes de presupuesto DEFAULT.
- Reutilizar funciones `checkPDPContextActive()` y `waitForPDPActivation()`.
- Insertar bucle de validación PDP en ruta DEFAULT.
- Actualizar versión y mensajes de log.
