# FIX-10: Refactor seguro del stack módem/LTE en JAMR_4.4.7

## 1. Objetivo

Refactorizar el código relacionado con módem/LTE/TCP de `JAMR_4.4.7` para:

- Reducir complejidad y riesgo de errores futuros.
- Mantener intacto el comportamiento funcional de FIX-3…FIX-9.
- Reforzar las prioridades del sistema:
  - Guardar datos de forma robusta.
  - Asegurar la conexión y envío cuando haya red disponible.
  - Proteger el ahorro de energía (presupuestos de tiempo).
  - Evitar estados "zombie" del módem.

**IMPORTANTE:** FIX-10 **no añade nuevas features de negocio**. Solo reorganiza y encapsula lógica existente.

---

## 2. Alcance

Incluye:
- Código en `JAMR_4.4/gsmlte.cpp` y `JAMR_4.4/gsmlte.h`.
- Creación de nuevos módulos de apoyo (archivos `.cpp/.h`) para separar responsabilidades.
- Ajustes mínimos en includes/llamadas para usar los nuevos módulos.

No incluye:
- Cambios en el protocolo de datos ni formato de payload.
- Cambios en FIX-3…FIX-9 a nivel de diseño (solo reubicación de código).
- Cambios en `sensores.cpp`, `sleepdev.cpp` u otros módulos ajenos al stack de comunicación.

---

## 3. Estrategia de refactorización (pasos seguros)

### Paso 1: Congelar interfaz pública

1. Documentar la interfaz actual expuesta por `gsmlte.h` (funciones usadas por el resto del firmware):
   - `setupModem`, `setupGpsSim`, `startGsm`, etc.
2. Comprometerse a **no cambiar firmas** de estas funciones en FIX-10.
3. Cualquier función nueva será interna a los nuevos módulos (`static` o no expuesta fuera del .cpp).

### Paso 2: Extraer módulos sin cambiar lógica

Crear nuevos archivos y mover código **sin modificar su contenido**, solo reubicando funciones:

1. `gsm_health.cpp/.h`  
   - Contendrá toda la lógica FIX-8:
     - `modemHealthReset`
     - `modemHealthRegisterTimeout`
     - `modemHealthAttemptRecovery`
     - `modemHealthMarkOk`
     - `modemHealthShouldAbortCycle`
   - Declarar `modem_health_state_t` en `type_def.h` como hasta ahora.

2. `gsm_comm_budget.cpp/.h`  
   - Contendrá FIX-6:
     - `resetCommunicationCycleBudget`
     - `remainingCommunicationCycleBudget`
     - `ensureCommunicationBudget`

3. `gsm_tcp.cpp/.h`  
   - Contendrá:
     - `isSocketOpen`
     - `tcpOpenSafe`
     - `tcpOpen`
     - `tcpClose`
     - `tcpSendData`
     - `waitForToken`
     - `waitForAnyToken`

4. `gsm_storage.cpp/.h` (opcional en primera iteración)
   - Contendrá:
     - `iniciarLittleFS`
     - `guardarDato`
     - `enviarDatos`
     - `limpiarEnviados`

5. `gsm_gps.cpp/.h` (opcional en primera iteración)
   - Contendrá:
     - `setupGpsSim`
     - `startGps`
     - `getGpsSim`
     - `stopGps`

En esta fase:
- `gsmlte.cpp` seguirá incluyendo estas funciones y actuará como "orquestador".
- Confirmar compilación y pruebas de mesa: los logs, tiempos y comportamientos deben coincidir con 4.4.7.

### Paso 3: Centralizar decisiones de salud y presupuesto

1. En `gsm_health.h` definir un helper de alto nivel:
   ```cpp
   bool health_onCriticalTimeout(const char* contextTag);
   ```
   - Internamente llamará a `modemHealthRegisterTimeout` y `modemHealthShouldAbortCycle`.
   - Devuelve `true` si se debe abortar el ciclo, `false` si se puede continuar.

2. Reemplazar en `gsmlte.cpp` patrones como:
   ```cpp
   modemHealthRegisterTimeout("X");
   if (modemHealthShouldAbortCycle("X")) {
     return false;
   }
   ```
   por:
   ```cpp
   if (health_onCriticalTimeout("X")) {
     return false;
   }
   ```

3. Verificar que TODOS los puntos críticos (CNACT, CAOPEN, envíos TCP, etc.) siguen usando FIX-8, pero ahora a través de una API compacta.

### Paso 4: Unificar el camino LTE en una sola función

1. Crear en `gsmlte.cpp` una función interna:
   ```cpp
   static bool connectLTE();
   ```
   Comportamiento propuesto:
   - Intentar primero `startLTE_autoLite()` (FIX-9) dentro del presupuesto global.
   - Si falla, caer a `startLTE_multiOperator()` (si está activado) o `startLTE()` estándar.

2. En `setupModem`, reemplazar el bloque actual:
   - Intento AUTO_LITE.
   - Luego intento multi-perfil / startLTE.

   Por una sola llamada:
   ```cpp
   bool lteOk = connectLTE();
   ```

3. Confirmar que:
   - Se siguen actualizando checkpoints (`CP_LTE_CONNECT`, `CP_LTE_OK`).
   - Los logs `[FIX-9]` mantienen la misma semántica.

### Paso 5: Revisión de seguridad energética

1. Revisar en el código refactorizado que:
   - Todas las rutas largas (LTE, TCP, envío de buffer) llaman a `ensureCommunicationBudget` al inicio y/o en sus bucles.
   - FIX-8 se sigue disparando en los mismos contextos (CNACT, CAOPEN, `tcpSendData`, etc.).

2. (Opcional) En builds de debug, añadir asserts sencillos:
   - Comentarios o checks internos del estilo: "ningún bucle de espera sin `ensureCommunicationBudget` dentro".

### Paso 6: Validación

1. **Pruebas de laboratorio:**
   - Ciclos con buena señal:
     - Comparar tiempos wake-up → LTE OK → datos enviados, antes y después de FIX-10.
   - Ciclos con fallos forzados:
     - Simular errores en `+CNACT`, `ensurePdpActive`, `+CAOPEN` y `tcpSendData`.
     - Confirmar que FIX-8 actúa igual (un intento profundo de recuperación, luego aborta si sigue fallando).

2. **Pruebas en campo:**
   - Ejecutar un número representativo de ciclos en sitios rurales / señal baja.
   - Confirmar:
     - Sin nuevos estados zombie.
     - Sin aumento significativo del tiempo medio de ciclo.
     - Sin incremento apreciable en consumo de batería.

---

## 4. Criterios de aceptación

- ✅ El firmware compila sin cambios en la interfaz pública (`gsmlte.h`).
- ✅ Las funciones de alto nivel (`setupModem`, `enviarDatos`, etc.) se comportan igual que en `JAMR_4.4.7`.
- ✅ Los logs críticos de FIX-3…FIX-9 siguen apareciendo con la misma semántica.
- ✅ No hay regresiones observadas en:
  - Tasa de éxito de conexión.
  - Consumo de energía por ciclo.
  - Protección contra estados zombie del módem.
- ✅ El código del stack módem/LTE queda separado en módulos coherentes, facilitando futuras correcciones sin sobreingeniería.
