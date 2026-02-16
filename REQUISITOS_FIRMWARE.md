# Requisitos de Alto Nivel — Datalogger IoT JARM 4.5

**Documento:** REQ-JARM45-001
**Version:** 1.3
**Fecha:** 2026-02-16
**Estado:** Borrador
**Autor:** Ingenieria de Firmware

---

## 1. Proposito

Este documento define los requisitos de alto nivel del sistema datalogger IoT JARM 4.5, un dispositivo autonomo de adquisicion de datos ambientales e industriales con transmision celular. Los requisitos estan redactados conforme a IEEE 29148:2018 y emplean la convencion **"El sistema debe"** (shall) para requisitos obligatorios y **"El sistema deberia"** (should) para requisitos deseables.

## 2. Alcance del sistema

El JARM 4.5 es un datalogger de campo alimentado por energia solar que:

- Adquiere datos de sensores analogicos, I2C y RS485 Modbus RTU
- Transmite tramas via LTE Cat-M1/NB-IoT a un servidor TCP
- Almacena datos localmente cuando la transmision no es posible
- Opera de forma autonoma sin intervencion humana

### 2.1 Principios rectores

| ID | Principio |
|----|-----------|
| PRINC-01 | **Prioridad absoluta a adquisicion + almacenamiento.** Mientras el sistema este energizado, el almacenamiento local de datos tiene prioridad sobre la transmision. Ningun fallo de red, energia o modem puede interrumpir la captura y persistencia de datos. |
| PRINC-02 | **Continuidad operativa en fallos criticos.** El sistema debe seguir leyendo y almacenando datos incluso en fallos severos (sin red, modem corrupto, reconexion fallida, reinicios) sin comprometer la integridad de datos existentes. |
| PRINC-03 | **Diseño modular y escalable.** La arquitectura debe permitir integrar nuevos sensores RS-485, I2C, 1-Wire, ADC u otros sin refactorizar el nucleo de adquisicion, almacenamiento o transmision. |

### 2.2 Partes interesadas

| Rol | Interes |
|-----|---------|
| Operador de campo | Instalacion, reubicacion, mantenimiento |
| Plataforma cloud | Recepcion y procesamiento de tramas |
| Ingenieria de producto | Escalabilidad, costo, confiabilidad |
| Cliente final | Datos continuos y confiables |

---

## 3. Requisitos funcionales

### 3.1 Adquisicion de datos

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| FR-01 | El sistema debe medir voltaje de bateria mediante ADC con divisor resistivo (rango 0–6.6V, resolucion 12 bits). | Alta |
| FR-02 | El sistema debe leer temperatura y humedad relativa via sensor I2C AHT20. | Alta |
| FR-03 | El sistema debe leer hasta 4 registros holding de un dispositivo esclavo RS485 Modbus RTU (9600 baud, timeout 1200ms). | Alta |
| FR-04 | El sistema debe descartar las primeras 5 lecturas de cada sensor y promediar las siguientes 5 lecturas para obtener un valor representativo. | Media |
| FR-05 | El sistema debe obtener coordenadas GPS (latitud, longitud, altitud) del modulo GNSS integrado en el SIM7080G. | Alta |
| FR-06 | El sistema debe obtener la marca de tiempo (epoch Unix) de un RTC externo DS3231 via I2C. | Alta |
| FR-07 | El sistema debe leer el ICCID de la tarjeta SIM via comando AT y almacenarlo en NVS para disponibilidad persistente. | Alta |
| FR-08 | Si la lectura del ICCID falla (modem apagado, bateria baja, timeout), el sistema debe recuperar el ICCID previamente almacenado en NVS. | Alta |

