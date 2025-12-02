# 🗺️ ROADMAP DE IMPLEMENTACIÓN - FIXES v4.2.x

**Fecha:** 31 Oct 2025  
**Versión Base:** v4.2.0-JAMR4-VERSION  
**Objetivo:** Operación confiable en señal baja (RSSI 8-14)

---

## 📊 ORDEN DE IMPLEMENTACIÓN (Por Dependencias)

### FASE 1: BASE (Semana 1)
Fixes que son base para otros o de bajo riesgo.

#### **v4.2.1 - FIX #1: Persistencia Estado** 🔴 CRÍTICA
- **Orden:** #1 (PRIMERO - Base para #2, #5, #6)
- **Tiempo:** 2h implementación + 1h testing
- **Riesgo:** ⚪ Bajo (solo NVS, no modifica lógica)
- **Depende de:** Nada
- **Bloquea a:** v4.2.2, v4.2.5, v4.2.6
- **Carpeta:** `v4.2.1_persistencia/`
- **Impacto:** -20s por ciclo post-reinicio

**✅ Se implementa PRIMERO porque:**
- Otros fixes necesitan `persistentState`
- Bajo riesgo, alto beneficio
- No depende de nada

---

#### **v4.2.3 - FIX #3: Init Módem Optimizado** 🟠 ALTA
- **Orden:** #2 (Independiente, bajo riesgo)
- **Tiempo:** 2h
- **Riesgo:** ⚪ Bajo (solo delays y timeouts)
- **Depende de:** Nada (pero mejor con v4.2.1)
- **Bloquea a:** Nada
- **Impacto:** -15s por ciclo

**✅ Se implementa SEGUNDO porque:**
- Independiente de otros fixes
- Bajo riesgo (solo ajustes de timing)
- Beneficio inmediato

---

#### **v4.2.4 - FIX #4: Higiene Sockets TCP** 🟠 ALTA
- **Orden:** #3 (Independiente)
- **Tiempo:** 3h
- **Riesgo:** 🟡 Medio (modifica lógica TCP)
- **Depende de:** Nada
- **Bloquea a:** Nada
- **Impacto:** +5% tasa éxito TCP

**✅ Se implementa TERCERO porque:**
- Independiente de otros fixes
- Riesgo medio → implementar antes de fixes complejos
- No bloquea a otros

---

### FASE 2: ADAPTATIVO (Semana 2)
Fixes que usan persistencia y son más complejos.

#### **v4.2.2 - FIX #2: Timeout LTE Dinámico** 🔴 CRÍTICA
- **Orden:** #4 (Depende de v4.2.1)
- **Tiempo:** 3h
- **Riesgo:** 🟡 Medio (modifica lógica LTE)
- **Depende de:** v4.2.1 (necesita `persistentState.lastRSSI`)
- **Bloquea a:** Nada
- **Impacto:** -90% timeouts en RSSI bajo

**✅ Se implementa CUARTO porque:**
- Necesita v4.2.1 funcionando
- Riesgo medio → después de fixes independientes
- Alto impacto cuando está v4.2.1

---

#### **v4.2.5 - FIX #5: UART Robusto** 🔴 CRÍTICA
- **Orden:** #5 (Depende de v4.2.1)
- **Tiempo:** 2h
- **Riesgo:** 🟡 Medio (modifica comunicación UART)
- **Depende de:** v4.2.1 (usa fallback a `persistentState.lastRSSI`)
- **Bloquea a:** Nada
- **Impacto:** Elimina 100% RSSI=99

**✅ Se implementa QUINTO porque:**
- Necesita v4.2.1 para fallback
- Riesgo medio → validar v4.2.1 primero

---

### FASE 3: OPTIMIZACIONES (Semana 3+)
Fixes opcionales o de menor prioridad.

#### **v4.2.6 - FIX #6: Banda LTE Inteligente** 🟡 MEDIA
- **Orden:** #6
- **Tiempo:** 4h
- **Riesgo:** 🟡 Medio
- **Depende de:** v4.2.1 (necesita `persistentState.lastSuccessfulBand`)
- **Impacto:** -25s búsqueda banda

