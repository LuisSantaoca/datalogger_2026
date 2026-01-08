/**
 * @file AppController.cpp
 * @brief Controlador principal de la aplicación de adquisición de datos con ESP32-S3
 * 
 * Este archivo implementa una máquina de estados finitos (FSM) que coordina todos
 * los módulos del sistema: sensores, GPS, LTE, almacenamiento y gestión de energía.
 * 
 * @details
 * El sistema opera en dos modos principales:
 * - Modo BLE: Configuración inicial via Bluetooth Low Energy (solo al arranque)
 * - Modo Ciclo: Lectura de sensores, GPS, envío de datos y sleep
 * 
 * Características principales:
 * - Lectura de múltiples sensores (ADC, I2C, RS485 Modbus)
 * - Adquisición GPS optimizada (solo primer ciclo post-boot)
 * - Transmisión LTE con selección inteligente de operadora
 * - Buffer persistente en LittleFS para tolerancia a fallos
 * - Deep sleep con timer wakeup para ahorro de energía
 * - Almacenamiento persistente (NVS) de configuraciones críticas
 * 
 * @author Sistema Sensores RV03
 * @date 2025-12-18
 * @version 2.0
 */

#include <Arduino.h>
#include <esp_sleep.h>
#include <Preferences.h>
#include "AppController.h"
#include "src/version_info.h"   // FEAT-V0: Sistema de control de versiones centralizado
#include "src/FeatureFlags.h"   // FEAT-V1: Sistema de feature flags
#include "src/CycleTiming.h"    // FEAT-V2: Sistema de timing de ciclos
#include "src/DebugConfig.h"

#include "src/data_buffer/BUFFERModule.h"
#include "src/data_buffer/BLEModule.h"
#include "src/data_buffer/config_data_buffer.h"

#include "src/data_format/FORMATModule.h"
#include "src/data_format/config_data_format.h"

#include "src/data_gps/GPSModule.h"
#include "src/data_gps/config_data_gps.h"

#include "src/data_lte/LTEModule.h"
#include "src/data_lte/config_data_lte.h"
#include "src/data_lte/config_operadoras.h"

#include "src/data_sensors/ADCSensorModule.h"
#include "src/data_sensors/I2CSensorModule.h"
#include "src/data_sensors/RS485Module.h"
#include "src/data_sensors/config_data_sensors.h"

#include "src/data_sleepwakeup/SLEEPModule.h"
#include "src/data_sleepwakeup/config_data_sleepwakeup.h"

#include "src/data_time/RTCModule.h"
#include "src/data_time/config_data_time.h"

/** @brief Número de muestras a descartar al inicio de cada lectura de sensor */
static const uint8_t DISCARD_SAMPLES = 5;

/** @brief Número de muestras a promediar después de descartar */
static const uint8_t KEEP_SAMPLES = 5;

/** @brief Configuración global de la aplicación (tiempo de sleep, etc.) */
static AppConfig g_cfg;

/** @brief Módulo de gestión de buffer persistente en LittleFS */
static BUFFERModule buffer;

/** @brief Módulo de configuración Bluetooth Low Energy (usa el buffer para configuración) */
static BLEModule ble(buffer);

/** @brief Módulo de formateo de tramas (texto y Base64) */
static FormatModule formatter;

/** @brief Módulo de sensor ADC para medición de voltaje de batería */
static ADCSensorModule adcSensor;

/** @brief Módulo de sensor I2C para temperatura y humedad */
static I2CSensorModule i2cSensor;

/** @brief Módulo de sensor RS485 con protocolo Modbus RTU */
static RS485Module rs485Sensor;

/** @brief Puerto serial compartido para comunicación con módulo SIM7080G (GPS y LTE) */
static HardwareSerial SerialLTE(1);

/** @brief Módulo GPS que usa el SIM7080G GNSS */
static GPSModule gps(SerialLTE, GPS_PWRKEY_PIN);

/** @brief Módulo LTE Cat-M/NB-IoT que usa el SIM7080G modem */
static LTEModule lte(SerialLTE);

/** @brief Módulo de gestión de deep sleep y wakeup */
static SleepModule sleepModule;

