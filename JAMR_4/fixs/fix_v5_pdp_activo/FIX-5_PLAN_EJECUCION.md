# FIX-5 — PLAN DE EJECUCIÓN

## Objetivo
Asegurar que **sólo se intenten aperturas de socket TCP cuando exista al menos un contexto PDP activo con IP real**, y coordinar esta verificación con el presupuesto de tiempo de FIX-4 (multi-operador eficiente).

## Principios y buenas prácticas
- **Fail-fast inteligente**: si `CNACT?` indica que no hay PDP activo (0.0.0.0 en todos los contextos), no abrir sockets; gestionar el fallo en un punto único y controlado.
- **Condición necesaria explícita**: 
  - Requisito mínimo para abrir socket: `+APP PDP: X,ACTIVE` y/o al menos una línea `+CNACT: N,1,"X.X.X.X"`.
- **Integración con FIX-4**:
  - Reutilizar el presupuesto de tiempo ya definido en `ENABLE_MULTI_OPERATOR_EFFICIENT`.
  - Permitir que el fallo de PDP sea un criterio más para cambiar de perfil de operador.
- **No degradar casos buenos**:
  - Mantener exactamente el comportamiento actual cuando el PDP se activa correctamente (mismos tiempos, mismos comandos AT).
- **Observabilidad**:
  - Añadir logs claros tipo `[FIX-5]` para distinguir fallos de PDP de fallos de socket.

## Pasos de ejecución

### Paso 1 — Análisis detallado del flujo actual
- [x] Revisar logs de FIX-2 (v4.1.0, v4.2.1) donde el envío TCP es exitoso.
- [x] Revisar logs de FIX-4 donde `+CAOPEN` falla.
- [x] Identificar el punto exacto en `gsmlte.cpp` donde se llama a `+CNACT` y luego a `+CAOPEN`.

### Paso 2 — Definir API interna para verificación de PDP
- [x] Diseñar utilidades dedicadas en `gsmlte.cpp`:
  - `captureCNACTResponse`, `extractActivePdpContext` y `ensurePdpActive`.
- [x] Responsabilidades implementadas:
  - Reforzar `+CNACT=0,1` dentro de un presupuesto acotado (`PDP_VALIDATION_*`).
  - Parsear la respuesta de `+CNACT?` y detectar IP distinta de `0.0.0.0`.
  - Reintentar de forma controlada alimentando el watchdog y registrando `[FIX-5]` en cada paso.

### Paso 3 — Integración con FIX-4 (multi-operador)
- [x] Integrar `ensurePdpActive` dentro de `startLTE()` (invocado por la lógica multi-perfil).
  - Se evalúa el presupuesto restante (`remaining`) de `LTE_CONNECT_BUDGET_MS` y se invoca la validación antes de declarar la LTE como exitosa.
- [x] Si la validación falla:
  - Se registra `[FIX-5] Registro LTE sin PDP activo, reintentando dentro del presupuesto`.
  - El ciclo `startLTE` continúa hasta agotar presupuesto, devolviendo `false` para que FIX-4 pruebe otro perfil.

### Paso 4 — Logging y trazabilidad
- [x] Nuevos mensajes `[FIX-5]` inyectados en `gsmlte.cpp` (detección, ausencia y agotamiento de PDP).
- [x] Validado que siguen el mismo estilo de FIX-2/FIX-4 y se apoyan en `logMessage` para conservar timestamps.

### Paso 5 — Validación en banco y campo
- [ ] Pruebas de banco:
  - Caso A: entorno donde `+CNACT?` devuelve PDP activo → confirmar que el flujo es idéntico al de FIX-4 actual (sin penalizar tiempos).
  - Caso B: entorno donde `+CNACT?` permanece en 0.0.0.0:
    - Confirmar que **no** se intenta `+CAOPEN`.
    - Confirmar que los datos quedan en buffer y que el módem entra en deep sleep de forma ordenada.
- [ ] Pruebas de campo (rural):
  - Capturar al menos 3 ciclos de log con señal buena, media y baja.
  - Verificar que los cambios no afectan negativamente al consumo ni a la estabilidad del watchdog.

## Criterios de aceptación
- No se llama a `+CAOPEN` si `+CNACT?` no muestra un contexto `,1,"X.X.X.X"`.
- En escenarios donde antes FIX-4 funcionaba bien, FIX-5 mantiene el mismo comportamiento (mismos tiempos de conexión y número de reintentos).
- En escenarios con PDP inactivo crónico (0.0.0.0), el equipo:
  - No entra en bucles largos de apertura/cierre de sockets.
  - Protege el watchdog y el consumo energético.
  - Deja trazas claras de que el fallo es de PDP/entorno de red.
