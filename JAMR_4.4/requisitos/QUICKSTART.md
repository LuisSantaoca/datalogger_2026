# 🚀 Guía Rápida: Desarrollo de Features en JAMR_4

**Propósito:** Checklist paso a paso para implementar cada requisito  
**Audiencia:** Desarrolladores trabajando en JAMR_4  
**Tiempo estimado por lectura:** 5 minutos

---

## 📖 Antes de Empezar

### Lee PRIMERO (orden sugerido):

1. **STATUS.md** (5 min) - Dónde estamos y hacia dónde vamos
2. **README.md** (10 min) - Plan completo y relación entre requisitos
3. **LECCIONES_APRENDIDAS_JAMR3.md** (15 min) - Qué NO hacer y por qué
4. **REQ-XXX específico** (10 min) - El requisito que vas a implementar

**Total: ~40 minutos de lectura antes de escribir código**

❓ *"¿Por qué tanto tiempo leyendo?"*  
💡 *Respuesta: Estos 40 minutos te ahorrarán días de retrabajos y frustraciones.*

---

## 🔄 Flujo de Trabajo por Feature

```
┌──────────────────────────────────────────────────────────┐
│ FASE 1: ENTENDER                                         │
├──────────────────────────────────────────────────────────┤
│ [ ] Leer requisito completo                              │
│ [ ] Entender QUÉ (objetivo), no solo CÓMO                │
│ [ ] Revisar anti-requisitos (qué NO hacer)               │
│ [ ] Estudiar métricas de éxito (¿cómo validamos?)        │
│ [ ] Consultar manuales técnicos referenciados            │
│ [ ] Si algo no claro: documentar pregunta                │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ FASE 2: PLANEAR                                          │
├──────────────────────────────────────────────────────────┤
│ [ ] Identificar archivos a modificar                     │
│ [ ] Esbozar enfoque (QUÉ modificar, no CÓMO aún)         │
│ [ ] Identificar puntos de validación (logs clave)        │
│ [ ] Estimar tiempo realista (sin presión)                │
│ [ ] Plan de testing (¿qué probaremos y cómo?)            │
│ [ ] Definir criterios de rollback (¿cuándo detener?)     │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ FASE 3: IMPLEMENTAR                                      │
├──────────────────────────────────────────────────────────┤
│ [ ] Crear branch de trabajo (ej: feature/req-002)        │
│ [ ] Implementar INCREMENTALMENTE                         │
│     • Pequeños cambios                                   │
│     • Compilar frecuentemente                            │
│     • Commits granulares                                 │
│ [ ] Agregar logs informativos en puntos clave            │
│ [ ] Comentarios explican POR QUÉ, no QUÉ                 │
│ [ ] Referencias a manuales en comentarios                │
│ [ ] Mantener simplicidad (menos código > más código)     │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ FASE 4: VALIDAR EN LAB                                   │
├──────────────────────────────────────────────────────────┤
│ [ ] Compilación limpia (0 warnings, 0 errors)            │
│ [ ] Code review interno (self-review primero)            │
│ [ ] Flashear en device de desarrollo                     │
│ [ ] Capturar logs completos de primer ciclo              │
│ [ ] ANALIZAR logs (no solo "funciona")                   │
│     • ¿Timing esperado?                                  │
│     • ¿Checkpoints correctos?                            │
│     • ¿Errores inesperados?                              │
│ [ ] Ejecutar 5+ ciclos, revisar consistencia             │
│ [ ] Verificar métricas de éxito del requisito            │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ FASE 5: VALIDAR EN CAMPO                                 │
├──────────────────────────────────────────────────────────┤
│ [ ] Desplegar en device de testing en ubicación real     │
│ [ ] Monitoreo 24h continuas                              │
│     • Sin intervención manual                            │
│     • Backend capturando telemetría                      │
│     • Alertas configuradas                               │
│ [ ] Análisis de telemetría:                              │
│     • 0 gaps > 12 minutos                                │
│     • Métricas dentro de targets                         │
│     • Health data muestra operación normal               │
│ [ ] Si criterios cumplidos → extender a 7 días           │
│ [ ] Si NO cumplidos → análisis de causa raíz             │
└──────────────────────────────────────────────────────────┘
                          ↓
┌──────────────────────────────────────────────────────────┐
│ FASE 6: DOCUMENTAR Y CERRAR                              │
├──────────────────────────────────────────────────────────┤
│ [ ] Actualizar CHANGELOG.md                              │
│ [ ] Documentar decisiones de implementación              │
│ [ ] Agregar comentarios en código si no estaban          │
│ [ ] Crear documento de "lecciones aprendidas"            │
│     • ¿Qué funcionó bien?                                │
│     • ¿Qué fue desafiante?                               │
│     • ¿Qué haríamos diferente?                           │
│ [ ] Code review con equipo                               │
│ [ ] Merge a main (solo después de 7 días OK)             │
│ [ ] Tag de versión (ej: v3.1.0)                          │
└──────────────────────────────────────────────────────────┘
```