#### **v4.2.7 - FIX #7: Detección Degradación** 🟡 MEDIA
- **Orden:** #7
- **Tiempo:** 4h
- **Riesgo:** 🟡 Medio
- **Depende de:** v4.2.1
- **Impacto:** Preventivo

#### **v4.2.8 - FIX #8: GPS Cache** 🟢 BAJA
- **Orden:** #8
- **Tiempo:** 2h
- **Riesgo:** ⚪ Bajo
- **Depende de:** v4.2.1
- **Impacto:** -20s GPS

#### **v4.2.9 - FIX #9: Fallback NB-IoT** 🟢 BAJA
- **Orden:** #9
- **Tiempo:** 3h
- **Riesgo:** 🟡 Medio
- **Depende de:** Nada
- **Impacto:** +3% en casos extremos

#### **v4.2.10 - FIX #10: Métricas Remotas** 🟢 OPCIONAL
- **Orden:** #10
- **Tiempo:** 6h
- **Riesgo:** ⚪ Bajo
- **Depende de:** Nada
- **Impacto:** Diagnóstico

---

## 🎯 ESTRATEGIA DE IMPLEMENTACIÓN

### Principios:

1. **Dependencias primero:** v4.2.1 se implementa antes que v4.2.2, v4.2.5, v4.2.6
2. **Bajo riesgo primero:** v4.2.3 y v4.2.4 antes que v4.2.2
3. **Validación gradual:** Cada fix se valida 24-48h antes del siguiente
4. **Rollback fácil:** Cada versión es independiente

### Semana 1 (Fixes #1-3):
```
Día 1-2: v4.2.1 (Persistencia) → Validar 24h
Día 3: v4.2.3 (Init Módem) → Validar 24h
Día 4: v4.2.4 (Sockets TCP) → Validar 24h
Día 5: Consolidar y documentar
```

### Semana 2 (Fixes #4-5):
```
Día 1-2: v4.2.2 (Timeout dinámico) → Validar 48h
Día 3-4: v4.2.5 (UART robusto) → Validar 48h
Día 5: Consolidar y documentar
```

### Semana 3+ (Fixes opcionales):
```
Implementar según prioridad de campo
Validar 1 semana antes de producción
```

---

## 📈 IMPACTO ACUMULADO

| Versión | Fix | Tiempo Ciclo | Éxito % | Impacto Acum. |
|---------|-----|--------------|---------|---------------|
| v4.2.0 | Base | 198s | 93.8% | - |
| v4.2.1 | Persistencia | 178s | 97% | -20s, +3.2% |
| v4.2.3 | Init Módem | 163s | 97.5% | -35s, +3.7% |
| v4.2.4 | Sockets TCP | 163s | 98% | -35s, +4.2% |
| v4.2.2 | Timeout LTE | 163s | 99% | -35s, +5.2% |
| v4.2.5 | UART Robusto | 148s | 99.5% | -50s, +5.7% |

**Meta Final (v4.2.5):** Ciclo 148s | Éxito 99.5% | **-25% tiempo, +6% éxito**

---

## ⚠️ CRITERIOS DE DECISIÓN

### ¿Cuándo implementar el siguiente fix?

✅ **Proceder si:**
- Fix actual validado 24-48h sin regresiones
- Métricas cumplen objetivo (tiempo, éxito, batería)
- Logs no muestran errores nuevos
- Team aprueba avanzar

⛔ **DETENER si:**
- Regresiones detectadas
- Tasa éxito < 95%
- Aumento de watchdog resets
- Fallos críticos en campo

### Rollback strategy:
```bash
# Si v4.2.2 falla, volver a v4.2.1
git checkout v4.2.1-JAMR4-PERSIST
# Device sigue funcionando con persistencia base
```

---

## 📞 CONTACTO Y DECISIONES

**Owner:** Equipo JAMR_4  
**Aprobación requerida para:**
- Avanzar a Semana 2 (después de v4.2.4)
- Implementar fixes de riesgo medio/alto
- Rollback a versión anterior

**Documentación obligatoria:**
- Plan de ejecución por fix
- Logs de cada paso
- Validación en hardware
- Reporte final por versión

---

**Roadmap creado:** 31 Oct 2025  
**Estado:** ✅ Listo para ejecutar  
**Próxima acción:** Implementar v4.2.1 (FIX #1)  
**Revisión:** Después de cada fix implementado
