# Investigación Exhaustiva: SIM7080G Estado Zombie / No Responde

**Fecha de investigación:** 2026-01-29  
**Contexto:** Diagnóstico Modem Zombie - Dispositivos 6948, 6963  
**Firmware afectado:** v2.7.1  
**Conclusión:** ✅ **PROBLEMA DOCUMENTADO** - Múltiples casos y soluciones identificadas

---

## 1. RESUMEN EJECUTIVO

Esta investigación documenta el problema del SIM7080G entrando en un "estado zombie" donde no responde a comandos AT ni a la secuencia PWRKEY, requiriendo un power cycle físico para recuperación. Se identificaron múltiples causas raíz documentadas en la comunidad y en la **documentación oficial de SIMCOM**.

### Hallazgos clave:
| Aspecto | Evidencia |
|---------|-----------|
| ¿Problema conocido? | ✅ Sí, múltiples reportes en foros y GitHub |
| ¿Causa identificada? | ✅ **Múltiples causas**: firmware modem, PSM, secuencia PWRKEY incorrecta, power supply |
| ¿Solución de software? | ⚠️ Mitigaciones parciales (esperar URC, PWRKEY >12.6s, deshabilitar PSM) |
| ¿Solución definitiva? | 🔴 **Hardware: MOSFET para corte de VBAT o GPIO a RESET** |

### Descubrimiento crítico:
Hay **dos tipos de "no responde"** que se mezclan:
1. **PSM/primer AT se pierde** → Se arregla con reintentos y `AT+CPSMS=0`
2. **Estado corrupto/intermedio** → Solo power cycle físico recupera

---

## 2. HALLAZGOS CRÍTICOS DEL DATASHEET SIMCOM (OFICIAL)

### 2.1 Tiempos de PWRKEY (Fuente: Hardware Design V1.04)

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| **Encendido** | >1 segundo LOW | Tiempo mínimo para encender módulo |
| **Apagado** | >1.2 segundos LOW | Tiempo mínimo para apagar módulo |
| **Reset forzado** | **>12.6 segundos LOW** | PWRKEY sostenido causa reset automático interno |
| **UART ready** | 1.8 segundos después de power-on | Tiempo antes de que UART responda |
| **Toff-on** | ≥2 segundos | Buffer time entre power-off y power-on |

