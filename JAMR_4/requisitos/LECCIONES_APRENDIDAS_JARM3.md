# Lecciones Aprendidas: Proceso de Iteración JAMR_3

**Fecha:** 2025-10-29  
**Contexto:** Análisis retrospectivo del desarrollo de JAMR_3  
**Propósito:** Documentar errores y aciertos para guiar JAMR_4

---

## 📖 Historia del Desarrollo JAMR_3

### Punto de Partida
- **Base:** `sensores_elathia_fix_gps` (firmware estable)
- **Problema identificado:** Módem permanece activo durante sleep (10 mA)
- **Síntomas:** Sistema se "cuelga" después de N ciclos, deja de transmitir

### Evolución de Fixes

#### ✅ FIX-001 & FIX-002: Watchdog Timer (EXITOSO)
**Versión:** v3.0.1  
**QUÉ:** Sistema debe recuperarse automáticamente de cuelgues

**Implementación:**
- Watchdog de 120s
- 16+ feeds estratégicos
- Eliminación de warning "Tasks still subscribed"

**Resultado:** ✅ **EXITOSO**
- Sistema se recupera de hangs
- No resets espurios
- Código limpio sin warnings

**Lección:** Fix bien definido con requisito claro funciona a la primera

---

#### ✅ FIX-003: Protección de Loops (EXITOSO)
**Versión:** v3.0.2  
**QUÉ:** Loops críticos no deben ejecutar infinitamente

**Implementación:**
- Límite 15 intentos en `startGsm()`
- Feeds cada 1s en espera LTE
- Garantía: nunca >120s sin feed

**Resultado:** ✅ **EXITOSO**
- Loops protegidos
- Timeouts prevenidos
- Sistema más robusto

**Lección:** Pequeños cambios incrementales, bien testeados, son efectivos

---

#### ✅ FIX-004: Health Data (EXITOSO)
**Versión:** v3.0.3  
**QUÉ:** Sistema debe reportar su estado de salud

**Implementación:**
- RTC Memory para persistencia
- 12 checkpoints críticos
- 7 tipos de crash detectables
- 6 bytes en payload

**Resultado:** ✅ **EXITOSO**
- Diagnóstico remoto funcional
- Crashes identificables
- Health data en telemetría

**Lección:** Observabilidad es inversión que paga dividendos inmediatamente

---

#### ⚠️ FIX-008: Seguridad de Watchdog (PARCIAL)
**Versión:** v3.0.8  
**QUÉ:** `setupModem()` debe indicar éxito/fracaso

**Implementación:**
- Funciones retornan `bool`
- No alimentar watchdog en caso de fallo
- Permitir reset limpio

**Resultado:** ⚠️ **PARCIALMENTE EXITOSO**
- Lógica correcta
- Pero expuso problema subyacente: módem no respondía

**Lección:** Fix correcto puede revelar problemas más profundos (esto es bueno)

---

#### ⚠️ FIX-009: Versionamiento Dinámico (INCOMPLETO)
**Versión:** v3.0.9  
**QUÉ:** Versión del firmware debe viajar en payload

**Implementación:**
- 3 bytes: MAJOR, MINOR, PATCH
- Parser actualizado
- Tests creados

**Resultado:** ⚠️ **IMPLEMENTADO PERO NO VALIDADO**
- Código agregado
- Nunca llegó a device real
- No confirmado funcionando end-to-end

**Lección:** Feature no es "completo" hasta validar en campo

---

#### ❌ FIX-010: Gestión Estado Módem (FALLIDO - MÚLTIPLES ITERACIONES)
**Versión:** v3.0.10  
**QUÉ (INTENTADO):** Módem debe responder inmediatamente tras despertar

**Iteración 1: CFUN=0 en lugar de power-off**
```
Cambios:
- setupGpsSim(): Reemplazar modemPwrKeyPulse() con AT+CFUN=0
- modemInitialized = true (modem responde a AT)
- Documentación de SIMCOM manual

Resultado: ❌ FALLÓ
- Bug: NO se removió modemPwrKeyPulse() en setupModem()
- Módem seguía apagándose al final del ciclo
- Estado zombi persistió
```

