# Memoria del Proyecto – Sensores RV03 (ESP32-S3 + Arduino IDE 2)

## Contexto
Este documento funciona como **memoria viva del proyecto Sensores RV03**, registrando decisiones técnicas,
buenas prácticas aprendidas y criterios de organización adoptados durante el desarrollo.

La idea es mantener una **documentación paralela** que explique *por qué* se hacen las cosas, no solo *qué* se hace.

---

## Entorno de Desarrollo

- **Placa:** ESP32-S3  
- **IDE:** Arduino IDE 2  
- **Lenguaje:** C++ (Arduino framework)

---

## Decisiones Clave Aprendidas

### 1. Estructura de Proyecto en Arduino IDE

- Un proyecto Arduino **solo debe tener un archivo `.ino`**
- El `.ino`:
  - Debe estar en la **raíz**
  - Debe llamarse igual que la carpeta del proyecto
- Todos los demás archivos deben ser `.cpp` y `.h`

✔ Correcto:
```
sensores_rv03/
  sensores_rv03.ino
```

❌ Incorrecto:
```
sensores_rv03/
  src/
    sensores_rv03.ino
```

---

### 2. Uso de Subcarpetas

Arduino IDE 2 **sí compila** archivos `.cpp` y `.h` dentro de subcarpetas.

Se adoptó una estructura modular por responsabilidad:

```
data_gps/
data_lte/
data_sensors/
data_time/
data_sleepwakeup/
data_buffer/
data_format/
```

Cada módulo contiene:
- `XModule.cpp` → implementación
- `XModule.h` → interfaz
- `config_data_x.h` → configuración del módulo

---

### 3. Eliminación de `.ino` en Módulos

Inicialmente, cada módulo tenía su propio `.ino` como test.

Aprendizaje:
- Múltiples `.ino` generan conflictos (`setup()` / `loop()`)
- Para firmware productivo:
  - **solo un `.ino`**
  - los `.ino` de test deben ir a `examples/` o eliminarse

---

### 4. Rol del archivo sensores_rv03.ino

El `.ino` principal:
- Es el **entry point**
- Contiene `setup()` y `loop()`
- Coordina módulos
- No debe contener lógica pesada

Ejemplo ideal:
```cpp
void setup() {
  AppInit();
}

void loop() {
  AppLoop();
}
```

---

### 5. ESP32-S3 y Sistema de Archivos

- La carpeta `data/` **sí es válida** en ESP32
- Se puede usar:
  - **LittleFS (recomendado)**
  - SPIFFS (legacy)
- Archivos como `buffer.txt` pueden almacenarse ahí

---

### 6. Regla importante para archivos `.cpp`

Todo `.cpp` que use Arduino debe incluir:

```cpp
#include <Arduino.h>
```

Esto evita errores de compilación en Arduino IDE 2.

---

### 7. BLE en ESP32-S3

- ESP32-S3 soporta BLE correctamente
- Se recomienda **NimBLE-Arduino** por:
  - menor consumo
  - menor RAM
  - mejor estabilidad

---

### 8. Buenas Prácticas de Arquitectura

- Evitar dependencias cruzadas entre módulos
- Un módulo **no debe conocer a otro**
- La orquestación se hace desde el `.ino` o un controlador central

Opcional:
```
AppController.cpp / .h
```

---

## Estado Actual del Proyecto

✔ Proyecto compila bajo Arduino IDE 2  
✔ Estructura modular clara  
✔ Preparado para crecer (LTE, GPS, sensores, BLE, sleep)  
✔ Compatible con ESP32-S3 y filesystem  

---

## Próximos Pasos Sugeridos

- ~~Definir inicialización central (`AppInit`)~~ ✅ Implementado
- ~~Definir ciclo principal (`AppLoop`)~~ ✅ Implementado
- ~~Decidir filesystem definitivo (LittleFS)~~ ✅ Usando LittleFS
- ~~Documentar flujo de datos entre módulos~~ ✅ Documentado

---

## Implementaciones Realizadas (Diciembre 2025)

### 9. Arquitectura del Sistema (AppController)

