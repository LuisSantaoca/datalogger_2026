# FIX-5 — PDP ACTIVO ANTES DE SOCKET

## Contexto
- Firmware base: JAMR_4 v4.1.1 / v4.2.1 con mejoras de watchdog (FIX-3) y operador eficiente (FIX-4).
- Módem: SIM7080G, red LTE CAT-M1.
- Problema observado en FIX-4: LTE se registra correctamente (CPSI OK), pero `+CAOPEN` falla sistemáticamente con `+CAOPEN: 0,1`.

## Hallazgos principales
- En logs de FIX-2 (v4.1.0, v4.2.1), siempre que el envío TCP es exitoso se observa la secuencia:
  - `AT+CNACT=0,1` → `+APP PDP: 0,ACTIVE`.
  - `AT+CNACT?` → al menos un contexto con `1,"X.X.X.X"` (IP real, distinta de 0.0.0.0).
  - Sólo después se abre socket: `+CAOPEN=0,0,...` → `+CAOPEN: 0,0` y datos enviados.
- En el log de FIX-4 problemático:
  - `AT+CNACT=0,1` → `+APP PDP: 0,DEACTIVE`.
  - `AT+CNACT?` repetido → **todos** los contextos en `0,0,"0.0.0.0"`.
  - Aun así, la lógica de firmware marca `✅ Conectado a la red LTE` y llama a `+CAOPEN`, que devuelve `+CAOPEN: 0,1` en todos los reintentos.

## Causa raíz a nivel firmware
- La lógica de control de datos es **optimista**:
  - Trata el `OK` de `+CNACT=0,1` como éxito suficiente sin validar el estado real del PDP.
  - No usa `+CNACT?` como condición dura para avanzar hacia `+CAOPEN`.
- Resultado:
  - El firmware intenta abrir sockets TCP cuando el módem aún está con **IP 0.0.0.0** (PDP no activado realmente).
  - El módem responde con error genérico en `+CAOPEN`, y los cierres `+CACLOSE` también fallan porque no hay socket válido.

## Hipótesis confirmada con logs
- En todos los casos buenos (FIX-2):
  - `+APP PDP: 0,ACTIVE` **y** `+CNACT: 0,1,"IP_REAL"` **preceden** a los sockets.
- En el caso malo (FIX-4):
  - Nunca aparece un contexto `1,"IP_REAL"` en `+CNACT?`.
- Deducción: usar "PDP activo con IP ≠ 0.0.0.0" como condición previa es **coherente con el comportamiento sano histórico del sistema**.

## Objetivo del FIX-5
- Introducir una capa de protección en firmware para asegurar que **no se intenta abrir sockets TCP si el PDP no está realmente activo**.
- Integrar esta protección con el presupuesto de tiempo de FIX-4 (multi-operador eficiente) para:
  - Reintentar activación de PDP dentro de un presupuesto.
  - Si no se logra PDP activo, cambiar de perfil de operador o abortar el envío almacenando datos en buffer.

## Riesgos mitigados
- Evitar bucles de reintento de sockets sobre un PDP inactivo.
- Reducir consumo energético inútil al no insistir con `+CAOPEN` cuando `CNACT?` muestra 0.0.0.0.
- Mejorar la capacidad de diagnóstico: se podrá distinguir claramente entre:
  - "LTE registrado pero sin PDP activo".
  - "LTE y PDP activos, pero fallo posterior (servidor/red externa)".

## Implementación realizada (24/11/2025)
- Nuevas funciones estáticas en `JAMR_4/gsmlte.cpp` (`captureCNACTResponse`, `extractActivePdpContext`, `ensurePdpActive`).
- `startLTE()` exige que `ensurePdpActive()` retorne `true` antes de declarar exitosa la conexión y sólo entonces emite `✅ Conectado a la red LTE con PDP activo`.
- Se limitó el presupuesto de validación (`PDP_VALIDATION_MIN/MAX`) para no romper la integración con FIX-4 y se añadieron logs `[FIX-5]` en cada ruta crítica.