/** @brief Sistema de almacenamiento persistente NVS para configuraciones críticas */
static Preferences preferences;

/**
 * @enum AppState
 * @brief Estados de la máquina de estados finitos (FSM) de la aplicación
 * 
 * @details
 * Flujo normal de operación:
 * 1. Boot: Inicialización (estado transitorio)
 * 2. BleOnly: Modo configuración BLE (solo en arranque desde apagado)
 * 3. Cycle_ReadSensors: Lectura de sensores ADC, I2C y RS485
 * 4. Cycle_Gps: Adquisición GPS (solo primer ciclo post-boot)
 * 5. Cycle_GetICCID: Lectura de ICCID del SIM
 * 6. Cycle_BuildFrame: Construcción de trama de datos (Base64)
 * 7. Cycle_BufferWrite: Escritura persistente en buffer
 * 8. Cycle_SendLTE: Transmisión por red celular
 * 9. Cycle_CompactBuffer: Eliminación de tramas enviadas
 * 10. Cycle_Sleep: Deep sleep con timer wakeup
 * 11. Error: Estado de error (detiene el sistema)
 */
enum class AppState : uint8_t {
  Boot = 0,              ///< Inicialización del sistema
  BleOnly,               ///< Modo configuración BLE exclusivo
  Cycle_ReadSensors,     ///< Lectura de todos los sensores
  Cycle_Gps,             ///< Adquisición de coordenadas GPS
  Cycle_GetICCID,        ///< Obtención del ICCID de la tarjeta SIM
  Cycle_BuildFrame,      ///< Construcción de trama de datos
  Cycle_BufferWrite,     ///< Escritura en buffer persistente
  Cycle_SendLTE,         ///< Transmisión de datos por LTE
  Cycle_CompactBuffer,   ///< Compactación del buffer
  Cycle_Sleep,           ///< Entrada a deep sleep
  Error                  ///< Estado de error fatal
};

/** @brief Estado actual de la máquina de estados */
static AppState g_state = AppState::Boot;

/** @brief Indica si el sistema ha sido inicializado correctamente */
static bool g_initialized = false;

/** @brief Causa del último wakeup (timer, pin externo, etc.) */
static esp_sleep_wakeup_cause_t g_wakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;

/** @brief Flag para ejecutar GPS solo en el primer ciclo después del boot */
static bool g_firstCycleAfterBoot = false;

/** @brief ICCID de la tarjeta SIM (identificador único) */
static String g_iccid;

/** @brief Timestamp Unix epoch en formato string */
static String g_epoch;

/** @brief Latitud en formato string con 6 decimales */
static char g_lat[COORD_LEN + 1];

/** @brief Longitud en formato string con 6 decimales */
static char g_lng[COORD_LEN + 1];

/** @brief Altitud en metros en formato string */
static char g_alt[ALT_LEN + 1];

/** @brief Array de valores de sensores en formato string (7 variables) */
static String g_varStr[VAR_COUNT];

/** @brief Buffer para trama completa codificada en Base64 */
static char g_frame[FRAME_BASE64_MAX_LEN];

#if ENABLE_FEAT_V2_CYCLE_TIMING
/** @brief Estructura global para timing de ciclo (FEAT-V2) */
static CycleTiming g_timing;
#endif

/**
 * @brief Rellena un buffer con caracteres '0' y agrega terminador nulo
 * 
 * Utilizada para inicializar coordenadas GPS cuando no hay datos válidos.
 * 
 * @param dst Puntero al buffer de destino
 * @param width Número de caracteres '0' a escribir
 * 
 * @note El buffer debe tener al menos width+1 bytes de capacidad
 */
static void fillZeros(char* dst, size_t width) {
  for (size_t i = 0; i < width; i++) dst[i] = '0';
  dst[width] = '\0';
}

/**
 * @brief Formatea una coordenada GPS (latitud o longitud) con 6 decimales
 * 
 * Convierte un valor float a string con precisión de 6 decimales, lo cual
 * proporciona aproximadamente 0.1 metros de precisión en el ecuador.
 * 
 * @param out Buffer de salida para el string resultante
 * @param outSz Tamaño del buffer de salida
 * @param v Valor de coordenada a formatear
 * 
 * @note Formato de salida: "±DD.DDDDDD" (ej: "-99.123456")
 */
