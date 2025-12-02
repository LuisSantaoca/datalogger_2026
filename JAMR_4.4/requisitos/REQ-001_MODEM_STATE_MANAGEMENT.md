# REQ-001: Gestión de Estado del Módem entre Ciclos de Sleep

**Versión:** 1.0  
**Fecha:** 2025-10-29  
**Prioridad:** CRÍTICA  
**Estado:** PENDIENTE

---

## 🎯 Objetivo (QUÉ)

El módem **DEBE** responder a comandos AT inmediatamente después de que el ESP32 despierte de deep sleep, sin requerir secuencias de encendido/apagado que causan delays de 5-10 segundos.

---

## 📋 Requisitos Funcionales

### RF-001: Preservación de Estado
El módem **DEBE** mantener la capacidad de responder a comandos AT después de cada ciclo de deep sleep del ESP32.

**Criterio de aceptación:**
- `modem.testAT(1000)` retorna `true` en el primer intento tras despertar
- Tiempo de respuesta: < 1 segundo desde despertar hasta primer AT command exitoso
- **NO** se permite uso de secuencias de power cycle (PWRKEY pulses) entre ciclos normales

### RF-002: Inicialización en Primer Boot
El sistema **DEBE** detectar el primer arranque (power-on inicial) y realizar la secuencia de inicialización del módem una sola vez.

**Criterio de aceptación:**
- Flag persistente identifica si módem ha sido inicializado
- Secuencia de power-on solo se ejecuta cuando flag indica "no inicializado"
- Después de primer boot exitoso, flag permanece en estado "inicializado" entre ciclos

### RF-003: Modo de Bajo Consumo
El módem **DEBE** entrar en modo de bajo consumo durante el deep sleep del ESP32, sin perder su estado de inicialización.

**Criterio de aceptación:**
- Consumo del módem: ≤ 1 mA durante sleep (vs 10 mA idle normal)
- Tiempo de wake-up del módem: < 500 ms
- **NO** requiere re-attach a la red celular en cada ciclo

### RF-004: Recuperación de Errores
El sistema **DEBE** detectar cuando el módem no responde y tomar acción correctiva sin entrar en loops infinitos.

**Criterio de aceptación:**
- Máximo 10 intentos de comunicación AT antes de declarar fallo
- Tiempo máximo de espera: 15 segundos total
- Si fallo persiste: permitir watchdog reset para reinicio limpio del sistema completo
- **NO** intentar power cycles repetidos que pueden empeorar el estado

---

## 🚫 Anti-Requisitos (QUÉ NO HACER)

### ANR-001: NO Power Cycling entre Ciclos Normales
**PROHIBIDO:** Llamar `modemPwrKeyPulse()` o equivalente después de transmisión exitosa o fallo de conexión.

**Razón:** Power cycling:
- Causa delays de 5-10s en siguiente ciclo
- Puede dejar el módem en estado indefinido si se interrumpe
- Consume más energía que modo sleep apropiado
- Requiere re-attach completo a red celular

### ANR-002: NO Apagar el Módem Completamente
**PROHIBIDO:** Usar comandos AT que apagan completamente el módem (ej: `CPOF`, power off completo).

**Razón:** Apagado completo:
- Pierde todo el estado interno del módem
- Requiere boot sequence de 4-6 segundos
- Obliga a re-registro en red (30-60s adicionales)
- Introduce puntos de fallo adicionales

### ANR-003: NO Lógica Compleja de Recuperación
**PROHIBIDO:** Implementar múltiples niveles de power cycling, double-toggles, o secuencias de recuperación con más de 2 pasos.

**Razón:** Complejidad:
- Dificulta diagnóstico de problemas reales
- Enmascara fallas de hardware que deben ser visibles
- Aumenta superficie de bugs
- Hace código no mantenible

---

## 📊 Métricas de Éxito

### Métricas Primarias
| Métrica | Objetivo | Método de Medición |
|---------|----------|-------------------|
| Tiempo wake-to-AT | < 1 segundo | Logs: timestamp entre "Despertando" y "AT OK" |
| Tasa de éxito AT | > 99% | Contador: AT exitosos / total despertares |
| Consumo en sleep | < 1 mA | Medición con amperímetro durante deep sleep |

### Métricas Secundarias
| Métrica | Objetivo | Método de Medición |
|---------|----------|-------------------|
| Gaps en telemetría | 0 por 24h | Backend: ausencia de transmisiones > 12 min |
| Resets de watchdog | 0 por 24h | Health data: `crash_reason` != TASK_WDT |
| Tiempo de reconexión LTE | < 5 segundos | Logs: timestamp entre CFUN=1 y "Network connected" |