**Iteración 2: Búsqueda exhaustiva de PWRKEY**
```
Cambios:
- grep completo de modemPwrKeyPulse()
- Remover en setupModem() línea 217 (éxito)
- Remover en setupModem() línea 203 (fallo LTE)
- Mantener CFUN=0 en todos los paths

Resultado: ❌ FALLÓ
- Logs mostraron: módem NO responde en primer boot
- startGsm() necesitaba encender módem inicialmente
```

**Iteración 3: Boot sequence en startGsm()**
```
Cambios:
- Detectar primer boot (!modemInitialized)
- Ejecutar modemPwrKeyPulse() + espera 5s
- Verificar AT command
- Marcar modemInitialized = true

Resultado: ❌ FALLÓ
- Boot sequence de 5s fue muy corto
- Módem no respondía en tiempo esperado
- Watchdog timeout antes de completar boot
```

**Iteración 4: Boot sequence extendido + retry logic**
```
Cambios:
- Boot sequence: 6s → 8s
- Intentos AT: 3 → 5
- Delay entre intentos: 200ms → 500ms
- Lógica de double power cycle en caso de fallo

Resultado: ❌ EMPEORÓ
- Código cada vez más complejo
- Double power cycle agregó más puntos de fallo
- Difícil de debuggear
- Nunca se validó en device real
```

**Iteración 5: Simplificación + más tiempo**
```
Cambios:
- Boot sequence: 8s (16 × 500ms con watchdog feeds)
- Intentos AT: 10 con delay 500ms
- Remover double power cycle
- Logs más informativos

Resultado: ❓ NO VALIDADO
- Última versión no probada en hardware
- En este punto se decidió DETENER y hacer reset
```

---

## 🔴 Punto de Degradación

### Síntomas de que el Proceso Falló

1. **Iteraciones sin validación**
   - Cambios sobre cambios sin probar en device real
   - "Teóricamente debería funcionar" sin evidencia

2. **Complejidad creciente**
   - Cada iteración agregaba más código
   - Boot sequence, retry logic, power cycles múltiples
   - Imposible razonar sobre el estado del sistema

3. **Pérdida de confianza en código**
   - No claridad sobre qué funcionaba y qué no
   - Miedo de que cambios rompan algo más
   - Necesidad de "empezar de cero"

4. **Desconexión con requisitos**
   - Se perdió de vista el QUÉ (módem debe responder rápido)
   - Enfocados en CÓMOs (power cycles, boot sequences, retries)
   - Sin criterios claros de éxito

---

## 📊 Análisis de Causa Raíz

### ¿Por qué FIX-010 Falló?

#### 1. **Falta de Requisito Claro**
❌ **Lo que se hizo:**
```
"Hacer que el módem responda después de sleep"
```

✅ **Lo que se debió hacer:**
```
REQUISITO:
- Módem DEBE responder a AT command en < 1s tras despertar
- Primer boot PUEDE tomar 6-8s (aceptable una vez)
- Ciclos subsecuentes NO deben usar power cycles
- Si falla después de 10 intentos: permitir watchdog reset

CRITERIO DE ÉXITO:
- 24h de operación sin power cycles entre ciclos
- Wake-to-transmit < 30s consistentemente
- 0 resets de watchdog en operación normal
```

#### 2. **Implementación sin Validación Iterativa**
❌ **Lo que se hizo:**
```
Iteración 1 → Código
Iteración 2 → Más código (sin probar iteración 1)
Iteración 3 → Más código (sin probar iteraciones anteriores)
...
```

✅ **Lo que se debió hacer:**
```
Iteración 1 → Código → Flash → Logs → ¿Funciona? NO → Análisis
Iteración 2 → Fix basado en logs reales → Flash → Logs → ¿Funciona?
```