static void formatCoord(char* out, size_t outSz, float v) {
  snprintf(out, outSz, "%.6f", (double)v);
}

/**
 * @brief Formatea la altitud GPS como entero (metros sobre nivel del mar)
 * 
 * Redondea la altitud al metro más cercano y la convierte a string.
 * 
 * @param out Buffer de salida para el string resultante
 * @param outSz Tamaño del buffer de salida
 * @param altMeters Altitud en metros (valor float)
 * 
 * @note La altitud se redondea para reducir tamaño de trama sin pérdida significativa de precisión
 */
static void formatAlt(char* out, size_t outSz, float altMeters) {
  long a = lroundf(altMeters);
  snprintf(out, outSz, "%ld", a);
}

/**
 * @brief Lee el sensor ADC descartando muestras iniciales y promediando
 * 
 * Implementa una estrategia de muestreo robusto:
 * 1. Descarta las primeras DISCARD_SAMPLES lecturas (5)
 * 2. Promedia las siguientes KEEP_SAMPLES lecturas (5)
 * 3. Redondea el resultado a entero
 * 
 * @param[out] outValueStr String de salida con el valor promediado
 * 
 * @return true si se obtuvieron lecturas válidas, false si todas fallaron
 * 
 * @note El descarte de muestras iniciales elimina efectos transitorios del multiplexor ADC
 */
static bool readADC_DiscardAndAverage(String& outValueStr) {
  double sum = 0.0;
  uint8_t got = 0;

  for (uint8_t i = 0; i < (DISCARD_SAMPLES + KEEP_SAMPLES); i++) {
    if (!adcSensor.readSensor()) continue;
    if (i >= DISCARD_SAMPLES) {
      sum += adcSensor.getValue();
      got++;
    }
  }

  if (got == 0) return false;

  int avgInt = (int)lround(sum / got);
  outValueStr = String(avgInt);
  return true;
}

/**
 * @brief Lee el sensor I2C (temperatura y humedad) con descarte y promediado
 * 
 * Implementa la misma estrategia de muestreo robusto que readADC_DiscardAndAverage:
 * 1. Descarta las primeras DISCARD_SAMPLES lecturas (5)
 * 2. Promedia las siguientes KEEP_SAMPLES lecturas (5)
 * 3. Multiplica por 100 para almacenar con 2 decimales como entero
 * 
 * @param[out] outTempStr String con temperatura promediada (en centésimas de grado)
 * @param[out] outHumStr String con humedad promediada (en centésimas de porcentaje)
 * 
 * @return true si se obtuvieron lecturas válidas, false si todas fallaron
 * 
 * @note Los valores se almacenan multiplicados por 100 (ej: 25.67°C → "2567")
 */
static bool readI2C_DiscardAndAverage(String& outTempStr, String& outHumStr) {
  double sumT = 0.0, sumH = 0.0;
  uint8_t got = 0;

  for (uint8_t i = 0; i < (DISCARD_SAMPLES + KEEP_SAMPLES); i++) {
    if (!i2cSensor.readSensor()) continue;
    if (i >= DISCARD_SAMPLES) {
      sumT += i2cSensor.getTemperature();
      sumH += i2cSensor.getHumidity();
      got++;
    }
  }

  if (got == 0) return false;

  int t100 = (int)lround((sumT / got) * 100.0);
  int h100 = (int)lround((sumH / got) * 100.0);
  outTempStr = String(t100);
  outHumStr  = String(h100);
  return true;
}

/**
 * @brief Lee el sensor RS485 Modbus descartando lecturas iniciales
 * 
 * Implementa estrategia de descarte pero toma solo la última lectura válida
 * (no promedia, ya que Modbus puede tener valores discretos):
 * 1. Descarta las primeras DISCARD_SAMPLES lecturas (5)
 * 2. Toma la última lectura válida de las siguientes KEEP_SAMPLES (5)
 * 
 * @param[out] regsOut Array de 4 strings con los valores de los 4 registros Modbus
 * 
 * @return true si se obtuvo al menos una lectura válida, false si todas fallaron
 * 
 * @note Lee 4 registros consecutivos via Modbus RTU (función 0x03 - Read Holding Registers)
 */