### 3.2 Formato y construccion de trama

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| FR-10 | El sistema debe construir una trama de texto plano con el formato: `$,<ICCID>,<epoch>,<lat>,<lng>,<alt>,<var1..var4>,<temp>,<hum>,<vbat>,#` donde cada campo tiene longitud fija con padding de ceros a la izquierda. | Alta |
| FR-11 | El sistema debe codificar la trama en Base64 antes de transmitirla o almacenarla. | Alta |
| FR-12 | La trama en texto plano no debe exceder 102 bytes. La trama codificada en Base64 no debe exceder 200 bytes. | Alta |
| FR-13 | Los valores de temperatura y humedad deben representarse como enteros multiplicados por 100 (ej: 21.56C = 2156). | Media |
| FR-14 | El voltaje de bateria debe representarse como entero multiplicado por 100 (ej: 3.81V = 0381). | Media |

### 3.3 Almacenamiento local (buffer)

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| FR-20 | El sistema debe almacenar tramas pendientes de envio en un archivo de texto en LittleFS (flash interna). | Alta |
| FR-21 | El sistema debe marcar cada trama enviada exitosamente con el prefijo `[P]` en el archivo de buffer. | Alta |
| FR-22 | El sistema debe eliminar las lineas marcadas como procesadas (`[P]`) al finalizar cada ciclo de transmision. | Alta |
| FR-23 | El sistema debe procesar un maximo de 50 tramas por ciclo de lectura del buffer. | Alta |
| FR-24 | El sistema debe conservar las tramas no enviadas en el buffer para reintento en ciclos posteriores. | Alta |
| FR-25 | La capacidad minima de almacenamiento debe ser 50 tramas (~8 horas a intervalos de 10 minutos). | Media |
| FR-26 | El sistema debe eliminar caracteres de control residuales (`\r`, `\n`, espacios) de cada linea leida del buffer para evitar corrupcion progresiva de tramas durante ciclos de reescritura. | Alta |

### 3.4 Comunicacion celular LTE

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| FR-30 | El sistema debe transmitir tramas almacenadas en buffer via conexion TCP al servidor configurado. | Alta |
| FR-31 | El sistema debe soportar al menos 4 operadoras celulares configurables con sus respectivos APN. | Alta |
| FR-32 | El sistema debe seleccionar automaticamente la operadora con mejor senal en la primera conexion, utilizando una formula de puntaje basada en SINR, RSRP y RSRQ. | Alta |
| FR-33 | El sistema debe almacenar en NVS el indice de la operadora exitosa y reutilizarla en ciclos posteriores. | Alta |
| FR-34 | Si la operadora almacenada falla, el sistema debe limpiar la seleccion guardada y escanear todas las operadoras disponibles (fallback). | Alta |
| FR-35 | Si ninguna operadora responde durante el escaneo, el sistema debe activar una proteccion anti-bucle que omita el escaneo durante los siguientes 3 ciclos. | Media |
| FR-36 | El sistema debe deshabilitar el modo PSM del modem (`AT+CPSMS=0`) tras cada encendido para evitar perdida del primer comando AT. | Alta |
| FR-37 | La secuencia de encendido del modem debe incluir pulso PWRKEY de al menos 1200ms con verificacion de respuesta (`isAlive`). | Alta |
| FR-38 | El sistema debe intentar hasta 3 pulsos PWRKEY antes de ejecutar un reset forzado de 12.6s como ultimo recurso. | Media |
| FR-38B | El sistema debe garantizar que tras un restart por software, todos los mecanismos de recovery del modem (incluyendo contadores de backoff de reintentos) operen desde su estado inicial, permitiendo un ciclo completo de recovery. | Alta |
| FR-39 | El sistema deberia distinguir entre fallo de encendido del modem (powerOn fallido) y fallo de red (modem encendio pero no pudo transmitir). Solo los fallos de encendido deberian contabilizarse para el mecanismo de recovery por fallos consecutivos (NFR-12B). | Media |

