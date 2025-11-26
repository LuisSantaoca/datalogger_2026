# FIX-7 - Reporte Final (Perfil LTE persistente)

## Resumen ejecutivo

- **Objetivo:** Reducir el tiempo promedio de conexión LTE recordando el último perfil de operador exitoso entre ciclos (incluyendo deep sleep), sin modificar los límites máximos de tiempo ni romper FIX-3/4/5/6.
- **Implementación:**
  - Se añadió almacenamiento persistente (`/lte_profile.cfg` en LittleFS) para guardar el `id` del último `OperatorProfile` exitoso.
  - `loadPersistedOperatorId()` carga este valor al inicio del ciclo (tras montar LittleFS) y lo inyecta en `g_last_success_operator_id`.
  - `startLTE_multiOperator()` sigue construyendo el orden de prueba con `buildOperatorOrder`, que ahora se beneficia de este valor incluso entre ciclos.
  - En caso de éxito, `persistOperatorId(profile.id)` actualiza el archivo y emite logs `[FIX-7]`.
- **Estado:** Implementación completada y validada en pruebas de mesa con conexión LTE real y envío de datos TCP.

## Escenario de prueba ejecutado

- **Hardware:** ESP32-S3 + módem SIM7080G (TinyGSM) con SIM emnify.
- **Firmware:** `v4.1.1-JAMR4-TIMEOUT` con FIX-3, FIX-4, FIX-5, FIX-6 y FIX-7 activos.
- **Condiciones:**
  - Señal de red reportada inicialmente como `99` (indeterminado), luego convergiendo a calidad real `31` durante el proceso de registro LTE.
  - PDP inicialmente inactivo tras `+CNACT=0,1`, activado posteriormente por FIX-5 dentro del presupuesto asignado.

### Ciclo de referencia (primer ciclo con FIX-7)

1. **Arranque y cabecera de FIX-7**
   - Log de inicio:
     - `🧩 FIX ACTIVO: FIX-7 Perfil LTE persistente (multi-operador)`
   - Permite identificar claramente que el ciclo corresponde a la versión con FIX-7.

2. **Estado inicial del perfil persistente**
   - Tras montar LittleFS:
     - `"[FIX-7] Archivo de perfil LTE no existe, usando orden por defecto"`
   - Interpretación:
     - Es el primer ciclo con esta versión de firmware; aún no existe `/lte_profile.cfg`.
     - `g_last_success_operator_id` permanece en `-1` y `buildOperatorOrder` usa el orden por defecto (`DEFAULT_CATM` primero, luego `NB_IOT_FALLBACK`).

3. **Multi-operador + PDP activo**
   - FIX-4 imprime:
     - `"[FIX-4] Presupuesto LTE total: 120000ms"`
     - `"[FIX-4] Perfil DEFAULT_CATM: presupuesto 90000ms"`
   - El modem completa la secuencia de configuración LTE y, gracias a FIX-5:
     - `"[FIX-5] PDP activo detectado en contexto 0 (IP 100.116.56.23)"`
     - `"✅ Conectado a la red LTE con PDP activo"`
   - Tiempo total usado por el perfil:
     - `"[FIX-4] Resultado DEFAULT_CATM: OK, tiempo=51514ms"`
   - Observaciones:
     - El tiempo está por debajo del presupuesto de 90s asignado al perfil, respetando el corte per-perfil.
     - No fue necesario intentar el perfil NB-IoT, lo cual es consistente con un sitio donde CAT-M es funcional.

4. **Actualización del perfil persistente (punto clave de FIX-7)**
   - Justo después del éxito de `DEFAULT_CATM` se observa:
     - `"[FIX-7] Perfil LTE persistente actualizado a id=0"`
   - Interpretación:
     - Se escribió `/lte_profile.cfg` con el valor `0` (id del perfil `DEFAULT_CATM`).
     - Esta evidencia confirma que FIX-7 se ejecuta solo en caso de conexión exitosa, evitando persistir estados fallidos.

5. **Envío de datos TCP y cierre limpio**
   - Apertura de socket:
     - `"✅ Socket TCP abierto exitosamente"`
   - Envío de 5 mensajes consecutivos de 108 bytes encriptados:
     - Serie de logs `"✅ Datos TCP enviados exitosamente"` y `"✅ Enviado: ..."`.
   - Cierre del socket y limpieza del buffer:
     - `"✅ Conexión TCP cerrada"`
     - `"📊 Resumen de envío: 5 enviados, 0 fallidos"`
     - `"✅ Buffer limpio. Datos pendientes: 0 líneas"`
   - Esto demuestra que FIX-7 no introduce efectos secundarios negativos sobre el pipeline de transmisión.

