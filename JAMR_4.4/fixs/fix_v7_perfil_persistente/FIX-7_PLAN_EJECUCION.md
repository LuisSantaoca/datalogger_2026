# FIX-7 - Plan de Ejecución (Perfil LTE persistente)

## Objetivo

Implementar una memoria persistente mínima para recordar el `id` del último `OperatorProfile` exitoso y priorizarlo en el siguiente ciclo, respetando FIX-3, FIX-4, FIX-5 y FIX-6.

## Alcance

- Archivos afectados:
  - `JAMR_4/gsmlte.cpp`
  - `JAMR_4/gsmlte.h` (si se declara alguna constante o helper público)
- No se modifican:
  - Presupuestos de tiempo (`LTE_CONNECT_BUDGET_MS`, `COMM_CYCLE_BUDGET_MS`).
  - Lógica de PDP activo (FIX-5).
  - Lógica de corte de ciclo (FIX-6).

## Pasos

1. **[x] Definir ruta de archivo y helpers**
   - Definir una constante para la ruta del archivo, por ejemplo:
     - `#define LTE_PROFILE_FILE "/lte_profile.cfg"`
   - Declarar (si es necesario) helpers internos para:
     - Leer `int8_t` desde el archivo.
     - Escribir `int8_t` al archivo.

2. **[x] Cargar perfil persistente al inicio**
   - En `setupModem` (después de montar LittleFS) o justo antes de llamar a `startLTE_multiOperator`:
     - Intentar abrir `LTE_PROFILE_FILE` en modo lectura.
     - Leer un entero (`id`) y validar que `0 <= id < OP_PROFILES_COUNT`.
     - Si es válido, asignar a `g_last_success_operator_id`.
     - Loguear algo como:
       - `"[FIX-7] Perfil LTE persistente cargado: id=X"`.

3. **[x] Actualizar perfil persistente en caso de éxito**
   - En `startLTE_multiOperator`, cuando un perfil logra conexión (antes de `return true;`):
     - Ya se asigna `g_last_success_operator_id = profile.id;`.
     - Añadir:
       - Abrir `LTE_PROFILE_FILE` en modo escritura.
       - Guardar `profile.id` en texto.
       - Cerrar archivo.
       - Log: `"[FIX-7] Perfil LTE persistente actualizado a id=X"`.

4. **[x] Mantener compatibilidad con `buildOperatorOrder`**
   - Verificar que `buildOperatorOrder` ya utiliza `g_last_success_operator_id` para mover al frente el último perfil exitoso.
   - Confirmar que este comportamiento ahora funcionará también **entre ciclos** gracias al archivo persistente.

5. **[x] Actualizar documentación y logs**
   - Marcar pasos completados en este plan.
   - Actualizar `README.md` del fix con el estado de implementación.
   - Preparar un lugar para logs de prueba (`logs/`), si es necesario.

6. **Pruebas y validación**
   - Escenario A (sitio estable):
     - Ciclo 1: dejar que se conecte con algún perfil; verificar que se crea/actualiza `lte_profile.cfg`.
     - Ciclo 2: comprobar en logs que `startLTE_multiOperator` usa ese perfil primero.
   - Escenario B (cambio de sitio):
     - Llevar el equipo a un sitio donde el perfil guardado ya no funciona, pero otro sí.
     - Verificar que tras uno o más ciclos, el nuevo perfil exitoso queda almacenado y pasa a ser el primero.

7. **Reporte final**
   - Documentar resultados de pruebas en `FIX-7_REPORTE_FINAL.md`.
   - Evaluar el impacto en tiempo medio de conexión y consumo estimado.
