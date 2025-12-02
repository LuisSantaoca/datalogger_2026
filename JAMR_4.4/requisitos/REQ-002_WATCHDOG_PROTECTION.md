# REQ-002: Protección contra Cuelgues del Sistema (Watchdog)

**Versión:** 1.0  
**Fecha:** 2025-10-29  
**Prioridad:** CRÍTICA  
**Estado:** PENDIENTE

---

## 🎯 Objetivo (QUÉ)

El sistema **DEBE** recuperarse automáticamente de cualquier cuelgue o bloqueo sin intervención humana, garantizando continuidad operativa en despliegues remotos.

---

## 📋 Requisitos Funcionales

### RF-001: Detección de Cuelgues
El sistema **DEBE** detectar automáticamente cuando el código se bloquea o entra en loop infinito.

**Criterio de aceptación:**
- Timeout máximo configurable (recomendado: 120 segundos)
- Detección funciona en cualquier punto del código
- No requiere modificación de librerías externas

### RF-002: Reset Automático
Cuando se detecta un cuelgue, el sistema **DEBE** reiniciarse automáticamente de forma limpia.

**Criterio de aceptación:**
- Reset ocurre antes de causar pérdida de datos críticos
- Sistema vuelve a estado funcional después del reset
- Proceso de reset no causa corrupción de filesystem o memoria

### RF-003: Prevención Proactiva
El código **DEBE** indicar regularmente que está ejecutándose correctamente para evitar resets innecesarios.

**Criterio de aceptación:**
- Puntos estratégicos de "alimentación" del watchdog distribuidos en código
- Cada operación larga (>5s) debe incluir alimentación
- Loops y esperas deben alimentar watchdog en cada iteración

### RF-004: Configuración Segura
El watchdog **DEBE** configurarse de manera que no cause resets espurios durante operaciones legítimas.

**Criterio de aceptación:**
- Timeout > duración de operación más larga esperada
- Margen de seguridad: timeout ≥ 2× operación más larga
- Ejemplo: Si operación más larga = 60s → timeout ≥ 120s

---

## 🚫 Anti-Requisitos (QUÉ NO HACER)

### ANR-001: NO Confiar en Watchdog para Flujo Normal
**PROHIBIDO:** Usar watchdog timeout como mecanismo de control de flujo o timing.

**Razón:**
- Watchdog es mecanismo de recuperación de errores, no de control
- Reset de watchdog indica **fallo del sistema**, debe ser excepcional
- Diseño correcto: sistema nunca debería alcanzar timeout en operación normal

### ANR-002: NO Alimentar Watchdog en Puntos Aleatorios
**PROHIBIDO:** Agregar alimentación de watchdog sin análisis de flujo de ejecución.

**Razón:**
- Puede enmascarar bugs reales
- Dificulta estimación de tiempos de ejecución
- Debe haber estrategia clara de dónde y por qué alimentar

### ANR-003: NO Ignorar Resets de Watchdog
**PROHIBIDO:** Considerar resets de watchdog como "normales" o no investigarlos.

**Razón:**
- Cada reset indica bug o problema de diseño
- Debe ser medido, logueado y analizado
- Meta: 0 resets de watchdog en operación normal (24h+)

---

## 📊 Métricas de Éxito

### Métricas Primarias
| Métrica | Objetivo | Método de Medición |
|---------|----------|-------------------|
| Resets de watchdog | 0 en 24h | Health data: `crash_reason == TASK_WDT` |
| Tiempo máximo sin feed | < 50% timeout | Instrumentación de código |
| Recuperación post-reset | 100% | Logs: sistema funcional tras reset |

### Métricas Secundarias
| Métrica | Objetivo | Método de Medición |
|---------|----------|-------------------|
| Operaciones largas identificadas | 100% | Code review: todas tienen feeds |
| Loops protegidos | 100% | Code review: todos tienen feeds |
| Timeout adecuado | ≥2× operación larga | Análisis timing |

---

## 🔍 Casos de Uso

### CU-001: Operación Normal
**Precondición:** Sistema ejecutando código sin errores

**Flujo:**
1. Sistema inicia operación (ej: conectar LTE)
2. En intervalos regulares (<50% timeout), código alimenta watchdog
3. Operación completa exitosamente
4. Sistema continúa a siguiente fase

**Postcondición:** Watchdog nunca alcanza timeout, sistema continúa normalmente

**Ejemplo:**
```
[0s]    Iniciar conexión LTE
[2s]    Feed watchdog (checkpoint 1)
[15s]   Feed watchdog (checkpoint 2)
[30s]   Feed watchdog (checkpoint 3)
[45s]   Conexión establecida
[45s]   Feed watchdog (checkpoint 4)
```

### CU-002: Cuelgue en Operación Externa
**Precondición:** Sistema esperando respuesta de módem que no llega