**Fuente:** [SIM7080G Hardware Design V1.04 - SIMCOM/Texim Europe](https://www.texim-europe.com/Cmsfile/SMM-SIM7080G-Hardware-Design-V1.04-DS-200525-TE.pdf), Sección 3.2, Tablas 8 y 9

### 2.2 Advertencia Oficial SIMCOM sobre PWRKEY

> "After the PWRKEY continues to pull down more than 12S, the system will automatically reset. Therefore, **long-term grounding is not recommended for PWRKEY pin**."

**Fuente:** [SIM7080G Hardware Design V1.04](https://www.texim-europe.com/Cmsfile/SMM-SIM7080G-Hardware-Design-V1.04-DS-200525-TE.pdf), Sección 2.2 Pin Description, página 16

### 2.3 El SIM7080G NO TIENE PIN RESET Dedicado

> "The SIM7080G is reset using the PWRKEY **NOT** a separate RESET pin! To reset the module, the PWRKEY is held low for 12.6s."

**Fuente:** [ModularSensors Documentation - EnviroDIY](https://envirodiy.github.io/ModularSensors/group__modem__sim7080.html)

### 2.4 Apagado "correcto" por AT y el URC esperado

El comando **`AT+CPOWD=1`** (normal power off) **debe** devolver el URC **"NORMAL POWER DOWN"**.
- Si el firmware **no espera/valida** ese URC, puede quedarse en estado intermedio
- VDD_EXT baja después de ~1.8 segundos del URC
- STATUS pin baja después de ~1.8 segundos del URC

**Fuente:** [SIM7070_SIM7080_SIM7090 Series_AT Command Manual V1.05](https://edworks.co.kr/wp-content/uploads/2022/04/SIM7070_SIM7080_SIM7090-Series_AT-Command-Manual_V1.05.pdf)

### 2.5 Advertencia sobre cortar VBAT directamente

> "**No es recomendable apagar desconectando VBAT**, porque hay riesgo de dañar el file system del módulo."

**Estrategia recomendada:** Intentar apagado limpio (`AT+CPOWD=1` + esperar URC) y **solo si falla**, hacer corte de VBAT.

**Fuente:** [SIM7080G Hardware Design V1.04 - Texim Europa](https://www.texim-europe.com/Cmsfile/SMM-SIM7080G-Hardware-Design-V1.04-DS-200525-TE.pdf)

---

## 3. PROBLEMA ESPECÍFICO: PSM (Power Saving Mode)

### 3.1 PSM puede hacer que el modem "ignore" comandos AT

Hay reportes donde, tras despertar de PSM, **los AT son ignorados** si no se "sale" correctamente del modo ahorro.

**Solución documentada:** Deshabilitar PSM con **`AT+CPSMS=0`** después de despertar para volver a tener AT confiable.

**Fuente:** [M5Stack Community - UIFlow and CAT M MODULE SIM7080G](https://community.m5stack.com/topic/4694/uiflow-and-cat-m-module-sim7080g)

### 3.2 Primer comando AT después de PSM se pierde

> "In my experience the **first AT command after the modem wakes up from PSM sleep is always ignored / lost**. Try issuing the Check module status command multiple times."

**Fuente:** [M5Stack Community - UIFlow and CAT M MODULE SIM7080G](https://community.m5stack.com/topic/4694/uiflow-and-cat-m-module-sim7080g)

### 3.3 URCs de PSM no se detectan confiablemente

> "There is no way to catch the unsolicited 'EXIT PSM' message from the UART. We are able to catch +CPSMSTATUS: ENTER PSM but **never ever to get EXIT PSM** (UART remains empty)."

**Fuente:** [M5Stack Community](https://community.m5stack.com/topic/4694/uiflow-and-cat-m-module-sim7080g)

---

## 4. CASOS DOCUMENTADOS EN FOROS Y GITHUB

### 4.1 GitHub: botletics/SIM7000-LTE-Shield

#### Issue #322: "SIM7000G DTR crash" ⭐⭐⭐⭐⭐ IDÉNTICO A NUESTRO CASO
**URL:** https://github.com/botletics/SIM7000-LTE-Shield/issues/322

**Descripción exacta del usuario:**
> "It works fine until I try to put module to sleep with putting DTR to high. Once I do that it seems that module shuts down completely (LED stops blinking) and **module is not responsive to commands. Even PWR_KEY stops working** and I can't turn the module back up **other than physically disconnecting the battery** and connecting it again."

**Relevancia:** IDÉNTICO
- Modem no responde a comandos
- PWRKEY deja de funcionar
- Solo power cycle físico recupera el dispositivo
- Ocurre después de intentar apagar/dormir el modem

#### Issue #298: "Modem connected via USB is sometimes not responding"
**URL:** https://github.com/botletics/SIM7000-LTE-Shield/issues/298

**Respuesta del mantenedor (Mark-Wills):**
> "Sounds like a **firmware issue on the modem**. In such a case, it is a good idea to **reset the modem (either by its reset pin, or by removing power and re-applying power under software control)**."

---

### 4.2 GitHub: Xinyuan-LilyGO/LilyGo-T-SIM7080G

#### Issue #164: "Hardware Failures" (Enero 2026 - MUY RECIENTE) ⭐⭐⭐⭐⭐
**URL:** https://github.com/Xinyuan-LilyGO/LilyGo-T-SIM7080G/issues/164

**Síntomas reportados (idénticos a nuestro caso):**
```
✓ ESP32-S3: All GPIO working, UART functional
✓ PWRKEY pulse: 500ms and 5-second attempts
✓ Boot wait time: 30+ seconds

✗ SIM7080G red LED: Never illuminated
✗ AT command response: No response at any baud rate
✗ Modem status: Appears completely unpowered
```

**Observación crítica del usuario:**
> "Please be aware however that **DC3 seems to turn off whenever all power is removed from the board**. Is this normal operation? This would be very problematic if the battery goes dead. **Remote IoT device would be permanently offline after such an event.**"

---

### 4.3 M5Stack Community - Reportes de Estabilidad

#### "SIM7080G Module speed and stability" (Septiembre 2023)
**URL:** https://community.m5stack.com/topic/5635/sim7080g-module-speed-and-stability

> "When I make repeated HTTP calls in a loop for testing, the device would freeze or stop responding after a random amount of time (minutes). When I reboot the CPU (ESP32) sometimes it would recover, sometimes not. Also when power is unplugged, **sometimes the module would go into a state where it would not respond AT commands at all. I would have to power cycle the module, sometimes more than once**, to get it to accept AT commands again."

#### "M5core2 with SIM7080G AT commands timeout" (Febrero 2022)
**URL:** https://community.m5stack.com/topic/4055/m5core2-with-sim7080g-at-commands-timeout

> "I think I know what might be going on - the modem is constantly resetting. The reset time is determined by the internal timer (default is 12.6 seconds). After the PWRKEY is pulled low, the module will be reset after 12.6 seconds. Therefore, **it is not recommended to connect PWRKEY to GND all the time**."

---

### 4.4 Arduino Forum

#### "SIM7080G not responding to serial due my design flaw?"
**URL:** https://forum.arduino.cc/t/sim7080g-not-responding-to-serial-due-my-design-flaw/1028570

Mucha gente reporta **SIM7080G encendido pero sin responder AT** (a veces USB sí, UART no), lo que apunta a **problemas de interfaz (UART/levels/baud/estado del pinado)** o a **estados internos del módem**.

---

### 4.5 GitHub: wottreng/SIM7080G-NB-IoT

#### GPS/GNSS + Cellular causa cuelgue
**URL:** https://github.com/wottreng/SIM7080G-NB-IoT-Cat-M-LTE-GPS

> "**GPS/GNSS and cellular can not be used together. Causes module to hang and be unresponsive.** Make sure to turn off network activity then use GPS/GNSS then turn network back on and use Data functions."

---

### 4.6 GitHub: vshymanskyy/TinyGSM

#### Issue #419: "Implement SIMCom SIM7070/SIM7080/SIM7090 Series"
**URL:** https://github.com/vshymanskyy/TinyGSM/issues/419

**Comentario relevante (SRGDamia1, colaborador):**
> "I had **zero success with SSL until I upgraded my module firmware**. For me, that required soldering to get to the USB pins and **one of my test modules refused to be recognized by my PC and accept the firmware no matter what**."

**Firmware problemático identificado:**
- `1951B03SIM7080` - Versión con problemas
- `1951B08SIM7080` - Versión mejorada (LilyGo)
- `1951B17SIM7080` - Versión más reciente disponible

---

### 4.7 Electronics StackExchange

#### "SIM7080 UART Noise interfacing with PIC18F4550"
**URL:** https://electronics.stackexchange.com/questions/618518

**Respuesta técnica clave:**
> "We've had **PCBs switch themselves off because the PWRKEY open collector transistor was too responsive to RF!**"
> 
> "You also need a fast (as in able to respond quickly to current demands) and stable power source. A Li-ion battery is not good enough on its own, you would also need some **low-ESR bypass capacitors** next to the baseband and RF power inputs to the modem."

---

## 5. CORRELACIÓN CON NUESTRO FIRMWARE v2.7.1

### 5.1 Problemas identificados en código actual

| Código Actual | Problema según datasheet/foros |
|---------------|-------------------------------|
| `if (!isAlive()) return true` | El modem puede NO responder pero estar encendido en estado corrupto |
| `delay(2000)` después de CPOWD | **NO espera URC "NORMAL POWER DOWN"** - puede no completar apagado |
| PWRKEY pulse 1200ms | Está en el límite mínimo (1.2s) - puede ser insuficiente |
| No hay reset por PWRKEY >12.6s | No usa la capacidad de reset forzado del modem |
| No verifica PSM | Puede estar en PSM y primer AT se pierde |
| No hay verificación post-sleep | No detecta si el modem quedó en estado zombie |

### 5.2 Secuencia de fallo hipotética (actualizada)

```
Ciclo N:
  1. Modem transmite OK
  2. powerOff() envía AT+CPOWD=1
  3. Modem inicia apagado pero NO COMPLETA (no se espera URC)
  4. ESP32 entra a deep sleep
  5. Modem queda en estado intermedio (o entra a PSM)
  
Ciclo N+1:
  6. ESP32 despierta
  7. powerOn() hace PWRKEY toggle (1.2s - mínimo)
  8. Si estaba en PSM: primer AT se pierde, no hay retry
  9. Si estaba en estado corrupto: PWRKEY no ayuda
  10. isAlive() retorna false
  11. Firmware asume "modem apagado" → INCORRECTO
  
Ciclo N+2...N+∞:
  12. Mismo comportamiento indefinidamente
  13. No se intenta PWRKEY >12.6s (reset forzado)
  14. Solo power cycle físico recupera
```

---

## 6. CHECKLIST: DIFERENCIAR PSM vs ZOMBIE REAL

Antes de declarar "modem zombie", verificar:

| # | Verificación | Acción si falla |
|---|--------------|-----------------|
| 1 | ¿El módulo estaba en PSM? | Mandar `AT+CPSMS=0` tras despertar |
| 2 | ¿Primer AT se perdió? | Reintentar "AT" 3-5 veces con delay |
| 3 | ¿Esperaste "NORMAL POWER DOWN" al apagar? | Implementar espera de URC |
| 4 | ¿Probaste PWRKEY LOW ~3 segundos? | Extender pulso |
| 5 | ¿Probaste PWRKEY LOW >12.6s (reset forzado)? | Implementar como último recurso |
| 6 | ¿Nada responde? | **ZOMBIE REAL** → requiere power cycle VBAT |

---

## 7. SOLUCIONES DOCUMENTADAS

### 7.1 Soluciones de Hardware (DEFINITIVAS)

| Solución | Fuente | Efectividad |
|----------|--------|-------------|
| **MOSFET para cortar VCC del modem** | botletics #298 | ⭐⭐⭐⭐⭐ Definitiva |
| GPIO conectado a pin RESET del modem | botletics #298 | ⭐⭐⭐⭐ Alta |
| PMU con control de rieles de alimentación | LilyGo #164 | ⭐⭐⭐⭐⭐ Definitiva |
| Capacitores low-ESR cerca del modem | StackExchange | ⭐⭐⭐ Preventiva |

### 7.2 Mitigaciones de Firmware (PALIATIVAS)

| Mitigación | Fuente | Efectividad |
|------------|--------|-------------|
| **Esperar URC "NORMAL POWER DOWN"** | Datasheet SIMCOM | ⭐⭐⭐⭐ Alta |
| **PWRKEY >12.6s como reset forzado** | Hardware Design V1.04 | ⭐⭐⭐⭐ Alta |
| Deshabilitar PSM con `AT+CPSMS=0` | M5Stack Community | ⭐⭐⭐ Media |
| Reintentar AT múltiples veces tras wake | M5Stack Community | ⭐⭐⭐ Media |
| PWRKEY pulse extendido (3-5 segundos) | LilyGo #164 | ⭐⭐⭐ Media |
| Verificar estado después de deep sleep | General | ⭐⭐⭐ Media |
| Buffer Toff-on ≥2 segundos | Datasheet | ⭐⭐⭐ Media |

---

## 8. CÓDIGO DE REFERENCIA MEJORADO

### 8.1 powerOff() robusto

```cpp
// Basado en datasheet SIMCOM y mejores prácticas documentadas
bool LTEModule::powerOff() {
    // 1. Intentar apagado graceful con AT+CPOWD
    _serial.println("AT+CPOWD=1");
    
    // 2. CRÍTICO: Esperar URC "NORMAL POWER DOWN" (hasta 5 segundos)
    unsigned long start = millis();
    while (millis() - start < 5000) {
        if (_serial.available()) {
            String response = _serial.readStringUntil('\n');
            if (response.indexOf("NORMAL POWER DOWN") != -1) {
                delay(500);  // Buffer adicional
                return true;
            }
        }
        delay(100);
    }
    
    // 3. Si no recibió URC, intentar PWRKEY extendido (3 segundos)
    debugPrint("URC no recibido, intentando PWRKEY 3s");
    digitalWrite(_pwrkey, LOW);
    delay(3000);
    digitalWrite(_pwrkey, HIGH);
    delay(1000);
    
    // 4. Verificar si respondió
    if (!isAlive()) {
        return true;
    }
    
    // 5. ÚLTIMO RECURSO: Reset forzado (>12.6 segundos según datasheet)
    debugPrint("Forzando reset por PWRKEY >12.6s");
    digitalWrite(_pwrkey, LOW);
    delay(13000);
    digitalWrite(_pwrkey, HIGH);
    delay(2000);  // Toff-on buffer (datasheet: ≥2s)
    
    return !isAlive();
}
```

### 8.2 powerOn() robusto

```cpp
bool LTEModule::powerOn() {
    // 1. Verificar si ya está encendido
    if (isAlive()) {
        return true;
    }
    
    for (int attempt = 0; attempt < 3; attempt++) {
        // 2. PWRKEY LOW por >1 segundo (usar 1.5s por seguridad)
        digitalWrite(_pwrkey, LOW);
        delay(1500);
        digitalWrite(_pwrkey, HIGH);
        
        // 3. Esperar UART ready (1.8s según datasheet)
        delay(2500);
        
        // 4. Enviar múltiples AT (primer comando puede perderse por PSM)
        for (int i = 0; i < 5; i++) {
            _serial.println("AT");
            delay(500);
            if (isAlive()) {
                // 5. Deshabilitar PSM para operación confiable
                _serial.println("AT+CPSMS=0");
                delay(200);
                return true;
            }
        }
        
        debugPrint("Intento " + String(attempt + 1) + " fallido");
    }
    
    // 6. Si falló 3 veces, intentar reset forzado (>12.6s)
    debugPrint("Intentando reset forzado PWRKEY >12.6s");
    digitalWrite(_pwrkey, LOW);
    delay(13000);
    digitalWrite(_pwrkey, HIGH);
    delay(2500);
    
    // 7. Última verificación
    for (int i = 0; i < 5; i++) {
        _serial.println("AT");
        delay(500);
        if (isAlive()) {
            _serial.println("AT+CPSMS=0");
            return true;
        }
    }
    
    // 8. Si todo falla, el modem está en estado zombie
    debugPrint("ALERTA: Modem en estado zombie - requiere power cycle VBAT");
    return false;
}
```

---

## 9. CONCLUSIONES

### 9.1 Validación del problema
✅ **El problema "modem zombie" es REAL y DOCUMENTADO** en múltiples foros con casos idénticos al nuestro.

### 9.2 Causas raíz identificadas

| Causa | Probabilidad | Mitigación |
|-------|--------------|------------|
| No esperar URC "NORMAL POWER DOWN" | **Alta** | Implementar espera activa |
| PSM activo y primer AT perdido | **Media-Alta** | `AT+CPSMS=0` + reintentos |
| PWRKEY pulse muy corto | **Media** | Extender a 1.5s mínimo |
| Estado corrupto interno | **Media** | Reset forzado >12.6s |
| Firmware del modem buggy | **Baja** | Actualizar firmware (requiere USB) |

### 9.3 Solución definitiva
🔴 **REQUIERE MODIFICACIÓN DE HARDWARE:**
- Control de alimentación del modem desde ESP32 (MOSFET en VBAT)
- **Estrategia:** Intentar apagado limpio primero, corte de VBAT solo si falla

### 9.4 El desarrollador tiene razón (parcialmente)
> "La solución es solo física"

**Correcto:** La solución DEFINITIVA y 100% confiable requiere hardware.  
**Incorrecto:** Existen **mitigaciones de firmware significativas** que pueden:
- Reducir frecuencia del problema (esperar URC, PWRKEY >12.6s)
- Recuperar de estados PSM (deshabilitar PSM, reintentos)
- Detectar y reportar cuando ocurre (para análisis post-mortem)

---

## 10. RECOMENDACIONES ACCIONABLES

### Corto plazo (firmware v2.8.x)
1. ✅ **Esperar URC "NORMAL POWER DOWN"** antes de asumir apagado
2. ✅ **Implementar PWRKEY >12.6s** como reset de emergencia
3. ✅ **Deshabilitar PSM** con `AT+CPSMS=0` después de encender
4. ✅ **Reintentar "AT" 5 veces** (primer comando puede perderse)
5. ✅ **Agregar contador de zombies** en ProductionDiag
6. ✅ **Respetar Toff-on ≥2s** entre apagado y encendido

### Mediano plazo (hardware rev 4.2)
1. Agregar **MOSFET (IRLML2502)** en línea VBAT del SIM7080G
2. Conectar GPIO de ESP32 al control del MOSFET
3. Estrategia: apagado limpio → si falla → corte VBAT → encendido
4. Revisar capacitores de desacople cerca del modem

### Largo plazo
1. Evaluar actualización de firmware del SIM7080G (requiere acceso USB)
2. Considerar alternativas de modem con mejor manejo de power states

---

## 11. REFERENCIAS COMPLETAS

### Documentación Oficial SIMCOM
1. **SIM7080G Hardware Design V1.04** (Tiempos PWRKEY, URC, especificaciones)  
   https://www.texim-europe.com/Cmsfile/SMM-SIM7080G-Hardware-Design-V1.04-DS-200525-TE.pdf

2. **SIM7070_SIM7080_SIM7090 Series AT Command Manual V1.05**  
   https://edworks.co.kr/wp-content/uploads/2022/04/SIM7070_SIM7080_SIM7090-Series_AT-Command-Manual_V1.05.pdf

3. **SIM7080G Low Power Mode Application Note V1.02**  
   https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/unit/sim7080g/en/SIM7070_SIM7080_SIM7090%20Series_Low%20Power%20Mode_Application%20Note_V1.02.pdf

### GitHub Issues
4. **botletics/SIM7000-LTE-Shield #322** - DTR crash, PWRKEY no funciona (IDÉNTICO)  
   https://github.com/botletics/SIM7000-LTE-Shield/issues/322

5. **botletics/SIM7000-LTE-Shield #298** - Modem not responding  
   https://github.com/botletics/SIM7000-LTE-Shield/issues/298

6. **LilyGo-T-SIM7080G #164** - Hardware failures, PMU control  
   https://github.com/Xinyuan-LilyGO/LilyGo-T-SIM7080G/issues/164

7. **TinyGSM #419** - SIM7080 implementation, firmware issues  
   https://github.com/vshymanskyy/TinyGSM/issues/419

8. **wottreng/SIM7080G-NB-IoT** - GPS+Cellular causa hang  
   https://github.com/wottreng/SIM7080G-NB-IoT-Cat-M-LTE-GPS

### Foros de la Comunidad
9. **M5Stack - SIM7080G Module speed and stability**  
   https://community.m5stack.com/topic/5635/sim7080g-module-speed-and-stability

10. **M5Stack - AT commands timeout** (Reset por PWRKEY 12.6s)  
    https://community.m5stack.com/topic/4055/m5core2-with-sim7080g-at-commands-timeout

11. **M5Stack - UIFlow and CAT M MODULE** (PSM, primer AT perdido)  
    https://community.m5stack.com/topic/4694/uiflow-and-cat-m-module-sim7080g

12. **Arduino Forum - SIM7080G not responding to serial**  
    https://forum.arduino.cc/t/sim7080g-not-responding-to-serial-due-my-design-flaw/1028570

13. **Electronics StackExchange - UART noise, power supply issues**  
    https://electronics.stackexchange.com/questions/618518

### Documentación de Librerías
14. **ModularSensors - SIM7080 Documentation** (Reset por PWRKEY)  
    https://envirodiy.github.io/ModularSensors/group__modem__sim7080.html

15. **PCB Artists - ESP32 4G Hotspot Example**  
    https://pcbartists.com/products/documentation/esp32-4g-hotspot-example-code/

---

*Documento generado: 2026-01-29*  
*Investigación para: Diagnóstico Modem Zombie - Firmware v2.7.1*  
*Dispositivos afectados: 6948, 6963*