### 3.5 GPS y geolocalizacion

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| FR-40 | El sistema debe intentar obtener fix GPS con un maximo de 100 reintentos (1 segundo entre cada uno). | Alta |
| FR-41 | Si el GPS obtiene fix, el sistema debe almacenar las coordenadas en NVS para uso en ciclos posteriores. | Alta |
| FR-42 | La decision de ejecutar GPS debe basarse en la existencia de coordenadas en NVS, no en `esp_reset_reason()`. | Alta |
| FR-43 | Si NVS contiene coordenadas validas, el sistema debe omitir la adquisicion GPS y usar las coordenadas almacenadas. | Alta |
| FR-44 | El GPS solo debe ejecutarse cuando no existan coordenadas en NVS (primer boot o reflash). | Alta |

### 3.6 Diagnostico y mantenimiento

| ID | Requisito | Prioridad | Estado |
|----|-----------|-----------|--------|
| FR-60 | Cuando ocurra un fallo critico (watchdog, brown-out, modem zombie, RS-485 sin respuesta), el sistema debe almacenar un log critico en NVS con contexto minimo (timestamp, tipo de fallo, estado FSM). Debe conservar al menos los ultimos 10 eventos, aun tras reinicios. | Media | No implementado |
| FR-61 | El sistema debe proveer un modo mantenimiento por puerto serial que permita exportar todos los datos almacenados en buffer y los logs criticos sin borrar la memoria interna. | Baja | No implementado |
| FR-62 | En modo mantenimiento, el sistema debe proveer comandos serial para consultar: voltaje de bateria, hora RTC, memoria usada (LittleFS + heap), estado del modem, y cola de transmision pendiente. Respuesta < 2s por comando. | Baja | No implementado |

### 3.7 Maquina de estados finitos (FSM)

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| FR-50 | El sistema debe operar mediante una maquina de estados finitos con los siguientes estados en orden: Boot, ReadSensors, GPS (condicional), GetICCID, BuildFrame, BufferWrite, SendLTE, CompactBuffer, Sleep. | Alta |
| FR-51 | El estado Error debe activarse unicamente si la inicializacion de LittleFS falla, y debe detener la ejecucion normal del sistema. | Alta |
| FR-52 | Cada transicion de estado debe ser determinista y no bloqueante, excepto en estados terminales (Sleep, Error). | Media |

---

## 4. Requisitos no funcionales

### 4.1 Energia y autonomia

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| NFR-01 | El consumo en deep sleep no debe exceder 50 uA. | Alta |
| NFR-02 | El intervalo de deep sleep debe ser configurable entre 30 segundos y 1 hora. El valor por defecto de produccion es 10 minutos. | Alta |
| NFR-03 | El sistema debe soportar alimentacion solar con bateria de respaldo. | Alta |
| NFR-04 | El sistema debe verificar que el voltaje de bateria sea >= 3.80V antes de encender el modem. Si es inferior, debe omitir GPS, ICCID y LTE, y almacenar datos en buffer. | Alta |
| NFR-05 | El ciclo activo completo (sin GPS) no debe exceder 120 segundos en condiciones normales de senal. | Media |
| NFR-06 | `[AUTOSWITCH]` El sistema debe ejecutar un reset fisico de seguridad (autoswitch via GPIO 2) cada 144 ciclos (~24 horas a 10 min/ciclo) como watchdog de hardware. | Alta |
| NFR-07 | `[AUTOSWITCH]` El contador de ciclos para el autoswitch debe persistir en RTC memory y reiniciarse a cero tras el reset fisico. | Alta |

