# Aprendizajes de JAMR_4.4 para Sensores RV03

**Documento creado:** 2026-01-07  
**Fuente:** Análisis profundo de JAMR_4.4 (versión 4.4.16)  
**Objetivo:** Documentar lecciones aprendidas que complementan la memoria existente

---

## 🎯 Contexto

JAMR_4.4 es un firmware maduro con 16+ fixes implementados en producción. Este documento extrae aprendizajes **no redundantes** con `MEMORIA_SENSORES_RV03.md` que deben considerarse para la evolución de Sensores RV03.

---

## 📐 PREMISAS DE DESARROLLO (JAMR_4.4)

JAMR_4.4 documenta 10 premisas fundamentales en `fixs/PREMISAS_DE_FIXS.md`. Las más relevantes son:

### Premisa #1: Aislamiento Total
> **"Si no lo toco, no lo rompo. Si lo toco, lo valido. Si falla, lo deshabilito."**

- Un branch por fix, un fix por branch
- Nunca trabajar directo en `main`
- Merge solo después de validación completa

**Aplicación en RV03:** Cada fix debe tener su branch `fix/vN-nombre`

---

### Premisa #3: Defaults Seguros (Fail-Safe)
Si el fix falla, el dispositivo debe comportarse como la versión anterior estable.

```cpp
// ✅ BIEN: Valores que permiten operación normal
ModemPersistentState persistentState = {
  15,      // lastRSSI - Conservador pero funcional
  4,       // lastSuccessfulBand - Band 4 común
  0,       // consecutiveFailures - Sin penalización
};

// ❌ MAL: Valores que pueden causar problemas
ModemPersistentState persistentState = {
  0,       // lastRSSI - ¡Imposible!
  999,     // lastSuccessfulBand - ¡No existe!
};
```

**Aplicación en RV03:** Revisar todos los valores por defecto en NVS/Preferences.

---

### Premisa #4: Feature Flags
Cada fix debe poder deshabilitarse en tiempo de compilación.

```cpp
#define ENABLE_FIX_V1_PDP_REDUNDANTE true  // Cambiar a false para rollback

#if ENABLE_FIX_V1_PDP_REDUNDANTE
  // Código nuevo
#else
  // Código legacy
#endif
```

**Aplicación en RV03:** Crear `src/FeatureFlags.h` con flags para cada fix.

---

### Premisa #6: No Cambiar Lógica Existente
El código que funciona en producción no se toca. Fixes agregan funcionalidad, no reemplazan.

```cpp
// 🆕 FIX-N: Intentar optimización primero
if (ENABLE_FIX_N && condicion_nueva) {
  // Lógica nueva
  if (exito) goto continuar;
}

// Lógica ORIGINAL sin modificar (fallback siempre disponible)
codigo_original();

continuar:
// Resto del flujo
```

**Aplicación en RV03:** Preservar `resetModem()` original, agregar condicional.

---

### Premisa #7: Testing Gradual (Pirámide)

```
         ┌─────────────┐
         │  Campo 7d   │  ← Validación final
         └─────────────┘
        ┌───────────────┐
        │  Hardware 24h │  ← Ciclos reales
        └───────────────┘
      ┌───────────────────┐
      │  Hardware 1 ciclo │  ← Funcionalidad completa
      └───────────────────┘
    ┌─────────────────────────┐
    │  Test unitario (5 min)  │  ← Función específica
    └─────────────────────────┘
  ┌───────────────────────────────┐
  │  Compilación (2 min)          │  ← Sin errores
  └───────────────────────────────┘
```

**Aplicación en RV03:** No desplegar sin pasar las 5 capas.

---

## 🔧 SISTEMAS IMPLEMENTADOS EN JAMR_4.4

### 1. Presupuesto de Ciclo de Comunicación (FIX-6)

JAMR_4.4 implementa un **budget global** para evitar ciclos infinitos:

