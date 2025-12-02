# FIX-8 - Guardia de salud del módem (anti estados zombie en rural)

## Objetivo

Reducir al mínimo el riesgo de que el módem SIM7080G quede en estados "zombie" (responde AT pero no progresa en señal/PDP/TCP) en entornos de cobertura rural, mediante una capa explícita de salud del módem que:

- Detecte falta de progreso real en la conexión (LTE/PDP/TCP) dentro de los presupuestos de tiempo ya definidos (FIX-4/FIX-6).
- Aplique una **recuperación forzada y acotada** (reset profundo de radio/módem una sola vez por ciclo).
- Marque claramente el ciclo como fallo de módem cuando no haya recuperación, evitando bucles de reintento costosos en batería.

## Alcance

- Firmware: `JAMR_4` (ESP32-S3 + SIM7080G, versión base v4.1.1-JAMR4-TIMEOUT).
- Archivos probables a tocar:
  - `JAMR_4/gsmlte.cpp` / `gsmlte.h`: máquina de estados de salud del módem y hooks.
  - `JAMR_4/type_def.h`: enums/structs de estado y health si hace falta.
  - `JAMR_4/JAMR_4.ino`: integración mínima con health data/reportes (si aplica).
- No cambia el protocolo de payload ni la encriptación.

## Hipótesis de fallo / escenarios a cubrir

1. **Radio aparente pero sin progreso en red**:
   - `+CFUN=1` OK, `+CPSI?` devuelve estado LTE pero la calidad de señal/no hay avance real en tiempos razonables.
2. **PDP activo pero sin tráfico útil**:
   - `+CNACT` y `+CNACT?` muestran contexto activo, pero apertura TCP o envío no progresan (timeouts repetidos, `+CAOPEN` nunca se estabiliza, etc.).
3. **Socket/TCP colgado**:
   - `+CASTATE?` devuelve estados inconsistentes o no responde, pero el módem sigue contestando otros AT.
4. **Reintentos excesivos en ventana corta**:
   - Varios ciclos consecutivos con patrón de "no progreso" que podrían drenar batería en rural.

## Estrategia de solución (resumen)

1. **Definir estados de salud del módem** (en código):
   - Ejemplo: `MODEM_HEALTH_OK`, `MODEM_HEALTH_TRYING`, `MODEM_HEALTH_ZOMBIE_DETECTED`, `MODEM_HEALTH_RECOVERED`, `MODEM_HEALTH_FAILED`.

2. **Añadir temporizadores y contadores de progreso** en puntos clave:
   - Inicio de LTE (`startLTE_multiOperator` / `tryConnectOperator`).
   - Validación PDP (`ensurePdpActive` / parseo de `+CNACT?`).
   - Apertura de socket (`openTcpWithValidation`).
   - Envío TCP (primer envío y reintentos).

3. **Criterios de "no progreso"** (dentro del presupuesto ya existente):
   - Tiempo máximo sin transición de estado saludable (ej. X segundos sin pasar de TRYING a OK).
   - Número de timeouts consecutivos en operaciones críticas (CNACT, CAOPEN, envío TCP).

4. **Recuperación profunda limitada**:
   - Si se detecta "no progreso":
     - Cerrar TCP (si está abierto).
     - Desactivar PDP (`+CNACT=0,0`).
     - Bajar radio (`+CFUN=0`).
     - Subir radio de nuevo (`+CFUN=1`) **una sola vez por ciclo**.

5. **Cierre ordenado del ciclo cuando no hay recuperación**:
   - Si tras la recuperación profunda no se consigue estado OK:
     - Marcar `MODEM_HEALTH_FAILED`.
     - Guardar datos en buffer (ya existe).
     - Apagar módem y salir a deep sleep **sin bucles adicionales de reintento**.

6. **Protección multi-ciclo (anti tormenta de reintentos)**:
   - Contador de ciclos recientes con `MODEM_HEALTH_FAILED`.
   - Si supera un umbral N en una ventana de tiempo aproximada, aumentar temporalmente el intervalo de sleep o reducir reintentos en ese sitio.
   - Nota: esta parte puede quedar como fase 2 si se ve compleja para la primera versión.

## Plan de ejecución

- [x] 1. Análisis de código actual de `gsmlte.cpp` y puntos de hook posibles (se revisaron `startLTE`, `startLTE_autoLite`, `tcpOpen/tcpSendData` y `enviarDatos`).
- [x] 2. Diseñar máquina de estados detallada (MODEM_HEALTH_OK/TRYING/ZOMBIE_DETECTED/RECOVERED/FAILED) y documentarla aquí.
- [x] 3. Definir estructuras y enums necesarios en `type_def.h` (enum agregado como parte de FIX-8).
- [x] 4. Implementar lógica de salud del módem y recuperación en `gsmlte.cpp` (detección de timeouts críticos + recuperación profunda única y abortos controlados respetando FIX-3/4/5/6/7).
- [x] 5. Añadir logs específicos `[FIX-8]` para trazabilidad (detección zombie, intento de recuperación, fallo definitivo o recuperación exitosa).
- [x] 6. Probar en entorno controlado (simulaciones de fallos PDP/TCP fuerzan la recuperación y validan que no se excede el presupuesto de ciclo).
- [ ] 7. Probar en campo rural y capturar logs de varios ciclos (pendiente de ventana de pruebas en sitio real).
- [x] 8. Actualizar documentación: reporte final de FIX-8, changelog y, si aplica, README.

## Riesgos y consideraciones

- Evitar que FIX-8 interfiera con los presupuestos de tiempo (debe **operar dentro** de los límites de FIX-4/FIX-6).
- Mantener la lógica simple y observable: demasiados estados podrían complicar el debug.
- Alinear la semántica de `MODEM_HEALTH_*` con el health general del sistema (para futuros usos en backend).
