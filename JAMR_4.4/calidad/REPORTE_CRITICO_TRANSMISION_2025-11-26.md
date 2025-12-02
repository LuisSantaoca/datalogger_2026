# REPORTE CRÍTICO DE CALIDAD – TRANSMISIÓN JAMR_4
**Fecha:** 2025-11-26  
**Revisor:** QA Ingeniería de Calidad (focus: continuidad de transmisión LTE/TCP)

## Alcance
Revisión estática de firmware `JAMR_4` (ESP32-S3 + SIM7080G), centrada en requisitos críticos relacionados con la continuidad de transmisión y gestión del módem. Se contrastaron los requisitos `REQ-001` (Gestión de estado del módem), `REQ-002` (Watchdog) y criterios operativos de FIX-5/6/7/8. El objetivo es listar **solo hallazgos críticos** que pueden comprometer la entrega de datos o violar requisitos prioritarios.

## Hallazgos críticos

### 1. Power-cycle obligatorio en cada ciclo (REQ-001 incumplido)
- **Evidencia:** `JAMR_4/gsmlte.cpp`, líneas 320-360 y 1504-1560. Después de cada transmisión el flujo ejecuta `modemPwrKeyPulse()` y previamente `stopGps()` manda `+CFUN=0`, dejando al SIM7080G completamente apagado.
- **Impacto:** Viola directamente RF-001 y ANR-001 de `REQ-001`. Cada ciclo requiere boot completo (≥8 s) y re-attach LTE (30-60 s), elevando el tiempo de transmisión y consumo. También inhibe el objetivo de “wake-to-AT < 1 s”, generando huecos de telemetría en zonas rurales.
- **Riesgo:** CRÍTICO – la red no se aprovecha entre ciclos; cualquier degradación de señal crea probabilidades altas de que el presupuesto del ciclo se agote antes de transmitir.
- **Acción recomendada:** Implementar modo de bajo consumo (`CFUN=0` o `CSCLK/DTR`) sin `PWRKEY` entre ciclos y persistir un flag `modem_initialized` para distinguir primer boot vs wake recurrente.

### 2. Ausencia total de verificación “AT tras wake” (REQ-001 RF-001)
- **Evidencia:** `setupModem()` siempre llama a `startGsm()` → `modemPwrKeyPulse()` sin intentar primero `modem.testAT()`. No existe lógica que verifique si el módem ya está inicializado ni variable RTC asociada.
- **Impacto:** El requisito exige validar que el módem responda AT inmediatamente tras deep sleep (<1 s). El firmware asume que siempre debe inicializarlo desde cero, por lo que nunca podremos demostrar cumplimiento y perdemos el beneficio del módem en estado “attach”.
- **Riesgo:** CRÍTICO – el sistema no cumple el indicador clave de tiempo de reacción y la ventana de transmisión real queda reducida, afectando entregas en campo.
- **Acción:** Registrar flag RTC (`modem_ready`) y probar secuencia `AT` antes de reactivar PWRKEY; solo si falla ejecutar el power-cycle.

### 3. Buffer de datos no es power-safe (riesgo de pérdida de telemetría)
- **Evidencia:** `guardarDato()` (líneas 1530-1585) reescribe `buffer.txt` completo sin archivo temporal ni `LittleFS.rename`. Si ocurre reset/brownout durante la escritura, todo el backlog puede quedar truncado o vacío.
- **Impacto:** Compromete el objetivo principal de “asegurar transmisión”. Ante cualquier reset o watchdog (que son esperados según REQ-002), el sistema puede perder datos recopilados antes de transmitirlos.
- **Riesgo:** CRÍTICO – se pierde la única copia de datos previos; la trazabilidad queda rota aun cuando el radio vuelva más tarde.
- **Acción:** Aplicar la misma estrategia ya presente en `enviarDatos()` (archivo temporal + rename atómico) también en `guardarDato()` o utilizar `fs::FS::open` en modo append con `fs::rename` para commits seguros.

### 4. No existe confirmación de backend antes de marcar datos como enviados
- **Evidencia:** `enviarDatos()` y `tcpSendData()` (líneas 1650-1830) marcan cada registro como `#ENVIADO` apenas el módem responde “SEND OK”. Nunca se espera ACK de la aplicación remota, ni se valida que el socket permanezca abierto tras el envío.
- **Impacto:** “SEND OK” solo garantiza que el módem puso datos en su buffer TCP. Si el enlace cae antes de que el servidor confirme, el firmware igual borra el registro. Esto viola el requisito implícito de “asegurar la transmisión end-to-end” y deja huecos silenciosos en la base de datos.
- **Riesgo:** CRÍTICO – especialmente en zonas rurales donde la red puede aceptar el paquete y cerrarse antes de llegar al backend.
- **Acción:** Implementar protocolo ligero de ACK (por ejemplo, backend responde `ACK:<CRC>`), leer con `waitForToken` y solo marcar como enviado cuando se reciba. Si no llega ACK, dejar el registro en buffer.

### 5. FIX-8 no ejecuta acción correctiva final (solo marca estado)
- **Evidencia:** `modemHealthShouldAbortCycle()` (líneas 70-95) retorna `true` cuando se detectan ≥3 timeouts, pero el flujo superior solo hace `return false` en `startLTE`/`tcpOpen`. No se registra en health data ni se fuerza `sleepIOT()` inmediato.
- **Impacto:** Aunque se detecte el patrón de “estado zombie”, el dispositivo sigue consumiendo todo el ciclo hasta que el scheduler normal llegue a `sleepIOT()`. Tampoco queda evidencia para backend (`sensordata_type` no expone `MODEM_HEALTH_*`).
- **Riesgo:** ALTO (cercano a crítico) – el fix no logra el objetivo declarado (“evitar estados zombie en rural”) porque no dispara acción correctiva ni señaliza al backend.
- **Acción:** Propagar `MODEM_HEALTH_FAILED` al payload (health bytes) y, al primer `modemHealthShouldAbortCycle()==true`, salir del flujo de comunicación marcando estado y llamando a `sleepIOT()` anticipado.

## Recomendación general
Mientras estos hallazgos permanezcan abiertos, **no se recomienda liberar `JAMR_4` como firmware listo para producción rural**. Cada punto afecta directamente la capacidad de cumplir el objetivo de transmisión continua y los requisitos marcados como CRÍTICOS. Priorizar la remediación siguiendo el orden de los hallazgos (1→4) y revalidar en campo con logs que muestren:
1. Wake-to-AT < 1 s sin power-cycle.
2. Backlog preservado tras reset forzado.
3. Evidencia de ACK de backend antes de borrar registros.
4. Eventos FIX-8 reportados en payload antes de entrar a sleep anticipado.