```cpp
// gsm_comm_budget.h
void resetCommunicationCycleBudget();      // Al inicio del ciclo LTE
uint32_t remainingCommunicationCycleBudget(); // Tiempo restante
bool ensureCommunicationBudget(const char* tag); // Verificar antes de operación

// Uso
if (!ensureCommunicationBudget("TCP_SEND")) {
  logMessage(1, "Presupuesto agotado, abortando");
  return false;
}
```

**Relevancia para RV03:** El problema de PDP redundante consume presupuesto innecesariamente. Considerar implementar budget global.

**Valores de JAMR_4.4:**
- `COMM_CYCLE_BUDGET_MS = 120000` (2 minutos total)
- `AUTO_LITE_BUDGET_MS = 30000` (30s para camino rápido)
- `DEFAULT_CATM_BUDGET_MS = 65000` (65s para camino DEFAULT)

---

### 2. Health del Módem (FIX-8)

Sistema para detectar y recuperar de estados "zombie" del módem:

```cpp
// gsm_health.h
typedef enum {
  MODEM_HEALTH_OK,
  MODEM_HEALTH_TRYING,
  MODEM_HEALTH_ZOMBIE_DETECTED,
  MODEM_HEALTH_FAILED
} modem_health_state_t;

void modemHealthReset();
void modemHealthRegisterTimeout(const char* tag);
bool modemHealthAttemptRecovery(const char* tag);
bool modemHealthShouldAbortCycle(const char* tag);
```

**Comportamiento:**
- 1 timeout → `MODEM_HEALTH_TRYING`
- 3+ timeouts → `MODEM_HEALTH_ZOMBIE_DETECTED`
- Intenta recuperación profunda (apagar/encender RF)
- Si falla recuperación → `MODEM_HEALTH_FAILED`, aborta ciclo

**Relevancia para RV03:** RV03 no tiene detección de estados zombie. Si el modem queda en estado inconsistente, puede quedar bloqueado.

---

### 3. Perfil LTE Persistente (FIX-7)

Similar al sistema de RV03 pero con diferencias:

```cpp
// Archivo en LittleFS (no NVS)
static const char* LTE_PROFILE_FILE = "/lte_profile.cfg";

static void loadPersistedOperatorId() {
  if (!LittleFS.exists(LTE_PROFILE_FILE)) return;
  File f = LittleFS.open(LTE_PROFILE_FILE, "r");
  // ... lectura y validación
}

static void persistOperatorId(int8_t id) {
  File f = LittleFS.open(LTE_PROFILE_FILE, "w");
  f.println(id);
  f.close();
}
```

**Diferencia con RV03:**
- JAMR usa archivo en LittleFS
- RV03 usa NVS (Preferences)

**Recomendación:** NVS es más robusto para datos pequeños. RV03 tiene la mejor práctica.

---

### 4. Versionamiento en Payload (REQ-004)

JAMR_4.4 incluye versión de firmware en cada transmisión:

```cpp
// En type_def.h
typedef struct {
  // ... otros campos ...
  byte fw_major;   // Versión major (X.0.0)
  byte fw_minor;   // Versión minor (0.Y.0)
  byte fw_patch;   // Versión patch (0.0.Z)
} sensordata_type;

// En JAMR_4.4.ino
sensordata.fw_major = FIRMWARE_VERSION_MAJOR;  // 4
sensordata.fw_minor = FIRMWARE_VERSION_MINOR;  // 4
sensordata.fw_patch = FIRMWARE_VERSION_PATCH;  // 16
```

**Relevancia para RV03:** RV03 imprime versión en Serial pero NO la incluye en la trama enviada. El servidor no sabe qué versión tiene cada dispositivo.

**Acción sugerida:** Agregar `fw_version` al formato de trama de RV03.

---

### 5. Health Data en Payload (FIX-4)

JAMR_4.4 envía diagnósticos en cada transmisión:

```cpp
typedef struct {
  // ... sensores ...
  byte health_checkpoint;        // Último checkpoint alcanzado
  byte health_crash_reason;      // Código de motivo de crash
  byte H_health_boot_count;      // Boot count (byte alto)
  byte L_health_boot_count;      // Boot count (byte bajo)
  byte H_health_crash_ts;        // Timestamp crash
  byte L_health_crash_ts;
} sensordata_type;
```

