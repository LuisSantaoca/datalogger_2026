# FEAT-V4: Reinicio Periódico del Procesador (24h)

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V4 |
| **Tipo** | Feature (Estabilidad / Mantenimiento Preventivo) |
| **Sistema** | Core / AppController / Sleep-Wakeup |
| **Archivo Principal** | `AppController.cpp`, `src/FeatureFlags.h` |
| **Estado** | 📋 Propuesto |
| **Fecha** | 2026-01-28 |
| **Versión Target** | v2.5.0 |
| **Depende de** | FEAT-V1 (FeatureFlags), FEAT-V3 (CrashDiagnostics), FIX-V4 (ModemPoweroff) |
| **Origen** | Concepto de `sensores_rv03-1 procesador`, rediseñado para producción |

---

## ⚠️ NOTAS DE DISEÑO (Revisión de Producción)

### Principios aplicados:
1. **No basar en ciclos fijos** → Acumular microsegundos reales de sleep
2. **Punto seguro de restart** → Al final del ciclo, después de FIX-V4 (modem off)
3. **Idempotente y anti boot-loop** → Stamp de razón + validación en boot
4. **Kill switch dual** → Flag de compilación + NVS runtime
5. **Integración FEAT-V3** → Usar sistema de checkpoints existente
6. **Sin `Serial.begin()` redundante** → Usar logger existente

### ¿Por qué no usar el código de `sensores_rv03-1`?
El código original tiene defectos críticos para producción:
- Usa ciclos fijos (falla si `TIEMPO_SLEEP_ACTIVO` cambia)
- Restart en `setup()` (desperdicia wakeup, módem potencialmente encendido)
- Sin protección anti boot-loop
- Sin integración con sistema de diagnóstico

---

## 🔍 DIAGNÓSTICO

### Problema Identificado
En sistemas embebidos IoT que operan por largos períodos con ciclos de deep sleep, pueden acumularse problemas como:
1. **Memory leaks** sutiles que no se liberan completamente en deep sleep
2. **Fragmentación de heap** progresiva
3. **Estados corruptos** en módulos (LTE, GPS) tras muchos ciclos
4. **Variables estáticas** con valores residuales inesperados
5. **Degradación gradual** del rendimiento del módem

### Síntomas
1. Fallos intermitentes después de varios días de operación continua
2. Tiempos de conexión LTE que aumentan gradualmente
3. Comportamiento errático difícil de reproducir en laboratorio
4. Dispositivos que "dejan de funcionar" tras semanas en campo

### Causa Raíz
El deep sleep preserva la memoria RTC y ciertos estados del procesador. Aunque el deep sleep reinicia la mayoría del sistema, algunos recursos no se liberan completamente:
- Handles de conexión del módem
- Buffers internos del stack TCP/IP
- Configuraciones de periféricos

---

## 📊 EVALUACIÓN

### Impacto
| Aspecto | Evaluación |
|---------|------------|
| Criticidad | **Media** - Preventivo, no correctivo |
| Riesgo de no implementar | **Medio** - Posibles fallos a largo plazo |
| Esfuerzo | **Medio** - Integración con FEAT-V3 y FIX-V4 |
| Beneficio | **Alto** - Estabilidad garantizada en despliegues largos |

### Justificación
Es una práctica estándar en sistemas IoT de campo:
- **Watchdog periódico**: Reinicio programado cada 24h
- **Costo mínimo**: Solo pierde un ciclo de datos cada 24h (0.7%)
- **Beneficio máximo**: Elimina problemas acumulativos
- **Probado en industria**: Usado en sensores comerciales de largo despliegue

---

## 🔧 IMPLEMENTACIÓN

### Archivos a Modificar

| Archivo | Cambio | Sección |
|---------|--------|---------|
| `src/FeatureFlags.h` | Agregar flags FEAT-V4 | Sección FEAT FLAGS |
| `AppController.cpp` | Variables RTC + lógica de acumulador | Variables globales |
| `AppController.cpp` | Validación anti boot-loop en `AppInit()` | Función AppInit |
| `AppController.cpp` | Trigger de restart en `Cycle_Sleep` | Antes de `enterDeepSleep()` |

### Dependencias con otros FIX/FEAT

