# 🌱 Sensores Elathia - Sistema IoT de Monitoreo Agrícola

[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Modem](https://img.shields.io/badge/modem-SIM7080G-green.svg)](https://www.simcom.com/product/SIM7080G.html)
[![Network](https://img.shields.io/badge/network-LTE%20CAT--M%20%2F%20NB--IoT-orange.svg)]()
[![License](https://img.shields.io/badge/license-MIT-lightgrey.svg)](LICENSE)

Sistema avanzado de monitoreo remoto para agricultura de precisión basado en ESP32-S3, con comunicación LTE (CAT-M/NB-IoT), sensores ambientales y de suelo, GPS integrado y gestión inteligente de energía.

## 📋 Tabla de Contenidos

- [Características](#-características)
- [Hardware](#-hardware)
- [Arquitectura del Sistema](#-arquitectura-del-sistema)
- [Sensores Soportados](#-sensores-soportados)
- [Estructura del Proyecto](#-estructura-del-proyecto)
- [Configuración](#-configuración)
- [Instalación](#-instalación)
- [Uso](#-uso)
- [Comunicación de Datos](#-comunicación-de-datos)
- [API Reference](#-api-reference)
- [Troubleshooting](#-troubleshooting)
- [Contribuir](#-contribuir)
- [Licencia](#-licencia)

---

## ✨ Características

### 🌐 Conectividad
- **LTE CAT-M / NB-IoT**: Comunicación celular de bajo consumo
- **Módem SIM7080G**: Integrado con GPS/GNSS
- **Selección automática de operador**: Evaluación inteligente de AT&T, Telcel y Movistar
- **Análisis de señal CPSI**: Métricas RSRP, RSRQ, RSSI y SNR para optimización
- **TCP/IP**: Envío seguro de datos al servidor
- **Buffer local**: Almacenamiento con LittleFS para datos no enviados
- **Cifrado AES**: Encriptación de datos con Base64

### 📡 Sensores
- **Sensores ambientales I2C**: AHT10/AHT20 (temperatura y humedad)
- **Sensores de suelo RS485**: 
  - Temperatura del suelo
  - Humedad del suelo
  - Conductividad eléctrica (EC)
  - pH del suelo
- **GPS**: Geolocalización de precisión
- **Batería**: Monitoreo de voltaje mediante ADC

### ⚡ Eficiencia Energética
- **Deep Sleep**: Consumo reducido (~5 mA)
- **Ciclos de 10 minutos**: Despertar automático programable
- **GPIO Hold**: Mantiene estado de pines en sleep
- **RTC externo**: DS3231 para timekeeping preciso

### 🔒 Seguridad
- **Cifrado AES-128**: Protección de datos sensibles
- **Codificación Base64**: Transmisión segura
- **Validación CRC16**: Integridad de comunicación RS485

---

## 🔧 Hardware

### Microcontrolador
- **ESP32-S3**: Dual-core Xtensa LX7, WiFi/BLE (no usado en este proyecto)
- **Flash**: LittleFS para almacenamiento persistente
- **RAM**: Gestión eficiente de buffers

### Módulo de Comunicación
- **SIM7080G**: Módem LTE CAT-M/NB-IoT con GPS integrado
- **Bandas soportadas**: 2, 4, 5 (CAT-M)
- **Protocolo**: TCP/IP sobre LTE

### Sensores Principales
- **AHT10 / AHT20**: Sensor I2C de temperatura y humedad ambiental
- **Sondas RS485 Modbus**: 
  - DFRobot EC (Conductividad)
  - DFRobot EC+pH
  - Seed Studio EC (actual configuración)

### Periféricos
- **RTC DS3231**: Reloj en tiempo real con batería de respaldo
- **ADC**: Medición de voltaje de batería (Pin 13)
- **LED**: Indicador de estado (Pin 12)

### Pinout ESP32-S3

```
┌─────────────────────────────────┐
│         ESP32-S3 Pinout         │
├─────────────────────────────────┤
│ Pin 9   → PWRKEY (SIM7080G)     │
│ Pin 10  → TX (SIM7080G)         │
│ Pin 11  → RX (SIM7080G)         │
│ Pin 12  → LED Status            │
│ Pin 13  → ADC Battery           │
│ Pin 15  → RS485 RX              │
│ Pin 16  → RS485 TX              │
│ Pin 25  → DTR (SIM7080G)        │
│ I2C     → AHT10/20 + DS3231     │
└─────────────────────────────────┘
```

---

## 🏗️ Arquitectura del Sistema

```
┌──────────────────────────────────────────────────────────────┐
│                     CICLO DE OPERACIÓN                        │
└──────────────────────────────────────────────────────────────┘
         │
         ▼
    ┌─────────┐
    │  SETUP  │  → Inicialización de hardware
    └─────────┘
         │
         ▼
    ┌─────────────────┐
    │  Leer Sensores  │  → AHT10/20, RS485, GPS, Batería
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │  Cifrar Datos   │  → AES-128 + Base64
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │ Seleccionar     │  → Evaluar AT&T, Telcel, Movistar
    │ Mejor Operador  │    (RSRP, RSRQ, SNR, RSSI)
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │  Conectar LTE   │  → Módem SIM7080G (CAT-M/NB-IoT)
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │  Enviar TCP/IP  │  → d04.elathia.ai:12607
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │  Buffer Local?  │  → Guardar si falla envío
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │  Deep Sleep     │  → 10 minutos (~5mA)
    └─────────────────┘
         │
         └──────────────┘ (Loop infinito)
```

### Módulos del Sistema

| Módulo | Archivo | Descripción |
|--------|---------|-------------|
| **Principal** | `sensoresElathia.ino` | Orquestación del sistema |
| **Sensores** | `sensores.h/cpp` | Lectura de AHT10/20 y RS485 |
| **Comunicación** | `gsmlte.h/cpp` | Gestión del módem SIM7080G |
| **Cifrado** | `cryptoaes.h/cpp` | Encriptación AES-128 + Base64 |
| **Tiempo** | `timedata.h/cpp` | RTC DS3231 y timestamps |
| **Energía** | `sleepdev.h/cpp` | Deep sleep y GPIO management |
| **Utilidades** | `crono.h/cpp` | Cronómetro para benchmarking |
| **Tipos** | `type_def.h` | Estructuras de datos |

---

## 📊 Sensores Soportados

### AHT10 / AHT20 (I2C)
```cpp
#define TYPE_AHT 2  // 1 = AHT10, 2 = AHT20
```
- **Temperatura**: -40°C a 85°C (±0.3°C)
- **Humedad**: 0-100% RH (±2%)
- **Dirección I2C**: 0x38

### Sondas RS485 Modbus

#### Seed Studio EC (Actual)
```cpp
#define TYPE_SONDA 3  // Configuración activa
```
- Temperatura del suelo
- Humedad del suelo
- Conductividad eléctrica (EC)

#### DFRobot EC (Opción 1)
```cpp
#define TYPE_SONDA 1
```
- Conductividad eléctrica

#### DFRobot EC+pH (Opción 2)
```cpp
#define TYPE_SONDA 2
```
- Conductividad eléctrica
- pH del suelo

---

## 📁 Estructura del Proyecto

```
sensoresElathia/
│
├── sensoresElathia.ino      # Programa principal
│
├── Módulos de Hardware
│   ├── sensores.h/cpp       # Gestión de sensores (I2C + RS485)
│   ├── gsmlte.h/cpp         # Comunicación LTE/GSM (SIM7080G)
│   └── sleepdev.h/cpp       # Gestión de energía (Deep Sleep)
│
├── Módulos de Software
│   ├── cryptoaes.h/cpp      # Cifrado AES-128 + Base64
│   ├── timedata.h/cpp       # RTC DS3231 y gestión de tiempo
│   └── crono.h/cpp          # Cronómetro (medición de tiempos)
│
├── Configuración
│   └── type_def.h           # Estructuras y tipos de datos
│
├── Datos
│   └── data/
│       └── buffer.txt       # Buffer local de LittleFS
│
└── Documentación
    ├── README.md
    └── LICENSE
```

---

## ⚙️ Configuración

### Parámetros del Módem (gsmlte.h)

```cpp
#define UART_BAUD 115200
#define MODEM_NETWORK_MODE 38    // CAT-M/NB-IoT
#define CAT_M 1
#define NB_IOT 2

// Servidor de datos
#define DB_SERVER_IP "d04.elathia.ai"
#define TCP_PORT "12607"

// APN de red
#define APN "\"em\""

// Operadores soportados (automático)
// AT&T    (33403)
// Telcel  (33420) 
// Movistar (334050)
```

### Parámetros de Sensores (sensores.h)

```cpp
#define TYPE_SONDA 3     // 1=DFRobot EC, 2=DFRobot EC+pH, 3=Seed EC
#define TYPE_AHT 2       // 1=AHT10, 2=AHT20

#define RS485_TX 16
#define RS485_RX 15
#define RS485_BAUD 9600
#define ADC_VOLT_BAT 13
```

### Deep Sleep (sleepdev.h)

```cpp
#define TIME_TO_SLEEP 600    // 10 minutos en segundos
#define uS_TO_S_FACTOR 1000000ULL
```

### Cifrado (cryptoaes.cpp)

```cpp
// ⚠️ CAMBIAR EN PRODUCCIÓN
static const uint8_t AES_KEY[16] = { /* ... */ };
static const uint8_t AES_IV[16] = { /* ... */ };
```

---

## 🚀 Instalación

### 1. Requisitos Previos

#### Software
- **Arduino IDE** 2.x o superior
- **PlatformIO** (opcional)

#### Librerías Requeridas
```bash
# Instalar mediante Library Manager de Arduino
TinyGSM                  # Gestión del módem SIM7080G
Adafruit_AHTX0           # Sensor AHT10/20
RTClib                   # RTC DS3231
mbedtls                  # Cifrado AES
LittleFS                 # Sistema de archivos
SoftwareSerial           # RS485 (ESP32 usa HardwareSerial)
```

#### Instalación automática (PlatformIO)
```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

lib_deps = 
    vshymanskyy/TinyGSM@^0.11.7
    adafruit/Adafruit AHTX0@^2.0.3
    adafruit/RTClib@^2.1.1
    arduino-libraries/Arduino_CRC32@^1.0.0
```

### 2. Configuración del Hardware

1. **Conectar el módulo SIM7080G**:
   ```
   ESP32 Pin 10 → TX (SIM7080G)
   ESP32 Pin 11 → RX (SIM7080G)
   ESP32 Pin 9  → PWRKEY
   ESP32 Pin 25 → DTR
   ```

2. **Conectar sensores I2C** (AHT10/20 + DS3231):
   ```
   SDA → GPIO 21 (por defecto)
   SCL → GPIO 22 (por defecto)
   ```

3. **Conectar RS485**:
   ```
   ESP32 Pin 16 → RS485 TX
   ESP32 Pin 15 → RS485 RX
   ```

4. **Insertar tarjeta SIM** en el módulo SIM7080G

### 3. Compilación y Carga

```bash
# Arduino IDE
1. Abrir sensoresElathia.ino
2. Seleccionar placa: ESP32-S3 Dev Module
3. Configurar:
   - Flash Size: 4MB
   - Partition Scheme: Default 4MB with spiffs
4. Compilar y subir

# PlatformIO
platformio run --target upload
```

---

## 💻 Uso

### Primer Arranque

Al encender el dispositivo por primera vez:

1. **Inicialización del sistema** (5-10 segundos)
2. **Sincronización RTC** con NTP
3. **Lectura de sensores**
### 2. **Evaluación de operadores** (30-90 segundos)
5. **Conexión LTE con mejor operador** (10-30 segundos adicionales)
6. **Envío de datos** al servidor
7. **Entrada en Deep Sleep** (10 minutos)

### Monitoreo Serial

Conectar a 115200 baud para ver logs:

```
🚀 SISTEMA DE MONITOREO AMBIENTAL ELATHIA
==========================================
Iniciando sistema...
⏱️  Cronómetro iniciado - midiendo tiempo de funcionamiento
🛰️  CONFIGURACIÓN DE GPS:
   GPS SIM (módem): ✅ HABILITADO
⏰ Inicializando sistema de tiempo RTC...
✅ RTC DS3231 inicializado
📅 Fecha/Hora: 20/10/2025 14:30:45
📡 Configurando GPS integrado del módem
✅ GPS inicializado correctamente
📊 Leyendo sensores...
🌱 DATOS DE SUELO:
   Temperatura: 22.1°C
   Humedad: 45.0%
   Conductividad: 1250 μS/cm
�️  DATOS AMBIENTALES:
   Temperatura: 25.3°C
   Humedad: 65.2%
🔋 DATOS DEL SISTEMA:
   Voltaje batería: 3.85V
🔐 Cifrando datos (AES-128)...
🤖 Iniciando selección automática del mejor operador
🔍 Evaluando operador: AT&T (33403)
📊 RSRP: -92 dBm, RSRQ: -12 dB, RSSI: -85 dBm, SNR: 8 dB
🔍 Evaluando operador: Telcel (33420)
📊 RSRP: -88 dBm, RSRQ: -10 dB, RSSI: -82 dBm, SNR: 12 dB
🔍 Evaluando operador: Movistar (334050)
📊 RSRP: -95 dBm, RSRQ: -15 dB, RSSI: -88 dBm, SNR: 5 dB
🏆 Mejor operador: Telcel con 77 puntos
✅ Telcel configurado como operador principal
🌐 Conectando a red LTE...
📡 Conectado a: Telcel - RSRP: -88 dBm - RSRQ: -10 dB - SNR: 12 dB
📤 Enviando datos a d04.elathia.ai:12607
✅ Datos enviados exitosamente
� ESTADÍSTICAS FINALES DEL SISTEMA:
   Tiempo total: 65420 ms
   Duración: 1 minutos y 5 segundos
✅ Sistema preparado para deep sleep
🌙 Entrando en modo deep sleep...
⏰ Próximo despertar en 600 segundos
```

### Indicadores LED

| Patrón | Significado |
|--------|-------------|
| **Encendido fijo** | Sistema activo |
| **Parpadeo lento** | Conectando a red |
| **Parpadeo rápido** | Error de conexión |
| **Apagado** | Deep Sleep |

---

## 🎯 Selección Automática de Operadores

### Sistema Inteligente de Conectividad

El sistema evalúa automáticamente tres operadores celulares y selecciona el mejor basándose en métricas de señal en tiempo real:

| Operador | Código MCC-MNC | Comando AT |
|----------|----------------|------------|
| **AT&T** | 33403 | `AT+COPS=1,2,"33403"` |
| **Telcel** | 33420 | `AT+COPS=1,2,"33420"` |
| **Movistar** | 334050 | `AT+COPS=1,2,"334050"` |

### Métricas de Evaluación (CPSI)

El comando `AT+CPSI?` proporciona información detallada de la señal:

```
+CPSI: LTE CAT-M1,Online,334-03,0x13BD,36786976,484,EUTRAN-BAND4,2225,4,4,-10,-104,-80,19
                                                                              │    │    │   │
                                                                              │    │    │   └─ SNR (dB)
                                                                              │    │    └───── RSSI (dBm)
                                                                              │    └────────── RSRQ (dB)
                                                                              └─────────────── RSRP (dBm)
```

### Sistema de Puntuación

Cada operador recibe una puntuación basada en:

#### RSRP (Reference Signal Received Power) - 50% del peso
```cpp
if (rsrp >= -80)  score += 50;  // Excelente
if (rsrp >= -90)  score += 40;  // Buena
if (rsrp >= -100) score += 30;  // Regular
if (rsrp >= -110) score += 20;  // Débil
if (rsrp >= -120) score += 10;  // Muy débil
```

#### RSRQ (Reference Signal Received Quality) - 30% del peso
```cpp
if (rsrq >= -8)   score += 30;  // Excelente
if (rsrq >= -12)  score += 25;  // Buena
if (rsrq >= -15)  score += 20;  // Regular
if (rsrq >= -18)  score += 15;  // Débil
```

#### SNR (Signal to Noise Ratio) - 20% del peso
```cpp
if (snr >= 20)    score += 20;  // Excelente
if (snr >= 15)    score += 15;  // Buena
if (snr >= 10)    score += 12;  // Regular
if (snr >= 5)     score += 8;   // Débil
if (snr >= 0)     score += 5;   // Muy débil
```

### Proceso de Selección

```
┌─────────────────────────────────────────────────────────────────────┐
│                    EVALUACIÓN DE OPERADORES                          │
└─────────────────────────────────────────────────────────────────────┘
         │
         ▼
    ┌─────────────────┐
    │ Configurar      │  → AT+COPS=1,2,"33403"
    │ AT&T (33403)    │    Timeout: 15s
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │ Verificar       │  → modem.isNetworkConnected()
    │ Conexión        │    Timeout: 15s
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │ Obtener CPSI    │  → AT+CPSI? 
    │ y Calcular      │    Parsear métricas
    │ Puntuación      │    Score = f(RSRP,RSRQ,SNR)
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │ Repetir para    │  → Telcel (33420) 
    │ Otros           │    Movistar (334050)
    │ Operadores      │
    └─────────────────┘
         │
         ▼
    ┌─────────────────┐
    │ Seleccionar     │  → max(score_att, score_telcel, score_movistar)
    │ Mejor           │    Configurar ganador
    │ Operador        │
    └─────────────────┘
```

### Ejemplo de Evaluación

```cpp
📊 Resultados de evaluación de operadores:
  AT&T (33403): 65 puntos - Conectado
    RSRP: -92 dBm, RSRQ: -12 dB, RSSI: -85 dBm, SNR: 8 dB
  
  Telcel (33420): 82 puntos - Conectado  
    RSRP: -85 dBm, RSRQ: -9 dB, RSSI: -78 dBm, SNR: 15 dB
  
  Movistar (334050): 45 puntos - Conectado
    RSRP: -98 dBm, RSRQ: -16 dB, RSSI: -91 dBm, SNR: 3 dB

🏆 Mejor operador: Telcel con 82 puntos
```

### Funciones de Debugging

```cpp
// Evaluación manual (para testing)
void manualOperatorEvaluation();

// Reporte completo de operadores
String getOperatorReport();

// Estadísticas del sistema
String getSystemStats();
```

### Consideraciones de Tiempo

- **Evaluación total**: 60-90 segundos (3 operadores × 20-30s cada uno)
- **Impacto en ciclo**: Tiempo adicional pero mejora significativa en confiabilidad
- **Fallback**: Modo automático (`AT+COPS=0`) si ningún operador está disponible

---

## 📡 Comunicación de Datos

### Formato de Datos

Los datos se envían en formato binario cifrado (AES-128) y codificado en Base64:

```cpp
struct sensordata_type {
    // Coordenadas GPS (4 bytes cada una)
    uint8_t lat0, lat1, lat2, lat3;  // Latitud (float)
    uint8_t lon0, lon1, lon2, lon3;  // Longitud (float)
    uint8_t alt0, alt1, alt2, alt3;  // Altitud (float)
    
    // Timestamp (4 bytes)
    uint8_t tms0, tms1, tms2, tms3;  // Unix timestamp
    
    // Sensor ambiental AHT (2 bytes cada uno)
    uint8_t t0, t1;    // Temperatura (int16_t * 100)
    uint8_t h0, h1;    // Humedad (int16_t * 100)
    
    // Sensores de suelo (2 bytes cada uno)
    uint8_t st0, st1;  // Temperatura suelo
    uint8_t sh0, sh1;  // Humedad suelo
    uint8_t ec0, ec1;  // Conductividad
    uint8_t ph0, ph1;  // pH (si aplica)
    
    // Batería (2 bytes)
    uint8_t bat0, bat1;
    
    // Identificador del dispositivo
    uint8_t iden;
};
```

### Protocolo TCP

1. **Conexión**: `d04.elathia.ai:12607`
2. **Formato**: Datos cifrados en Base64
3. **Timeout**: 30 segundos
4. **Reintentos**: 6 intentos máximo
5. **Buffer local**: Almacena datos no enviados en LittleFS

### Buffer Local (LittleFS)

Si falla el envío, los datos se guardan localmente:

```
/data/buffer.txt
```

Formato: Una línea por lectura, con timestamp y datos cifrados.

---

##  API Reference

### Módulo: sensores

```cpp
/**
 * @brief Inicializa y lee todos los sensores
 * @param data Estructura para almacenar datos
 */
void setupSensores(sensordata_type* data);

/**
 * @brief Lee sensor ambiental AHT10
 */
void readAht10();

/**
 * @brief Lee sensor ambiental AHT20
 */
void readAht20();

/**
 * @brief Lee sonda de suelo RS485 (Seed EC)
 */
void read_seed_sonda_ec();

/**
 * @brief Lee voltaje de batería
 */
void readBateria();
```

### Módulo: gsmlte

```cpp
/**
 * @brief Inicializa el módem SIM7080G
 * @param data Estructura de datos del sensor
 */
void setupModem(sensordata_type* data);

/**
 * @brief Conecta a la red LTE
 * @return true si la conexión es exitosa
 */
bool startLTE();

/**
 * @brief Abre conexión TCP
 * @return true si se abre exitosamente
 */
bool tcpOpen();

/**
 * @brief Envía datos por TCP
 * @param datos String de datos cifrados
 * @param timeout_ms Timeout en milisegundos
 * @return true si el envío es exitoso
 */
bool tcpSendData(const String& datos, uint32_t timeout_ms);

/**
 * @brief Cierra conexión TCP
 * @return true si se cierra exitosamente
 */
bool tcpClose();

/**
 * @brief Selecciona automáticamente el mejor operador
 * @return Índice del mejor operador (-1 si ninguno disponible)
 */
int selectBestOperator();

/**
 * @brief Evalúa un operador específico
 * @param operatorIndex Índice del operador (0=AT&T, 1=Telcel, 2=Movistar)
 * @return true si la evaluación fue exitosa
 */
bool evaluateOperator(int operatorIndex);

/**
 * @brief Parsea respuesta CPSI y extrae métricas de señal
 * @param cpsiResponse Respuesta del comando AT+CPSI?
 * @param operatorInfo Estructura donde almacenar datos parseados
 * @return true si el parseo fue exitoso
 */
bool parseCpsiResponse(const String& cpsiResponse, OperatorInfo& operatorInfo);

/**
 * @brief Calcula puntuación de operador basada en métricas
 * @param operatorInfo Información del operador
 * @return Puntuación calculada (mayor es mejor)
 */
int calculateOperatorScore(const OperatorInfo& operatorInfo);

/**
 * @brief Genera reporte completo de operadores evaluados
 * @return String con reporte formateado
 */
String getOperatorReport();

/**
 * @brief Evaluación manual de operadores (debugging)
 */
void manualOperatorEvaluation();
```

### Módulo: cryptoaes

```cpp
/**
 * @brief Cifra datos con AES-128
 * @param data Estructura de datos del sensor
 * @return String con datos cifrados en Base64
 */
String encrypt(sensordata_type* data);
```

### Módulo: timedata

```cpp
/**
 * @brief Inicializa el RTC DS3231
 * @return true si la inicialización es exitosa
 */
bool setupTimeData();

/**
 * @brief Obtiene timestamp Unix actual
 * @return uint32_t con timestamp
 */
uint32_t getUnixTime();
```

### Módulo: sleepdev

```cpp
/**
 * @brief Configura pines GPIO para Deep Sleep
 */
void setupGPIO();

/**
 * @brief Entra en modo Deep Sleep
 * @warning Esta función no retorna
 */
void sleepIOT();
```

---

## 🔍 Troubleshooting

### El módem no responde

```cpp
// Verificar conexiones físicas
// Pin 9 (PWRKEY) debe estar conectado
// Verificar alimentación del módulo (requiere 2A pico)
```

**Solución**:
1. Revisar conexiones UART (TX/RX cruzados)
2. Verificar pulso en PWRKEY (1.2 segundos)
3. Comprobar alimentación estable

### No se conecta a la red LTE

```cpp
// Verificar en logs:
// - ICCID de la SIM
// - Calidad de señal (debe ser > 10)
// - Modo de red configurado
```

**Solución**:
1. Verificar cobertura CAT-M/NB-IoT en la zona
2. Comprobar APN correcto para el operador
3. Verificar SIM activada y con datos

### Sensores RS485 no responden

```cpp
// Verificar dirección Modbus
// Comprobar baudrate (9600)
// Verificar conexión A/B del RS485
```

**Solución**:
1. Usar multímetro para verificar voltaje en RS485
2. Comprobar polaridad A/B
3. Verificar resistencia de terminación (120Ω)

### Errores de cifrado

```cpp
// Error: "AES encryption failed"
```

**Solución**:
1. Verificar tamaño de datos (debe ser múltiplo de 16)
2. Comprobar que las claves AES sean correctas
3. Revisar memoria disponible (heap)

### RTC pierde la hora

```cpp
// Error: "RTC not running"
```

**Solución**:
1. Reemplazar batería CR2032 del DS3231
2. Sincronizar con NTP al inicio
3. Verificar conexiones I2C

### Selección de operador falla o es lenta

```cpp
// Error: "No se encontró operador disponible"
// Timeout en evaluación de operadores
```

**Solución**:
1. **Verificar cobertura**: Asegurar que al menos un operador tenga señal CAT-M/NB-IoT
2. **Revisar SIM**: Confirmar que la SIM sea multi-operador o roaming habilitado
3. **Debugging manual**:
   ```cpp
   // Agregar en setup() para testing
   manualOperatorEvaluation();
   ```
4. **Ajustar timeouts**: Aumentar timeouts en zonas de señal débil:
   ```cpp
   // En evaluateOperator(), línea ~570
   if (!sendATCommand(copsCommand, "OK", 20000)) // Aumentar de 15000 a 20000
   ```
5. **Verificar comando CPSI**: Algunos módem pueden requerir variaciones:
   ```cpp
   // Alternativas si AT+CPSI? no funciona
   AT+COPS?     // Operador actual
   AT+CSQ       // Calidad de señal básica
   AT+CGREG?    // Estado de registro de red
   ```

### Consumo excesivo en Deep Sleep

**Solución**:
1. Verificar que todos los periféricos se desactiven
2. Comprobar que GPIO Hold esté activo
3. Medir corriente con amperímetro (esperado: ~5mA)
4. Verificar que el RTC DS3231 esté alimentado correctamente

---

## 🤝 Contribuir

¡Las contribuciones son bienvenidas! Por favor, sigue estas pautas:

### Reportar Bugs

1. Verifica que el bug no esté ya reportado
2. Incluye logs del monitor serial
3. Describe el comportamiento esperado vs. actual
4. Indica versión de hardware y software

### Proponer Features

1. Abre un Issue describiendo la funcionalidad
2. Explica el caso de uso
3. Considera impacto en consumo energético

### Pull Requests

1. Fork el repositorio
2. Crea una rama para tu feature (`git checkout -b feature/AmazingFeature`)
3. Commit tus cambios (`git commit -m 'Add AmazingFeature'`)
4. Push a la rama (`git push origin feature/AmazingFeature`)
5. Abre un Pull Request

### Estilo de Código

- **Documentación**: Doxygen para todas las funciones públicas
- **Nomenclatura**: camelCase para funciones, UPPER_CASE para constantes
- **Comentarios**: Explicar el "por qué", no el "qué"
- **Testing**: Probar en hardware real antes de PR

---

## 📄 Licencia

Este proyecto está licenciado bajo la **MIT License** - ver el archivo [LICENSE](LICENSE) para más detalles.

```
MIT License

Copyright (c) 2025 Elathia

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software...
```

---

## 🙏 Agradecimientos

- **TinyGSM**: Librería de comunicación con módems
- **Adafruit**: Librerías de sensores I2C
- **Espressif**: ESP32-S3 SDK y herramientas
- **SIMCOM**: Documentación del SIM7080G

---

## 📞 Soporte

- **Email**: support@elathia.ai
- **Website**: [elathia.ai](https://elathia.ai)
- **Issues**: [GitHub Issues](https://github.com/jamr123/sensoresElathia/issues)

---

## 📊 Changelog

### v2.1 (2025-10-24)
- 🎯 **NUEVA FUNCIONALIDAD**: Selección automática de operadores
- 🏆 Evaluación inteligente de AT&T (33403), Telcel (33420) y Movistar (334050)
- 📊 Análisis de métricas CPSI: RSRP, RSRQ, RSSI, SNR
- 🤖 Sistema de puntuación para optimización automática de conectividad
- 🔍 Funciones de debugging: `manualOperatorEvaluation()`, `getOperatorReport()`
- ⚡ Mejora significativa en confiabilidad de conexión celular
- 🌐 Fallback a modo automático si ningún operador está disponible
- 📝 Documentación completa de la nueva funcionalidad en README

### v2.0 (2025-10-20)
- ✨ Documentación completa con Doxygen
- 🔧 Optimización de código (30-70% reducción de comentarios)
- 🐛 Fix: Corrupción de claves AES (memcpy en buffer)
- 🧹 Limpieza de código muerto (funciones diagnósticas)
- 📝 README completo y profesional
- 🌐 Actualización de servidor a d04.elathia.ai
- 🔄 Corrección de constantes (TIME_TO_SLEEP)

### v1.5 (2025-09)
- 🚀 Primera versión estable
- 📡 Soporte CAT-M/NB-IoT
- 🔐 Cifrado AES-128 implementado
- 💾 Sistema de buffer local con LittleFS

---

<div align="center">

**Desarrollado con ❤️ por el equipo de Elathia**

🌱 *Agricultura Inteligente · IoT Sostenible · Tecnología para el Campo*

</div>
