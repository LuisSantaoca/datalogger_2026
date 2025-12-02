# FIX-7 - Análisis de Código (Perfil LTE persistente)

## Contexto

El firmware JAMR_4 ya cuenta con:

- FIX-3: loops defensivos y protección de watchdog.
- FIX-4: lógica multi-perfil de operador (`OperatorProfile`, `startLTE_multiOperator`).
- FIX-5: verificación explícita de PDP activo antes de abrir sockets TCP.
- FIX-6: presupuesto global de tiempo para el ciclo de comunicación LTE/TCP.

Actualmente, la tabla de perfiles (`OP_PROFILES`) permite definir distintos escenarios (por ejemplo, CAT-M principal y NB-IoT como fallback), y `startLTE_multiOperator()` reparte el presupuesto `LTE_CONNECT_BUDGET_MS` entre ellos.

Además, existe una variable en RAM:

- `g_last_success_operator_id` que recuerda el último perfil que logró conexión LTE dentro de **un mismo ciclo de encendido**.

Limitación detectada:

- `g_last_success_operator_id` **no es persistente**: tras un deep sleep o reinicio, se pierde y se vuelve al orden por defecto de perfiles.
- En sitios donde un perfil es claramente mejor, cada ciclo vuelve a probar perfiles menos probables antes de llegar al ganador, gastando tiempo y energía innecesariamente.

## Problema a resolver

- Cada ciclo de comunicación (despertar → medir → enviar → dormir) empieza con el mismo orden de perfiles LTE, aunque la experiencia previa en el sitio indique que un perfil específico suele ser exitoso.
- Esto aumenta el **tiempo promedio** de conexión LTE, especialmente cuando hay varios perfiles configurados.

## Objetivo del FIX-7

- Introducir una **memoria persistente mínima** que recuerde el `id` del último `OperatorProfile` exitoso.
- Utilizar esa información para **priorizar ese perfil en el siguiente ciclo**, incluso después de deep sleep.
- No alterar:
  - Los límites de tiempo (`LTE_CONNECT_BUDGET_MS`, `COMM_CYCLE_BUDGET_MS`).
  - La lógica de PDP activo (FIX-5).
  - La lógica de corte global de ciclo (FIX-6).

## Consideraciones de diseño

- Persistencia:
  - Usar LittleFS (ya inicializado en `setupModem`) para almacenar un archivo simple de configuración.
  - Ejemplo: archivo `"/lte_profile.cfg"` con un entero (`0`, `1`, `2`, ...) en texto.
- Robustez:
  - Validar que el valor leído esté en rango (`< OP_PROFILES_COUNT`).
  - En caso de error, ignorar el archivo y usar el orden por defecto.
- Seguridad energética:
  - No modificar `LTE_CONNECT_BUDGET_MS` ni `COMM_CYCLE_BUDGET_MS`.
  - No cambiar la forma en que FIX-4/FIX-6 cortan los intentos; solo el orden de prueba.

## Hipótesis de impacto

- En sitios estables (misma celda / mismo operador ganador):
  - Disminuye el tiempo promedio para lograr LTE+PDP, al probar primero el perfil ganador histórico.
- En sitios cambiantes:
  - El sistema puede adaptarse en pocos ciclos a un nuevo perfil ganador.
- En todos los casos:
  - El tiempo **máximo** sigue acotado por FIX-4 y FIX-6; no aumenta el consumo energético en casos peores.

## Puntos de integración

- `gsmlte.cpp`:
  - Uso actual de `g_last_success_operator_id` en `buildOperatorOrder`.
  - Lugar donde se marca éxito de un perfil (`startLTE_multiOperator` / `tryConnectOperator`).
  - Punto de inicialización del sistema de archivos y del ciclo (`setupModem`).

Estos serán los sitios principales a modificar en el plan de ejecución.