#### 3. **Falta de Simplicidad**
❌ **Lo que se hizo:**
```cpp
if (!modemInitialized) {
  powerCycle();
  wait();
  if (falla) {
    if (!attemptedPowerCycle) {
      powerCycle();
      wait();
      attemptedPowerCycle = true;
      retry = 0;
      continue;
    }
    return false;
  }
}
```

✅ **Lo que se debió hacer:**
```cpp
if (!modemInitialized) {
  powerOn();  // Una vez, bien hecho
  wait(8s);   // Tiempo suficiente
  modemInitialized = true;
}

if (!modem.testAT()) {
  // Después de 10 intentos: es problema de hardware
  // Dejar que watchdog maneje
  return false;
}
```

#### 4. **No Consultar Documentación a Fondo**
❌ **Lo que se hizo:**
```
- Referencia rápida a manual SIM7080
- Asumir tiempos de boot
- Probar valores "típicos"
```

✅ **Lo que se debió hacer:**
```
- Leer sección completa de Power Management
- Identificar TODOS los modos disponibles (CFUN, DTR, PSM, eDRX)
- Entender trade-offs de cada uno
- Elegir basado en análisis, no en "lo que parece más fácil"
```

---

## ✅ Lo que SÍ Funcionó en el Proceso

### 1. **Fixes Incrementales Pequeños (FIX-001 a FIX-004)**
- Cada uno resolvía problema específico
- Se validaban antes de continuar
- Construían sobre base estable

### 2. **Documentación Detallada**
- CHANGELOG.md con cada cambio
- Documentos de FIX con justificación
- Referencias a manuales técnicos

### 3. **Health Data como Herramienta**
- Permitió ver qué estaba pasando remotamente
- Identificó que módem no respondía (sin esto, estaríamos ciegos)

### 4. **Decisión de Detener y Reiniciar**
- Reconocer cuando proceso no funciona
- Volver a versión estable
- Definir requisitos antes de continuar

---

## 🎓 Lecciones para JAMR_4

### Proceso de Desarrollo

#### 1. **Requisitos Primero**
```
PASO 1: ¿Qué problema resolvemos? (QUÉ)
PASO 2: ¿Cómo medimos éxito? (Métricas)
PASO 3: ¿Cómo lo implementamos? (CÓMO)
PASO 4: ¿Funcionó según métricas? (Validación)
```

#### 2. **Validación Iterativa**
```
Código → Compilar → Flash → Logs → Análisis → ¿Éxito?
                                              ↓ NO
                                        Ajustar basado en EVIDENCIA
```

#### 3. **Simplicidad como Principio**
```
Antes de agregar código:
- ¿Es absolutamente necesario?
- ¿Hay forma más simple?
- ¿Qué puede salir mal?
- ¿Cómo lo debuggeamos si falla?
```

#### 4. **Documentación Técnica Exhaustiva**
```
Antes de implementar:
- Leer manual completo de feature relevante
- Entender todos los modos/opciones
- Documentar por qué elegimos uno sobre otro
- Incluir referencias en código
```

#### 5. **Fail Fast, No Workarounds**
```
Si algo falla:
- NO agregar más lógica de recovery
- Identificar causa raíz
- Fix la causa, no los síntomas
- Si es problema de HW: hacer visible, no ocultar
```

---

## 📋 Checklist para Cada Feature

### Antes de Escribir Código

- [ ] Requisito documentado (QUÉ, no CÓMO)
- [ ] Criterios de éxito definidos (medibles)
- [ ] Anti-requisitos identificados (qué NO hacer)
- [ ] Documentación técnica consultada
- [ ] Enfoque más simple identificado

### Durante Implementación

- [ ] Código sigue principio de simplicidad
- [ ] Comentarios explican POR QUÉ, no QUÉ
- [ ] Referencias a manuales incluidas
- [ ] Logs informativos en puntos clave
- [ ] Error handling claro (fail fast)

### Después de Implementación

- [ ] Compilación sin warnings
- [ ] Flasheado en device real
- [ ] Logs analizados (no solo "funciona")
- [ ] Criterios de éxito verificados
- [ ] Validación 24h+ en condiciones reales