static bool readRS485_DiscardAndTakeLast(String regsOut[4]) {
  bool okLast = false;

  for (uint8_t i = 0; i < (DISCARD_SAMPLES + KEEP_SAMPLES); i++) {
    bool ok = rs485Sensor.readSensor();
    if (i >= DISCARD_SAMPLES && ok) {
      okLast = true;
      regsOut[0] = rs485Sensor.getRegisterString(0);
      regsOut[1] = rs485Sensor.getRegisterString(1);
      regsOut[2] = rs485Sensor.getRegisterString(2);
      regsOut[3] = rs485Sensor.getRegisterString(3);
    }
  }

  return okLast;
}

/**
 * @brief Envía todas las tramas del buffer por LTE y las marca como procesadas
 * 
 * Implementa un sistema completo de transmisión con selección inteligente de operadora:
 * 
 * 1. **Recuperación de operadora persistente:**
 *    - Lee operadora exitosa anterior desde NVS ("lastOperator")
 *    - Si no existe, escanea todas las operadoras disponibles
 * 
 * 2. **Selección de operadora:**
 *    - Si hay operadora guardada: la usa directamente (optimización)
 *    - Si no: testea todas y selecciona la mejor según score de señal
 *    - Score = (4×SINR) + 2×(RSRP+120) + (RSRQ+20)
 * 
 * 3. **Conexión LTE:**
 *    - Enciende modem SIM7080G
 *    - Configura operadora seleccionada
 *    - Attach a red, activa PDP context
 *    - Abre conexión TCP al servidor
 * 
 * 4. **Transmisión de tramas:**
 *    - Lee todas las líneas del buffer (máx MAX_LINES_TO_READ)
 *    - Salta líneas ya marcadas como procesadas
 *    - Envía cada línea por TCP
 *    - Marca como procesada si envío exitoso
 *    - Detiene envío al primer fallo (preserva datos)
 * 
 * 5. **Gestión de operadora persistente:**
 *    - Si hubo éxito: guarda operadora en NVS para próximos ciclos
 *    - Si falló todo: borra operadora guardada para forzar escaneo
 * 
 * @return true si se envió al menos una trama exitosamente, false si no se pudo enviar ninguna
 * 
 * @note Las tramas que no se enviaron permanecen en el buffer para el próximo ciclo
 * @note El sistema optimiza reconectándose a la última operadora exitosa
 * @see BUFFERModule::markLineAsProcessed()
 * @see LTEModule::testOperator()
 * @see LTEModule::getBestOperator()
 */