### 4.2 Confiabilidad

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| NFR-10 | El sistema debe operar de forma autonoma sin intervencion humana durante al menos 30 dias. | Alta |
| NFR-11 | Ningun fallo de comunicacion debe causar perdida de datos: las tramas deben permanecer en buffer hasta su envio exitoso. | Alta |
| NFR-12 | El sistema debe intentar recuperarse de estados zombie tipo A del modem dentro de cada ciclo mediante la secuencia: reintentos PWRKEY + reset forzado 12.6s. Si la recovery intra-ciclo falla, el resultado se propaga a NFR-12B. `[AUTOSWITCH]` En hardware con autoswitch, el reset fisico cada 24h es una capa adicional. | Alta |
| NFR-12B | El sistema debe mantener un contador persistente entre deep sleeps de ciclos consecutivos con fallo de modem. Cuando el contador supere un umbral configurable (por defecto: 6), el sistema debe ejecutar un restart por software como intento de recovery. Tras el restart, el sistema debe operar en modo backoff (solo sensores + buffer) durante un numero configurable de ciclos (por defecto: 3) antes de reintentar el modem. El contador debe reiniciarse a cero tras una transmision exitosa. | Alta |
| NFR-13 | Los datos de configuracion criticos (operadora, coordenadas GPS, ICCID, proteccion anti-bucle) deben almacenarse en NVS (flash), no en RTC memory, para sobrevivir cortes de alimentacion. | Alta |
| NFR-14 | El uso de memoria de stack por funcion no debe exceder 4 KB para evitar stack overflow en el task principal (8 KB). | Alta |

### 4.3 Condiciones ambientales

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| NFR-15 | El sistema debe operar en un rango de temperatura de -10°C a 60°C. | Alta |
| NFR-16 | El sistema debe operar en un rango de humedad relativa de 5% a 95% sin condensacion. | Alta |
| NFR-17 | Si el voltaje cae por debajo del umbral minimo operativo durante una escritura a LittleFS o NVS, el sistema de archivos no debe quedar en estado corrupto. LittleFS provee esta garantia por diseño (copy-on-write + journaling). | Alta |

### 4.4 Mantenibilidad

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| NFR-20 | El firmware debe imprimir un banner de boot con version, causa de reset y causa de wakeup al iniciar cada ciclo. | Media |
| NFR-21 | El firmware debe imprimir un resumen de ciclo antes de entrar en deep sleep con tiempos por fase y resultado de transmision. | Media |
| NFR-22 | La version de firmware debe estar definida en un unico punto del codigo fuente. | Media |

### 4.5 Seguridad

| ID | Requisito | Prioridad | Estado |
|----|-----------|-----------|--------|
| NFR-30 | Las tramas deben transmitirse codificadas en Base64 sobre TCP. | Alta | Implementado |
| NFR-31 | El ICCID incluido en cada trama debe permitir identificar unicamente al dispositivo. | Alta | Implementado |
| NFR-32 | Todo envio de datos deberia ir cifrado mediante TLS 1.2+ cuando el modem y la red lo soporten. | Media | No implementado (el SIM7080G soporta SSL/TLS via AT+CSSLCFG) |

---

## 5. Requisitos de interfaz

### 5.1 Interfaces de hardware

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| IR-01 | El sistema debe comunicarse con el modem SIM7080G via UART a 115200 baud (GPIO 10 TX, GPIO 11 RX). | Alta |
| IR-02 | El sistema debe controlar el encendido/apagado del modem via PWRKEY (GPIO 9, pulso HIGH). | Alta |
| IR-03 | El sistema debe medir voltaje de bateria via ADC en GPIO 13 con divisor resistivo de relacion 0.5. | Alta |
| IR-04 | El sistema debe comunicarse con el sensor AHT20 via I2C (GPIO 6 SDA, GPIO 12 SCL, 100 kHz). | Alta |
| IR-05 | El sistema debe comunicarse con sensores RS485 Modbus RTU via UART a 9600 baud (GPIO 16 TX, GPIO 15 RX). | Alta |
| IR-06 | `[AUTOSWITCH]` El sistema debe controlar el circuito de autoswitch via GPIO 2 (HIGH activa el reset fisico). | Alta |
| IR-07 | El sistema debe leer la hora de un RTC DS3231 via I2C. | Alta |

