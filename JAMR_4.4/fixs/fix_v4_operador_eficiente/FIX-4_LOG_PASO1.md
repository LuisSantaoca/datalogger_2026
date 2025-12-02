# FIX-4: Log Paso 1

## 2025-11-24 – Implementación inicial del operador eficiente

1. **Bandera y estructura base (`gsmlte.h`)**
	- Se agregó `ENABLE_MULTI_OPERATOR_EFFICIENT`, el presupuesto global `LTE_CONNECT_BUDGET_MS`
	  y la estructura `OperatorProfile` para describir cada perfil.

2. **Perfiles y estado (`gsmlte.cpp`)**
	- Tabla `OP_PROFILES` con dos perfiles iniciales: `DEFAULT_CATM` y `NB_IOT_FALLBACK`.
	- Variables para recordar el último perfil exitoso, contadores de fallos y el límite dinámico `g_lte_max_wait_ms`.

3. **Adaptación de `startLTE`**
	- Ahora respeta `g_lte_max_wait_ms` para que cada intento tenga un límite propio sin modificar el resto del flujo.

4. **Funciones nuevas**
	- `buildOperatorOrder`, `tryConnectOperator` y `startLTE_multiOperator` implementan el bucle de presupuesto por operador con logging `[FIX-4]`.

5. **Integración en `setupModem`**
	- Se introduce `lteOk = startLTE_multiOperator()` cuando el flag está activo, manteniendo intacta la secuencia de buffer → LTE/TCP → envío.

Con esto, FIX-4 queda listo para pruebas de laboratorio/campo antes de ajustar perfiles o presupuestos.