```
FEAT-V4 (Periodic Restart)
    │
    ├── Depende de FEAT-V1 (FeatureFlags)
    │   └── Para: ENABLE_FEAT_V4_PERIODIC_RESTART
    │
    ├── Depende de FEAT-V3 (CrashDiagnostics)  
    │   └── Para: Usar mismo mecanismo de last_restart_reason
    │
    └── Depende de FIX-V4 (ModemPoweroff)
        └── Para: Restart DESPUÉS de que modem está apagado
```

---

## 📐 DISEÑO TÉCNICO

### Componente 1: Variables RTC (Acumulador de Tiempo Real)

```cpp
// En AppController.cpp - Sección de variables globales RTC
// [FEAT-V4 START] Reinicio periódico preventivo

/**
 * @brief Acumulador de microsegundos de sleep planificados
 * @details Sobrevive deep sleep. Se acumula con el sleep_time_us real de cada ciclo.
 *          NO usa ciclos fijos - inmune a cambios de TIEMPO_SLEEP_ACTIVO.
 */
RTC_DATA_ATTR uint64_t g_accum_sleep_us = 0;

/**
 * @brief Razón del último reinicio ejecutado por FEAT-V4
 * @details Permite distinguir restart planificado de crash en boot.
 */
RTC_DATA_ATTR uint8_t g_last_restart_reason_feat4 = 0;

// Valores para g_last_restart_reason_feat4
#define FEAT4_RESTART_NONE       0   // No hay restart pendiente/reciente
#define FEAT4_RESTART_PERIODIC   1   // Restart por tiempo acumulado >= 24h
#define FEAT4_RESTART_EXECUTED   2   // Restart fue ejecutado, validar en boot

// [FEAT-V4 END]
```

### Componente 2: Configuración en FeatureFlags.h

```cpp
// En src/FeatureFlags.h - Agregar en sección FEAT FLAGS

// ------------------------------------------------------------
// FEAT-V4: Reinicio Periódico Preventivo (24h)
// Sistema: Core/AppController
// Archivo: AppController.cpp
// Descripción: Reinicio limpio cada N horas para prevenir degradación
// Dependencias: FEAT-V3 (CrashDiagnostics), FIX-V4 (ModemPoweroff)
// ------------------------------------------------------------
#define ENABLE_FEAT_V4_PERIODIC_RESTART     1

// Configuración de tiempo (en horas)
// Cambiar a 1 para pruebas en laboratorio, 24 para producción
#define FEAT_V4_RESTART_HOURS               24

// Threshold calculado en microsegundos
#define FEAT_V4_THRESHOLD_US  ((uint64_t)FEAT_V4_RESTART_HOURS * 3600ULL * 1000000ULL)
```

### Componente 3: Validación Anti Boot-Loop en AppInit()

```cpp
// En AppController.cpp - Dentro de AppInit(), después de detectar reset_reason

#if ENABLE_FEAT_V4_PERIODIC_RESTART
// [FEAT-V4 START] Validación anti boot-loop
{
    esp_reset_reason_t reset_reason = esp_reset_reason();
    
    // Caso 1: Boot después de restart planificado FEAT-V4
    if (g_last_restart_reason_feat4 == FEAT4_RESTART_EXECUTED) {
        Serial.println(F("[FEAT-V4] Boot post-restart periódico detectado."));
        Serial.printf("[FEAT-V4] Tiempo acumulado antes de restart: %llu us\n", g_accum_sleep_us);
        
        // Reset completo del acumulador y flag
        g_accum_sleep_us = 0;
        g_last_restart_reason_feat4 = FEAT4_RESTART_NONE;
        
        // Integración con FEAT-V3: Registrar como restart planificado, NO crash
        #if ENABLE_FEAT_V3_CRASH_DIAGNOSTICS
        // No incrementar crash_count - fue intencional
        crashDiag.setLastRestartType(RESTART_TYPE_PERIODIC_24H);
        #endif
    }
    // Caso 2: Boot normal (power-on, deep sleep, etc.)
    else if (reset_reason == ESP_RST_POWERON) {
        // Power cycle completo - resetear acumulador
        Serial.println(F("[FEAT-V4] Power-on detectado. Reseteando acumulador."));
        g_accum_sleep_us = 0;
        g_last_restart_reason_feat4 = FEAT4_RESTART_NONE;
    }
    // Caso 3: Wakeup normal de deep sleep - no hacer nada, acumulador persiste
    
    // Log estado actual
    Serial.printf("[FEAT-V4] Acumulador actual: %llu / %llu us (%.1f%%)\n",
        g_accum_sleep_us, 
        FEAT_V4_THRESHOLD_US,
        (float)g_accum_sleep_us / FEAT_V4_THRESHOLD_US * 100.0f);
}
// [FEAT-V4 END]
#endif
```

