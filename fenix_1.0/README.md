# Fenix 1.0

Firmware para datalogger ESP32-S3 + SIM7080G **sin circuito autoswitch**.

## Origen

Fork de `jarm_4.5_autoswich` v2.3.0. Mismo hardware (PCB), pero sin el circuito del autoswitch poblado (GPIO 2 sin efecto).

## Diferencia principal

| Aspecto | jarm_4.5_autoswich | Fenix 1.0 |
|---------|-------------------|-----------|
| Autoswitch GPIO 2 | Corte fisico VBAT cada 24h | No disponible |
| Recovery modem zombie | Hardware (VBAT cut) | Software (esp_restart + backoff) |
| Contador de ciclos | Reset a 0 cada 144 | Libre (diagnostico) |

## Recuperacion del modem

1. PWRKEY toggle 1200ms x 3 intentos (FIX-V3)
2. PWRKEY forzado 13s x 1 (FIX-V4)
3. `esp_restart()` tras 6 fallos consecutivos
4. Backoff: 3 ciclos sin modem (sensores siguen, datos en buffer)
5. Buffering pasivo en LittleFS hasta recuperacion

## Estructura

```
fenix_1.0/
  fenix_1.0.ino          # Punto de entrada (sin autoswitch)
  AppController.cpp/h    # FSM con zombie recovery
  CHANGELOG.md           # Historial de cambios
  src/                   # Modulos (identicos a jarm_4.5_autoswich v2.3.0)
    data_buffer/         # BUFFERModule (LittleFS, FIX-V13)
    data_format/         # FORMATModule (Base64)
    data_gps/            # GPSModule (FIX-V8 NVS)
    data_lte/            # LTEModule (FIX-V1 a V5)
    data_sensors/        # ADC (FIX-V11), I2C, RS485
    data_sleepwakeup/    # SLEEPModule
    data_time/           # RTCModule (DS3231)
```

## Documentacion detallada

- [CHANGELOG.md](CHANGELOG.md) — Cambios vs autoswitch, fixes heredados
- `src/data_info/*.md` — Documentacion de cada modulo