---

## 🔍 Casos de Uso

### CU-001: Ciclo Normal (Éxito)
**Precondición:** Módem inicializado en ciclo anterior, ESP32 despierta de deep sleep

**Flujo:**
1. ESP32 despierta
2. Sistema verifica flag "módem inicializado" = true
3. Sistema envía `AT` command
4. Módem responde `OK` en < 1s
5. Sistema continúa con lectura de sensores y transmisión

**Postcondición:** Módem permanece en estado "inicializado" para siguiente ciclo

### CU-002: Primer Boot del Sistema
**Precondición:** Sistema recién alimentado, módem nunca inicializado

**Flujo:**
1. ESP32 arranca por primera vez
2. Sistema detecta flag "módem inicializado" = false
3. Sistema ejecuta secuencia de power-on del módem (PWRKEY pulse)
4. Sistema espera boot sequence del módem (6-8 segundos)
5. Sistema verifica comunicación AT
6. Sistema marca flag "módem inicializado" = true

**Postcondición:** Módem listo, flag persistente activo para ciclos futuros

### CU-003: Módem No Responde
**Precondición:** Módem debería estar inicializado pero no responde a AT

**Flujo:**
1. Sistema envía `AT`, no hay respuesta
2. Sistema reintenta hasta 10 veces con delays de 500ms
3. Si después de 10 intentos sigue sin responder:
   - Log error crítico con diagnóstico
   - **NO** alimentar watchdog
   - Permitir watchdog reset del sistema completo

**Postcondición:** Sistema reinicia limpiamente, módem volverá a inicializarse (CU-002)

---

## 🔗 Dependencias

### Hardware
- Pin PWRKEY del módem conectado y funcional
- Pin DTR del módem conectado (opcional pero recomendado)
- Alimentación estable del módem (no debe cortarse durante sleep)

### Software
- Watchdog configurado y funcional (ver REQ-002)
- Sistema de flags persistentes (RTC memory o equivalente)
- Health data para diagnóstico (ver REQ-003)

### Documentación de Referencia
- SIM7080 AT Command Manual V1.02, página 55 (CFUN modes)
- SIM7080 AT Command Manual V1.02, página 14 (Boot sequence)
- SIM7080 Hardware Design Guide (DTR pin)

---

## ✅ Criterios de Validación

### Validación en Desarrollo
- [ ] Código compila sin warnings
- [ ] Unit tests pasan (si aplica)
- [ ] Logs muestran flujo esperado en simulación

### Validación en Campo (Mínimo 24h)
- [ ] Sin gaps > 12 minutos en telemetría
- [ ] `firmware_version` consistente en todas las transmisiones
- [ ] `boot_count` incrementa solo en power-on real
- [ ] Consumo medido < 2 mA promedio
- [ ] Tiempo wake-to-transmit < 30 segundos consistentemente

---

## 📝 Notas de Implementación

### Enfoques Sugeridos (NO PRESCRIPTIVOS)

**Opción A: CFUN Mode**
- Usar `AT+CFUN=0` (minimum functionality) durante sleep
- Ventaja: RF off, AT commands activos
- Trade-off: ~10 mA vs <1 mA

**Opción B: DTR Sleep Mode**
- Usar `AT+CSCLK=1` + DTR pin control
- Ventaja: <1 mA, wake-up rápido
- Trade-off: Requiere hardware support

**Opción C: Híbrido**
- CFUN=0 para ciclos cortos (<15 min)
- DTR para ciclos largos (>15 min)

**El implementador decide** el enfoque basado en análisis técnico detallado.

---

## 🐛 Lecciones de Intentos Anteriores

### Lo que NO funcionó en JAMR_3:
1. **Power cycling después de cada transmisión:** Causaba estado zombi
2. **Double power cycle en recovery:** Agregaba complejidad sin beneficio
3. **Boot sequence muy corto (5s):** No daba tiempo suficiente al módem
4. **Lógica de retry compleja:** Difícil de debuggear

### Insights Clave:
- El módem SIM7080 **puede** mantener estado entre ciclos si se usa correctamente
- Simplicidad > Complejidad en lógica de recuperación
- Logs detallados son críticos para diagnóstico
- Hardware issues deben ser visibles, no enmascarados con workarounds

---

**Documento creado:** 2025-10-29  
**Responsable:** Por definir  
**Revisión siguiente:** Tras implementación inicial