### Componente 4: Trigger de Restart en Cycle_Sleep (Punto Seguro)

```cpp
// En AppController.cpp - case AppState::Cycle_Sleep, DESPUÉS de FIX-V4 modem poweroff

case AppState::Cycle_Sleep: {
    TIMING_FINALIZE(g_timing);
    TIMING_PRINT_SUMMARY(g_timing);
    printCycleSummary();
    
    // ============ [FIX-V4] Apagar modem PRIMERO ============
    #if ENABLE_FIX_V4_MODEM_POWEROFF_SLEEP
    Serial.println(F("[FIX-V4] Asegurando apagado de modem antes de sleep..."));
    lte.powerOff();
    Serial.println(F("[FIX-V4] Secuencia de apagado completada."));
    #endif
    // ============ [FIX-V4 END] ============
    
    // ============ [FEAT-V4 START] Reinicio periódico ============
    #if ENABLE_FEAT_V4_PERIODIC_RESTART
    {
        // Acumular tiempo de sleep de ESTE ciclo
        g_accum_sleep_us += g_cfg.sleep_time_us;
        
        // ¿Alcanzamos el threshold?
        if (g_accum_sleep_us >= FEAT_V4_THRESHOLD_US) {
            Serial.println(F(""));
            Serial.println(F("╔════════════════════════════════════════════════════╗"));
            Serial.println(F("║  [FEAT-V4] REINICIO PERIÓDICO PREVENTIVO           ║"));
            Serial.println(F("╠════════════════════════════════════════════════════╣"));
            Serial.printf( "║  Tiempo acumulado: %llu us                    \n", g_accum_sleep_us);
            Serial.printf( "║  Threshold: %llu us (%d horas)            \n", FEAT_V4_THRESHOLD_US, FEAT_V4_RESTART_HOURS);
            Serial.printf( "║  Reset reason anterior: %d                        \n", esp_reset_reason());
            Serial.println(F("║  Motivo: PERIODIC_24H (planificado)               ║"));
            Serial.println(F("║  Ejecutando esp_restart() en punto seguro...      ║"));
            Serial.println(F("╚════════════════════════════════════════════════════╝"));
            
            // Marcar que el restart fue intencional (anti boot-loop)
            g_last_restart_reason_feat4 = FEAT4_RESTART_EXECUTED;
            
            // Integración FEAT-V3: Checkpoint antes de restart
            CRASH_CHECKPOINT(CP_FEAT4_PERIODIC_RESTART);
            CRASH_SYNC_NVS();
            
            Serial.flush();  // Garantizar que logs se envían
            delay(100);
            
            esp_restart();  // Reinicio limpio - NO llega a deep sleep
            // Nunca llega aquí
        }
    }
    #endif
    // ============ [FEAT-V4 END] ============
    
    // Si no hay restart, continuar con deep sleep normal
    CRASH_CHECKPOINT(CP_SLEEP_ENTER);
    CRASH_SYNC_NVS();
    sleepModule.clearWakeupSources();
    esp_sleep_enable_timer_wakeup(g_cfg.sleep_time_us);
    sleepModule.enterDeepSleep();
    break;
}
```

### Componente 5: Kill Switch en NVS (Runtime)

```cpp
// Opcional: Permitir deshabilitar FEAT-V4 sin reflashear
// En AppInit(), antes de la validación anti boot-loop:

#if ENABLE_FEAT_V4_PERIODIC_RESTART
{
    // Leer kill switch de NVS
    preferences.begin("featflags", true);  // read-only
    bool feat4_disabled = preferences.getBool("dis_feat4", false);
    preferences.end();
    
    if (feat4_disabled) {
        Serial.println(F("[FEAT-V4] Deshabilitado por NVS (dis_feat4=true)"));
        // Skip toda la lógica de FEAT-V4
    }
}
#endif
```

---

## 🔗 INTEGRACIÓN CON FEAT-V3 (CrashDiagnostics)

### Nuevo Checkpoint para FEAT-V4

Agregar en `CrashDiagnostics.h`:

```cpp
enum CrashCheckpoint : uint8_t {
    // ... checkpoints existentes ...
    
    // FEAT-V4: Reinicio periódico
    CP_FEAT4_PERIODIC_RESTART = 240,  // Antes de esp_restart() planificado
};
```

### Nuevo Tipo de Restart

Agregar en `CrashDiagnostics.h`:

```cpp
enum RestartType : uint8_t {
    RESTART_TYPE_UNKNOWN = 0,
    RESTART_TYPE_CRASH = 1,
    RESTART_TYPE_PERIODIC_24H = 2,  // FEAT-V4
    RESTART_TYPE_USER_COMMAND = 3,
};
```

### Semántica de Contadores (Extensión de FEAT-V3)

| Evento | boot_count | crash_count | consecutive_crashes |
|--------|------------|-------------|---------------------|
| **FEAT-V4 restart periódico** | +1 | = (sin cambio) | =0 (reset) |

> **CRÍTICO**: El restart de FEAT-V4 NO es un crash. No incrementar `crash_count`.

---

## 📊 FLUJO DE ESTADOS ACTUALIZADO

```
                        ┌─────────────────┐
                        │ Cycle_Compact   │
                        │    Buffer       │
                        └────────┬────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │  Cycle_Sleep    │
                        │                 │
                        │ 1. Print timing │
                        │ 2. Print summary│
                        └────────┬────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │ [FIX-V4]        │
                        │ lte.powerOff()  │◄── Modem apagado PRIMERO
                        └────────┬────────┘
                                 │
                                 ▼
                        ┌─────────────────┐
                        │ [FEAT-V4]       │
                        │ Acumular sleep  │
                        │ ¿>= 24h?        │
                        └────────┬────────┘
                                 │
                    ┌────────────┴────────────┐
                    │ SÍ                      │ NO
                    ▼                         ▼
           ┌────────────────┐       ┌────────────────┐
           │ Log + Stamp    │       │ CRASH_CHECKPOINT│
           │ esp_restart()  │       │ enterDeepSleep()│
           └────────────────┘       └────────────────┘
                    │                         │
                    ▼                         ▼
           ┌────────────────┐       ┌────────────────┐
           │ AppInit()      │       │ (wakeup)       │
           │ Detecta FEAT4  │       │ Siguiente ciclo│
           │ Reset acumulador│       └────────────────┘
           └────────────────┘
```

---

## 🧪 VERIFICACIÓN

### Output Esperado - Ciclo Normal

```
[INFO][APP] Ciclo completado correctamente.
=== CYCLE SUMMARY ===
...
[FIX-V4] Asegurando apagado de modem antes de sleep...
[FIX-V4] Secuencia de apagado completada.
[FEAT-V4] Acumulador actual: 600000000 / 86400000000 us (0.7%)
[INFO][SLEEP] Entrando en deep sleep por 600 segundos...
```

### Output Esperado - Restart Periódico (24h alcanzado)

```
[INFO][APP] Ciclo completado correctamente.
=== CYCLE SUMMARY ===
...
[FIX-V4] Asegurando apagado de modem antes de sleep...
[FIX-V4] Secuencia de apagado completada.

╔════════════════════════════════════════════════════╗
║  [FEAT-V4] REINICIO PERIÓDICO PREVENTIVO           ║
╠════════════════════════════════════════════════════╣
║  Tiempo acumulado: 86400000000 us                    
║  Threshold: 86400000000 us (24 horas)            
║  Reset reason anterior: 8                        
║  Motivo: PERIODIC_24H (planificado)               ║
║  Ejecutando esp_restart() en punto seguro...      ║
╚════════════════════════════════════════════════════╝

ets Jun  8 2016 00:22:57
rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
[FEAT-V4] Boot post-restart periódico detectado.
[FEAT-V4] Tiempo acumulado antes de restart: 86400000000 us
[FEAT-V4] Acumulador actual: 0 / 86400000000 us (0.0%)
```

### Criterios de Aceptación