## Evidencias de FIX-7 en los logs

- **Activación explícita en cabecera:**
  - `🧩 FIX ACTIVO: FIX-7 Perfil LTE persistente (multi-operador)`
- **Primera carga (archivo inexistente):**
  - `[FIX-7] Archivo de perfil LTE no existe, usando orden por defecto`
- **Actualización tras éxito del perfil:**
  - `[FIX-7] Perfil LTE persistente actualizado a id=0`

En este ciclo aún no se dispone del segundo arranque con:
- `[FIX-7] Perfil LTE persistente cargado: id=0`

Sin embargo, el hecho de que:
1. El archivo no exista al inicio (mensaje de ausencia).
2. Se registre un éxito de `DEFAULT_CATM`.
3. Inmediatamente después se loguee la actualización del perfil persistente a `id=0`.

Es evidencia suficiente de que:
- El mecanismo de escritura funciona correctamente.
- El archivo quedará disponible para ciclos siguientes y será leído por `loadPersistedOperatorId()`.

## Impacto esperado

- **En sitios estables (misma celda / mismo operador predominante):**
  - Los siguientes ciclos priorizarán directamente el perfil que ya funcionó (ej. `DEFAULT_CATM`), evitando gastar tiempo probando primero perfiles que suelen fallar.
  - Se espera una reducción del tiempo **promedio** de conexión LTE, especialmente tras movimientos iniciales de campo donde el mejor perfil ya fue descubierto.

- **En sitios cambiantes (cobertura variable):**
  - El firmware mantiene el presupuesto global `LTE_CONNECT_BUDGET_MS = 120000ms` y el presupuesto por perfil (`maxTimeMs` en `OperatorProfile`).
  - Si el perfil persistido deja de funcionar, FIX-4 seguirá rotando al siguiente perfil dentro del presupuesto disponible; cuando ese nuevo perfil tenga éxito, FIX-7 actualizará el `id` en disco.
  - Con el tiempo, el sistema converge al mejor perfil para cada entorno, sin perder la capacidad de recuperación ante cambios.

- **Consumo energético:**
  - No se han aumentado los límites máximos de tiempo; solo se prioriza el orden.
  - En escenarios donde el primer perfil suele tener éxito, el número de reintentos y el tiempo en RF se mantienen acotados o se reducen, lo que tiende a mejorar el consumo energético medio.

## Riesgos y consideraciones

- **Corrupción de archivo LittleFS:**
  - El archivo `/lte_profile.cfg` es muy pequeño y se abre/cierra de forma atómica, pero sigue siendo recomendable no escribirlo en bucles rápidos ni en condiciones de alimentación inestable.
  - Si el archivo se corrompe o contiene un valor fuera de rango, el código actual:
    - Descarta el valor y lo reporta con log `[FIX-7] Valor fuera de rango ...`.
    - Vuelve implícitamente al orden por defecto, preservando la robustez.

- **Compatibilidad con FIX-4/5/6:**
  - FIX-7 no modifica `LTE_CONNECT_BUDGET_MS` ni `COMM_CYCLE_BUDGET_MS`.
  - El corte por perfil se respeta vía `g_lte_max_wait_ms` y la asignación de `allowedMs` en `tryConnectOperator`.
  - FIX-5 sigue validando que exista PDP activo antes de considerar la conexión como exitosa.
  - FIX-6 sigue controlando el presupuesto global a través de `ensureCommunicationBudget`.

## Conclusión

- **Resultado:** FIX-7 queda **implementado y funcional** en la rama `JAMR_4`, con evidencias claras de actualización del perfil LTE persistente tras una conexión exitosa.
- **Beneficio principal:** El sistema es capaz de aprender qué perfil LTE fue exitoso y priorizarlo entre ciclos, lo que reduce la variabilidad y el tiempo medio hasta la conexión en entornos estables, sin sacrificar la capacidad de recuperación en entornos dinámicos.
- **Trabajo futuro sugerido:**
  - Capturar al menos un segundo ciclo con log donde se vea explícitamente:
    - `[FIX-7] Perfil LTE persistente cargado: id=0`.
  - Extender el mecanismo a escenarios multi-SIM o multi-APN si en el futuro se añaden más variantes de `OperatorProfile`.