**Implementación:**
- Sistema de máquina de estados finitos (FSM) en `AppController.cpp`
- Estados del sistema:
  - `Boot` - Inicialización
  - `BleOnly` - Modo configuración BLE (primer encendido)
  - `Cycle_ReadSensors` - Lectura de sensores
  - `Cycle_Gps` - Obtención GPS
  - `Cycle_GetICCID` - Lectura ICCID del SIM
  - `Cycle_BuildFrame` - Construcción de trama
  - `Cycle_BufferWrite` - Escritura en buffer
  - `Cycle_SendLTE` - Envío por red celular
  - `Cycle_CompactBuffer` - Limpieza de datos procesados
  - `Cycle_Sleep` - Deep sleep
  - `Error` - Estado de error

**Flujo de operación:**
```
Primer encendido: Boot → BleOnly → ReadSensors → GPS → GetICCID → BuildFrame → BufferWrite → SendLTE → CompactBuffer → Sleep

Ciclos subsecuentes: ReadSensors → GetICCID → BuildFrame → BufferWrite → SendLTE → CompactBuffer → Sleep
```

---

### 10. Correcciones de Configuración

**Problemas resueltos:**
- ✅ Duplicación de `SERIAL_BAUD_RATE` eliminada
- ✅ Pin GPS corregido a `GPS_PWRKEY_PIN`
- ✅ RS485 configurado para usar `Serial2` (era Serial1)

---

### 11. Sistema de Transmisión de Datos

**Formato de trama:**
```
$,<iccid>,<epoch>,<lat>,<lng>,<alt>,<var1>,<var2>,<var3>,<var4>,<var5>,<var6>,<var7>,#
```

**Codificación:**
- Trama se genera en formato texto
- Se codifica en **Base64** antes de enviar
- `FRAME_MAX_LEN = 102` (texto)
- `FRAME_BASE64_MAX_LEN = 200` (Base64)

**Variables de sensores:**
- var1-4: Registros RS485 (Modbus)
- var5: Temperatura (I2C)
- var6: Humedad (I2C)
- var7: Voltaje batería (ADC)

---

### 12. Sistema de Selección Inteligente de Operadora

**Operadoras soportadas:**
- Telcel (334020)
- AT&T México 334090
- AT&T México 334050
- Movistar (334030)
- Altan (334140)

**Algoritmo:**
1. **Primer envío:** Escanea todas las operadoras, mide calidad de señal (RSRP, RSRQ, SINR)
2. Calcula score: `(4 × SINR) + 2 × (RSRP + 120) + (RSRQ + 20)`
3. Selecciona operadora con mejor score
4. **Envíos subsecuentes:** Usa operadora guardada (optimización)

**Fórmula de score:**
```cpp
score = (4 * sinr) + 2 * (rsrp + 120) + (rsrq + 20)
```

**Almacenamiento persistente:**
- Si envío exitoso → guarda operadora en NVS
- Si envío falla → elimina operadora guardada (fuerza escaneo en próximo ciclo)
- Namespace: `"sensores"`, Key: `"lastOperator"`

---

### 13. Sistema de Debug Homogéneo

**Archivo central:** `src/DebugConfig.h`

**Características:**
- Control global ON/OFF
- 4 niveles de debug (ERROR, WARNING, INFO, VERBOSE)
- Control independiente por módulo
- Macros estandarizadas:
  ```cpp
  DEBUG_ERROR(MODULE, "mensaje")
  DEBUG_WARN(MODULE, "mensaje")
  DEBUG_INFO(MODULE, "mensaje")
  DEBUG_VERBOSE(MODULE, "mensaje")
  DEBUG_PRINT_VALUE(MODULE, "label: ", valor)
  ```

**Formato de salida:**
```
[ERROR][GPS] No se pudo encender el GNSS
[INFO][APP] Trama guardada exitosamente en buffer
[WARN][LTE] Timeout esperando respuesta
```

**Módulos con debug:**
- ADC, I2C, RS485, GPS, LTE, BUFFER, BLE, FORMAT, SLEEP, RTC, APP

---

### 14. Configuración de Tiempos de Sleep

