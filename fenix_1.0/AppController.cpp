/**
 * @file AppController.cpp
 * @brief Controlador principal de la aplicación de adquisición de datos con ESP32-S3
 * 
 * Este archivo implementa una máquina de estados finitos (FSM) que coordina todos
 * los módulos del sistema: sensores, GPS, LTE, almacenamiento y gestión de energía.
 * 
 * @details
 * El sistema opera en modo ciclo continuo:
 * Lectura de sensores → GPS (primer ciclo) → envío LTE → deep sleep
 *
 * Características principales:
 * - Lectura de múltiples sensores (ADC, I2C, RS485 Modbus)
 * - Adquisición GPS optimizada (solo primer ciclo post-boot, basado en NVS)
 * - Transmisión LTE con selección inteligente de operadora
 * - Buffer persistente en LittleFS para tolerancia a fallos
 * - Deep sleep con timer wakeup para ahorro de energía
 * - Almacenamiento persistente (NVS) de configuraciones críticas
 * - Gate de voltaje de batería antes de encender modem (FIX-V7)
 * - Logging diagnóstico: banner, timing por fase, resumen pre-sleep (FEAT-V2)
 *
 * @author Fenix 1.0
 * @date 2026-02-15
 * @version 1.0.0
 */

#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_system.h>    // FIX-V6: esp_reset_reason()
#include <Preferences.h>
#include "AppController.h"
#include "src/DebugConfig.h"

#include "src/data_buffer/BUFFERModule.h"
// FIX-V6: BLEModule deshabilitado — no necesario en operación actual
// #include "src/data_buffer/BLEModule.h"
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

/** @brief Variable RTC del .ino para persistir voltaje entre ciclos */
extern float prevBatVoltage;

/** @brief Variable RTC del .ino para tracking de fallos consecutivos del modem */
extern uint8_t modemConsecFails;

/** @brief Ciclos restantes para saltarse operaciones del modem (backoff post-restart) */
static RTC_DATA_ATTR uint8_t modemSkipCycles = 0;

/** @brief Flag por ciclo: true si el modem puede usarse este ciclo */
static bool g_modemAllowed = true;

/** @brief Estima porcentaje de batería LiPo (lineal: 3.0V=0%, 4.2V=100%) */
static int batteryPercent(float v) {
  int pct = (int)((v - 3.0f) / 1.2f * 100.0f);
  if (pct > 100) pct = 100;
  if (pct < 0) pct = 0;
  return pct;
}

/** @brief Número de muestras a descartar al inicio de cada lectura de sensor */
static const uint8_t DISCARD_SAMPLES = 5;

/** @brief Número de muestras a promediar después de descartar */
static const uint8_t KEEP_SAMPLES = 5;

/** @brief Configuración global de la aplicación (tiempo de sleep, etc.) */
static AppConfig g_cfg;

/** @brief Módulo de gestión de buffer persistente en LittleFS */
static BUFFERModule buffer;

// FIX-V6: BLEModule deshabilitado
// static BLEModule ble(buffer);

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
 * @brief Callback invocado cuando el comando COPS falla o la conexión TCP falla en LTEModule
 * 
 * @details
 * Esta función se ejecuta cuando LTEModule detecta un error en:
 * - Comando AT+COPS durante la configuración de operadora
 * - Apertura de conexión TCP (AT+CAOPEN) tras 3 intentos fallidos
 * 
 * Estos errores indican que la operadora guardada en NVS ya no es válida,
 * la red no está disponible, o el servidor no es alcanzable con esa operadora.
 * 
 * Acción: Borra la entrada "lastOperator" del namespace "sensores" en NVS,
 * forzando al sistema a realizar un escaneo completo de operadoras en el
 * próximo ciclo de transmisión.
 */
static void onCOPSError() {
  Serial.println("[WARN][NVS] Error de red LTE detectado. Borrando operadora guardada del NVS...");
  preferences.begin("sensores", false);
  if (preferences.isKey("lastOperator")) {
    preferences.remove("lastOperator");
    Serial.println("[INFO][NVS] Operadora eliminada del NVS. Próximo ciclo escaneará todas.");
  } else {
    Serial.println("[INFO][NVS] No había operadora guardada en NVS.");
  }
  preferences.end();
}

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
  Boot = 0,             ///< Inicialización del sistema
  // FIX-V6: BleOnly eliminado — BLE deshabilitado
  Cycle_ReadSensors,    ///< Lectura de todos los sensores
  Cycle_Gps,            ///< Adquisición de coordenadas GPS
  Cycle_GetICCID,       ///< Obtención del ICCID de la tarjeta SIM
  Cycle_BuildFrame,     ///< Construcción de trama de datos
  Cycle_BufferWrite,    ///< Escritura en buffer persistente
  Cycle_SendLTE,        ///< Transmisión de datos por LTE
  Cycle_CompactBuffer,  ///< Compactación del buffer
  Cycle_Sleep,          ///< Entrada a deep sleep
  Error                 ///< Estado de error fatal
};

