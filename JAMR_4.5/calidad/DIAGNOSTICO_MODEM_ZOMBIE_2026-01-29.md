# Diagnóstico: Modem SIM7080G en Estado Irrecuperable

**Fecha:** 2026-01-29  
**Firmware:** v2.7.1  
**Dispositivos afectados:** 6948, 6963 (confirmados)  
**Severidad:** CRÍTICA - Pérdida total de transmisión

---

## 1. DESCRIPCIÓN DEL PROBLEMA

### Síntoma
Los dataloggers dejan de transmitir durante **días consecutivos** aunque el ciclo de sensores continúa ejecutándose normalmente.

### Comportamiento observado en logs
```
Encendiendo SIM7080G...
Intento de encendido 1 de 3
PWRKEY toggled
Intento de encendido 2 de 3
PWRKEY toggled
Intento de encendido 3 de 3
PWRKEY toggled
Error: No se pudo encender el modulo
```

Este patrón se repite en:
- GPS (usa GNSS del SIM7080G)
- Lectura de ICCID
- Transmisión LTE

### Evidencia temporal (dispositivo 6948)

| Epoch | Hora aprox | GPS | ICCID | LTE | Batería |
|-------|------------|-----|-------|-----|---------|
| 1769691720 | 29-ene 12:02 | ❌ 3x | ❌ 3x | ❌ 3x | 4.07V |
| 1769691952 | 29-ene 12:05 | ❌ 3x | ❌ 3x | ❌ 3x | 4.07V |
| 1769692106 | 29-ene 12:08 | ❌ 3x | ❌ 3x | ❌ 3x | 4.07V |
| 1769692594 | 29-ene 12:16 | ❌ 3x | ❌ 3x | ❌ 3x | 4.06V |
| 1769692758 | 29-ene 12:19 | ❌ 3x | ❌ 3x | ❌ 3x | 4.06V |

### Recuperación
- **Power cycle físico** (desconectar batería) → Modem funciona normalmente
- Después del power cycle, dispositivo 6963 envió **50 tramas acumuladas** exitosamente

---

## 2. ANÁLISIS TÉCNICO

### 2.1 Descartados como causa

| Factor | Estado | Evidencia |
|--------|--------|-----------|
| Batería baja | ❌ Descartado | 4.06-4.14V (normal) |
| Sensores | ❌ Descartado | RS485, I2C, ADC funcionando |
| RTC | ❌ Descartado | Epochs válidos y consecutivos |
| ESP32 | ❌ Descartado | Firmware ejecutando correctamente |
| Buffer LittleFS | ❌ Descartado | Tramas guardadas correctamente |

### 2.2 Causa raíz identificada

El modem SIM7080G entra en un **estado zombie** donde:
1. No está completamente apagado
2. No responde a comandos AT
3. No responde a PWRKEY toggle
4. Solo se recupera con power cycle físico

### 2.3 Origen probable del estado zombie

El firmware actual hace:
```cpp
bool LTEModule::powerOff() {
    if (!isAlive()) {
        debugPrint("Modulo ya esta apagado");
        return true;  // ← ASUME apagado si no responde
    }
    
    _serial.println("AT+CPOWD=1");
    delay(2000);  // ← Espera fija, no verifica URC
    // ...
}
```

**Problemas identificados:**

| Código actual | Problema |
|---------------|----------|
| `if (!isAlive()) return true` | Falso positivo: modem puede no responder pero estar encendido |
| `delay(2000)` después de CPOWD | No espera URC "NORMAL POWER DOWN" |
| PWRKEY pulse 1200ms | Puede ser insuficiente para forzar apagado |
| No hay verificación post-deep-sleep | No detecta si el problema persiste |

### 2.4 Secuencia de fallo hipotética

```
Ciclo N:
  1. Modem transmite OK
  2. powerOff() envía AT+CPOWD=1
  3. Modem inicia apagado pero NO COMPLETA
  4. ESP32 entra a deep sleep
  5. Modem queda en estado intermedio
  
Ciclo N+1:
  6. ESP32 despierta
  7. powerOn() hace PWRKEY toggle
  8. Modem NO RESPONDE (estado corrupto)
  9. isAlive() retorna false
  10. Firmware asume "modem apagado" → INCORRECTO
  
Ciclo N+2...N+∞:
  11. Mismo comportamiento indefinidamente
```