**Opciones predefinidas en `sensores_rv03.ino`:**
```cpp
// Pruebas
SLEEP_30_SECONDS  // 30 segundos
SLEEP_1_MINUTE    // 1 minuto
SLEEP_2_MINUTES   // 2 minutos

// Operación normal
SLEEP_5_MINUTES   // 5 minutos
SLEEP_10_MINUTES  // 10 minutos (defecto)
SLEEP_15_MINUTES  // 15 minutos
SLEEP_30_MINUTES  // 30 minutos

// Largo plazo
SLEEP_1_HOUR      // 1 hora
SLEEP_2_HOURS     // 2 horas
SLEEP_6_HOURS     // 6 horas
```

**Cambio simple:**
```cpp
const uint64_t TIEMPO_SLEEP_ACTIVO = SLEEP_10_MINUTES;
```

---

### 15. Sistema de Buffer Persistente

**Garantías:**
- ✅ Trama **SIEMPRE** se guarda en buffer antes de enviar
- ✅ Solo se marca como procesada si el envío fue exitoso
- ✅ Tramas no enviadas permanecen en buffer
- ✅ Próximo ciclo reintenta enviar tramas pendientes
- ✅ Sobrevive deep sleep y reinicios

**Flujo:**
1. Genera trama → Guarda en buffer (archivo LittleFS)
2. Intenta enviar → Marca como procesada SOLO si exitoso
3. Compacta buffer → Elimina SOLO las procesadas
4. Tramas fallidas quedan para próximo ciclo

**Ejemplo de recuperación:**
- Ciclo 1: Guarda 3 tramas, envía 1, falla en la 2da → buffer queda con 2
- Ciclo 2: Guarda 1 nueva → buffer tiene 3 (2 viejas + 1 nueva) → reintenta

**Marcador de procesadas:** `[P]` al inicio de línea

---

### 16. Optimización de GPS

**Problema:** GPS consume mucha energía (~150mA) y tarda 30-60 segundos

**Solución implementada:**
- GPS **solo se activa en primer ciclo** (después de boot + BLE)
- Coordenadas se **guardan en NVS** (Preferences)
- Ciclos subsecuentes **recuperan de NVS**

**Almacenamiento persistente:**
```cpp
preferences.putFloat("gps_lat", latitude);
preferences.putFloat("gps_lng", longitude);
preferences.putFloat("gps_alt", altitude);
```

**Ventajas:**
- ✅ Ahorro energético significativo
- ✅ Ciclos 1-2 minutos más rápidos
- ✅ Última posición conocida en todas las tramas
- ✅ Coordenadas sobreviven deep sleep

**Comportamiento:**
- Primer ciclo: Lee GPS, guarda coordenadas
- Ciclos subsecuentes: Lee coordenadas de NVS
- Si no hay GPS guardado: Usa ceros sin romper sistema

---

### 17. Estrategia de Muestreo de Sensores

**Técnica:** Promediado con descarte (para mejorar precisión)

**ADC (Batería):**
- Descarta 5 primeras muestras
- Promedia las siguientes 5
- Retorna valor promedio redondeado

**I2C (Temperatura/Humedad):**
- Descarta 5 primeras lecturas
- Promedia las siguientes 5
- Retorna temperatura y humedad × 100 (enteros)

**RS485 (Modbus):**
- Descarta 5 primeras lecturas
- Toma la última lectura válida
- Lee 4 registros simultáneos

---

### 18. Gestión de Energía

**Deep Sleep:**
- Configurado por timer (defecto 10 minutos)
- GPIOs en hold durante sleep (7, 3, 9)
- Wakeup por timer o UNDEFINED (primer boot)

**Optimizaciones:**
- GPS solo en primer ciclo
- LTE usa operadora guardada (evita escaneo)
- Sensores: solo muestras necesarias
- BLE: timeout 1 minuto, luego se apaga

---

### 19. Manejo de Errores y Robustez

**Estrategias implementadas:**
1. **Buffer persistente** → No se pierden datos ante fallos
2. **Operadora auto-recuperable** → Si falla, vuelve a escanear
3. **GPS con fallback** → Si no hay fix, usa última conocida o ceros
4. **Logs extensivos** → Debug de cada paso del proceso
5. **Estados de error** → Sistema maneja fallos sin crashear