---

## 🚨 Red Flags: Cuándo Detener y Replantear

### Durante Implementación

❌ **"Esto es más complicado de lo que pensé"**
- Detener, revisar enfoque
- ¿Hay forma más simple?
- Consultar con equipo antes de continuar

❌ **"Voy agregando workarounds"**
- STOP inmediato
- Identificar causa raíz del problema
- No tapar problemas con más código

❌ **"No estoy seguro por qué esto funciona"**
- Si no entiendes tu propio código, hay problema
- Simplificar hasta que sea comprensible
- Si funciona por suerte, no funciona

❌ **"Solo un fix más y estará listo"**
- Señal de que enfoque no es correcto
- Cada "fix" sobre "fix" = código frágil
- Considerar empezar de nuevo con enfoque diferente

❌ **"No tengo tiempo de validar, necesito avanzar"**
- Validación no es opcional
- "Ahorrar" tiempo ahora = perderlo después
- Feature sin validar = feature no existente

---

## ✅ Green Lights: Señales de Progreso Saludable

### Durante Implementación

✅ **"Este código es simple y claro"**
- Cualquiera puede entenderlo
- Mínimas líneas necesarias
- Flujo lógico obvio

✅ **"Los logs me dicen exactamente qué está pasando"**
- Cada punto crítico logueado
- Valores y estados visibles
- Fácil de debuggear

✅ **"Esto funciona consistentemente"**
- 5/5 pruebas exitosas
- Timing predecible
- Sin comportamientos raros

✅ **"Puedo explicar cada línea de código"**
- Entiendes el POR QUÉ
- Referencias a documentación
- Decisiones justificadas

✅ **"Las métricas muestran mejora clara"**
- Números respaldan que funciona
- No solo "sensación" de que está bien
- Comparación con baseline

---

## 📊 Templates de Documentación

### Template: Commit Message

```
[REQ-00X] Brief description of change

What:
- Specific change made (e.g., "Added boot sequence in startGsm()")

Why:
- Reason for change (e.g., "Modem requires 6-8s to initialize")

Testing:
- How validated (e.g., "Tested 10 cycles, logs show boot complete in 7.2s avg")

References:
- SIM7080 AT Manual V1.02, page 14 (boot sequence timing)
```

### Template: Log Analysis

```markdown
## Log Analysis - [Feature Name]

### Test Conditions
- Device: [ID]
- Firmware: v[X.Y.Z]
- Duration: [X cycles / X hours]
- Environment: [Lab / Field]

### Expected Behavior
- [What should happen according to requisito]

### Observed Behavior
- [What actually happened]

### Timing Analysis
| Checkpoint | Expected | Observed | Delta | Status |
|------------|----------|----------|-------|--------|
| Boot       | <8s      | 7.2s     | -0.8s | ✅ OK  |
| ...        | ...      | ...      | ...   | ...    |

### Anomalies
- [List any unexpected behaviors, even if minor]

### Conclusion
- ✅ PASS / ❌ FAIL / ⚠️ PASS WITH NOTES
- [Brief summary]
```

### Template: Lecciones Aprendidas

```markdown
## Lecciones Aprendidas - [REQ-00X]

### ✅ Lo que funcionó bien
- [Decisión o enfoque exitoso]
- [Por qué funcionó]

### ⚠️ Desafíos encontrados
- [Problema o dificultad]
- [Cómo se resolvió]

### 💡 Insights para futuro
- [Aprendizaje aplicable a otros requisitos]

### 🔄 Si lo hiciera de nuevo
- [Qué cambiaría]
- [Por qué sería mejor]
```

---

## 🎯 Quick Reference: Por Requisito

### REQ-001: Gestión Estado Módem

**Archivos principales:**
- `gsmlte.cpp` (startGsm, setupModem, setupGpsSim)
- `sleepdev.cpp` (preparación de sleep)
- `type_def.h` (flag modemInitialized)

**Validación clave:**
- Primer AT command < 1s tras wake
- Sin power cycles entre ciclos normales
- Consumo < 1 mA en sleep

**Documentación:**
- SIM7080 AT Manual, páginas 14, 55, 146

---

### REQ-002: Watchdog Protection