**Checkpoints:**
```cpp
#define CP_BOOT           0x01
#define CP_GPIO_OK        0x02
#define CP_SENSORS_OK     0x03
#define CP_GPS_FIX        0x04
#define CP_GSM_OK         0x05
#define CP_LTE_CONNECT    0x06
#define CP_DATA_SENT      0x07
```

**Relevancia para RV03:** RV03 no reporta health al servidor. Si falla en campo, no hay forma de saber en qué fase falló.

---

## ⚠️ HALLAZGOS CRÍTICOS (QA JAMR_4.4)

Del reporte de calidad `REPORTE_CRITICO_TRANSMISION_2025-11-26.md`:

### 1. Power-cycle en cada ciclo
JAMR_4.4 apaga completamente el modem después de cada transmisión, lo que viola el requisito de "wake-to-AT < 1s".

**RV03 tiene el mismo problema:** `resetModem()` ejecuta `AT+CFUN=1,1` que reinicia la radio.

---

### 2. Buffer no es power-safe
Si ocurre reset durante escritura del buffer, se puede corromper.

**RV03 debe verificar:** ¿`BUFFERModule` usa escritura atómica (temp + rename)?

---

### 3. Sin confirmación de backend
"SEND OK" solo garantiza que el modem puso datos en buffer TCP, no que llegaron al servidor.

**RV03 tiene el mismo riesgo:** Marca como enviado al recibir "OK" del modem, no ACK del servidor.

---

## 📋 ACCIONES RECOMENDADAS PARA RV03

### Prioridad Alta
1. [ ] Implementar Feature Flags (`FeatureFlags.h`)
2. [ ] Agregar versión de firmware a la trama enviada
3. [ ] Implementar health checkpoints básicos

### Prioridad Media
4. [ ] Implementar presupuesto de ciclo (`CommBudget`)
5. [ ] Agregar detección de modem zombie
6. [ ] Verificar escritura atómica en BUFFERModule

### Prioridad Baja (Futuro)
7. [ ] Implementar ACK de backend antes de marcar como enviado
8. [ ] Modo bajo consumo del modem sin power-cycle completo

---

## 📊 Comparación JAMR_4.4 vs RV03

| Feature | JAMR_4.4 | RV03 v2.0 | Acción |
|---------|----------|-----------|--------|
| Versión en payload | ✅ | ❌ | Implementar |
| Health checkpoints | ✅ | ❌ | Implementar básico |
| Presupuesto ciclo | ✅ | ❌ | Evaluar necesidad |
| Detección zombie | ✅ | ❌ | Evaluar necesidad |
| Operadora persistente | ✅ (LittleFS) | ✅ (NVS) | RV03 mejor |
| Feature flags | ✅ | ❌ | Implementar |
| Buffer resiliente | ⚠️ | ✅ | RV03 mejor |
| GPS persistente | ⚠️ | ✅ | RV03 mejor |
| Documentación fixes | ✅ Excelente | ✅ Buena | Mantener |
| Testing gradual | ✅ | ❌ | Adoptar pirámide |

---

## 🔑 Conclusiones

1. **JAMR_4.4 tiene más fixes pero también más complejidad.** RV03 es más simple y eso es una ventaja.

2. **Las premisas de desarrollo de JAMR_4.4 son excelentes** y deben adoptarse en RV03.

3. **Feature flags son críticos** para poder hacer rollback rápido de fixes.

4. **Versión en payload es importante** para tracking en producción.

5. **RV03 tiene mejores prácticas en:**
   - Buffer resiliente (marca procesados, no borra hasta confirmar)
   - GPS persistente (NVS con coordenadas)
   - Modularidad (arquitectura más limpia)

6. **RV03 puede mejorar en:**
   - Presupuesto de ciclo
   - Health tracking
   - Detección de estados zombie