static bool sendBufferOverLTE_AndMarkProcessed() {
  if (!lte.powerOn()) return false;

  Operadora operadoraAUsar;
  bool tieneOperadoraGuardada = false;

  preferences.begin("sensores", false);
  if (preferences.isKey("lastOperator")) {
    operadoraAUsar = (Operadora)preferences.getUChar("lastOperator", 0);
    tieneOperadoraGuardada = true;
    Serial.print("[INFO][APP] Usando operadora guardada: ");
    Serial.println(OPERADORAS[operadoraAUsar].nombre);
  }
  preferences.end();

  if (!tieneOperadoraGuardada) {
    Serial.println("[INFO][APP] No hay operadora guardada. Probando todas...");
    for (uint8_t i = 0; i < NUM_OPERADORAS; i++) {
      lte.testOperator((Operadora)i);
    }
    operadoraAUsar = lte.getBestOperator();
    Serial.print("[INFO][APP] Mejor operadora seleccionada: ");
    Serial.println(OPERADORAS[operadoraAUsar].nombre);
  }

#if ENABLE_FIX_V1_SKIP_RESET_PDP
  // ============ [FIX-V1 START] Si tiene operadora guardada, skip reset ============
  if (!lte.configureOperator(operadoraAUsar, tieneOperadoraGuardada)) { lte.powerOff(); return false; }
  // ============ [FIX-V1 END] ============
#else
  if (!lte.configureOperator(operadoraAUsar))   { lte.powerOff(); return false; }
#endif
  if (!lte.attachNetwork())                     { lte.powerOff(); return false; }
  if (!lte.activatePDP())                       { lte.powerOff(); return false; }
  if (!lte.openTCPConnection())                 { lte.deactivatePDP(); lte.powerOff(); return false; }

  String allLines[MAX_LINES_TO_READ];
  int total = 0;
  if (!buffer.readLines(allLines, MAX_LINES_TO_READ, total)) {
    lte.closeTCPConnection();
    lte.deactivatePDP();
    lte.powerOff();
    Serial.println("[ERROR][APP] No se pudieron leer líneas del buffer");
    return false;
  }

  Serial.print("[INFO][APP] Líneas en buffer: ");
  Serial.println(total);

  bool anySent = false;
  int sentCount = 0;

  for (int i = 0; i < total; i++) {
    if (allLines[i].startsWith(PROCESSED_MARKER)) {
      Serial.print("[DEBUG][APP] Línea ");
      Serial.print(i+1);
      Serial.println(" ya procesada, saltando...");
      continue;
    }

    Serial.print("[INFO][APP] Enviando línea ");
    Serial.print(i+1);
    Serial.print("/");
    Serial.println(total);
    
    bool sentOk = lte.sendTCPData(allLines[i]);
    if (sentOk) {
      buffer.markLineAsProcessed(i);
      anySent = true;
      sentCount++;
      Serial.print("[INFO][APP] Línea ");
      Serial.print(i+1);
      Serial.println(" enviada y marcada como procesada");
      delay(50);
    } else {
      Serial.print("[WARN][APP] Fallo al enviar línea ");
      Serial.print(i+1);
      Serial.println(". Deteniendo envío. Línea permanece en buffer.");
      break;
    }
  }

  lte.closeTCPConnection();
  lte.deactivatePDP();
  lte.detachNetwork();
  lte.powerOff();

  Serial.print("[INFO][APP] Resumen: ");
  Serial.print(sentCount);
  Serial.print(" de ");
  Serial.print(total);
  Serial.println(" tramas enviadas");

  preferences.begin("sensores", false);
  if (anySent) {
    preferences.putUChar("lastOperator", (uint8_t)operadoraAUsar);
    Serial.print("[INFO][APP] Operadora guardada para futuros envios: ");
    Serial.println(OPERADORAS[operadoraAUsar].nombre);
  } else {
    if (tieneOperadoraGuardada) {
      preferences.remove("lastOperator");
      Serial.println("[WARN][APP] Envio fallido. Operadora eliminada. Próximo ciclo escaneará todas.");
    }
  }
  preferences.end();

  return anySent;
}

/**
 * @brief Inicializa la aplicación y todos sus módulos
 * 
 * Secuencia de inicialización:
 * 
 * 1. **Configuración básica:**
 *    - Guarda configuración de usuario (tiempo de sleep)
 *    - Inicia comunicación serial para debug (115200 baud)
 * 
 * 2. **Gestión de energía:**
 *    - Inicia módulo de sleep
 *    - Detecta causa del wakeup (timer, pin, o boot desde apagado)
 * 
 * 3. **Sistema de tiempo:**
 *    - Inicializa RTC DS1307 para timestamps
 * 
 * 4. **Almacenamiento:**
 *    - Monta sistema LittleFS para buffer persistente
 *    - Si falla: entra en estado de Error
 * 
 * 5. **Sensores:**
 *    - Inicializa ADC (voltaje batería)
 *    - Inicializa I2C (temperatura/humedad)
 *    - Inicializa RS485 Modbus RTU
 * 
 * 6. **Comunicaciones:**
 *    - Configura módulo LTE (SIM7080G)
 *    - Habilita debug del modem
 * 
 * 7. **Selección de modo:**
 *    - **Si wakeup == UNDEFINED (boot desde apagado):**
 *      * Inicia BLE para configuración
 *      * Estado inicial: BleOnly
 *      * Marca próximo ciclo para lectura GPS
 *    - **Si wakeup == TIMER (wakeup desde sleep):**
 *      * Salta BLE
 *      * Estado inicial: Cycle_ReadSensors
 *      * NO lee GPS (usa coordenadas guardadas)
 * 
 * @param cfg Estructura de configuración con parámetros de usuario
 * 
 * @note BLE solo se activa en el primer boot, no en wakeups subsecuentes
 * @note GPS solo se lee en el primer ciclo post-boot para ahorrar energía
 */