/** @brief Estado actual de la máquina de estados */
static AppState g_state = AppState::Boot;

/** @brief Indica si el sistema ha sido inicializado correctamente */
static bool g_initialized = false;

/** @brief Causa del último wakeup (timer, pin externo, etc.) */
static esp_sleep_wakeup_cause_t g_wakeupCause = ESP_SLEEP_WAKEUP_UNDEFINED;

/** @brief Flag para ejecutar GPS solo en el primer ciclo después del boot */
static bool g_firstCycleAfterBoot = false;

// ============================================================================
// FEAT-V2: Variables de tracking para logging diagnóstico
// ============================================================================
static unsigned long g_cycleStartMs = 0;
static unsigned long g_phaseStartMs = 0;
static bool g_gpsOk = false;
static bool g_lteOk = false;
static int  g_lteSentCount = 0;
static int  g_lteTotalCount = 0;

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
  outHumStr = String(h100);
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

// ============================================================================
// FIX-V7: Constantes de voltaje de batería (usadas por FEAT-V2 y FIX-V7)
// IO13 conectado a divisor R23(10K)/R24(10K) — factor 0.5
// ============================================================================

/** @brief Pin ADC para medición de voltaje de batería (divisor R23/R24) */
static const uint8_t BATTERY_ADC_PIN = 13;

/** @brief Voltaje mínimo de batería para operaciones con modem (en voltios) */
static const float BATTERY_MIN_VOLTAGE = 3.80f;

// ============================================================================
// FEAT-V2: Funciones de logging diagnóstico
// ============================================================================

/**
 * @brief Convierte esp_reset_reason_t a string legible
 */
static const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_SW:        return "SW_RESET";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}

/**
 * @brief Convierte esp_sleep_wakeup_cause_t a string legible
 */
static const char* wakeupCauseStr(esp_sleep_wakeup_cause_t w) {
  switch (w) {
    case ESP_SLEEP_WAKEUP_TIMER:     return "TIMER";
    case ESP_SLEEP_WAKEUP_EXT0:      return "EXT0";
    case ESP_SLEEP_WAKEUP_EXT1:      return "EXT1";
    case ESP_SLEEP_WAKEUP_TOUCHPAD:  return "TOUCH";
    case ESP_SLEEP_WAKEUP_ULP:       return "ULP";
    case ESP_SLEEP_WAKEUP_GPIO:      return "GPIO";
    case ESP_SLEEP_WAKEUP_UART:      return "UART";
    default:                         return "UNDEFINED";
  }
}

/**
 * @brief Imprime banner diagnóstico completo al boot
 */
