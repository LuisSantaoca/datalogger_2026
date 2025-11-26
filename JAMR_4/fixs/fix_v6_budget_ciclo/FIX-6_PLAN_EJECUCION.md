# FIX-6 - Plan de Ejecución

## Objetivo

Implementar un **límite global de tiempo** para el ciclo de comunicación LTE/TCP del datalogger JAMR_4, de forma compatible con FIX-3, FIX-4 y FIX-5.

## Alcance

- Afecta principalmente a `gsmlte.cpp` / `gsmlte.h`.
- Se integra con:
  - `setupModem` (punto de entrada del ciclo de comunicación).
  - `startLTE` / `startLTE_multiOperator`.
  - Funciones de envío TCP (`tcpOpenSafe`, `enviarDatos`, etc.).

## Pasos

1. **[x] Definir constante de presupuesto global**
   - Añadir en `gsmlte.h` una constante, por ejemplo:
     - `#define COMM_CYCLE_BUDGET_MS 150000UL` (ajustable según pruebas).
   - Documentar que este valor representa el **máximo tiempo permitido** para todo el ciclo de comunicación.

2. **[x] Registrar inicio de ciclo**
   - En `gsmlte.cpp`, dentro de `setupModem` (o la función que inicie la comunicación LTE/TCP), registrar `cycleStartMillis = millis()` en una variable global/estática.

3. **[x] Crear función auxiliar de chequeo de presupuesto**
   - Implementar una función, por ejemplo:
     - `bool cycleBudgetRemaining(const char* contextTag)`.
   - Comportamiento sugerido:
     - Calcular `elapsed = millis() - cycleStartMillis`.
     - Si `elapsed > COMM_CYCLE_BUDGET_MS`:
       - Log: `"[FIX-6] Presupuesto de ciclo agotado en " + String(contextTag)`.
       - Retornar `false`.
     - Si no, retornar `true`.

4. **[x] Integrar chequeo en puntos críticos**
   - En bucles de:
     - `startLTE_multiOperator` / `tryConnectOperator`.
     - Espera de registro LTE en `startLTE`.
     - Validación PDP (`ensurePdpActive`, si aplica).
     - Reintentos de TCP (`tcpOpenSafe`, `enviarDatos`).
   - Antes de cada iteración importante, llamar a `cycleBudgetRemaining`.
   - Si devuelve `false`, salir de la función con fallo controlado.

5. **[ ] Actualizar documentación y logs**
   - Asegurar que los mensajes de log incluyan etiquetas `[FIX-6]` cuando el presupuesto se agota.
   - Actualizar este plan marcando pasos completados.

6. **[ ] Pruebas y validación**
   - **Prueba urbana (buena señal)**:
     - Verificar que el ciclo termina con éxito **sin tocar** el límite.
     - Confirmar que los tiempos típicos quedan muy por debajo de `COMM_CYCLE_BUDGET_MS`.
   - **Prueba rural (mala señal)**:
     - Forzar un escenario de red mala/pérdida de servidor donde tradicionalmente el ciclo se alargaría mucho.
     - Confirmar que ahora aparece el log `[FIX-6]` y que el dispositivo pasa a deep sleep en un tiempo acotado.

7. **[ ] Reporte final**
   - Documentar resultados de pruebas en `FIX-6_REPORTE_FINAL.md`.
   - Incluir recomendaciones de valor por defecto para `COMM_CYCLE_BUDGET_MS` para producción.