### 5.2 Interfaces de comunicacion

| ID | Requisito | Prioridad |
|----|-----------|-----------|
| IR-10 | El sistema debe conectarse al servidor via TCP en la direccion y puerto configurados (por defecto: d04.elathia.ai:13607). | Alta |
| IR-11 | El sistema debe soportar las bandas y tecnologias de acceso de al menos 4 operadoras en Mexico (Telcel, AT&T 334090, AT&T 334050, Movistar). | Alta |
| IR-12 | El sistema debe imprimir logs de diagnostico por Serial a 115200 baud. | Media |

---

## 6. Restricciones

| ID | Restriccion |
|----|-------------|
| C-01 | MCU: ESP32-S3 con 8 KB de stack para el task principal. |
| C-02 | Modem: SIM7080G con GPS y LTE Cat-M1/NB-IoT integrados. Control de encendido/apagado exclusivamente via PWRKEY (GPIO 9) y comando AT+CPOWD=1. Sin MOSFET de corte en VBAT. Sin RESET pin rutado. No existe forma de cortar alimentacion al modem por software. Un zombie tipo B (latch-up electrico) solo se resuelve con power cycle fisico externo. |
| C-03 | Almacenamiento: Flash interna unicamente (sin SD card). Particion LittleFS para buffer, NVS para configuracion. |
| C-04 | Regulador: TLV62569 con tension de salida 3.82V. Opera en dropout con bateria < 3.80V bajo carga de 500mA del modem. |
| C-05 | GPIO 13 es compartido entre ADC de bateria y otras funciones. Debe configurarse como INPUT antes de deep sleep. |
| C-06 | `[AUTOSWITCH]` El autoswitch (GPIO 2) causa un corte total de alimentacion, borrando RTC memory. Toda persistencia entre resets debe usar NVS o LittleFS. |
| C-07 | El framework es Arduino (bare-metal, sin RTOS). El loop principal es cooperativo y no preemptivo. |

---

## 7. Supuestos

| ID | Supuesto |
|----|----------|
| A-01 | El dispositivo opera en exteriores con cobertura celular Cat-M1 o NB-IoT de al menos una operadora. |
| A-02 | La alimentacion solar es suficiente para mantener la bateria por encima de 3.80V durante el dia. |
| A-03 | El servidor TCP esta disponible y acepta conexiones en el puerto configurado. |
| A-04 | La ubicacion fisica del dispositivo no cambia entre ciclos (las coordenadas GPS se obtienen una sola vez). |
| A-05 | Los sensores RS485 Modbus responden dentro del timeout de 1200ms. |

---

## 8. Trazabilidad

### Requisitos → Modulos de firmware (JAMR_4.5)

| Modulo | Requisitos cubiertos |
|--------|---------------------|
| AppController | FR-50, FR-51, FR-52, NFR-20, NFR-21, NFR-12B, PRINC-01, PRINC-02 |
| SensorModule (ADC) | FR-01, FR-04, IR-03 |
| AHT20Module (I2C) | FR-02, FR-04, FR-13, IR-04 |
| RS485Module (Modbus) | FR-03, FR-04, IR-05 |
| FRAMEModule | FR-10, FR-11, FR-12, FR-13, FR-14, NFR-31 |
| BUFFERModule | FR-20, FR-21, FR-22, FR-23, FR-24, FR-25, FR-26, NFR-14 |
| LTEModule | FR-30, FR-31, FR-32, FR-33, FR-34, FR-35, FR-36, FR-37, FR-38, FR-38B, IR-01, IR-02 |
| GPSModule | FR-05, FR-40, FR-41 |
| AppController (GPS NVS) | FR-42, FR-43, FR-44 |
| AppController (ICCID NVS) | FR-07, FR-08 |
| RTCModule | FR-06, IR-07 |
| SLEEPModule | NFR-01, NFR-02 |

