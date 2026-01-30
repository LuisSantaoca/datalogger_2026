# FIX-V4: Garantizar Apagado de Modem antes de Deep Sleep

## Metadata
| Campo | Valor |
|-------|-------|
| **ID** | FIX-V4 |
| **Nombre** | MODEM_POWEROFF_SLEEP |
| **Sistema** | LTE / Sleep |
| **Archivo(s)** | `AppController.cpp` |
| **Feature Flag** | `ENABLE_FIX_V4_MODEM_POWEROFF_SLEEP` |
| **Estado** | ✅ Implementado |
| **Complemento** | FIX-V6 (lógica robusta de powerOff) |

## Problema

El ESP32-S3 entraba a deep sleep sin garantizar el apagado limpio del modem SIM7080G. Esto causaba:

1. **Modem en estado indeterminado** al despertar
2. **Consumo residual** durante deep sleep (~mA en vez de ~µA)
3. **Estados corruptos persistentes** que requerían power cycle

Según el datasheet SIM7080G Hardware Design v1.05 (Page 23, 27):
> "It is strongly recommended to turn off the module through PWRKEY or AT command before disconnecting the module VBAT power."

## Solución

FIX-V4 garantiza que `lte.powerOff()` se llame **ANTES** de entrar a deep sleep.

```cpp
// AppController.cpp - Antes de deep sleep
#if ENABLE_FIX_V4_MODEM_POWEROFF_SLEEP
Serial.println(F("[FIX-V4] Asegurando apagado de modem antes de sleep..."));
lte.powerOff();  // Delegado a FIX-V6 para secuencia robusta
Serial.println(F("[FIX-V4] Secuencia de apagado completada."));
#endif
```

## Relación con FIX-V6

| FIX | Responsabilidad |
|-----|-----------------|
| **FIX-V4** | **QUÉ**: Garantiza que se llame `powerOff()` antes de sleep |
| **FIX-V6** | **CÓMO**: Implementa la secuencia robusta dentro de `powerOff()` |

**Ambos deben estar habilitados** para protección completa:
- `ENABLE_FIX_V4_MODEM_POWEROFF_SLEEP = 1`
- `ENABLE_FIX_V6_MODEM_POWER_SEQUENCE = 1`

## Parámetros (Legacy)

Los parámetros de FIX-V4 están **deprecados** - usar los de FIX-V6:

| Parámetro FIX-V4 (deprecado) | Parámetro FIX-V6 (actual) |
|------------------------------|---------------------------|
| `FIX_V4_URC_TIMEOUT_MS` (5000) | `FIX_V6_URC_WAIT_TIMEOUT_MS` (10000) |
| `FIX_V4_PWRKEY_WAIT_MS` (3000) | `FIX_V6_PWRKEY_OFF_TIME_MS` (1500) |

## Ubicación del Código

```
JAMR_4.5/
├── AppController.cpp          # Línea ~1550: Llamada a powerOff()
└── src/
    ├── FeatureFlags.h         # Flag ENABLE_FIX_V4_MODEM_POWEROFF_SLEEP
    └── data_lte/
        └── LTEModule.cpp      # powerOff() implementación (FIX-V6)
```

## Testing

1. Verificar que el log muestre `[FIX-V4] Asegurando apagado...` antes de cada sleep
2. Medir corriente durante deep sleep (debe ser <50µA)
3. Verificar que el modem responda correctamente al despertar

## Historial

| Fecha | Cambio |
|-------|--------|
| 2026-01-28 | Implementación inicial |
| 2026-01-29 | Documentación creada, clarificada relación con FIX-V6 |