---

## 3. LIMITACIONES DE HARDWARE

### 3.1 Esquemático actual (según BOM)

| Componente | Función | Control desde ESP32 |
|------------|---------|---------------------|
| SIM7080G (U1) | Modem LTE/GNSS | Solo PWRKEY (GPIO9) |
| IRLML2502 (U9) | MOSFET | **No conectado a VCC del modem** |

### 3.2 Lo que NO existe en el PCB actual

| Capacidad | Estado | Impacto |
|-----------|--------|---------|
| GPIO → RESET del SIM7080G | ❌ No existe | No se puede hacer hard reset |
| MOSFET en VCC del modem | ❌ No existe | No se puede cortar alimentación |
| Circuito de power-on-reset | ❌ No existe | Modem depende solo de PWRKEY |

---

## 4. OPCIONES DE SOLUCIÓN

### 4.1 Solución de hardware (DEFINITIVA)

**Opción A: MOSFET en línea de alimentación**
- Agregar MOSFET (ej: IRLML2502) entre batería y VCC del SIM7080G
- Controlado por GPIO del ESP32
- Permite cortar alimentación completamente

**Opción B: GPIO a pin RESET del SIM7080G**
- Conectar GPIO del ESP32 al pin RESET (pin 15) del SIM7080G
- Permite hard reset sin cortar alimentación

**Recomendación:** Opción A es más robusta (corta todo el circuito del modem)

### 4.2 Mitigaciones de firmware (PALIATIVAS)

| Mitigación | Efectividad estimada | Complejidad |
|------------|---------------------|-------------|
| Esperar URC "NORMAL POWER DOWN" | Media | Baja |
| PWRKEY extendido (>10s) según datasheet | Media | Baja |
| Contador de fallos consecutivos + alerta | Baja (solo detecta) | Baja |
| Delay extra antes de deep sleep | Baja | Muy baja |
| Múltiples intentos de PWRKEY agresivos | Baja-Media | Media |

**Nota:** Las mitigaciones de firmware **no garantizan** recuperación si el modem está en estado zombie profundo.

---

## 5. RECOMENDACIONES

### Corto plazo (firmware)
1. Implementar espera de URC "NORMAL POWER DOWN"
2. Aumentar pulso PWRKEY a 3000ms en intento final
3. Agregar contador de ciclos sin modem funcional
4. Log específico para diagnóstico post-mortem

### Mediano plazo (hardware)
1. Diseñar modificación de PCB con MOSFET en VCC del modem
2. Probar en prototipo antes de producción
3. Actualizar firmware para usar nuevo GPIO

### Largo plazo (nueva revisión PCB)
1. Incluir MOSFET de corte de alimentación del modem
2. Incluir GPIO a RESET del modem (redundancia)
3. Considerar watchdog externo para el sistema completo

---

## 6. INFORMACIÓN PARA DIAGNÓSTICO FUTURO

### Comandos Serial disponibles (v2.7.1)
```
STATS  → Contadores de ciclos, LTE OK/FAIL, EMI
LOG    → Eventos con timestamp (últimos 20)
DIAG   → Último crash (checkpoint)
```

### Datos a recolectar en campo
1. Output de `STATS` → Ver `lteSendFail` acumulado
2. Output de `LOG` → Ver timeline de eventos
3. Epoch del último envío exitoso vs. época actual
4. Voltaje de batería

---

## 7. REFERENCIAS

- Datasheet SIM7080G Hardware Design v1.05
- Logs analizados: `6948 2026-01-29`, `6963 2026-01-29`, `6963) 2026-01-29`
- BOM: `BOM_tarjeta_Intelagro_Sensores_cambio_sim_2026-01-08.csv`

---

**Autor:** Análisis automatizado  
**Revisión:** Pendiente validación con desarrollador hardware