void AppInit(const AppConfig& cfg) {
  g_cfg = cfg;

  Serial.begin(115200);
  delay(200);

  // ============ [FEAT-V0 START] Imprimir versión al iniciar ============
  printFirmwareVersion();
  // ============ [FEAT-V0 END] ============

  // ============ [FEAT-V1 START] Imprimir feature flags activos ============
  printActiveFlags();
  // ============ [FEAT-V1 END] ============

  sleepModule.begin();
  g_wakeupCause = sleepModule.getWakeupCause();

  (void)initializeRTC();

  if (!buffer.begin()) {
    g_state = AppState::Error;
    g_initialized = true;
    return;
  }

  (void)adcSensor.begin();
  (void)i2cSensor.begin();
  (void)rs485Sensor.begin();

  lte.begin();
  lte.setDebug(true, &Serial);

  if (g_wakeupCause == ESP_SLEEP_WAKEUP_UNDEFINED) {
    ble.begin("RV03");
    g_state = AppState::BleOnly;
    g_firstCycleAfterBoot = true;
  } else {
    g_state = AppState::Cycle_ReadSensors;
    g_firstCycleAfterBoot = false;
  }

  g_initialized = true;
}

/**
 * @brief Loop principal de la máquina de estados finitos (FSM)
 * 
 * Ejecuta la lógica de cada estado de la FSM. Debe ser llamada continuamente
 * desde loop() de Arduino.
 * 
 * **Estados y transiciones:**
 * 
 * - **BleOnly:**
 *   - Actualiza servicio BLE
 *   - Cuando BLE se desactiva → Cycle_ReadSensors
 * 
 * - **Cycle_ReadSensors:**
 *   - Lee ADC (batería), I2C (temp/hum), RS485 (4 registros)
 *   - Si es primer ciclo → Cycle_Gps
 *   - Si no → recupera GPS de NVS y va a Cycle_GetICCID
 * 
 * - **Cycle_Gps:**
 *   - Enciende GNSS, espera fix, guarda coordenadas en NVS
 *   - Si no hay fix → usa ceros
 *   - Siempre → Cycle_GetICCID
 * 
 * - **Cycle_GetICCID:**
 *   - Enciende LTE brevemente para leer ICCID
 *   - Apaga LTE
 *   - Siempre → Cycle_BuildFrame
 * 
 * - **Cycle_BuildFrame:**
 *   - Construye trama texto: $,iccid,epoch,lat,lng,alt,var1-7,#
 *   - Codifica a Base64
 *   - Muestra ambas versiones por serial
 *   - Si éxito → Cycle_BufferWrite
 *   - Si fallo → Error
 * 
 * - **Cycle_BufferWrite:**
 *   - Guarda trama Base64 en buffer LittleFS (persistente)
 *   - Siempre → Cycle_SendLTE (aunque falle guardado)
 * 
 * - **Cycle_SendLTE:**
 *   - Llama sendBufferOverLTE_AndMarkProcessed()
 *   - Siempre → Cycle_CompactBuffer
 * 
 * - **Cycle_CompactBuffer:**
 *   - Elimina del buffer solo líneas marcadas como procesadas
 *   - Tramas no enviadas permanecen para próximo ciclo
 *   - Siempre → Cycle_Sleep
 * 
 * - **Cycle_Sleep:**
 *   - Configura timer wakeup (según cfg.sleep_time_us)
 *   - Entra en deep sleep (no retorna)
 *   - Próximo wakeup → reinicia ESP32 → AppInit()
 * 
 * - **Boot / Error:**
 *   - Espera indefinidamente (delay 1s)
 * 
 * @note Esta función nunca debe ser bloqueante excepto en estados finales
 * @note El sistema usa deep sleep, por lo que cada wakeup reinicia completamente el ESP32
 */