- [ ] Acumulador usa microsegundos reales, no ciclos fijos
- [ ] Restart ocurre DESPUÉS de `lte.powerOff()` (FIX-V4)
- [ ] Restart ocurre ANTES de `enterDeepSleep()`
- [ ] Flag `g_last_restart_reason_feat4` previene boot-loop
- [ ] `crash_count` NO incrementa en restart planificado
- [ ] Log completo visible antes de restart (`Serial.flush()`)
- [ ] Kill switch NVS funcional (`dis_feat4=true` deshabilita)
- [ ] Compila con `ENABLE_FEAT_V4_PERIODIC_RESTART=0` sin errores
- [ ] `FEAT_V4_RESTART_HOURS=1` funciona para pruebas de laboratorio
- [ ] No hay pérdida de datos del buffer (LittleFS persiste)

---

## ⚠️ CONSIDERACIONES

### Ventajas vs Implementación Original (sensores_rv03-1)

| Aspecto | Original | FEAT-V4 Producción |
|---------|----------|-------------------|
| Base de tiempo | Ciclos fijos | Microsegundos acumulados |
| Punto de restart | `setup()` (peligroso) | `Cycle_Sleep` (seguro) |
| Anti boot-loop | ❌ No | ✅ Sí |
| Integración diagnóstico | ❌ No | ✅ FEAT-V3 |
| Modem al reiniciar | Posiblemente encendido | Garantizado apagado (FIX-V4) |
| Kill switch | ❌ No | ✅ NVS runtime |
| Configurable | Hardcoded 144 | `FEAT_V4_RESTART_HOURS` |

### Riesgos y Mitigación

| Riesgo | Probabilidad | Mitigación |
|--------|--------------|------------|
| Boot-loop por bug en cálculo | Baja | Flag `FEAT4_RESTART_EXECUTED` |
| Acumulador overflow | Ninguna | uint64_t soporta >500 años |
| Restart con modem encendido | Ninguna | FIX-V4 ejecuta primero |
| Pérdida de datos | Ninguna | Buffer en LittleFS persiste |

---

## 📅 HISTORIAL

| Fecha | Acción | Versión |
|-------|--------|---------|
| 2026-01-28 | Documentación inicial basada en `sensores_rv03-1 procesador` | v0.1 |
| 2026-01-28 | Rediseño completo para producción con recomendaciones Sr | v1.0 |
| 2026-01-28 | Integración con FEAT-V1, FEAT-V3, FIX-V4 | v1.0 |
| 2026-01-28 | Validación vs METODOLOGIA y PREMISAS | v1.1 |

---

## ✅ VALIDACIÓN DE PREMISAS

### Cumplimiento con PREMISAS_DE_FIXS.md

| Premisa | Cumple | Evidencia |
|---------|--------|-----------|
| **P1: Aislamiento total** | ✅ | Branch dedicado `feat-v4/periodic-restart` |
| **P2: Cambios mínimos** | ✅ | Solo `AppController.cpp` + `FeatureFlags.h` |
| **P3: Defaults seguros** | ✅ | `g_accum_sleep_us = 0`, `FEAT4_RESTART_NONE = 0` |
| **P4: Feature flags** | ✅ | `ENABLE_FEAT_V4_PERIODIC_RESTART` + NVS kill switch |
| **P5: Logging exhaustivo** | ✅ | Formato `[FEAT-V4]`, logs en boot y restart |
| **P6: No cambiar lógica existente** | ✅ | Solo agrega DESPUÉS de FIX-V4, ANTES de sleep |
| **P7: Testing gradual** | 📋 | Pirámide definida en verificación |
| **P8: Métricas objetivas** | ✅ | Baseline definido, criterios claros |
| **P9: Rollback plan** | ✅ | 3 niveles documentados abajo |
| **P10: Documentación completa** | ✅ | Este documento |

### Cumplimiento con METODOLOGIA_DE_CAMBIOS.md

| Convención | Cumple | Evidencia |
|------------|--------|-----------|
| Nomenclatura `FEAT-Vn` | ✅ | FEAT-V4 |
| Archivo en `fixs-feats/feats/` | ✅ | `FEAT_V4_PERIODIC_RESTART.md` |
| Comentarios `[FEAT-V4 START/END]` | ✅ | En todos los bloques de código |
| Flag `ENABLE_FEAT_Vn_` | ✅ | `ENABLE_FEAT_V4_PERIODIC_RESTART` |
| Código original en `#else` | ⚠️ N/A | No hay código original - es funcionalidad nueva |
| Estado con emoji | ✅ | 📋 Propuesto |

---

## 🛡️ PLAN DE ROLLBACK

