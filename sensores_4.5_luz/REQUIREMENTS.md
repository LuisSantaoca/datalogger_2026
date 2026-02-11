# REQUIREMENTS — sensores_4.5_luz

**Documento generado por análisis estático de código fuente**
**Fecha:** 2026-02-10
**Versión de firmware analizada:** sensores_4.5_luz (basada en Sensores RV03 v2.0)

---

## Índice

1. [Requisitos de Plataforma y Hardware](#1-requisitos-de-plataforma-y-hardware)
2. [Requisitos Funcionales del Sistema](#2-requisitos-funcionales-del-sistema)
3. [Requisitos de Adquisición de Datos](#3-requisitos-de-adquisición-de-datos)
4. [Requisitos de Geolocalización (GPS/GNSS)](#4-requisitos-de-geolocalización-gpsgnss)
5. [Requisitos de Comunicación Celular (LTE)](#5-requisitos-de-comunicación-celular-lte)
6. [Requisitos de Formato y Transmisión de Datos](#6-requisitos-de-formato-y-transmisión-de-datos)
7. [Requisitos de Almacenamiento Persistente](#7-requisitos-de-almacenamiento-persistente)
8. [Requisitos de Gestión de Energía](#8-requisitos-de-gestión-de-energía)
9. [Requisitos de Configuración Inalámbrica (BLE)](#9-requisitos-de-configuración-inalámbrica-ble)
10. [Requisitos de Observabilidad y Debug](#10-requisitos-de-observabilidad-y-debug)
11. [Requisitos de Confiabilidad y Tolerancia a Fallos](#11-requisitos-de-confiabilidad-y-tolerancia-a-fallos)
12. [Requisitos de Arquitectura de Software](#12-requisitos-de-arquitectura-de-software)
13. [Requisitos de Control de Periféricos Eléctricos](#13-requisitos-de-control-de-periféricos-eléctricos)

---

## 1. Requisitos de Plataforma y Hardware

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| HW-01 | El sistema debe operar sobre un MCU **ESP32-S3** con soporte de deep sleep. | `#include <esp_sleep.h>`, API `esp_deep_sleep_start()`, `analogSetAttenuation(ADC_11db)` |
| HW-02 | El sistema debe comunicarse con un módulo módem **SIM7080G** compartido entre GPS (GNSS) y LTE, a través de UART1 a 115200 baud (pines TX=10, RX=11, PWRKEY=9). | `HardwareSerial SerialLTE(1)`, `GPS_PIN_TX=10`, `LTE_PIN_TX=10`, `GPS_SIM_BAUD=115200` |
| HW-03 | El sistema debe integrar un **RTC externo DS1307** conectado por I2C (SDA=GPIO6, SCL=GPIO12, 100 kHz) para generar timestamps epoch. | `RTC_DS1307 rtc`, `I2C_SDA_PIN=GPIO_NUM_6`, `I2C_SCL_PIN=GPIO_NUM_12`, `I2C_FREQUENCY=100000` |
| HW-04 | El sistema debe integrar un **sensor AHT20** (I2C) para medir temperatura y humedad ambiente. | `#include <AHT20.h>`, `aht20_.getTemperature()`, pines `I2C_SDA=6, I2C_SCL=12` |
| HW-05 | El sistema debe integrar sensores industriales **RS485 Modbus RTU** (pines TX=16, RX=15, 9600 baud) con lectura de 4 registros holding (dirección base 0x0000, slave ID 48). | `RS485_TX=16`, `RS485_RX=15`, `MODBUS_SLAVE_ID=48`, `MODBUS_START_ADDRESS=0x0000`, `MODBUS_REGISTER_COUNT=4` |
| HW-06 | El sistema debe medir el **voltaje de batería** mediante el ADC interno del ESP32-S3 en GPIO13 (resolución 12 bits, referencia 3.3V). | `ADC_VOLT_BAT=13`, `ADC_RESOLUTION=4095.0`, `ADC_VREF=3.3`, `analogReadResolution(12)` |
| HW-07 | El sistema debe controlar la **alimentación de sensores externos** mediante un pin de enable (GPIO3) como salida de potencia. | `ENPOWER=3`, `enablePow() → digitalWrite(ENPOWER, HIGH)` |

---

## 2. Requisitos Funcionales del Sistema

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| SYS-01 | El sistema debe implementar una **Máquina de Estados Finitos (FSM)** con 11 estados que orqueste el ciclo completo: `Boot → BleOnly → ReadSensors → Gps → GetICCID → BuildFrame → BufferWrite → SendLTE → CompactBuffer → Sleep → (repetir)`. | `enum class AppState : uint8_t { Boot=0, BleOnly, Cycle_ReadSensors, ... Error }` |
| SYS-02 | El sistema debe ejecutar un ciclo completo de adquisición-transmisión-sleep de forma autónoma, sin intervención del usuario en operación normal. | Flujo completo en `AppLoop()`: sensores → GPS → trama → buffer → LTE → compactar → sleep |
| SYS-03 | El intervalo de operación predeterminado debe ser **10 minutos**, configurable entre 30 segundos y 1 hora. | `SLEEP_10_MINUTES`, `TIEMPO_SLEEP_ACTIVO = SLEEP_10_MINUTES`, constantes de 30s a 1h definidas |
| SYS-04 | El sistema debe realizar un **reinicio preventivo cada ~24 horas** (144 ciclos × 10 min) para liberar recursos acumulados. | `ciclos++; if (ciclos >= 144) { ciclos = 0; esp_restart(); }` |
| SYS-05 | Toda la coordinación entre módulos debe pasar exclusivamente por el `AppController` (patrón mediador); los módulos no deben comunicarse directamente entre sí. | Todos los módulos son instanciados como `static` en `AppController.cpp`; ningún módulo importa otro módulo |

---

## 3. Requisitos de Adquisición de Datos

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| DAT-01 | Cada lectura de sensor ADC e I2C debe aplicar un **muestreo robusto**: descartar las primeras 20 muestras y promediar las siguientes 5. | `DISCARD_SAMPLES=20`, `KEEP_SAMPLES=5`, funciones `readADC_DiscardAndAverage()`, `readI2C_DiscardAndAverage()` |
| DAT-02 | El voltaje de batería (ADC) debe leerse con 10 muestras internas por lectura, promediadas internamente, con fórmula: `valor = ((voltaje_raw × 2) + 0.3) × 100`. | `ADC_SAMPLES=10`, `value_ = ((voltage_ * 2) + ADC_ADJUSTMENT) * ADC_MULTIPLIER` (ADJUSTMENT=0.3, MULTIPLIER=100) |
| DAT-03 | Los valores de temperatura y humedad I2C deben almacenarse multiplicados por 100 (centésimas) para preservar 2 decimales como entero. | `int t100 = (int)lround((sumT / got) * 100.0)` |
| DAT-04 | La lectura RS485 Modbus debe descartar las primeras 20 lecturas y **tomar la última lectura válida** (sin promediar, por ser valores discretos). | `readRS485_DiscardAndTakeLast()`: solo guarda la última lectura válida del bloque KEEP_SAMPLES |
| DAT-05 | La trama de datos debe transportar **7 variables** en orden fijo: `var1-4` = registros RS485 Modbus, `var5` = temperatura, `var6` = humedad, `var7` = voltaje batería. | `g_varStr[0..3]=regs[0..3]`, `g_varStr[4]=vTemp`, `g_varStr[5]=vHum`, `g_varStr[6]=vBat` |
| DAT-06 | En caso de fallo de lectura de cualquier sensor, el sistema debe sustituir con **valor por defecto "0"** y continuar el ciclo sin abortar. | `if (!readADC_...) vBat = "0"`, `if (!readI2C_...) { vTemp="0"; vHum="0"; }`, regs iniciaizados a `"0"` |

---

## 4. Requisitos de Geolocalización (GPS/GNSS)

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| GPS-01 | El GPS (GNSS integrado en SIM7080G) debe activarse **solo en el primer ciclo después de un boot desde apagado** (wakeup cause UNDEFINED). | `g_firstCycleAfterBoot = true` solo cuando `g_wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED` |
| GPS-02 | En ciclos subsecuentes (wakeup por timer), las coordenadas GPS deben **recuperarse de NVS** en lugar de encender el GNSS. | `preferences.getFloat("gps_lat"...)` en estado `Cycle_ReadSensors` cuando `!g_firstCycleAfterBoot` |
| GPS-03 | Las coordenadas obtenidas por GPS deben **persistirse en NVS** (namespace `"sensores"`, keys `gps_lat`, `gps_lng`, `gps_alt`) para uso en ciclos futuros. | `preferences.putFloat("gps_lat", fix.latitude)` en estado `Cycle_Gps` |
| GPS-04 | El encendido del módulo GPS debe utilizar una secuencia PWRKEY (pulso de 1200 ms, post-delay de 3000 ms) con hasta **3 intentos**. | `GPS_PWRKEY_PULSE_MS=1200`, `GPS_PWRKEY_POST_DELAY_MS=3000`, `GPS_POWER_ON_ATTEMPTS=3` |
| GPS-05 | La búsqueda de fix GNSS debe reintentar hasta **100 veces** con intervalo de **1 segundo** entre intentos antes de declarar fallo. | `GPS_GNSS_MAX_RETRIES=100`, `GPS_GNSS_RETRY_DELAY_MS=1000` |
| GPS-06 | Si no se obtiene fix GPS, el sistema debe usar **coordenadas cero** (strings rellenos de '0') y continuar sin abortar. | `fillZeros(g_lat, COORD_LEN)` como default; `"usando ceros"` en log |
| GPS-07 | Las coordenadas deben formatearse con **6 decimales** de precisión (~0.1 m en el ecuador). La altitud se redondea al metro entero más cercano. | `snprintf(out, outSz, "%.6f", (double)v)`, `long a = lroundf(altMeters)` |

---

## 5. Requisitos de Comunicación Celular (LTE)

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| LTE-01 | El sistema debe soportar conexión LTE **Cat-M1** con al menos 4 operadoras: Telcel (334020), AT&T México (334090), AT&T México (334050), Movistar (334030). | `enum Operadora { TELCEL, ATT, ATT2, MOVISTAR }`, `OPERADORAS[]` con MCC-MNC |
| LTE-02 | El sistema debe implementar **selección inteligente de operadora** con fórmula de scoring: `Score = (4 × SINR) + 2 × (RSRP + 120) + (RSRQ + 20)`. | `parseSignalQuality()`, `getBestOperator()`, `testOperator()` en LTEModule |
| LTE-03 | La operadora exitosa debe **persistirse en NVS** (key `"lastOperator"`) y reutilizarse en ciclos futuros sin escaneo. | `preferences.putUChar("lastOperator", (uint8_t)operadoraAUsar)` |
| LTE-04 | Si la transmisión falla con la operadora guardada, el sistema debe **borrar la operadora del NVS** y forzar un escaneo completo en el próximo ciclo. | `preferences.remove("lastOperator")` cuando `!anySent && tieneOperadoraGuardada` |
| LTE-05 | El sistema debe registrar un **callback de error para comandos COPS** y apertura TCP fallida, que borre la operadora guardada. | `lte.setErrorCallback(onCOPSError)`, `onCOPSError()` borra NVS |
| LTE-06 | El flujo de conexión LTE debe seguir la secuencia: `powerOn → configureOperator → attachNetwork → activatePDP → openTCPConnection`. | Secuencia completa en `sendBufferOverLTE_AndMarkProcessed()` |
| LTE-07 | El flujo de desconexión LTE debe seguir la secuencia: `closeTCPConnection → deactivatePDP → detachNetwork → powerOff`. | Secuencia de cleanup en `sendBufferOverLTE_AndMarkProcessed()` |
| LTE-08 | La transmisión de datos debe realizarse vía **TCP** al servidor `d04.elathia.ai` en puerto `12608`. | `DB_SERVER_IP "d04.elathia.ai"`, `TCP_PORT " 12608"` |
| LTE-09 | El sistema debe obtener el **ICCID** de la tarjeta SIM encendiendo brevemente el módem, leyendo el ICCID, y apagándolo antes de construir la trama. | Estado `Cycle_GetICCID`: `lte.powerOn()`, `g_iccid = lte.getICCID()`, `lte.powerOff()` |
| LTE-10 | El APN configurado para todas las operadoras debe ser **"em"**. | `apn: "em"` en `OPERADORAS[]` |
| LTE-11 | Todas las operadoras deben configurarse como **LTE Only** (`AT+CNMP=38`) con modo **Cat-M1** (`AT+CMNB=1`). | `cnmp: "AT+CNMP=38"`, `cmnb: "AT+CMNB=1"` en cada `OperadoraConfig` |
| LTE-12 | El sistema debe soportar envío de **SMS** a un número configurado (`+523327022768`). | `sendSMS()` en LTEModule, `SMS_PHONE_NUMBER[] = "+523327022768"` |

---

## 6. Requisitos de Formato y Transmisión de Datos

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| FRM-01 | La trama de datos debe usar el formato: `$,<iccid>,<epoch>,<lat>,<lng>,<alt>,<var1>,<var2>,<var3>,<var4>,<var5>,<var6>,<var7>,#` | `buildFrame()` construye exactamente este formato con separadores coma |
| FRM-02 | El ICCID debe formatearse a **20 caracteres** con relleno de ceros a la izquierda. | `ICCID_LEN=20`, `copyRightAligned(iccid_, ICCID_LEN, iccid)` |
| FRM-03 | El epoch debe formatearse a **10 caracteres** con relleno de ceros a la izquierda. | `EPOCH_LEN=10`, `copyRightAligned(epoch_, EPOCH_LEN, epoch)` |
| FRM-04 | Las coordenadas (latitud, longitud) deben usar hasta **12 caracteres** en formato libre con 6 decimales. | `COORD_LEN=12`, `copyCoordAligned()` |
| FRM-05 | La altitud y las 7 variables deben formatearse a **4 caracteres** con relleno de ceros a la izquierda. | `ALT_LEN=4`, `VAR_LEN=4`, `copyRightAligned()` |
| FRM-06 | La trama texto (máx 102 bytes) debe codificarse a **Base64** (máx 200 bytes) antes de almacenarse y transmitirse. | `FRAME_MAX_LEN=102`, `FRAME_BASE64_MAX_LEN=200`, `buildFrameBase64()` |
| FRM-07 | La codificación Base64 debe implementarse **sin dependencias de librerías externas** (tabla de codificación inline). | `encodeBase64()` en FORMATModule.cpp con tabla hardcoded `ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/` |
| FRM-08 | La trama debe soportar valores negativos en variables, preservando el signo al alinear a la derecha. | `copyRightAligned()`: lógica especial `if (hasNegative)` preserva el signo '-' |

---

## 7. Requisitos de Almacenamiento Persistente

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| BUF-01 | El sistema debe almacenar tramas de datos en un **archivo de buffer persistente** en LittleFS (`/buffer.txt`). | `BUFFER_FILE_PATH "/buffer.txt"`, `LittleFS.begin()`, `appendLine()` |
| BUF-02 | El buffer debe soportar hasta **50 líneas** de tramas acumuladas. | `MAX_LINES_TO_READ 50` |
| BUF-03 | Las tramas enviadas exitosamente deben **marcarse con prefijo `[P]`** en el archivo, sin eliminarlas inmediatamente. | `PROCESSED_MARKER "[P]"`, `markLineAsProcessed()` antepone `[P]` a la línea |
| BUF-04 | La eliminación de tramas procesadas debe realizarse en una **fase de compactación separada** posterior al envío. | Estado `Cycle_CompactBuffer`: `buffer.removeProcessedLines()` |
| BUF-05 | Las tramas no enviadas deben **permanecer en el buffer** para reintento en el próximo ciclo. | Se salta líneas con `[P]`; en fallo de envío se ejecuta `break` sin marcar |
| BUF-06 | Si el envío falla para una trama, el sistema debe **detener el envío** de las tramas restantes (fail-fast) para preservar coherencia. | `if (!sentOk) { ... break; }` en el loop de envío |
| BUF-07 | El almacenamiento de configuraciones críticas (operadora, coordenadas GPS) debe usar **NVS** (namespace `"sensores"`). | `preferences.begin("sensores", ...)`, keys: `lastOperator`, `gps_lat`, `gps_lng`, `gps_alt` |
| BUF-08 | Aunque el guardado en buffer falle, el sistema debe **continuar al envío LTE** para intentar transmitir tramas previas almacenadas. | `Cycle_BufferWrite` siempre transiciona a `Cycle_SendLTE` independientemente del resultado |

---

## 8. Requisitos de Gestión de Energía

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| PWR-01 | El sistema debe entrar en **deep sleep** al final de cada ciclo, con wakeup por timer configurable. | `esp_sleep_enable_timer_wakeup(g_cfg.sleep_time_us)`, `sleepModule.enterDeepSleep()` |
| PWR-02 | Antes de entrar en deep sleep, los GPIOs de control (GPIO3 y GPIO9) deben **desactivarse y mantenerse en estado bajo** usando gpio_hold. | `digitalWrite(GPIO_NUM_3, LOW); gpio_hold_en(GPIO_NUM_3)` en `enterDeepSleep()` |
| PWR-03 | Al despertar de deep sleep, los GPIO hold deben **liberarse** para permitir operación normal. | `gpio_hold_dis(GPIO_NUM_3); gpio_hold_dis(GPIO_NUM_9)` en `SleepModule::begin()` |
| PWR-04 | El GPS debe activarse solo en el **primer ciclo post-boot** para minimizar consumo; ciclos subsecuentes reutilizan coordenadas de NVS. | `g_firstCycleAfterBoot` flag → GPS solo si true |
| PWR-05 | El módulo LTE debe seguir un ciclo estricto de **encendido selectivo**: encender para ICCID → apagar → encender para envío → apagar. | `Cycle_GetICCID` enciende/apaga LTE; `sendBufferOverLTE_AndMarkProcessed()` enciende/apaga LTE |
| PWR-06 | La alimentación de sensores RS485 debe controlarse mediante un **pin de enable** (GPIO3) para cortar energía cuando no se necesitan. | `enablePow() → digitalWrite(ENPOWER, HIGH)`, `ENPOWER=3` |
| PWR-07 | Cada ciclo de wakeup debe limpiar las fuentes de wakeup previas antes de configurar una nueva. | `sleepModule.clearWakeupSources()` → `esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL)` |

---

## 9. Requisitos de Configuración Inalámbrica (BLE)

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| BLE-01 | El BLE debe activarse **exclusivamente en el primer boot desde apagado** (wakeup cause UNDEFINED); nunca en wakeups por timer. | `if (g_wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED) { ble.begin("RV03"); g_state = AppState::BleOnly; }` |
| BLE-02 | El servicio BLE debe publicitarse con nombre de dispositivo **"RV03"**. | `ble.begin("RV03")` |
| BLE-03 | El BLE debe implementar **3 características GATT**: lectura de buffer (Read/Notify), control/comandos (Write), estado del buffer (Read/Notify). | `CHAR_UUID_READ`, `CHAR_UUID_CONTROL`, `CHAR_UUID_STATUS` con propiedades correspondientes |
| BLE-04 | El BLE debe soportar los comandos: `READ_ALL`, `STATUS`, `CLEAR`, `READ:<n>`, `REMOVE_PROCESSED`. | `processCommand()` con handlers para cada comando |
| BLE-05 | El BLE debe **apagarse automáticamente** después de 60 segundos sin conexión o al desconectarse un cliente. | `if (millis() - startTime > 60000) { end(); }`, `if (!deviceConnected && oldDeviceConnected) { end(); }` |
| BLE-06 | Al desactivarse el BLE, el FSM debe transicionar automáticamente al estado `Cycle_ReadSensors`. | `if (!ble.isActive()) { g_state = AppState::Cycle_ReadSensors; }` |
| BLE-07 | El BLE debe permitir la **lectura y limpieza remota del buffer** de datos almacenado. | Comandos `READ_ALL` → `sendBufferData()`, `CLEAR` → `buffer.clearFile()` |

---

## 10. Requisitos de Observabilidad y Debug

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| DBG-01 | El sistema debe implementar un sistema de debug **configurable por módulo y por nivel** (ERROR, WARN, INFO, VERBOSE). | `DebugConfig.h`: `DEBUG_GLOBAL_ENABLED`, niveles 0-4, flags per-module `DEBUG_ADC_ENABLED`, etc. |
| DBG-02 | Los mensajes de debug deben seguir el formato `[NIVEL][MODULO] Mensaje` (ej: `[ERROR][GPS] No se pudo encender`). | Macros `DEBUG_ERROR(module, msg)` → `Serial.print("[ERROR]["); Serial.print(#module);...` |
| DBG-03 | El nivel de debug predeterminado debe ser **INFO** (nivel 3), mostrando errores, advertencias e información. | `#define DEBUG_LEVEL DEBUG_LEVEL_INFO` |
| DBG-04 | Debe ser posible desactivar el debug **completamente** con un solo flag global sin modificar código de módulos. | `#define DEBUG_GLOBAL_ENABLED true/false` controla todas las macros |
| DBG-05 | El módulo LTE debe tener un **modo debug propio** activable que muestra tráfico AT en serial. | `lte.setDebug(true, &Serial)` |
| DBG-06 | La comunicación serial de debug debe operar a **115200 baud**. | `Serial.begin(115200)`, `SERIAL_BAUD_RATE 115200` |

---

## 11. Requisitos de Confiabilidad y Tolerancia a Fallos

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| REL-01 | El fallo de inicialización de LittleFS debe llevar al sistema a un **estado de error** que detenga la operación. | `if (!buffer.begin()) { g_state = AppState::Error; return; }` |
| REL-02 | Los fallos de inicialización de sensores (ADC, I2C, RS485) y RTC **no deben impedir** la continuidad del ciclo. | `(void)adcSensor.begin()`, `(void)i2cSensor.begin()`: resultado ignorado, ciclo continúa |
| REL-03 | El fallo de encendido del módulo LTE para ICCID debe resultar en un **ICCID vacío**, no en aborto del ciclo. | `if (lte.powerOn()) { g_iccid = lte.getICCID(); } else { g_iccid = ""; }` |
| REL-04 | El fallo de construcción de trama (formato o Base64) debe transicionar al **estado Error**. | `if (!formatter.buildFrame(...)) { g_state = AppState::Error; break; }` |
| REL-05 | El envío LTE debe detenerse al **primer fallo de transmisión** de una trama para preservar la integridad del buffer. | `if (!sentOk) { break; }` — las tramas restantes no se marcan como procesadas |
| REL-06 | Si la operadora guardada falla en el envío, el sistema debe **borrarla de NVS** para forzar escaneo en el próximo ciclo. | `preferences.remove("lastOperator")` cuando `!anySent && tieneOperadoraGuardada` |
| REL-07 | El callback `onCOPSError` debe borrar la operadora guardada cuando el comando `AT+COPS` o la apertura TCP fallan, forzando re-escaneo. | `onCOPSError() → preferences.remove("lastOperator")` |
| REL-08 | El reinicio preventivo cada 24 horas (144 ciclos) debe prevenir **acumulación de fugas de memoria** o estados degradados. | `if (ciclos >= 144) { ciclos = 0; esp_restart(); }` |
| REL-09 | Cada etapa de conexión LTE que falle debe ejecutar **limpieza parcial** apropiada antes de retornar (apago de dependencias ya encendidas). | `configureOperator` falla → `powerOff()`; `activatePDP` falla → `powerOff()`; `openTCP` falla → `deactivatePDP() + powerOff()` |

---

## 12. Requisitos de Arquitectura de Software

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| ARC-01 | El sistema debe seguir el **patrón mediador** donde `AppController` es el único orquestador; los módulos no se importan ni se invocan entre sí. | Ningún módulo (GPS, LTE, Buffer, etc.) incluye headers de otro módulo; solo AppController los usa |
| ARC-02 | Cada módulo debe seguir la estructura de carpeta: `src/data_<nombre>/` con archivos `config_<nombre>.h`, `<Nombre>Module.h`, `<Nombre>Module.cpp`. | Carpetas: `data_buffer/`, `data_format/`, `data_gps/`, `data_lte/`, `data_sensors/`, `data_sleepwakeup/`, `data_time/` |
| ARC-03 | Cada módulo debe exponer una interfaz `begin()` → `execute/read()` → `end()` con estados internos privados. | Todas las clases: `begin()`, método principal (`readSensor`, `powerOn`, etc.), miembros privados |
| ARC-04 | Las constantes de configuración deben residir en archivos `config_*.h` dedicados, separados de la lógica. | `config_data_buffer.h`, `config_data_format.h`, `config_data_gps.h`, `config_data_lte.h`, `config_data_sensors.h`, `config_data_sleepwakeup.h`, `config_data_time.h` |
| ARC-05 | El punto de entrada (`*.ino`) debe ser mínimo: solo configurar `AppConfig` y delegar a `AppInit()` / `AppLoop()`. | `sensores_4.5_luz.ino`: 6 líneas funcionales en `setup()`, 1 línea en `loop()` |
| ARC-06 | Los módulos de sensores (ADC, I2C, RS485) deben ser **clases independientes** con interfaces uniformes. | `ADCSensorModule`, `I2CSensorModule`, `RS485Module`: todas con `begin()`, `readSensor()`, getters |
| ARC-07 | El módulo BLE debe recibir una **referencia al BUFFERModule** por inyección de dependencia en el constructor. | `BLEModule(BUFFERModule& bufferModule)`, `static BLEModule ble(buffer)` |

---

## 13. Requisitos de Control de Periféricos Eléctricos

| ID | Requisito | Evidencia en código |
|----|-----------|---------------------|
| CTL-01 | El pin **GPIO3** (`ENPOWER`) debe funcionar como control de alimentación de sensores: HIGH para encender, LOW para apagar. | `enablePow() → digitalWrite(ENPOWER, HIGH)`, `disablePow() → digitalWrite(ENPOWER, LOW)` |
| CTL-02 | El pin **GPIO9** debe funcionar como control PWRKEY del módulo SIM7080G: activo en HIGH con pulso de 1200 ms. | `LTE_PWRKEY_PIN=9`, `LTE_PWRKEY_ACTIVE_HIGH=true`, `LTE_PWRKEY_PULSE_MS=1200` |
| CTL-03 | Antes de cada lectura de sensores I2C (AHT20), el pin de energía **GPIO3 debe activarse HIGH** con un delay de estabilización de 1 segundo. | `readSensor() → digitalWrite(GPIO_NUM_3, HIGH); delay(1000);` en I2CSensorModule.cpp |
| CTL-04 | Antes de la lectura de tiempo (BuildFrame), el sistema debe activar **GPIO3=HIGH y GPIO9=LOW** con delay de 5 segundos para estabilizar alimentación. | `Cycle_BuildFrame: digitalWrite(GPIO_NUM_3, HIGH); digitalWrite(GPIO_NUM_9, LOW); delay(5000);` |
| CTL-05 | Los pines GPIO3 y GPIO9 deben mantenerse en **LOW durante deep sleep** mediante `gpio_hold_en()`. | `enterDeepSleep(): digitalWrite(GPIO_NUM_3, LOW); gpio_hold_en(GPIO_NUM_3)` |

---

## Resumen de Trazabilidad

| Área | IDs | Cantidad |
|------|-----|----------|
| Hardware | HW-01 a HW-07 | 7 |
| Sistema | SYS-01 a SYS-05 | 5 |
| Adquisición de Datos | DAT-01 a DAT-06 | 6 |
| GPS/GNSS | GPS-01 a GPS-07 | 7 |
| LTE/Celular | LTE-01 a LTE-12 | 12 |
| Formato/Transmisión | FRM-01 a FRM-08 | 8 |
| Almacenamiento | BUF-01 a BUF-08 | 8 |
| Gestión de Energía | PWR-01 a PWR-07 | 7 |
| BLE | BLE-01 a BLE-07 | 7 |
| Debug/Observabilidad | DBG-01 a DBG-06 | 6 |
| Confiabilidad | REL-01 a REL-09 | 9 |
| Arquitectura | ARC-01 a ARC-07 | 7 |
| Control Eléctrico | CTL-01 a CTL-05 | 5 |
| **TOTAL** | | **94** |