### Requisitos → Fixes implementados (JAMR_4.5)

Nota: La numeracion FIX-Vn es especifica de JAMR_4.5. No confundir con la numeracion
de jarm_4.5_autoswich que usa numeros diferentes para fixes distintos.

| Fix | Requisitos que satisface | Estado |
|-----|-------------------------|--------|
| FIX-V1 | FR-33 (skip reset PDP cuando operadora guardada) | Implementado (v2.0.2) |
| FIX-V2 | FR-34, FR-35 (fallback operadora + anti-bucle) | Implementado (v2.2.0) |
| FIX-V3 | NFR-04 (modo reposo por bateria baja, histeresis 3.20/3.80V) | Implementado (v2.3.0) |
| FIX-V4 | NFR-12 parcial (powerOff antes de deep sleep, prevenir zombie) | Implementado (v2.4.0) |
| FIX-V5 | — (watchdog, DEPRECADO: ESP-IDF v5.x provee TWDT) | Deshabilitado |
| FIX-V6 | FR-37, FR-38 (secuencia robusta powerOn/Off per datasheet SIM7080G) | Implementado (v2.8.0) |
| FIX-V7 | NFR-12, FR-36, FR-38 (zombie mitigation: PSM disable + PWRKEY retries + reset forzado) | Implementado (v2.9.0) |
| FIX-V8 | FR-60 parcial (logging de fallos ICCID via ProdDiag) | Implementado (v2.9.0) |
| FIX-V9 | — (hard power cycle IO13, DEPRECADO: hardware no soporta) | Deshabilitado |
| FIX-V11 | FR-07, FR-08 (cache ICCID en NVS con fallback) | Pendiente (v2.12.0) |
| FIX-V12 | FR-26 (trim CR en lecturas de buffer LittleFS) | Pendiente (v2.11.0) |
| FIX-V13 | NFR-12, NFR-12B, FR-38B (recovery por fallos consecutivos + esp_restart + backoff) | Pendiente (v2.13.0) |

---

## 9. Glosario

| Termino | Definicion |
|---------|-----------|
| Autoswitch | Circuito externo controlado por GPIO 2 que corta la alimentacion del ESP32 para forzar un reinicio completo |
| Brownout | Caida de voltaje por debajo del minimo operativo del regulador durante picos de corriente del modem |
| Deep sleep | Modo de bajo consumo del ESP32 (~10 uA) que preserva RTC memory pero detiene la CPU |
| FSM | Finite State Machine — patron de diseno donde el sistema transita entre estados discretos |
| ICCID | Integrated Circuit Card Identifier — numero unico de 20 digitos de la tarjeta SIM |
| LittleFS | Sistema de archivos flash diseñado para microcontroladores con wear leveling |
| NVS | Non-Volatile Storage — almacenamiento clave-valor en flash del ESP32 que sobrevive resets y cortes de energia |
| PSM | Power Saving Mode del modem LTE que puede causar perdida del primer comando AT tras despertar |
| PWRKEY | Pin de control de encendido/apagado del modem SIM7080G |
| RTC memory | Memoria que sobrevive deep sleep pero NO cortes de alimentacion |
| Trama | Paquete de datos estructurado que contiene una lectura completa de todos los sensores |
| TLS | Transport Layer Security — protocolo criptografico para comunicaciones seguras sobre TCP |
| UTS | Umbral de Transmision Segura — voltaje minimo de bateria (3.80V) para operar el modem sin riesgo de brownout |
| Zombie tipo A (modem) | Estado donde el modem SIM7080G no responde a comandos AT ni a PWRKEY por causa de software (PSM activo, UART corrupto). Recuperable via reset forzado 12.6s o esp_restart() |
| Zombie tipo B (modem) | Estado donde el modem SIM7080G esta en latch-up electrico. No responde a PWRKEY ni a reset forzado. Solo recuperable con corte fisico de VBAT (power cycle externo) |
