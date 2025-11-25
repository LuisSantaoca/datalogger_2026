# FIX-1: Plan de Ejecución Controlado con Validación

**Fecha:** 2025-10-29  
**Objetivo:** Implementar cambios críticos de watchdog con retroalimentación en cada paso

---

## 📋 Lista de Cambios Identificados

### ✅ Estado Actual Verificado

**Línea 554 (while en startGps):**
- ✅ YA TIENE: `esp_task_wdt_reset()` dentro del loop
- ✅ YA TIENE: `maxTotalAttempts = 10` (límite absoluto)
- ✅ **NO REQUIERE MODIFICACIÓN**

**Línea 841 (while en startGsm):**
- ✅ YA TIENE: `esp_task_wdt_reset()` dentro del loop
- ✅ YA TIENE: `maxTotalAttempts = 15` (límite absoluto)
- ✅ **NO REQUIERE MODIFICACIÓN**

**Conclusión:** Los while loops ya están correctamente protegidos por FIX-003 previo.

### 🔴 Cambios Críticos Requeridos

Solo 2 cambios críticos identificados:

---

## 🎯 PASO 1: Fragmentar delay(3000) en startGps()

**Archivo:** `gsmlte.cpp`  
**Línea:** 577  
**Prioridad:** 🔴 CRÍTICA

### Estado Actual
```cpp
// Configurar modo de funcionamiento
modem.sendAT("+CFUN=0");
modem.waitResponse();
delay(3000);

// Deshabilitar GPS previo
```

### Estado Esperado
```cpp
// Configurar modo de funcionamiento
modem.sendAT("+CFUN=0");
modem.waitResponse();

// Esperar estabilización del módem (fragmentado para watchdog)
for (int i = 0; i < 6; i++) {
  delay(500);
  esp_task_wdt_reset();
}

// Deshabilitar GPS previo
```

### Comandos de Validación
```bash
# Después del cambio:
cd /srv/stack_elathia/docs/datalogger/JAMR_4

# 1. Verificar sintaxis (compilación rápida)
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc . 2>&1 | head -20

# 2. Verificar warnings específicos
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc . 2>&1 | grep -i "warning"

# 3. Confirmar que el cambio está presente
grep -A 5 "CFUN=0" gsmlte.cpp | grep -E "(delay|esp_task_wdt_reset)"
```

### Criterio de Éxito
- ✅ Compilación sin errores
- ✅ 0 warnings nuevos
- ✅ Código muestra 6 iteraciones con feeds

---

## 🎯 PASO 2: Fragmentar delay(2000) en startGsm()

**Archivo:** `gsmlte.cpp`  
**Línea:** 882  
**Prioridad:** 🔴 CRÍTICA

### Estado Actual
```cpp
  }
  
  delay(2000); // Esperar estabilización de la RF
  
  // Verificar estado de la RF
```

### Estado Esperado
```cpp
  }
  
  // Esperar estabilización de la RF (fragmentado para watchdog)
  for (int i = 0; i < 4; i++) {
    delay(500);
    esp_task_wdt_reset();
  }
  
  // Verificar estado de la RF
```

### Comandos de Validación
```bash
# Después del cambio:

# 1. Verificar compilación
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc . 2>&1 | tail -10

# 2. Verificar tamaño del firmware
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc . 2>&1 | grep "Sketch uses"

# 3. Confirmar cambio
grep -B 2 -A 2 "estabilización de la RF" gsmlte.cpp | grep -E "(delay|esp_task_wdt_reset)"
```

### Criterio de Éxito
- ✅ Compilación sin errores
- ✅ 0 warnings nuevos
- ✅ Código muestra 4 iteraciones con feeds
- ✅ Tamaño firmware < 90% de espacio disponible

---

## 🎯 PASO 3: Compilación Final y Verificación

### Comandos de Validación Completa
```bash
# 1. Compilación completa con verbose
arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc . --verbose 2>&1 | tee compile.log

# 2. Verificar 0 errores
grep -i "error:" compile.log

# 3. Verificar 0 warnings
grep -i "warning:" compile.log

# 4. Confirmar número total de esp_task_wdt_reset()
grep -c "esp_task_wdt_reset" gsmlte.cpp
# Esperado: 18 (era 16 + 2 nuevos de los for loops)

# 5. Verificar que no hay delays >1000ms sin fragmentar
grep "delay([0-9]\+)" gsmlte.cpp | awk -F'[()]' '{print $2}' | awk '$1 > 1000 {print "⚠️  delay(" $1 ") encontrado"}'
# Esperado: Solo delays de inicialización <1000ms o ya fragmentados

# 6. Generar resumen de cambios
echo "=== RESUMEN FIX-1 ===" > FIX-1_RESULTADO.txt
echo "Fecha: $(date)" >> FIX-1_RESULTADO.txt
echo "" >> FIX-1_RESULTADO.txt
echo "Feeds watchdog totales:" >> FIX-1_RESULTADO.txt
grep -c "esp_task_wdt_reset" gsmlte.cpp >> FIX-1_RESULTADO.txt
echo "" >> FIX-1_RESULTADO.txt
echo "Delays largos restantes (>1000ms):" >> FIX-1_RESULTADO.txt
grep "delay([0-9]\+)" gsmlte.cpp | awk -F'[()]' '{if ($2 > 1000) print $0}' >> FIX-1_RESULTADO.txt
cat FIX-1_RESULTADO.txt
```

### Criterio de Éxito Final
- ✅ 0 errores de compilación
- ✅ 0 warnings
- ✅ 18 feeds de watchdog en gsmlte.cpp
- ✅ 0 delays >1000ms sin fragmentar (excepto JAMR_4.ino que es bajo riesgo)
- ✅ Firmware compilado exitosamente

---

## 📊 Checklist de Retroalimentación

Después de cada paso, reportar:

```
[ ] PASO completado
[ ] Compilación exitosa (sí/no)
[ ] Warnings encontrados: <número>
[ ] Cambio verificado en código (sí/no)
[ ] Continuar al siguiente paso (sí/no/revisar)
```

---

## 🚨 Criterios de Rollback

Si en algún paso:
- ❌ Compilación falla
- ❌ Aparecen >2 warnings nuevos
- ❌ Tamaño firmware excede límite

**Acción:** Revertir cambio inmediatamente usando git

```bash
# Revertir último cambio
git checkout gsmlte.cpp

# Verificar estado
git status
```

---

## 📝 Notas de Implementación

1. **Orden de ejecución:** Los pasos DEBEN ejecutarse en secuencia (1→2→3)
2. **Validación obligatoria:** No avanzar sin validar paso anterior
3. **Documentación:** Capturar salida de consola en cada validación
4. **Safety:** Hacer commit después de cada paso exitoso

---

**Listo para comenzar:** ✅  
**Próximo paso:** PASO 1 - Fragmentar delay(3000) línea 577