**Flujo:**
1. Sistema envía comando AT al módem
2. Sistema espera respuesta (con timeout)
3. Módem no responde (hardware issue)
4. Código entra en loop esperando
5. **Sin feeds de watchdog, timer incrementa**
6. A los 120s: Watchdog dispara reset
7. Sistema reinicia desde boot
8. Health data registra: `crash_reason = TASK_WDT`

**Postcondición:** Sistema recuperado, operativo de nuevo, incidente logueado

### CU-003: Bug en Loop Infinito
**Precondición:** Bug en código causa loop sin escape

**Flujo:**
1. Sistema ejecuta función con bug
2. Loop infinito: `while(true) { /* sin break ni feeds */ }`
3. **Watchdog no es alimentado**
4. A los 120s: Watchdog dispara reset
5. Sistema reinicia
6. Loop vuelve a ocurrir
7. Patrón se repite

**Postcondición:** Health data muestra múltiples `TASK_WDT` → indica bug crítico para arreglar

**Nota:** Este caso indica **fallo de diseño** que debe corregirse en código, no ajuste de watchdog.

---

## 🔗 Dependencias

### Hardware
- ESP32-S3 con soporte de watchdog de hardware
- Memoria RTC para persistir causa de reset (ver REQ-003)

### Software
- ESP-IDF v5.3+ (para API moderna de watchdog)
- Health data system para registrar causa de resets

### Incompatibilidades Conocidas
- Algunas librerías de terceros pueden tener operaciones bloqueantes largas
- Actualización OTA puede necesitar timeout extendido temporalmente

---

## ✅ Criterios de Validación

### Validación en Desarrollo
- [ ] Watchdog configurado con timeout documentado
- [ ] Mapa de feeds de watchdog documentado (dónde y por qué)
- [ ] Operación más larga identificada y medida
- [ ] Timeout ≥ 2× operación más larga

### Validación en Pruebas
- [ ] Test: desconectar módem causa reset después de timeout
- [ ] Test: operación normal nunca alcanza timeout en 1 hora
- [ ] Test: sistema recupera funcionalidad después de reset forzado

### Validación en Campo (24h mínimo)
- [ ] 0 resets de watchdog en operación normal
- [ ] Si hay reset: sistema se recupera y continúa operando
- [ ] Health data registra correctamente causa de resets

---

## 📝 Notas de Implementación

### Operaciones Críticas que Requieren Feeds

| Operación | Duración Típica | Estrategia de Feed |
|-----------|----------------|-------------------|
| Conexión LTE | 30-60s | Feed cada 5-10s |
| Obtención GPS fix | 60-180s | Feed cada 10-15s |
| Envío de datos TCP | 5-15s | Feed cada 2-3s |
| Comandos AT individuales | 1-5s | Feed después del comando |
| Loop de retry | Variable | Feed en cada iteración |

### Puntos Estratégicos de Feed

1. **Inicio de cada función mayor** (GPS, LTE, send data)
2. **Dentro de loops** (especialmente `while` con condición externa)
3. **Después de comandos AT** con timeout >5s
4. **Durante delays largos** (fragmentar en chunks de 500ms con feeds)

### Cálculo de Timeout

```
Operaciones identificadas:
- GPS fix: hasta 180s (worst case)
- LTE connection: hasta 60s
- TCP send: hasta 15s

Operación más larga: 180s (GPS)
Timeout recomendado: 180s × 2 = 360s

Sin embargo, con feeds estratégicos cada 10-15s dentro de GPS:
- Timeout puede reducirse a 30-60s
- Más seguro sin sacrificar recovery rápida
```

**Recomendación Final:** Timeout = 120s con feeds cada 10-15s en operaciones largas

---

## 🐛 Lecciones de Intentos Anteriores

### Lo que SÍ funcionó en JAMR_3:
1. **Feeds en loops de espera:** Evitó resets durante conexión LTE lenta
2. **Timeout de 120s:** Balance adecuado entre seguridad y recovery rápida
3. **Feeds después de comandos AT largos:** Crítico para comandos con delays internos

### Lo que necesita mejora:
1. **Documentación de feeds:** No estaba claro por qué cada feed existía
2. **Estrategia de fragmentación:** Delays largos deben fragmentarse consistentemente
3. **Testing de edge cases:** No se probó suficientemente módem no respondiendo

---

## 📚 Referencias Técnicas

- ESP-IDF Watchdog Timer API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html
- ESP32-S3 Technical Reference Manual, sección 3.2.7 (Watchdog Timers)
- Best Practices for Embedded Watchdog: https://betterembsw.blogspot.com/2014/05/watchdog-timer-best-practices.html

---

**Documento creado:** 2025-10-29  
**Responsable:** Por definir  
**Revisión siguiente:** Tras implementación inicial