void AppLoop() {
  if (!g_initialized) return;

  ble.update();

  switch (g_state) {
    case AppState::BleOnly: {
      if (!ble.isActive()) {
        TIMING_RESET(g_timing);  // Inicia timing del ciclo cuando termina BLE
        g_state = AppState::Cycle_ReadSensors;
      }
      break;
    }

    case AppState::Cycle_ReadSensors: {
      TIMING_START(g_timing, sensors);
      String vBat;
      if (!readADC_DiscardAndAverage(vBat)) vBat = "0";

      String vTemp, vHum;
      if (!readI2C_DiscardAndAverage(vTemp, vHum)) { vTemp = "0"; vHum = "0"; }

      String regs[4] = {"0","0","0","0"};
      (void)readRS485_DiscardAndTakeLast(regs);

      
      g_varStr[0] = regs[0];
      g_varStr[1] = regs[1];
      g_varStr[2] = regs[2];
      g_varStr[3] = regs[3];
      g_varStr[4] = vTemp;
      g_varStr[5] = vHum;
      g_varStr[6] = vBat;
      TIMING_END(g_timing, sensors);

      if (g_firstCycleAfterBoot) {
        Serial.println("[INFO][APP] Primer ciclo: leyendo GPS");
        g_state = AppState::Cycle_Gps;
        g_firstCycleAfterBoot = false;
      } else {
        TIMING_START(g_timing, nvsGps);
        Serial.println("[INFO][APP] Ciclo subsecuente: recuperando coordenadas GPS de NVS");
        
        preferences.begin("sensores", true);
        if (preferences.isKey("gps_lat") && preferences.isKey("gps_lng") && preferences.isKey("gps_alt")) {
          float lat = preferences.getFloat("gps_lat", 0.0f);
          float lng = preferences.getFloat("gps_lng", 0.0f);
          float alt = preferences.getFloat("gps_alt", 0.0f);
          preferences.end();
          
          formatCoord(g_lat, sizeof(g_lat), lat);
          formatCoord(g_lng, sizeof(g_lng), lng);
          formatAlt(g_alt, sizeof(g_alt), alt);
          
          Serial.print("[INFO][APP] GPS recuperado - Lat: ");
          Serial.print(g_lat);
          Serial.print(", Lng: ");
          Serial.print(g_lng);
          Serial.print(", Alt: ");
          Serial.println(g_alt);
        } else {
          preferences.end();
          fillZeros(g_lat, COORD_LEN);
          fillZeros(g_lng, COORD_LEN);
          fillZeros(g_alt, ALT_LEN);
          Serial.println("[WARN][APP] No hay coordenadas GPS en NVS, usando ceros");
        }
        TIMING_END(g_timing, nvsGps);
        g_state = AppState::Cycle_GetICCID;
      }
      break;
    }

    case AppState::Cycle_Gps: {
      TIMING_START(g_timing, gps);
      fillZeros(g_lat, COORD_LEN);
      fillZeros(g_lng, COORD_LEN);
      fillZeros(g_alt, ALT_LEN);

      GpsFix fix;
      bool gotFix = false;

      if (gps.powerOn()) {
        gotFix = gps.getCoordinatesAndShutdown(fix);
      }

      if (gotFix && fix.hasFix) {
        formatCoord(g_lat, sizeof(g_lat), fix.latitude);
        formatCoord(g_lng, sizeof(g_lng), fix.longitude);
        formatAlt(g_alt, sizeof(g_alt), fix.altitude);
        
        preferences.begin("sensores", false);
        preferences.putFloat("gps_lat", fix.latitude);
        preferences.putFloat("gps_lng", fix.longitude);
        preferences.putFloat("gps_alt", fix.altitude);
        preferences.end();
        
        Serial.println("[INFO][APP] Coordenadas GPS guardadas en NVS");
        Serial.print("[INFO][APP] Lat: ");
        Serial.print(g_lat);
        Serial.print(", Lng: ");
        Serial.print(g_lng);
        Serial.print(", Alt: ");
        Serial.println(g_alt);
      } else {
        Serial.println("[WARN][APP] No se obtuvo fix GPS, usando ceros");
      }
      TIMING_END(g_timing, gps);
      g_state = AppState::Cycle_GetICCID;
      break;
    }
    case AppState::Cycle_GetICCID: {
      TIMING_START(g_timing, iccid);
      if (lte.powerOn()) {
        g_iccid = lte.getICCID();
        lte.powerOff();
      } else {
        g_iccid = "";
      }
      TIMING_END(g_timing, iccid);
      g_state = AppState::Cycle_BuildFrame;
      break;
    }
    case AppState::Cycle_BuildFrame: {
      TIMING_START(g_timing, buildFrame);
      g_epoch = getEpochString();

      formatter.reset();
      formatter.setIccid(g_iccid.c_str());
      formatter.setEpoch(g_epoch.c_str());
      formatter.setLat(g_lat);
      formatter.setLng(g_lng);
      formatter.setAlt(g_alt);

      for (uint8_t i = 0; i < VAR_COUNT; i++) {
        formatter.setVar(i, g_varStr[i].c_str());
      }

      char frameNormal[FRAME_MAX_LEN];
      if (!formatter.buildFrame(frameNormal, sizeof(frameNormal))) {
        g_state = AppState::Error;
        break;
      }
      Serial.println("[INFO][APP] === TRAMA NORMAL ===");
      Serial.println(frameNormal);

      Serial.println("[DEBUG][APP] Generando trama Base64...");
      if (!formatter.buildFrameBase64(g_frame, sizeof(g_frame))) {
        g_state = AppState::Error;
        break;
      }
      Serial.println("[INFO][APP] === TRAMA BASE64 ===");
      Serial.println(g_frame);
      Serial.println();
      TIMING_END(g_timing, buildFrame);
      g_state = AppState::Cycle_BufferWrite;
      break;
    }

    case AppState::Cycle_BufferWrite: {
      TIMING_START(g_timing, bufferWrite);
      Serial.println("[INFO][APP] Guardando trama en buffer (persistente)...");
      bool saved = buffer.appendLine(String(g_frame));
      if (saved) {
        Serial.println("[INFO][APP] Trama guardada exitosamente en buffer");
      } else {
        Serial.println("[ERROR][APP] Fallo al guardar trama en buffer");
      }
      Serial.println("[DEBUG][APP] Pasando a Cycle_SendLTE");
      TIMING_END(g_timing, bufferWrite);
      g_state = AppState::Cycle_SendLTE;
      break;
    }

    case AppState::Cycle_SendLTE: {
      TIMING_START(g_timing, sendLte);
      Serial.println("[DEBUG][APP] Iniciando envio por LTE...");
      (void)sendBufferOverLTE_AndMarkProcessed();
      Serial.println("[DEBUG][APP] Envio completado, pasando a CompactBuffer");
      TIMING_END(g_timing, sendLte);
      g_state = AppState::Cycle_CompactBuffer;
      break;
    }

    case AppState::Cycle_CompactBuffer: {
      TIMING_START(g_timing, compactBuffer);
      Serial.println("[INFO][APP] Eliminando solo tramas procesadas del buffer...");
      (void)buffer.removeProcessedLines();
      Serial.println("[INFO][APP] Buffer compactado. Tramas no enviadas permanecen para próximo ciclo.");
      TIMING_END(g_timing, compactBuffer);
      g_state = AppState::Cycle_Sleep;
      break;
    }

    case AppState::Cycle_Sleep: {
      TIMING_FINALIZE(g_timing);
      TIMING_PRINT_SUMMARY(g_timing);
      sleepModule.clearWakeupSources();
      esp_sleep_enable_timer_wakeup(g_cfg.sleep_time_us);
      sleepModule.enterDeepSleep();
      break;
    }

    case AppState::Boot:
    case AppState::Error:
    default:
      delay(1000);
      break;
  }
}