static void printBootBanner(esp_reset_reason_t resetReason) {
  Serial.println("=====================================");
  Serial.print("FENIX v");
  Serial.println(FW_VERSION);
  Serial.println("=====================================");

  // Hora RTC legible
  {
    DateTime now = rtc.now();
    char dtBuf[20];
    snprintf(dtBuf, sizeof(dtBuf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    Serial.print("[DIAG] Hora RTC     : ");
    Serial.println(dtBuf);
  }

  // Uptime desde boot
  Serial.print("[DIAG] Uptime       : ");
  Serial.print(millis());
  Serial.println(" ms");

  // Ciclo libre (sin autoswitch)
  Serial.print("[DIAG] Ciclo        : ");
  Serial.println(g_cfg.ciclos);

  // Estado de recuperacion modem
  Serial.print("[DIAG] Modem fails  : ");
  Serial.print(g_cfg.modemConsecFails);
  Serial.print(" consecutivos");
  if (modemSkipCycles > 0) {
    Serial.print(" (backoff ");
    Serial.print(modemSkipCycles);
    Serial.print(" ciclos)");
  }
  Serial.println();

  // Reset y wakeup
  Serial.print("[DIAG] Reset reason : ");
  Serial.print(resetReasonStr(resetReason));
  Serial.print(" (");
  Serial.print((int)resetReason);
  Serial.println(")");

  Serial.print("[DIAG] Wakeup cause : ");
  Serial.print(wakeupCauseStr(g_wakeupCause));
  Serial.print(" (");
  Serial.print((int)g_wakeupCause);
  Serial.println(")");

  // Memoria
  Serial.print("[DIAG] Free heap    : ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");

  // Batería (FIX-V11: analogReadMilliVolts calibrado por eFuse)
  int mV = analogReadMilliVolts(BATTERY_ADC_PIN);
  float vBat = (mV / 1000.0f) * 2.0f;  // ×2 por divisor resistivo
  int pctNow = batteryPercent(vBat);
  Serial.print("[DIAG] Bateria      : ");
  Serial.print(vBat, 2);
  Serial.print("V (");
  Serial.print(pctNow);
  Serial.print("%) [");
  Serial.print(mV);
  Serial.println("mV pin]");

  // Delta respecto al ciclo anterior
  if (g_cfg.prevBatVoltage > 0.01f) {
    float delta = vBat - g_cfg.prevBatVoltage;
    int pctPrev = batteryPercent(g_cfg.prevBatVoltage);
    int pctDelta = pctNow - pctPrev;
    Serial.print("[DIAG] Bat anterior : ");
    Serial.print(g_cfg.prevBatVoltage, 2);
    Serial.print("V (");
    Serial.print(pctPrev);
    Serial.print("%) delta: ");
    if (delta >= 0) Serial.print("+");
    Serial.print(delta, 2);
    Serial.print("V (");
    if (pctDelta >= 0) Serial.print("+");
    Serial.print(pctDelta);
    Serial.println("%)");
  } else {
    Serial.println("[DIAG] Bat anterior : primer ciclo (sin referencia)");
  }

  // Buffer status
  if (buffer.fileExists()) {
    int count = 0;
    String tempLines[MAX_LINES_TO_READ];
    buffer.readLines(tempLines, MAX_LINES_TO_READ, count);
    int pending = 0;
    for (int i = 0; i < count; i++) {
      if (!tempLines[i].startsWith(PROCESSED_MARKER)) pending++;
    }
    Serial.print("[DIAG] Buffer       : ");
    Serial.print(pending);
    Serial.print(" pendientes, ");
    Serial.print(buffer.getFileSize());
    Serial.println(" bytes");
  } else {
    Serial.println("[DIAG] Buffer       : vacio");
  }

  // NVS GPS
  preferences.begin("sensores", true);
  if (preferences.isKey("gps_lat")) {
    Serial.print("[DIAG] NVS GPS      : lat=");
    Serial.print(preferences.getFloat("gps_lat", 0.0f), 6);
    Serial.print(", lng=");
    Serial.println(preferences.getFloat("gps_lng", 0.0f), 6);
  } else {
    Serial.println("[DIAG] NVS GPS      : sin datos");
  }

  // NVS Operadora
  if (preferences.isKey("lastOperator")) {
    uint8_t op = preferences.getUChar("lastOperator", 0);
    Serial.print("[DIAG] NVS Operadora: ");
    Serial.print(OPERADORAS[op].nombre);
    Serial.println(" (guardada)");
  } else {
    Serial.println("[DIAG] NVS Operadora: sin guardar");
  }
  preferences.end();

  // Decisiones del ciclo
  Serial.print("[DIAG] GPS este ciclo: ");
  Serial.println(g_firstCycleAfterBoot ? "SI" : "NO (NVS)");

  Serial.print("[DIAG] Sleep config : ");
  Serial.print((uint32_t)(g_cfg.sleep_time_us / 1000000ULL));
  Serial.println("s");

  Serial.println("=====================================");
}

/**
 * @brief Imprime timing de una fase completada
 */
static void printPhaseTime(const char* phase) {
  unsigned long elapsed = millis() - g_phaseStartMs;
  Serial.print("[TIME][");
  Serial.print(phase);
  Serial.print("] ");
  Serial.print(elapsed);
  Serial.println(" ms");
}

/**
 * @brief Imprime resumen compacto del ciclo antes de sleep
 * @note No re-lee el buffer para evitar asignar String[200] en stack
 */
static void printCycleSummary() {
  unsigned long totalMs = millis() - g_cycleStartMs;
  int mV = analogReadMilliVolts(BATTERY_ADC_PIN);
  float vBat = (mV / 1000.0f) * 2.0f;

  // Hora RTC al momento de dormir
  {
    DateTime now = rtc.now();
    char dtBuf[20];
    snprintf(dtBuf, sizeof(dtBuf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    Serial.print("[CYCLE] hora=");
    Serial.print(dtBuf);
  }

  Serial.print(" ciclo=");
  Serial.print(g_cfg.ciclos);
  Serial.print(" mfail=");
  Serial.print(g_cfg.modemConsecFails);

  Serial.print(" bat=");
  Serial.print(vBat, 2);
  Serial.print("V(");
  Serial.print(batteryPercent(vBat));
  Serial.print("%) heap=");
  Serial.print(ESP.getFreeHeap() / 1024);
  Serial.print("KB buf=");
  Serial.print(buffer.getFileSize());
  Serial.print("B");

  Serial.print(" gps=");
  Serial.print(g_gpsOk ? "OK" : "SKIP");

  Serial.print(" lte=");
  Serial.print(g_lteSentCount);
  Serial.print("/");
  Serial.print(g_lteTotalCount);

  Serial.print(" time=");
  Serial.print(totalMs / 1000);
  Serial.print("s");

  Serial.print(" reset=");
  Serial.println(resetReasonStr(esp_reset_reason()));
}

// ============================================================================
// FIX-V7: Gate de voltaje de batería antes de encender modem
// (constantes BATTERY_ADC_PIN y BATTERY_MIN_VOLTAGE definidas arriba)
// ============================================================================

/**
 * @brief Verifica si el voltaje de batería es suficiente para operar el modem
 *
 * Lee el ADC en IO13 (divisor 0.5) y compara con BATTERY_MIN_VOLTAGE.
 * Si la batería está por debajo del umbral, el modem no debe encenderse
 * para evitar brownout y desperdicio de energía.
 *
 * @return true si la batería está por encima del umbral, false si es insuficiente
 */
static bool isBatteryOkForModem() {
  int mV = analogReadMilliVolts(BATTERY_ADC_PIN);
  float vBat = (mV / 1000.0f) * 2.0f;  // FIX-V11: calibrado eFuse, ×2 divisor
  Serial.print("[INFO][APP] Voltaje bateria: ");
  Serial.print(vBat, 2);
  Serial.print("V (");
  Serial.print(mV);
  Serial.println("mV pin)");
  if (vBat < BATTERY_MIN_VOLTAGE) {
    Serial.print("[WARN][APP] Bateria baja (");
    Serial.print(vBat, 2);
    Serial.print("V < ");
    Serial.print(BATTERY_MIN_VOLTAGE, 2);
    Serial.println("V) — modem deshabilitado");
    return false;
  }
  return true;
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
  // FIX-V7: Verificar batería antes de encender modem para LTE
  if (!isBatteryOkForModem()) {
    Serial.println("[WARN][APP] LTE omitido por bateria baja. Datos permanecen en buffer.");
    return false;
  }
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

  bool configOk = lte.configureOperator(operadoraAUsar);

  // FIX-V2: Fallback — si operadora guardada falla, escanear todas
  if (!configOk && tieneOperadoraGuardada) {
    Serial.println("[WARN][APP] Operadora guardada fallo. Iniciando escaneo...");

    // Proteccion anti-bucle: si ya fallamos un escaneo reciente, saltar
    preferences.begin("sensores", false);
    uint8_t skipCycles = preferences.getUChar("skipScanCycles", 0);
    if (skipCycles > 0) {
      preferences.putUChar("skipScanCycles", skipCycles - 1);
      preferences.end();
      Serial.print("[WARN][APP] Saltando escaneo. Ciclos restantes: ");
      Serial.println(skipCycles - 1);
      lte.powerOff();
      return false;
    }
    preferences.end();

    // Borrar operadora de NVS y escanear todas
    preferences.begin("sensores", false);
    preferences.remove("lastOperator");
    preferences.end();
    tieneOperadoraGuardada = false;

    for (uint8_t i = 0; i < NUM_OPERADORAS; i++) {
      lte.testOperator((Operadora)i);
    }
    operadoraAUsar = lte.getBestOperator();
    int bestScore = lte.getOperatorScore(operadoraAUsar);

    Serial.print("[INFO][APP] Nueva operadora: ");
    Serial.print(OPERADORAS[operadoraAUsar].nombre);
    Serial.print(" (Score: ");
    Serial.print(bestScore);
    Serial.println(")");

    // Si ninguna tiene señal, activar proteccion anti-bucle (3 ciclos)
    if (bestScore <= -999) {
      Serial.println("[ERROR][APP] Sin señal. Saltando proximos 3 escaneos.");
      preferences.begin("sensores", false);
      preferences.putUChar("skipScanCycles", 3);
      preferences.end();
      lte.powerOff();
      return false;
    }

    configOk = lte.configureOperator(operadoraAUsar);
  }

  if (!configOk) {
    lte.powerOff();
    return false;
  }
  if (!lte.attachNetwork()) {
    lte.powerOff();
    return false;
  }
  if (!lte.activatePDP()) {
    lte.powerOff();
    return false;
  }
  if (!lte.openTCPConnection()) {
    lte.deactivatePDP();
    lte.powerOff();
    return false;
  }

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
      Serial.print(i + 1);
      Serial.println(" ya procesada, saltando...");
      continue;
    }

    Serial.print("[INFO][APP] Enviando línea ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.println(total);

    bool sentOk = lte.sendTCPData(allLines[i]);
    if (sentOk) {
      buffer.markLineAsProcessed(i);
      anySent = true;
      sentCount++;
      Serial.print("[INFO][APP] Línea ");
      Serial.print(i + 1);
      Serial.println(" enviada y marcada como procesada");
      delay(50);
    } else {
      Serial.print("[WARN][APP] Fallo al enviar línea ");
      Serial.print(i + 1);
      Serial.println(". Deteniendo envío. Línea permanece en buffer.");
      break;
    }
  }

  lte.closeTCPConnection();
  lte.deactivatePDP();
  lte.detachNetwork();
  lte.powerOff();

  // FEAT-V2: Guardar contadores para resumen del ciclo
  g_lteSentCount = sentCount;
  g_lteTotalCount = total;

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
 *    - Inicializa RTC DS3231 para timestamps
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
 * 7. **Decisión de GPS (FIX-V8):**
 *    - Si no hay coordenadas en NVS → g_firstCycleAfterBoot = true (leerá GPS)
 *    - Si ya hay coordenadas en NVS → skip GPS (usa coordenadas guardadas)
 *    - Estado inicial siempre: Cycle_ReadSensors
 *
 * 8. **Banner diagnóstico (FEAT-V2):**
 *    - Imprime hora RTC, ciclo, reset reason, batería, buffer, NVS
 *
 * @param cfg Estructura de configuración con parámetros de usuario
 *
 * @note GPS solo se lee cuando no hay coordenadas en NVS (primer boot o NVS limpio)
 */
void AppInit(const AppConfig& cfg) {
  g_cfg = cfg;

  Serial.begin(115200);
  delay(200);



  sleepModule.begin();
  g_wakeupCause = sleepModule.getWakeupCause();

        pinMode(GPIO_NUM_3, OUTPUT);
        digitalWrite(GPIO_NUM_3, HIGH);
        delay(5000);

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
  lte.setErrorCallback(onCOPSError);  // Registrar callback para borrar NVS en error COPS

  // FIX-V8: Decidir GPS basado en NVS, no en reset reason
  esp_reset_reason_t resetReason = esp_reset_reason();

  // FENIX: Zombie recovery — determinar si el modem puede usarse este ciclo
  {
    // 1. Detectar post-restart: leer flag de NVS
    Preferences fenixPrefs;
    fenixPrefs.begin("fenix", false);
    uint8_t postRestart = fenixPrefs.getUChar("postRestart", 0);
    if (postRestart > 0) {
      modemSkipCycles = fenixPrefs.getUChar("skipCycles", MODEM_SKIP_AFTER_RESTART);
      fenixPrefs.putUChar("postRestart", 0);  // Limpiar flag
      modemConsecFails = 0;  // Reset tras restart
      Serial.print("[INFO][FENIX] Post-restart recovery. Backoff ");
      Serial.print(modemSkipCycles);
      Serial.println(" ciclos.");
    }
    fenixPrefs.end();

    // 2. Evaluar estado del modem
    if (modemSkipCycles > 0) {
      modemSkipCycles--;
      g_modemAllowed = false;
      Serial.print("[WARN][FENIX] Modem en backoff. Ciclos restantes: ");
      Serial.println(modemSkipCycles);
    } else if (g_cfg.modemConsecFails >= MODEM_FAIL_THRESHOLD_RESTART) {
      // Umbral alcanzado: guardar backoff en NVS y hacer esp_restart()
      Serial.print("[CRIT][FENIX] Modem zombie detectado tras ");
      Serial.print(g_cfg.modemConsecFails);
      Serial.println(" fallos consecutivos.");
      Serial.println("[CRIT][FENIX] Ejecutando esp_restart() como recovery...");

      Preferences fenixSave;
      fenixSave.begin("fenix", false);
      fenixSave.putUChar("skipCycles", MODEM_SKIP_AFTER_RESTART);
      fenixSave.putUChar("postRestart", 1);
      fenixSave.end();

      modemConsecFails = 0;  // Reset para que no dispare de nuevo inmediatamente
      Serial.flush();
      delay(200);
      esp_restart();
      // No retorna
    } else {
      g_modemAllowed = true;
    }
  }

  preferences.begin("sensores", true);
  bool hasGpsInNvs = preferences.isKey("gps_lat") && preferences.isKey("gps_lng");
  preferences.end();

  g_firstCycleAfterBoot = !hasGpsInNvs;
  g_state = AppState::Cycle_ReadSensors;

  // FEAT-V2: Inicializar tracking y imprimir banner diagnóstico
  g_cycleStartMs = millis();
  g_gpsOk = false;
  g_lteOk = false;
  g_lteSentCount = 0;
  g_lteTotalCount = 0;
  printBootBanner(resetReason);

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
 * - **Cycle_ReadSensors:**
 *   - Lee ADC (batería), I2C (temp/hum), RS485 (4 registros)
 *   - Si es primer ciclo (sin GPS en NVS) → Cycle_Gps
 *   - Si no → recupera GPS de NVS y va a Cycle_GetICCID
 *
 * - **Cycle_Gps:**
 *   - Verifica batería (FIX-V7). Si baja → skip GPS, usa NVS
 *   - Enciende GNSS, espera fix, guarda coordenadas en NVS
 *   - Si no hay fix → usa ceros
 *   - Siempre → Cycle_GetICCID
 *
 * - **Cycle_GetICCID:**
 *   - Verifica batería (FIX-V7). Si baja → skip ICCID
 *   - Enciende LTE brevemente para leer ICCID, apaga LTE
 *   - Siempre → Cycle_BuildFrame
 *
 * - **Cycle_BuildFrame:**
 *   - Construye trama texto: $,iccid,epoch,lat,lng,alt,var1-7,#
 *   - Codifica a Base64
 *   - Si fallo → Cycle_Sleep (FIX-V12, no detiene el sistema)
 *   - Si éxito → Cycle_BufferWrite
 *
 * - **Cycle_BufferWrite:**
 *   - Guarda trama Base64 en buffer LittleFS (persistente)
 *   - Siempre → Cycle_SendLTE (aunque falle guardado)
 *
 * - **Cycle_SendLTE:**
 *   - Verifica batería (FIX-V7). Si baja → skip LTE
 *   - Llama sendBufferOverLTE_AndMarkProcessed()
 *   - Siempre → Cycle_CompactBuffer
 *
 * - **Cycle_CompactBuffer:**
 *   - Elimina del buffer solo líneas marcadas como procesadas
 *   - Tramas no enviadas permanecen para próximo ciclo
 *   - Siempre → Cycle_Sleep
 *
 * - **Cycle_Sleep:**
 *   - Guarda voltaje en RTC memory (FEAT-V2)
 *   - Imprime resumen del ciclo (FEAT-V2)
 *   - Configura timer wakeup (según cfg.sleep_time_us)
 *   - Entra en deep sleep (no retorna)
 *
 * - **Boot / Error:**
 *   - Espera indefinidamente (delay 1s)
 * 
 * @note Esta función nunca debe ser bloqueante excepto en estados finales
 * @note El sistema usa deep sleep, por lo que cada wakeup reinicia completamente el ESP32
 */
void AppLoop() {
  if (!g_initialized) return;

  // FIX-V6: BLE deshabilitado — ble.update() y estado BleOnly eliminados

  switch (g_state) {
    case AppState::Cycle_ReadSensors:
      {
        g_phaseStartMs = millis();  // FEAT-V2
        String vBat;
        if (!readADC_DiscardAndAverage(vBat)) vBat = "0";

        String vTemp, vHum;
        if (!readI2C_DiscardAndAverage(vTemp, vHum)) {
          vTemp = "0";
          vHum = "0";
        }

        String regs[4] = { "0", "0", "0", "0" };
        (void)readRS485_DiscardAndTakeLast(regs);


        g_varStr[0] = regs[0];
        g_varStr[1] = regs[1];
        g_varStr[2] = regs[2];
        g_varStr[3] = regs[3];
        g_varStr[4] = vTemp;
        g_varStr[5] = vHum;
        g_varStr[6] = vBat;

        printPhaseTime("Sensors");  // FEAT-V2

        if (g_firstCycleAfterBoot) {
          Serial.println("[INFO][APP] Primer ciclo: leyendo GPS");
          g_state = AppState::Cycle_Gps;
          g_firstCycleAfterBoot = false;
        } else {
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

          g_state = AppState::Cycle_GetICCID;
        }
        break;
      }

    case AppState::Cycle_Gps:
      {
        g_phaseStartMs = millis();  // FEAT-V2
        fillZeros(g_lat, COORD_LEN);
        fillZeros(g_lng, COORD_LEN);
        fillZeros(g_alt, ALT_LEN);

        GpsFix fix;
        bool gotFix = false;

        // FENIX: Modem en backoff → skip GPS, usar NVS
        if (!g_modemAllowed) {
          Serial.println("[WARN][FENIX] GPS omitido: modem en backoff. Usando NVS.");
          preferences.begin("sensores", true);
          if (preferences.isKey("gps_lat") && preferences.isKey("gps_lng") && preferences.isKey("gps_alt")) {
            float lat = preferences.getFloat("gps_lat", 0.0f);
            float lng = preferences.getFloat("gps_lng", 0.0f);
            float alt = preferences.getFloat("gps_alt", 0.0f);
            preferences.end();
            formatCoord(g_lat, sizeof(g_lat), lat);
            formatCoord(g_lng, sizeof(g_lng), lng);
            formatAlt(g_alt, sizeof(g_alt), alt);
          } else {
            preferences.end();
          }
          printPhaseTime("GPS");
          g_state = AppState::Cycle_GetICCID;
          break;
        }

        // FIX-V7: Verificar batería antes de encender modem para GPS/GNSS
        if (!isBatteryOkForModem()) {
          Serial.println("[WARN][APP] GPS omitido por bateria baja. Usando coordenadas NVS.");
          // Recuperar coordenadas de NVS si existen
          preferences.begin("sensores", true);
          if (preferences.isKey("gps_lat") && preferences.isKey("gps_lng") && preferences.isKey("gps_alt")) {
            float lat = preferences.getFloat("gps_lat", 0.0f);
            float lng = preferences.getFloat("gps_lng", 0.0f);
            float alt = preferences.getFloat("gps_alt", 0.0f);
            preferences.end();
            formatCoord(g_lat, sizeof(g_lat), lat);
            formatCoord(g_lng, sizeof(g_lng), lng);
            formatAlt(g_alt, sizeof(g_alt), alt);
          } else {
            preferences.end();
          }
          g_state = AppState::Cycle_GetICCID;
          break;
        }

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

          g_gpsOk = true;  // FEAT-V2
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

        printPhaseTime("GPS");  // FEAT-V2
        g_state = AppState::Cycle_GetICCID;
        break;
      }
    case AppState::Cycle_GetICCID:
      {
        g_phaseStartMs = millis();  // FEAT-V2

        // FIX-V14: Intentar leer ICCID del modem, con fallback a NVS
        bool gotIccid = false;
        // FENIX: Modem en backoff → skip directo a NVS
        // FIX-V7: Verificar batería antes de encender modem para ICCID
        if (g_modemAllowed && isBatteryOkForModem() && lte.powerOn()) {
          g_iccid = lte.getICCID();
          lte.powerOff();
          if (g_iccid.length() > 0) {
            gotIccid = true;
            // Guardar en NVS para futuros ciclos
            preferences.begin("sensores", false);
            preferences.putString("iccid", g_iccid);
            preferences.end();
            Serial.print("[INFO][APP] ICCID guardado en NVS: ");
            Serial.println(g_iccid);
          }
        }

        // Fallback: recuperar de NVS
        if (!gotIccid) {
          preferences.begin("sensores", true);
          if (preferences.isKey("iccid")) {
            g_iccid = preferences.getString("iccid", "");
            Serial.print("[INFO][APP] ICCID recuperado de NVS: ");
            Serial.println(g_iccid);
          } else {
            g_iccid = "";
            Serial.println("[WARN][APP] Sin ICCID: modem fallo y NVS vacio");
          }
          preferences.end();
        }

        printPhaseTime("ICCID");  // FEAT-V2
        g_state = AppState::Cycle_BuildFrame;
        break;
      }
    case AppState::Cycle_BuildFrame:
      {
        g_phaseStartMs = millis();  // FEAT-V2
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
          // FIX-V12: No detener el sistema por fallo de formato.
          // Saltar a Sleep para preservar bateria y reintentar en proximo ciclo.
          Serial.println("[ERROR][APP] buildFrame fallo. Saltando a Sleep.");
          printPhaseTime("Build");
          g_state = AppState::Cycle_Sleep;
          break;
        }
        Serial.println("[INFO][APP] === TRAMA NORMAL ===");
        Serial.println(frameNormal);

        Serial.println("[DEBUG][APP] Generando trama Base64...");
        if (!formatter.buildFrameBase64(g_frame, sizeof(g_frame))) {
          // FIX-V12: Idem — no detener el sistema.
          Serial.println("[ERROR][APP] buildFrameBase64 fallo. Saltando a Sleep.");
          printPhaseTime("Build");
          g_state = AppState::Cycle_Sleep;
          break;
        }
        Serial.println("[INFO][APP] === TRAMA BASE64 ===");
        Serial.println(g_frame);
        Serial.println();

        printPhaseTime("Build");  // FEAT-V2
        g_state = AppState::Cycle_BufferWrite;
        break;
      }

    case AppState::Cycle_BufferWrite:
      {
        g_phaseStartMs = millis();  // FEAT-V2
        Serial.println("[INFO][APP] Guardando trama en buffer (persistente)...");
        bool saved = buffer.appendLine(String(g_frame));
        if (saved) {
          Serial.println("[INFO][APP] Trama guardada exitosamente en buffer");
        } else {
          Serial.println("[ERROR][APP] Fallo al guardar trama en buffer");
        }
        printPhaseTime("Buffer");  // FEAT-V2
        Serial.println("[DEBUG][APP] Pasando a Cycle_SendLTE");
        g_state = AppState::Cycle_SendLTE;
        break;
      }

    case AppState::Cycle_SendLTE:
      {
        g_phaseStartMs = millis();  // FEAT-V2

        // FENIX: Verificar si el modem esta en backoff
        if (!g_modemAllowed) {
          Serial.println("[WARN][FENIX] LTE omitido: modem en backoff. Datos en buffer.");
          g_lteOk = false;
          printPhaseTime("LTE");
          g_state = AppState::Cycle_CompactBuffer;
          break;
        }

        Serial.println("[DEBUG][APP] Iniciando envio por LTE...");
        g_lteOk = sendBufferOverLTE_AndMarkProcessed();

        // FENIX: Tracking de fallos consecutivos del modem
        if (g_lteOk) {
          modemConsecFails = 0;
        } else {
          if (modemConsecFails < 255) {
            modemConsecFails++;
          }
          Serial.print("[WARN][FENIX] Modem fallo. Consecutivos: ");
          Serial.print(modemConsecFails);
          Serial.print("/");
          Serial.println(MODEM_FAIL_THRESHOLD_RESTART);
        }

        Serial.println("[DEBUG][APP] Envio completado, pasando a CompactBuffer");
        printPhaseTime("LTE");  // FEAT-V2
        g_state = AppState::Cycle_CompactBuffer;
        break;
      }

    case AppState::Cycle_CompactBuffer:
      {
        g_phaseStartMs = millis();  // FEAT-V2
        Serial.println("[INFO][APP] Eliminando solo tramas procesadas del buffer...");
        (void)buffer.removeProcessedLines();
        Serial.println("[INFO][APP] Buffer compactado. Tramas no enviadas permanecen para próximo ciclo.");
        printPhaseTime("Compact");  // FEAT-V2
        g_state = AppState::Cycle_Sleep;
        break;
      }

    case AppState::Cycle_Sleep:
      {
        // Guardar voltaje actual en RTC memory para comparar en proximo ciclo
        {
          int mV = analogReadMilliVolts(BATTERY_ADC_PIN);
          prevBatVoltage = (mV / 1000.0f) * 2.0f;
        }
        // FEAT-V2: Resumen del ciclo antes de dormir
        printCycleSummary();
        lte.powerOff();
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