**Reintentos automáticos:**
- Buffer: Tramas pendientes se reintentan en próximo ciclo
- Operadora: Si falla la guardada, escanea todas
- Sensores: Valores por defecto "0" si falla lectura

---

### 20. Configuración de Hardware

**Pines ESP32-S3:**
```cpp
// RS485 Modbus
RS485_TX: 16
RS485_RX: 15
ENPOWER: 3

// I2C (Temp/Hum)
I2C_SDA: 6
I2C_SCL: 12

// ADC (Batería)
ADC_PIN: 13

// GPS (SIM7080G)
GPS_TX: 10
GPS_RX: 11
GPS_PWRKEY: 9

// LTE (SIM7080G - comparte UART con GPS)
LTE_TX: 10
LTE_RX: 11
LTE_PWRKEY: 7

// RTC
RTC_ENABLE_LOW: (definir)
RTC_ENABLE_HIGH: (definir)
```

**Seriales:**
- Serial0 (USB): Debug/Monitor
- Serial1: LTE/GPS (compartido)
- Serial2: RS485

---

### 21. Lessons Learned - Debugging

**Errores comunes resueltos:**

1. **Buffer overflow en Base64:**
   - Problema: `FRAME_MAX_LEN = 102` insuficiente
   - Solución: Crear `FRAME_BASE64_MAX_LEN = 200`

2. **Macros de debug en conflicto:**
   - Problema: GPSModule tenía macros propias vs. sistema centralizado
   - Solución: Comentar macros antiguas, usar sistema centralizado

3. **Secuencia de operación incorrecta:**
   - Problema: ICCID se obtenía sin encender LTE
   - Solución: Agregar estado `Cycle_GetICCID` dedicado

4. **GPS en todos los ciclos:**
   - Problema: Consumo excesivo de energía
   - Solución: GPS solo en primer ciclo + almacenamiento persistente

---

## Estructura de Código Final

```
sensores_rv03/
├── sensores_rv03.ino           # Entry point (setup/loop)
├── AppController.cpp/.h         # Máquina de estados principal
├── data/
│   └── buffer.txt              # Buffer persistente LittleFS
├── memoria_de_proyecto/
│   └── MEMORIA_SENSORES_RV03.md
└── src/
    ├── DebugConfig.h           # Sistema debug centralizado
    ├── DEBUG_SYSTEM.md         # Documentación debug
    ├── data_buffer/
    │   ├── BLEModule.*
    │   ├── BUFFERModule.*
    │   └── config_data_buffer.h
    ├── data_format/
    │   ├── FORMATModule.*      # Construcción trama + Base64
    │   └── config_data_format.h
    ├── data_gps/
    │   ├── GPSModule.*
    │   └── config_data_gps.h
    ├── data_lte/
    │   ├── LTEModule.*         # Comunicación celular
    │   ├── config_data_lte.h
    │   └── config_operadoras.h
    ├── data_sensors/
    │   ├── ADCSensorModule.*
    │   ├── I2CSensorModule.*
    │   ├── RS485Module.*
    │   └── config_data_sensors.h
    ├── data_sleepwakeup/
    │   ├── SLEEPModule.*
    │   └── config_data_sleepwakeup.h
    └── data_time/
        ├── RTCModule.*
        └── config_data_time.h
```

---

## Métricas del Sistema

**Consumo estimado:**
- Deep sleep: ~10 μA
- Lectura sensores: ~50 mA × 2-5 segundos
- GPS (primer ciclo): ~150 mA × 30-60 segundos
- LTE transmisión: ~200-400 mA × 10-30 segundos

**Ciclo típico (sin GPS):**
- Duración: ~1-2 minutos
- Consumo promedio: ~100 mA

**Ciclo con GPS (primer boot):**
- Duración: ~2-3 minutos
- Consumo promedio: ~150 mA

---

## Nota Final

Esta memoria no sustituye al README técnico.
Su objetivo es **capturar el razonamiento**, para que:
- el proyecto sea mantenible
- el conocimiento no se pierda
- ChatGPT pueda usarse como asistente con contexto histórico

**Última actualización:** Diciembre 17, 2025