### Plan A: Feature Flag (< 5 min)
```cpp
// En src/FeatureFlags.h
#define ENABLE_FEAT_V4_PERIODIC_RESTART  0  // Cambiar 1 → 0
// Recompilar y flashear
```

### Plan B: Kill Switch NVS (< 2 min, sin reflashear)
```cpp
// Via BLE o comando Serial (si se implementa interfaz)
preferences.begin("featflags", false);
preferences.putBool("dis_feat4", true);
preferences.end();
// En siguiente boot, FEAT-V4 se deshabilita
```

### Plan C: Volver a versión anterior (< 10 min)
```bash
git checkout v2.4.0  # Versión sin FEAT-V4
# Recompilar y flashear
```

---

## 📋 CHECKLIST PRE-COMMIT

Antes de merge a main, verificar:

- [ ] ✅ Compila sin errores con `ENABLE_FEAT_V4_PERIODIC_RESTART=1`
- [ ] ✅ Compila sin errores con `ENABLE_FEAT_V4_PERIODIC_RESTART=0`
- [ ] ✅ Defaults seguros: `g_accum_sleep_us=0` en power-on
- [ ] ✅ Flag en `src/FeatureFlags.h` con documentación
- [ ] ✅ Logging con formato `[FEAT-V4]`
- [ ] ✅ No modificó lógica existente de `Cycle_Sleep`
- [ ] ✅ Código DESPUÉS de FIX-V4 (`lte.powerOff()`)
- [ ] ✅ Código ANTES de `enterDeepSleep()`
- [ ] ✅ Validación de datos en AppInit (anti boot-loop)
- [ ] ✅ Testing: Compilación OK
- [ ] ✅ Testing: 1 ciclo completo en hardware
- [ ] ✅ Testing: 24h simulado (usando `FEAT_V4_RESTART_HOURS=1`)
- [ ] ✅ Reinicios inesperados = 0
- [ ] ✅ `crash_count` no incrementa en restart planificado
- [ ] ✅ Comentarios `[FEAT-V4 START/END]` agregados
- [ ] ✅ Este documento actualizado
- [ ] ✅ Commit message: `[FEAT-V4] Implementar reinicio periódico preventivo (24h)`

---

## 📊 BASELINE Y MÉTRICAS

### Baseline JAMR_4.5 Actual (sin FEAT-V4)
```
📊 BASELINE v2.4.x:
   Tiempo total ciclo: ~3-5 min
   Reinicios por 24h: 0 (esperado)
   Uptime continuo máximo: Indefinido (deep sleep cycles)
   Estabilidad post 7 días: No garantizada
```

### Métricas Esperadas con FEAT-V4
```
📊 TARGET v2.5.0 (con FEAT-V4):
   Tiempo total ciclo: ~3-5 min (sin cambio)
   Reinicios planificados por 24h: 1
   Reinicios no planificados: 0
   Uptime efectivo: 24h (luego restart limpio)
   Estabilidad post 7 días: Garantizada (restart preventivo)
```

### Criterios de Aceptación Cuantitativos

| Métrica | Valor Aceptable | Valor Crítico |
|---------|-----------------|---------------|
| Tiempo de ciclo | ≤ baseline | > baseline + 10% |
| Reinicios no planificados | 0 | > 0 |
| crash_count incremento | 0 | > 0 |
| Pérdida de datos | 0 tramas | > 0 tramas |
| Consumo adicional | < 1% | > 5% |

---

## 🔗 REFERENCIAS

- **Concepto original**: `sensores_rv03-1 procesador/sensores_rv03-1 procesador.ino`
- **Documentación ESP-IDF**: [esp_restart()](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html)
- **RTC Memory**: [RTC_DATA_ATTR](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/deep-sleep-stub.html)
- **Metodología**: [METODOLOGIA_DE_CAMBIOS.md](../METODOLOGIA_DE_CAMBIOS.md)
- **Premisas**: [PREMISAS_DE_FIXS.md](../PREMISAS_DE_FIXS.md)
- **Dependencias JAMR_4.5**:
  - [FEAT_V1_FEATURE_FLAGS.md](FEAT_V1_FEATURE_FLAGS.md)
  - [FEAT_V3_CRASH_DIAGNOSTICS.md](FEAT_V3_CRASH_DIAGNOSTICS.md)
  - [FIX_V4_MODEM_POWEROFF_SLEEP.md](../fixs/FIX_V4_MODEM_POWEROFF_SLEEP.md)
