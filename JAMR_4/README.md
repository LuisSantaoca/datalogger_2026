# JAMR_4 Firmware

Firmware de desarrollo controlado para el sistema de monitoreo ambiental basado en ESP32-S3 + SIM7080G.

## Estado actual

- Versión de firmware activa: **4.1.1-JAMR4-TIMEOUT**
- Plataforma: ESP32-S3 + módem SIM7080G (TinyGSM)
- Almacenamiento: LittleFS
- Encriptación: AES para payload de datos

## Fixes activos en esta rama

- **FIX-003 – Watchdog defensivo en bucles largos**  
  Protege contra resets espurios del WDT durante:
  - Inicio del módem (`startGsm`).
  - Comandos AT de larga duración.
  - Obtención de GPS/RTC.

- **FIX-004 – Health / diagnóstico de ciclo**  
  - Checkpoints de ciclo (BOOT, GPIO_OK, SENSORS_OK, GSM_OK, LTE_CONNECT, LTE_OK, DATA_SENT, etc.).
  - Persistencia de `boot_count`, último checkpoint y timestamp de crash.
  - Inclusión de health data y versión de firmware en `sensordata_type`.

- **FIX-005 – PDP activo obligatorio**  
  - Valida IP real vía `+CNACT?` y parsea contexto activo.
  - Reintenta activación PDP dentro de un presupuesto acotado.
  - No abre TCP si no hay PDP con IP válida.

- **FIX-006 – Presupuesto global de ciclo de comunicación**  
  - Introduce un presupuesto global (`COMM_CYCLE_BUDGET_MS`) para LTE+TCP en cada ciclo.
  - Helpers: `resetCommunicationCycleBudget`, `remainingCommunicationCycleBudget`, `ensureCommunicationBudget`.
  - Evita que un solo ciclo se dispare en duración por reintentos excesivos.

- **FIX-007 – Perfil LTE persistente (multi-operador eficiente)**  
  - Almacena en `/lte_profile.cfg` el `id` del último `OperatorProfile` exitoso.
  - En ciclos siguientes prioriza ese perfil al construir el orden de prueba.
  - Estado: validado en prueba de mesa con conexión LTE real y envío de 5 mensajes TCP.

## Documentación de fixes

La documentación detallada de cada fix se encuentra en `JAMR_4/fixs/`:

- `fix_v3_watchdog_loops/`
- `fix_v4_operador_eficiente/`
- `fix_v5_pdp_activo/`
- `fix_v6_presupuesto_ciclo/`
- `fix_v7_perfil_persistente/`

Cada carpeta contiene:

- `ANALISIS_*.md` / `PLAN_EJECUCION.md` – diseño y plan de cambio.
- `LOG_*.md` – evidencias de pruebas.
- `REPORTE_FINAL.md` – conclusiones y estado.

## Changelog

Consultar `CHANGELOG.md` para el historial completo de versiones y fixes aplicados.