### Antes de Declarar "Completo"

- [ ] Todos los criterios de éxito cumplidos
- [ ] No degradación de features existentes
- [ ] Documentación actualizada (CHANGELOG, README)
- [ ] Lecciones aprendidas documentadas
- [ ] Aprobación de code review

---

## 🔮 Predicciones para JAMR_4

### Lo que Probablemente Funcionará

1. **Watchdog + Health Data**
   - Ya validados en JAMR_3
   - Implementación directa desde requisitos

2. **Versionamiento en Payload**
   - Concepto simple y claro
   - Fácil de validar end-to-end

3. **Proceso de Requisitos → Implementación**
   - Estructura clara reduce ambigüedad
   - Criterios de éxito guían desarrollo

### Lo que Será Desafiante

1. **Gestión de Estado del Módem**
   - Requiere entender profundamente SIM7080
   - Múltiples opciones (CFUN, DTR, PSM)
   - Puede necesitar iteraciones (pero con validación cada vez)

2. **Balance Consumo vs. Wake-up Speed**
   - CFUN=0: rápido pero 10 mA
   - DTR: lento pero <1 mA
   - Decisión basada en requisitos del proyecto

3. **Testing Exhaustivo**
   - Condiciones reales difieren de lab
   - Necesita tiempo (7+ días) para validar
   - Paciencia será clave

---

## 💡 Sabiduría Destilada

### Principios No Negociables

1. **"Funcionó en mi máquina" no es suficiente**
   - Device real > Simulación
   - Logs reales > Supuestos
   - 24h field test > 5 minutos en lab

2. **"Más código" raramente es la solución**
   - Si algo no funciona, entender POR QUÉ
   - No agregar workarounds sobre workarounds
   - Simplicidad = Mantenibilidad

3. **"Lo arreglaré después" es mentira**
   - Si algo está mal, arreglarlo AHORA
   - Deuda técnica crece exponencialmente
   - Código legacy es código que "funcionó una vez"

4. **"Nadie leerá la documentación" es excusa**
   - Documentar es para tu yo futuro
   - 6 meses después, no recordarás nada
   - La documentación es el código que sobrevive

5. **"Este fix es urgente" no justifica mala ingeniería**
   - Fix rápido mal hecho = 10x el tiempo después
   - Tomarse el tiempo ahora ahorra tiempo después
   - Urgencia no es excusa para saltarse proceso

---

## 📚 Recursos para JAMR_4

### Documentación Creada

1. **Carpeta `/requisitos/`**
   - REQ-001: Gestión Estado Módem
   - REQ-002: Watchdog
   - REQ-003: Health Data
   - REQ-004: Versionamiento
   - README.md (índice y plan)

2. **Esta Lección Aprendida**
   - Referencia de qué NO hacer
   - Checklist de proceso
   - Predicciones y principios

### Próximos Pasos

1. **Reunión de Kickoff JAMR_4**
   - Revisar requisitos con equipo
   - Asignar responsabilidades
   - Establecer cronograma realista

2. **Fase 1: Implementación de Fundamentos**
   - REQ-002 + REQ-003 (Watchdog + Health Data)
   - Validación exhaustiva antes de continuar

3. **Fase 2: Feature Principal**
   - REQ-001 (Gestión Estado Módem)
   - Múltiples opciones exploradas con criterio
   - Validación en cada iteración

4. **Fase 3: Observabilidad**
   - REQ-004 (Versionamiento)
   - Dashboard actualizado
   - Alertas configuradas

5. **Fase 4: Field Testing**
   - 7+ días en condiciones reales
   - Análisis de telemetría
   - Ajustes basados en evidencia

---

**Documento creado:** 2025-10-29  
**Propósito:** Guía para no repetir errores del pasado  
**Destinatario:** Equipo de desarrollo JAMR_4  
**Mensaje final:** El mejor código es el que nunca tuviste que escribir. Piensa antes de codear.