**Archivos principales:**
- `JAMR_4.ino` (setup, configuración inicial)
- `gsmlte.cpp` (feeds en operaciones largas)
- `sleepdev.cpp` (feeds antes de sleep)

**Validación clave:**
- 0 resets de watchdog en 24h
- Máximo tiempo sin feed < 60s
- Recovery funcional tras timeout forzado

**Documentación:**
- ESP-IDF Watchdog Timer API

---

### REQ-003: Health Diagnostics

**Archivos principales:**
- `type_def.h` (estructura de health data)
- `sleepdev.cpp` (RTC memory, checkpoints)
- `gsmlte.cpp` (inclusión en payload)

**Validación clave:**
- Health data presente en 100% transmisiones
- Crash reason correcta post-reset
- Backend recibe y almacena datos

**Documentación:**
- ESP-IDF Reset Reason API
- RTC Memory docs

---

### REQ-004: Firmware Versioning

**Archivos principales:**
- `type_def.h` o `version.h` (constantes)
- `gsmlte.cpp` (payload builder)
- Backend: `listener_encrypted/src/parser.js`

**Validación clave:**
- Versión correcta en telemetría
- Consistencia en 24h por device
- Dashboard muestra versión

**Documentación:**
- Semantic Versioning 2.0.0

---

## 🧰 Herramientas y Comandos Útiles

### Análisis de Código

```bash
# Buscar uso de función
grep -rn "functionName" *.cpp *.h

# Buscar TODO/FIXME
grep -rn "TODO\|FIXME" *.cpp *.h

# Contar líneas de código
find . -name "*.cpp" -o -name "*.h" | xargs wc -l

# Ver cambios desde baseline
git diff baseline_tag..HEAD
```

### Validación

```bash
# Compilación
arduino-cli compile --fqbn esp32:esp32:esp32s3

# Logs en tiempo real
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200

# Filtrar logs por nivel
arduino-cli monitor | grep "INFO\|ERROR"
```

### Telemetría

```bash
# Query último registro de device
psql -c "SELECT * FROM datos_sensores WHERE device_id='XXX' ORDER BY timestamp DESC LIMIT 1"

# Contar transmisiones en 24h
psql -c "SELECT COUNT(*) FROM datos_sensores WHERE device_id='XXX' AND timestamp > NOW() - INTERVAL '24 hours'"

# Ver distribución de crash_reason
psql -c "SELECT crash_reason, COUNT(*) FROM datos_sensores GROUP BY crash_reason"
```

---

## 🎓 Mantras del Desarrollador JAMR_4

1. **"Requisito primero, código después"**
2. **"Simple hasta que se demuestre que complejo es necesario"**
3. **"Si no puedo explicarlo, no lo entiendo"**
4. **"Logs son mis ojos en el campo"**
5. **"Validación no es opcional"**
6. **"Mejor mañana bien que hoy mal"**
7. **"Documentar es para mi yo futuro"**
8. **"Cuando en duda, consultar el manual"**
9. **"Un bug oculto es peor que un bug visible"**
10. **"El mejor código es el que no tuve que escribir"**

---

## ❓ FAQ

**P: ¿Puedo empezar a codear antes de leer los requisitos?**  
R: NO. Los 40 minutos de lectura te ahorrarán días de retrabajos.

**P: ¿Y si tengo una idea mejor que la del requisito?**  
R: Excelente! Documéntala, discútela con el equipo, actualiza el requisito si procede, LUEGO implementa.

**P: ¿Realmente necesito 24h de testing?**  
R: SÍ. Muchos bugs solo aparecen después de varios ciclos. 5 minutos de "funciona" no es suficiente.

**P: ¿Qué hago si algo no funciona como esperaba?**  
R: STOP. Analiza logs. Identifica causa raíz. NO agregues workarounds. Si necesario, replantea enfoque.

**P: ¿Puedo saltarme la documentación para ir más rápido?**  
R: Esa es la mentalidad que creó el problema de JAMR_3. La respuesta es NO.

**P: ¿Cuándo está "completo" un requisito?**  
R: Cuando cumple TODOS los criterios de "Definition of Done". No antes.

---

## 📞 Ayuda y Soporte

**Si te atascas:**
1. Revisa la documentación del requisito
2. Consulta LECCIONES_APRENDIDAS_JAMR3.md
3. Revisa manuales técnicos
4. Discute con equipo (no sufras en silencio)
5. Documenta el problema claramente
6. Si es blocker: replantea enfoque

**Recuerda:** Pedir ayuda temprano > luchar solo por horas

---

**Última actualización:** 2025-10-29  
**Versión:** 1.0  
**Feedback:** Documenta sugerencias de mejora a esta guía
