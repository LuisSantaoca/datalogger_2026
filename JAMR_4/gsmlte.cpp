#include "gsmlte.h"
#include "sleepdev.h"
#include <TinyGsmClient.h>
#include <LittleFS.h>
#include "cryptoaes.h"

struct PdpContextInfo {
  int cid = -1;
  String ip;
};

static bool captureCNACTResponse(String& response, unsigned long timeoutMs);
static bool extractActivePdpContext(const String& response, PdpContextInfo& ctx);
static bool ensurePdpActive(uint32_t budgetMs);

static const uint32_t PDP_VALIDATION_MIN_BUDGET_MS = 4000UL;
static const uint32_t PDP_VALIDATION_MAX_SLICE_MS = 20000UL;

static unsigned long g_cycle_start_ms = 0;
static bool g_cycle_budget_set = false;

// FIX-8: Estado global de salud del módem en el ciclo actual
modem_health_state_t g_modem_health_state = MODEM_HEALTH_OK;
static uint8_t g_modem_timeouts_critical = 0; // cuenta timeouts en operaciones críticas (CNACT, CAOPEN, envío TCP)

#if ENABLE_MULTI_OPERATOR_EFFICIENT
// Tabla de perfiles de operador. Ajustar según resultados de campo.
static const OperatorProfile OP_PROFILES[] = {
  { "DEFAULT_CATM", MODEM_NETWORK_MODE, CAT_M,  "em", 90000UL, 0 },
  { "NB_IOT_FALLBACK", MODEM_NETWORK_MODE, NB_IOT, "em", 30000UL, 1 },
};
static const size_t OP_PROFILES_COUNT = sizeof(OP_PROFILES) / sizeof(OP_PROFILES[0]);

static int8_t g_last_success_operator_id = -1;                 // -1 = ninguno
static uint8_t g_operator_failures[OP_PROFILES_COUNT] = {0};
static uint32_t g_lte_max_wait_ms = LTE_CONNECT_BUDGET_MS;     // límite dinámico por perfil

static const char* LTE_PROFILE_FILE = "/lte_profile.cfg";
static void loadPersistedOperatorId();
static void persistOperatorId(int8_t id);

static bool startLTE_multiOperator();
static bool tryConnectOperator(const OperatorProfile& profile, uint32_t allowedMs, uint32_t& elapsedMs);
static void buildOperatorOrder(uint8_t* order, size_t& count);
#else
static const size_t OP_PROFILES_COUNT = 0; // Evita warnings cuando el flag está apagado
static uint32_t g_lte_max_wait_ms = 120000UL;
#endif

// =============================================================================
// VARIABLES GLOBALES
// =============================================================================

// Instancia del módem TinyGSM
TinyGsm modem(SerialAT);

// Configuración del módem
ModemConfig modemConfig;

// Variables de estado del sistema
bool modemInitialized = false;
bool gpsEnabled = false;
unsigned long lastConnectionAttempt = 0;
int consecutiveFailures = 0;
static const char* FIX6_DEFAULT_CONTEXT = "contexto-desconocido";

// =============================================================================
// FIX-8: Helpers de salud del módem
// =============================================================================

static void modemHealthReset() {
  g_modem_health_state = MODEM_HEALTH_OK;
  g_modem_timeouts_critical = 0;
}

static void modemHealthRegisterTimeout(const char* contextTag) {
  g_modem_timeouts_critical++;

  if (g_modem_timeouts_critical == 1) {
    logMessage(2, String("[FIX-8] Primer timeout crítico detectado en ") + contextTag);
    g_modem_health_state = MODEM_HEALTH_TRYING;
  } else if (g_modem_timeouts_critical >= 3 && g_modem_health_state != MODEM_HEALTH_FAILED) {
    logMessage(1, String("[FIX-8] Múltiples timeouts críticos (") + g_modem_timeouts_critical + ") en " + contextTag + ", posible estado zombie");
    g_modem_health_state = MODEM_HEALTH_ZOMBIE_DETECTED;
  }
}

static void modemHealthMarkOk() {
  if (g_modem_health_state == MODEM_HEALTH_TRYING || g_modem_health_state == MODEM_HEALTH_ZOMBIE_DETECTED) {
    g_modem_health_state = MODEM_HEALTH_RECOVERED;
    logMessage(2, "[FIX-8] Módem recuperado tras timeouts previos");
  } else {
    g_modem_health_state = MODEM_HEALTH_OK;
  }
  g_modem_timeouts_critical = 0;
}

static bool modemHealthShouldAbortCycle() {
  if (g_modem_health_state == MODEM_HEALTH_ZOMBIE_DETECTED && g_modem_timeouts_critical >= 3) {
    logMessage(1, "[FIX-8] Estado zombie confirmado: se marcará fallo de módem en este ciclo");
    g_modem_health_state = MODEM_HEALTH_FAILED;
    return true;
  }
  return false;
}

void resetCommunicationCycleBudget() {
#if ENABLE_WDT_DEFENSIVE_LOOPS
  esp_task_wdt_reset();
#endif
  g_cycle_start_ms = millis();
  g_cycle_budget_set = true;
}

uint32_t remainingCommunicationCycleBudget() {
  if (!g_cycle_budget_set) {
    return COMM_CYCLE_BUDGET_MS;
  }

  unsigned long now = millis();
  unsigned long elapsed = now - g_cycle_start_ms;
  if (elapsed >= COMM_CYCLE_BUDGET_MS) {
    return 0;
  }
  return COMM_CYCLE_BUDGET_MS - elapsed;
}

bool ensureCommunicationBudget(const char* contextTag) {
  if (remainingCommunicationCycleBudget() > 0) {
    return true;
  }

  const char* tag = (contextTag && contextTag[0] != '\0') ? contextTag : FIX6_DEFAULT_CONTEXT;
  logMessage(1, String("[FIX-6] Presupuesto de ciclo agotado en ") + tag);
  return false;
}

#if ENABLE_MULTI_OPERATOR_EFFICIENT
static void loadPersistedOperatorId() {
  if (!LittleFS.exists(LTE_PROFILE_FILE)) {
    logMessage(3, "[FIX-7] Archivo de perfil LTE no existe, usando orden por defecto");
    return;
  }

  File f = LittleFS.open(LTE_PROFILE_FILE, "r");
  if (!f) {
    logMessage(1, "[FIX-7] No se pudo abrir archivo de perfil LTE para lectura");
    return;
  }

  String line = f.readStringUntil('\n');
  f.close();
  line.trim();

  if (line.length() == 0) {
    logMessage(1, "[FIX-7] Archivo de perfil LTE vacío");
    return;
  }

  int val = line.toInt();
  if (val >= 0 && val < static_cast<int>(OP_PROFILES_COUNT)) {
    g_last_success_operator_id = static_cast<int8_t>(val);
    logMessage(2, "[FIX-7] Perfil LTE persistente cargado: id=" + String(val));
  } else {
    logMessage(1, "[FIX-7] Valor fuera de rango en archivo de perfil LTE: " + line);
  }
}

static void persistOperatorId(int8_t id) {
  File f = LittleFS.open(LTE_PROFILE_FILE, "w");
  if (!f) {
    logMessage(1, "[FIX-7] No se pudo abrir archivo de perfil LTE para escritura");
    return;
  }

  f.println(id);
  f.close();
  logMessage(2, "[FIX-7] Perfil LTE persistente actualizado a id=" + String(id));
}
#endif

// Variables para almacenamiento de datos
String iccidsim0 = "";
int signalsim0 = 0;
const char* ARCHIVO_BUFFER = "/buffer.txt";

// Union para conversión de float a bytes
union FloatToBytes {
  float f;
  uint8_t b[4];
} latConverter, lonConverter, altConverter;

// Variables GPS
float gps_latitude = 0;
float gps_longitude = 0;
float gps_speed = 0;
float gps_altitude = 0;
int gps_vsat = 0;
int gps_usat = 0;
float gps_accuracy = 0;
int gps_year = 0;
int gps_month = 0;
int gps_day = 0;
int gps_hour = 0;
int gps_minute = 0;
int gps_second = 0;

/**
 * Inicializa la configuración por defecto del módem
 */
void initModemConfig() {
  modemConfig.serverIP = "d01.elathia.ai";
  modemConfig.serverPort = "11235";
  modemConfig.apn = "em";
  modemConfig.networkMode = MODEM_NETWORK_MODE;
  modemConfig.bandMode = CAT_M;
  modemConfig.maxRetries = 5;
  modemConfig.baseTimeout = 5000;
  modemConfig.enableDebug = true;
}

/**
 * Configura parámetros del módem dinámicamente
 * @param serverIP - IP del servidor
 * @param serverPort - Puerto del servidor
 * @param apn - APN de la red
 * @param networkMode - Modo de red (38=CAT-M/NB-IoT)
 * @param bandMode - Modo de banda (1=CAT-M, 2=NB-IoT)
 */
void configureModem(const String& serverIP, const String& serverPort,
                    const String& apn, int networkMode, int bandMode) {
  modemConfig.serverIP = serverIP;
  modemConfig.serverPort = serverPort;
  modemConfig.apn = apn;
  modemConfig.networkMode = networkMode;
  modemConfig.bandMode = bandMode;

  if (modemConfig.enableDebug) {
    Serial.println("🔧 Configuración del módem actualizada:");
    Serial.println("   Servidor: " + modemConfig.serverIP + ":" + modemConfig.serverPort);
    Serial.println("   APN: " + modemConfig.apn);
    Serial.println("   Modo Red: " + String(modemConfig.networkMode));
    Serial.println("   Modo Banda: " + String(modemConfig.bandMode));
  }
}

/**
 * Calcula timeout adaptativo basado en calidad de señal y fallos previos
 * @return Timeout en milisegundos
 */
unsigned long getAdaptiveTimeout() {
  unsigned long baseTimeout = modemConfig.baseTimeout;

  // Ajustar según calidad de señal con rangos más específicos
  if (signalsim0 >= 20) {
    baseTimeout *= 0.7;  // Señal excelente: timeout más corto
  } else if (signalsim0 >= 15) {
    baseTimeout *= 0.8;  // Señal buena: timeout ligeramente más corto
  } else if (signalsim0 >= 10) {
    baseTimeout *= 1.2;  // Señal regular: timeout ligeramente más largo
  } else if (signalsim0 >= 5) {
    baseTimeout *= 1.8;  // Señal mala: timeout mucho más largo
  } else {
    baseTimeout *= 2.5;  // Señal muy mala: timeout máximo
  }

  // Ajustar según fallos consecutivos con escalamiento progresivo
  if (consecutiveFailures > 0) {
    float multiplier = 1.0 + (consecutiveFailures * 0.3);
    if (consecutiveFailures >= 3) {
      multiplier += 0.5; // Penalización adicional después de 3 fallos
    }
    baseTimeout *= multiplier;
  }

  // Limitar timeout con rangos más específicos según operación
  if (baseTimeout > 45000) baseTimeout = 45000; // Máximo 45s para operaciones críticas
  if (baseTimeout < 2000) baseTimeout = 2000;   // Mínimo 2s para estabilidad

  if (modemConfig.enableDebug && consecutiveFailures > 0) {
    logMessage(3, "🕐 Timeout adaptativo: " + String(baseTimeout) + "ms (señal: " + String(signalsim0) + ", fallos: " + String(consecutiveFailures) + ")");
  }

  return baseTimeout;
}

/**
 * Sistema de logging mejorado con niveles y timestamps (integrado)
 * @param level - Nivel de log (0=Error, 1=Warning, 2=Info, 3=Debug)
 * @param message - Mensaje a loguear
 */
void logMessage(int level, const String& message) {
  if (!modemConfig.enableDebug && level > 2) return;

  String timestamp = String(millis()) + "ms";
  String levelStr;

  switch (level) {
    case 0: levelStr = "❌ ERROR"; break;
    case 1: levelStr = "⚠️  WARN"; break;
    case 2: levelStr = "ℹ️  INFO"; break;
    case 3: levelStr = "🔍 DEBUG"; break;
    default: levelStr = "❓ UNKN"; break;
  }

  Serial.println("[" + timestamp + "] " + levelStr + ": " + message);
}

/**
 * Habilita o deshabilita el modo debug
 * @param enable - true para habilitar, false para deshabilitar
 */
void setDebugMode(bool enable) {
  modemConfig.enableDebug = enable;
  logMessage(2, "🔧 Modo debug " + String(enable ? "habilitado" : "deshabilitado"));
}

/**
 * Obtiene estadísticas del sistema
 * @return String con estadísticas formateadas
 */
String getSystemStats() {
  String stats = "=== ESTADÍSTICAS DEL SISTEMA ===\n";
  stats += "Módem inicializado: " + String(modemInitialized ? "Sí" : "No") + "\n";
  stats += "GPS habilitado: " + String(gpsEnabled ? "Sí" : "No") + "\n";
  stats += "Calidad de señal: " + String(signalsim0) + "\n";
  stats += "Fallos consecutivos: " + String(consecutiveFailures) + "\n";
  stats += "Último intento de conexión: " + String(lastConnectionAttempt) + "ms\n";
  stats += "Timeout adaptativo actual: " + String(getAdaptiveTimeout()) + "ms\n";

  return stats;
}

/**
 * Configura e inicializa el módem LTE/GSM
 * @param data - Estructura de datos de sensores
 */
void setupModem(sensordata_type* data) {
  logMessage(2, "🚀 Iniciando configuración del módem LTE/GSM");

  // FIX-8: reiniciar estado de salud del módem al inicio de cada ciclo
  modemHealthReset();

  // Inicializar configuración por defecto
  initModemConfig();

  // Configurar comunicación serial
  SerialMon.begin(115200);
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  // Inicializar GSM
  startGsm();
  updateCheckpoint(CP_GSM_OK); // 🆕 FIX-004: Modem respondiendo OK
  esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed watchdog después de iniciar GSM
  getIccid();

  // Inicializar sistema de archivos
  if (!iniciarLittleFS(5, 500)) {
    logMessage(0, "🚫 Abortando ejecución por error de LittleFS");
    ESP.restart();
  }
  esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed watchdog después de LittleFS

#if ENABLE_MULTI_OPERATOR_EFFICIENT
  loadPersistedOperatorId();
#endif

  // Preparar y guardar datos
  String cadenaEncriptada = dataSend(data);
  logMessage(2, "📦 Datos preparados y encriptados: " + String(cadenaEncriptada.length()) + " bytes");
  guardarDato(cadenaEncriptada);

  // Intentar conexión LTE y envío
  resetCommunicationCycleBudget();
  logMessage(2, String("[FIX-6] Presupuesto global de ciclo: ") + COMM_CYCLE_BUDGET_MS + "ms");

  bool lteOk = false;
#if ENABLE_MULTI_OPERATOR_EFFICIENT
  lteOk = startLTE_multiOperator();
#else
  lteOk = startLTE();
#endif

  if (lteOk) {
    updateCheckpoint(CP_LTE_OK); // 🆕 FIX-004: Conexión LTE establecida
    esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed watchdog después de conexión LTE
    logMessage(2, "✅ Conexión LTE establecida, enviando datos");
    enviarDatos();
    updateCheckpoint(CP_DATA_SENT); // 🆕 FIX-004: Datos enviados exitosamente
    limpiarEnviados();
    esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed watchdog después de enviar datos
    consecutiveFailures = 0;  // Resetear contador de fallos
    modemHealthMarkOk();      // 🆕 FIX-8: ciclo con módem sano
  } else {
    consecutiveFailures++;
    logMessage(1, "⚠️  Fallo en conexión LTE (intento " + String(consecutiveFailures) + ")");
  }

  // Apagar módem para ahorrar energía
  modemPwrKeyPulse();
  modemInitialized = true;

  logMessage(2, "🏁 Configuración del módem completada");
}

/**
 * Controla el pin de encendido del módem con timing optimizado
 */
void modemPwrKeyPulse() {
  logMessage(3, "🔌 Pulsando pin PWRKEY del módem");

  digitalWrite(PWRKEY_PIN, HIGH);
  delay(MODEM_PWRKEY_DELAY);
  digitalWrite(PWRKEY_PIN, LOW);
  delay(MODEM_STABILIZE_DELAY);

  logMessage(3, "✅ Pulsado PWRKEY completado");
}

/**
 * Reinicia el módem completamente
 */
void modemRestart() {
  logMessage(2, "🔄 Reiniciando módem completamente");

  modemPwrKeyPulse();
  delay(LONG_DELAY);
  modemPwrKeyPulse();
  delay(LONG_DELAY);

  // Resetear variables de estado
  modemInitialized = false;
  gpsEnabled = false;
  consecutiveFailures = 0;

  logMessage(2, "✅ Reinicio del módem completado");
}

static bool captureCNACTResponse(String& response, unsigned long timeoutMs) {
  flushPortSerial();
  modem.sendAT("+CNACT?");

  response = "";
  unsigned long start = millis();
  unsigned long lastFeed = millis();
  bool receivedOk = false;

  while (millis() - start < timeoutMs) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      if (modemConfig.enableDebug) {
        Serial.print(c);
      }

      if (response.endsWith("OK\r\n")) {
        receivedOk = true;
      }

      #if ENABLE_WDT_DEFENSIVE_LOOPS
      if (millis() - lastFeed >= 150) {
        esp_task_wdt_reset();
        lastFeed = millis();
        delay(1);
      }
      #endif
    }

    if (receivedOk) {
      break;
    }

    esp_task_wdt_reset();
    delay(1);
  }

  if (!receivedOk) {
    logMessage(1, "[FIX-5] Respuesta incompleta de +CNACT?");
    return false;
  }

  if (modemConfig.enableDebug) {
    logMessage(3, String("[FIX-5] Respuesta +CNACT?: ") + response);
  }

  return true;
}

static bool extractActivePdpContext(const String& response, PdpContextInfo& ctx) {
  int idx = 0;

  while ((idx = response.indexOf("+CNACT:", idx)) != -1) {
    int lineEnd = response.indexOf('\n', idx);
    String line = response.substring(idx, (lineEnd == -1) ? response.length() : lineEnd);
    idx = (lineEnd == -1) ? response.length() : lineEnd;

    int colon = line.indexOf(':');
    if (colon == -1) {
      continue;
    }

    String payload = line.substring(colon + 1);
    payload.trim();
    payload.replace("\r", "");

    int firstComma = payload.indexOf(',');
    if (firstComma == -1) {
      continue;
    }

    int secondComma = payload.indexOf(',', firstComma + 1);
    if (secondComma == -1) {
      continue;
    }

    int cid = payload.substring(0, firstComma).toInt();
    int state = payload.substring(firstComma + 1, secondComma).toInt();
    String ip = payload.substring(secondComma + 1);
    ip.trim();
    ip.replace("\"", "");

    if (state == 1 && ip.length() > 0 && ip != "0.0.0.0") {
      ctx.cid = cid;
      ctx.ip = ip;
      return true;
    }
  }

  return false;
}

static bool ensurePdpActive(uint32_t budgetMs) {
  if (budgetMs < PDP_VALIDATION_MIN_BUDGET_MS) {
    budgetMs = PDP_VALIDATION_MIN_BUDGET_MS;
  }

  if (budgetMs > PDP_VALIDATION_MAX_SLICE_MS) {
    budgetMs = PDP_VALIDATION_MAX_SLICE_MS;
  }

  logMessage(2, String("[FIX-5] Validando PDP activo (presupuesto ") + budgetMs + "ms)");

  // Refuerza la activación antes de iniciar la validación.
  sendATCommand("+CNACT=0,1", "OK", 5000);

  unsigned long start = millis();
  uint8_t attempt = 0;

  while (millis() - start < budgetMs) {
    attempt++;

    PdpContextInfo ctx;
    String response;

    if (captureCNACTResponse(response, 4000) && extractActivePdpContext(response, ctx)) {
      logMessage(2, String("[FIX-5] PDP activo detectado en contexto ") + ctx.cid + " (IP " + ctx.ip + ")");
      return true;
    }

    logMessage(3, String("[FIX-5] CNACT? sin PDP activo (intento ") + attempt + ")");

    if ((attempt % 2) == 0) {
      logMessage(3, "[FIX-5] Reintentando activación PDP");
      sendATCommand("+CNACT=0,1", "OK", 5000);
    }

    if (millis() - start < budgetMs) {
      esp_task_wdt_reset();
      delay(1500);
    }
  }

  logMessage(1, String("[FIX-5] Agotado presupuesto PDP sin IP válida tras ") + budgetMs + "ms");
  return false;
}

/**
 * Establece conexión LTE con configuración adaptativa
 * @return true si la conexión es exitosa
 */
bool startLTE() {
  if (!ensureCommunicationBudget("startLTE_begin")) {
    return false;
  }

  logMessage(2, "🌐 Iniciando conexión LTE");
  updateCheckpoint(CP_LTE_CONNECT); // 🆕 FIX-004: Conectando a LTE

  // Configurar modo de red
  if (!sendATCommand("+CNMP=" + String(modemConfig.networkMode), "OK", getAdaptiveTimeout())) {
    logMessage(0, "❌ Fallo configurando modo de red");
    return false;
  }

  // Configurar modo de banda
  if (!sendATCommand("+CMNB=" + String(modemConfig.bandMode), "OK", getAdaptiveTimeout())) {
    logMessage(0, "❌ Fallo configurando modo de banda");
    return false;
  }

  // Configurar bandas específicas - Espectro completo validado en campo
  const String catmBands = "+CBANDCFG=\"CAT-M\",1,2,3,4,5,8,12,13,14,18,19,20,25,26,27,28,66,85";
  if (!sendATCommand(catmBands, "OK", getAdaptiveTimeout())) {
    logMessage(1, "⚠️  Fallo configurando bandas CAT-M completas, reintentando");
    delay(500);
    flushPortSerial();
    if (!sendATCommand(catmBands, "OK", getAdaptiveTimeout())) {
      logMessage(0, "❌ No se pudo configurar bandas CAT-M completas tras reintento");
    }
  }

  // Configurar NB-IoT como fallback con bandas principales
  if (!sendATCommand("+CBANDCFG=\"NB-IOT\",2,3,5,8,20,28", "OK", getAdaptiveTimeout())) {
    logMessage(1, "⚠️  Fallo configurando bandas NB-IoT");
  }

  // Verificar configuración
  sendATCommand("+CBANDCFG?", "OK", getAdaptiveTimeout());
  delay(LONG_DELAY);

  // Configurar contexto PDP
  String pdpCommand = "+CGDCONT=1,\"IP\",\"" + modemConfig.apn + "\"";
  if (!sendATCommand(pdpCommand, "OK", getAdaptiveTimeout())) {
    logMessage(0, "❌ Fallo configurando contexto PDP");
    return false;
  }

  // Activar contexto PDP
  if (!sendATCommand("+CNACT=0,1", "OK", 5000)) {
    logMessage(0, "❌ Fallo activando contexto PDP");
    modemHealthRegisterTimeout("startLTE_CNACT"); // 🆕 FIX-8
    if (modemHealthShouldAbortCycle()) {
      return false;
    }
    return false;
  }

  // Esperar conexión a la red
  unsigned long t0 = millis();
  unsigned long maxWaitTime = 120000;  // 🆕 FIX-4.1.1: 120s para zonas de señal baja (antes 60s)
#if ENABLE_MULTI_OPERATOR_EFFICIENT
  // En modo multi-operador, g_lte_max_wait_ms define el máximo
  // absoluto para este perfil (presupuesto ya recortado en tryConnectOperator).
  if (g_lte_max_wait_ms > 0 && g_lte_max_wait_ms < maxWaitTime) {
    maxWaitTime = g_lte_max_wait_ms;
  }
#endif

  while (true) {
    unsigned long now = millis();
    unsigned long elapsed = now - t0;
    if (elapsed >= maxWaitTime) {
      break;
    }
    if (!ensureCommunicationBudget("startLTE_wait")) {
      return false;
    }

    esp_task_wdt_reset(); // 🆕 FIX-003: Feed watchdog en cada iteración
    int signalQuality = modem.getSignalQuality();
    logMessage(3, "📶 Calidad de señal: " + String(signalQuality));

    if (modem.isNetworkConnected()) {
      uint32_t elapsed = millis() - t0;
      uint32_t remaining = (elapsed >= maxWaitTime) ? 0 : (maxWaitTime - elapsed);
      uint32_t cycleRemaining = remainingCommunicationCycleBudget();
      if (cycleRemaining > 0 && cycleRemaining < remaining) {
        remaining = cycleRemaining;
      }

      if (!ensurePdpActive(remaining)) {
        logMessage(1, "[FIX-5] Registro LTE sin PDP activo, reintentando dentro del presupuesto");
        modemHealthRegisterTimeout("startLTE_ensurePdpActive"); // 🆕 FIX-8
        if (modemHealthShouldAbortCycle()) {
          return false;
        }
      } else {
        logMessage(2, "✅ Conectado a la red LTE con PDP activo");
        sendATCommand("+CPSI?", "OK", getAdaptiveTimeout());
        flushPortSerial();
        modemHealthMarkOk(); // 🆕 FIX-8: se logró conexión LTE estable
        return true;
      }
    }

    delay(1000);
  }

  logMessage(0, "❌ Timeout: No se pudo conectar a la red LTE");
  return false;
}

static void buildOperatorOrder(uint8_t* order, size_t& count) {
  count = 0;
  for (size_t i = 0; i < OP_PROFILES_COUNT; ++i) {
    order[count++] = i;
  }

  if (count == 0) {
    return;
  }

  // Mover el último perfil exitoso al frente para priorizarlo.
  if (g_last_success_operator_id >= 0) {
    for (size_t i = 0; i < count; ++i) {
      if (order[i] == static_cast<uint8_t>(g_last_success_operator_id)) {
        if (i != 0) {
          uint8_t tmp = order[0];
          order[0] = order[i];
          order[i] = tmp;
        }
        break;
      }
    }
  }

  // Ordenar el resto por menor número de fallos (burbuja simple, pocos perfiles).
  for (size_t i = 1; i < count; ++i) {
    for (size_t j = i + 1; j < count; ++j) {
      if (g_operator_failures[order[j]] < g_operator_failures[order[i]]) {
        uint8_t tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
      }
    }
  }
}

static bool tryConnectOperator(const OperatorProfile& profile, uint32_t allowedMs, uint32_t& elapsedMs) {
  if (!ensureCommunicationBudget("tryConnectOperator")) {
    elapsedMs = 0;
    return false;
  }

  if (allowedMs < 5000UL) {
    allowedMs = 5000UL;  // Tiempo mínimo razonable para que startLTE haga algo útil
  }

  logMessage(2, String("[FIX-4] Perfil ") + profile.name +
                 ": presupuesto " + String(allowedMs) + "ms");

  modemConfig.networkMode = profile.networkMode;
  modemConfig.bandMode = profile.bandMode;
  modemConfig.apn = profile.apn;

  uint32_t startMs = millis();
  // Limitar el tiempo máximo interno de startLTE para este perfil.
  g_lte_max_wait_ms = allowedMs;

  bool ok = startLTE();

  elapsedMs = millis() - startMs;
  // Restaurar presupuesto global para siguientes llamadas fuera del modo multi-operador.
  g_lte_max_wait_ms = LTE_CONNECT_BUDGET_MS;

  logMessage(ok ? 2 : 1,
             String("[FIX-4] Resultado ") + profile.name +
             ": " + (ok ? "OK" : "FAIL") +
             ", tiempo=" + String(elapsedMs) + "ms");

  return ok;
}

static bool startLTE_multiOperator() {
  if (!ensureCommunicationBudget("startLTE_multiOperator")) {
    return false;
  }

  if (OP_PROFILES_COUNT == 0) {
    logMessage(1, "[FIX-4] Sin perfiles definidos, usando startLTE estándar");
    return startLTE();
  }

  uint8_t order[OP_PROFILES_COUNT];
  size_t count = 0;
  buildOperatorOrder(order, count);

  if (count == 0) {
    logMessage(1, "[FIX-4] No se pudo construir orden de operadores");
    return startLTE();
  }

  uint32_t remainingBudget = LTE_CONNECT_BUDGET_MS;
  logMessage(2, "[FIX-4] Presupuesto LTE total: " + String(remainingBudget) + "ms");

  for (size_t idx = 0; idx < count; ++idx) {
    if (!ensureCommunicationBudget("startLTE_multiOperator_loop")) {
      return false;
    }

    if (remainingBudget < 2000UL) {
      logMessage(1, "[FIX-4] Presupuesto LTE agotado antes de probar más perfiles");
      break;
    }

    const OperatorProfile& profile = OP_PROFILES[order[idx]];
    uint32_t allowed = profile.maxTimeMs;
    if (allowed > remainingBudget) {
      allowed = remainingBudget;
    }

    uint32_t elapsed = 0;
    bool ok = tryConnectOperator(profile, allowed, elapsed);

    if (ok) {
      g_last_success_operator_id = profile.id;
      g_operator_failures[profile.id] = 0;
      persistOperatorId(profile.id);
      return true;
    }

    if (g_operator_failures[profile.id] < 255) {
      g_operator_failures[profile.id]++;
    }

    if (elapsed >= remainingBudget) {
      remainingBudget = 0;
    } else {
      remainingBudget -= elapsed;
    }
  }

  logMessage(1, "[FIX-4] Ningún perfil logró conexión en el presupuesto disponible");
  return false;
}

/**
 * Verifica el estado actual del socket TCP
 * @return true si hay un socket abierto
 */
bool isSocketOpen() {
  logMessage(3, "🔍 Verificando estado del socket TCP");

  flushPortSerial();
  modem.sendAT("+CASTATE?");

  String response = "";
  bool receivedOk = false;
  unsigned long start = millis();
  const unsigned long timeoutMs = 3000;

  while (millis() - start < timeoutMs) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      if (c == 'K' && response.endsWith("OK\r\n")) {
        receivedOk = true;
      }
    }
    if (receivedOk) {
      break;
    }
    esp_task_wdt_reset();
    delay(1);
  }

  if (!receivedOk) {
    logMessage(1, "⚠️  Respuesta incompleta al consultar +CASTATE?, asumiendo socket inactivo");
  }

  bool socketActive = (response.indexOf("+CASTATE: 0,1") >= 0) ||
                      (response.indexOf("+CASTATE: 0,2") >= 0);

  if (socketActive) {
    logMessage(3, "🔍 Socket TCP detectado como activo");
  } else {
    logMessage(3, "🔍 Socket TCP detectado como inactivo");
  }

  return socketActive;
}

/**
 * Abre conexión TCP con validación de estado previa
 * @return true si la conexión se abre exitosamente
 */
bool tcpOpenSafe() {
  logMessage(2, "🔌 Abriendo conexión TCP con validación de estado");

  if (!ensureCommunicationBudget("tcpOpenSafe")) {
    return false;
  }
  
  // Verificar si hay socket abierto
  if (isSocketOpen()) {
    logMessage(2, "⚠️  Socket TCP ya existe, cerrando antes de abrir nuevo");
    if (!tcpClose()) {
      logMessage(0, "❌ No se pudo cerrar socket existente, reintentando");
      delay(500);
      if (!tcpClose()) {
        logMessage(0, "❌ Fallo al cerrar socket activo, abortando apertura segura");
        return false;
      }
    }
    delay(200);
    if (isSocketOpen()) {
      logMessage(0, "❌ Socket sigue reportado como activo tras cierre, abortando reapertura");
      return false;
    }
  }
  
  // Proceder con apertura normal
  return tcpOpen();
}

/**
 * Abre conexión TCP con reintentos adaptativos
 * @return true si la conexión se abre exitosamente
 */
bool tcpOpen() {
  logMessage(2, "🔌 Abriendo conexión TCP a " + modemConfig.serverIP + ":" + modemConfig.serverPort);

  int maxAttempts = modemConfig.maxRetries;
  unsigned long timeout = getAdaptiveTimeout();

  for (int intento = 0; intento < maxAttempts; intento++) {
    if (!ensureCommunicationBudget("tcpOpen_attempt")) {
      return false;
    }

    logMessage(3, "🔄 Intento " + String(intento + 1) + " de " + String(maxAttempts));

    String openCommand = "+CAOPEN=0,0,\"TCP\",\"" + modemConfig.serverIP + "\"," + modemConfig.serverPort;

    if (sendATCommand(openCommand, "+CAOPEN: 0,0", timeout)) {
      logMessage(2, "✅ Socket TCP abierto exitosamente");
      modemHealthMarkOk(); // 🆕 FIX-8: progreso real en TCP
      return true;
    }

    // Registrar timeout crítico para FIX-8
    modemHealthRegisterTimeout("tcpOpen_CAOPEN");
    if (modemHealthShouldAbortCycle()) {
      // Evitar loops de reapertura inútiles
      return false;
    }

    // Cerrar socket por si quedó en estado inconsistente
    sendATCommand("+CACLOSE=0", "OK", 5000);
    logMessage(3, "🔄 Reintentando apertura de socket...");
    delay(1000);
  }

  logMessage(0, "❌ No se pudo abrir socket TCP después de " + String(maxAttempts) + " intentos");
  return false;
}

/**
 * Cierra la conexión TCP
 * @return true si se cierra exitosamente
 */
bool tcpClose() {
  logMessage(3, "🔌 Cerrando conexión TCP");
  bool result = sendATCommand("+CACLOSE=0", "OK", getAdaptiveTimeout());

  if (result) {
    logMessage(3, "✅ Conexión TCP cerrada");
  } else {
    logMessage(1, "⚠️  Fallo cerrando conexión TCP");
  }

  return result;
}

/**
 * Lee datos de la conexión TCP
 * @return true si la lectura es exitosa
 */
bool tcpReadData() {
  logMessage(3, "📖 Leyendo datos TCP");
  return sendATCommand("+CARECV=0,10", "+CARECV: 1", getAdaptiveTimeout());
}

/**
 * Limpia todos los buffers de comunicación serial
 */
void flushPortSerial() {
  int bytesCleared = 0;
  while (SerialAT.available()) {
    char c = SerialAT.read();
    bytesCleared++;
  }

  if (bytesCleared > 0 && modemConfig.enableDebug) {
    logMessage(3, "🧹 Limpiados " + String(bytesCleared) + " bytes del buffer serial");
  }
}

/**
 * Lee respuesta del módem con timeout adaptativo
 * @param timeout - Timeout base en milisegundos
 * @return Respuesta del módem como String
 */
String readResponse(unsigned long timeout) {
  unsigned long start = millis();
  String response = "";
  unsigned long adaptiveTimeout = getAdaptiveTimeout();

  // Usar el timeout más largo entre el proporcionado y el adaptativo
  unsigned long finalTimeout = (timeout > adaptiveTimeout) ? timeout : adaptiveTimeout;

  flushPortSerial();

  unsigned long lastFeed = millis();

  while (millis() - start < finalTimeout) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      #if ENABLE_WDT_DEFENSIVE_LOOPS
      if (millis() - lastFeed >= 150) {
        esp_task_wdt_reset();
        lastFeed = millis();
        delay(1);
      }
      #endif
    }

    esp_task_wdt_reset();       // 🆕 FIX-006: Alimentar watchdog durante esperas largas
    delay(1);                   // 🆕 FIX-006: Ceder CPU para evitar busy-wait
  }

  if (modemConfig.enableDebug) {
    logMessage(3, "📥 Respuesta recibida (" + String(response.length()) + " bytes): " + response);
  }

  return response;
}

/**
 * Envía comando AT y espera respuesta específica
 * @param command - Comando AT a enviar
 * @param expectedResponse - Respuesta esperada
 * @param timeout - Timeout en milisegundos
 * @return true si se recibe la respuesta esperada
 */
bool sendATCommand(const String& command, const String& expectedResponse, unsigned long timeout) {
  logMessage(3, "📤 Enviando comando AT: " + command);
  logMessage(3, "⏱️  Usando timeout especificado: " + String(timeout) + "ms");

  String response = "";
  unsigned long start = millis();
  
  // Usar timeout específico proporcionado, no adaptativo para comandos críticos
  unsigned long finalTimeout = timeout;
  
  // Solo usar timeout adaptativo si no se especifica uno específico (timeout <= 1000)
  if (timeout <= 1000) {
    finalTimeout = getAdaptiveTimeout();
    logMessage(3, "⏱️  Usando timeout adaptativo: " + String(finalTimeout) + "ms");
  }

  flushPortSerial();

  unsigned long lastFeed = millis();

  modem.sendAT(command);

  while (millis() - start < finalTimeout) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      if (modemConfig.enableDebug) {
        Serial.print(c);
      }
      #if ENABLE_WDT_DEFENSIVE_LOOPS
      if (millis() - lastFeed >= 150) {
        esp_task_wdt_reset();
        lastFeed = millis();
        delay(1);
      }
      #endif
    }

    esp_task_wdt_reset();       // 🆕 FIX-006: Alimentar watchdog durante comandos AT prolongados
    delay(1);                   // 🆕 FIX-006: Evitar bloqueo del scheduler
  }

  if (response.indexOf(expectedResponse) != -1) {
    logMessage(3, "✅ Comando AT exitoso: " + command);
    return true;
  }

  logMessage(1, "⚠️  Comando AT falló: " + command + " (esperaba: " + expectedResponse + ")");
  return false;
}

/**
 * Busca texto específico en una cadena principal
 * @param textoPrincipal - Texto donde buscar
 * @param textoBuscado - Texto a buscar
 * @return true si se encuentra el texto
 */
bool findText(String textoPrincipal, String textoBuscado) {
  int posicion = textoPrincipal.indexOf(textoBuscado);
  return (posicion >= 0);
}

/**
 * Configura y obtiene datos GPS del módem integrado
 * @param data - Estructura de datos donde almacenar coordenadas GPS
 */
void setupGpsSim(sensordata_type* data) {
  logMessage(2, "🛰️  Configurando GPS integrado del módem");
  updateCheckpoint(CP_GPS_START); // 🆕 FIX-004: Iniciando GPS
  esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed al inicio de GPS

  SerialMon.begin(115200);
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  if (startGps()) {
    esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed después de startGps
    if (getGpsSim()) {
      updateCheckpoint(CP_GPS_FIX); // 🆕 FIX-004: GPS fix obtenido
      // Almacenar coordenadas en la estructura de datos
      data->lat0 = latConverter.b[0];
      data->lat1 = latConverter.b[1];
      data->lat2 = latConverter.b[2];
      data->lat3 = latConverter.b[3];

      data->lon0 = lonConverter.b[0];
      data->lon1 = lonConverter.b[1];
      data->lon2 = lonConverter.b[2];
      data->lon3 = lonConverter.b[3];

      data->alt0 = altConverter.b[0];
      data->alt1 = altConverter.b[1];
      data->alt2 = altConverter.b[2];
      data->alt3 = altConverter.b[3];

      logMessage(2, "✅ Coordenadas GPS obtenidas y almacenadas");
    } else {
      logMessage(1, "⚠️  No se pudieron obtener coordenadas GPS");
    }
  } else {
    logMessage(1, "⚠️  Fallo iniciando GPS del módem");
  }

  // Detener GPS para ahorrar energía
  if (stopGps()) {
    logMessage(2, "🔋 GPS detenido para ahorrar energía");
  } else {
    logMessage(1, "⚠️  Error al detener GPS, continuando...");
  }

  // Apagar módem completamente
  logMessage(2, "🔌 Apagando módem para máximo ahorro de energía");
  
  // Desactivar funciones del módem
  if (sendATCommand("+CFUN=0", "OK", 5000)) {
    logMessage(2, "✅ Funciones del módem desactivadas");
  } else {
    logMessage(1, "⚠️  Error al desactivar funciones del módem");
  }
  
  delay(1000);
  
  // Apagar módem usando PWRKEY
  modemPwrKeyPulse();
  
  // Actualizar estado del módem
  modemInitialized = false;
  
  logMessage(2, "🔋 Módem apagado completamente - máximo ahorro de energía");
}

/**
 * Inicia el módulo GPS integrado
 * @return true si el GPS se inicia exitosamente
 */
bool startGps() {
  logMessage(2, "🛰️  Iniciando módulo GPS integrado");

  int retry = 0;
  const int maxRetries = 3;
  const int maxTotalAttempts = 10; // 🆕 JAMR_3 FIX-001: Límite total de intentos
  int totalAttempts = 0; // 🆕 JAMR_3 FIX-001: Contador de intentos totales

  while (!modem.testAT(1000)) {
    esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed en cada iteración del while
    flushPortSerial();
    logMessage(3, "🔄 Esperando respuesta del módem...");

    if (retry++ > maxRetries) {
      modemPwrKeyPulse();
      retry = 0;
      logMessage(2, "🔄 Reintentando inicio del módem");
    }
    
    // 🆕 JAMR_3 FIX-001: Salir si excede intentos totales
    if (++totalAttempts > maxTotalAttempts) {
      logMessage(0, "❌ ERROR: Módem no responde después de múltiples intentos");
      return false;
    }
  }

  logMessage(2, "✅ Módem iniciado correctamente");

  // Configurar modo de funcionamiento
  modem.sendAT("+CFUN=0");
  modem.waitResponse();
  
  // 🆕 JAMR4 FIX-1: Esperar estabilización del módem (fragmentado para watchdog)
  for (int i = 0; i < 6; i++) {
    delay(500);
    esp_task_wdt_reset();
  }

  // Deshabilitar GPS previo
  modem.disableGPS();
  delay(500);

  // Configurar modo GPS
  modem.sendAT("+CGNSMOD=1,0,1,0,0");
  modem.waitResponse();

  if (modem.enableGPS()) {
    gpsEnabled = true;
    logMessage(2, "✅ GPS habilitado correctamente");
    return true;
  }

  logMessage(0, "❌ Fallo habilitando GPS");
  return false;
}

/**
 * Obtiene coordenadas GPS con reintentos adaptativos
 * @return true si se obtienen coordenadas válidas
 */
bool getGpsSim() {
  logMessage(2, "🛰️  Obteniendo coordenadas GPS del módem SIM...");

  // Configurar GPS del módem
  if (!sendATCommand("+CGNSPWR=1", "OK", 5000)) {
    logMessage(1, "⚠️  Fallo al encender GPS del módem");
    return false;
  }

  delay(1000);  // Esperar que el GPS se estabilice

  // Obtener coordenadas GPS con timeouts defensivos (FIX-3)
  const unsigned long GPS_SINGLE_ATTEMPT_TIMEOUT_MS = 5000;
  const unsigned long GPS_TOTAL_TIMEOUT_MS = 80000;
  unsigned long gpsGlobalStart = millis();

  for (int i = 0; i < 50; ++i) {
    #if ENABLE_WDT_DEFENSIVE_LOOPS
    if (millis() - gpsGlobalStart > GPS_TOTAL_TIMEOUT_MS) {
      logMessage(1, "[FIX-3] ⏱️ Timeout total de GPS tras " + String(i) + " intentos");
      break;
    }
    #endif

    esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed en cada intento de GPS

    unsigned long attemptStart = millis();

    // Obtener datos GPS del módem SIM con timeout por intento
    while (millis() - attemptStart < GPS_SINGLE_ATTEMPT_TIMEOUT_MS) {
      if (modem.getGPS(&latConverter.f,  // float* lat
                       &lonConverter.f,  // float* lon
                       &gps_speed,       // float* speed
                       &altConverter.f,  // float* alt
                       &gps_vsat,        // int* vsat
                       &gps_usat,        // int* usat
                       &gps_accuracy,    // float* accuracy
                       &gps_year,        // int* year
                       &gps_month,       // int* month
                       &gps_day,         // int* day
                       &gps_hour,        // int* hour
                       &gps_minute,      // int* minute
                       &gps_second)) {

      logMessage(2, "✅ Coordenadas GPS obtenidas en " + String(i + 1) + " intentos");

      // Log detallado de coordenadas
      if (modemConfig.enableDebug) {
        logMessage(3, "📍 Latitud: " + String(latConverter.f, 6));
        logMessage(3, "📍 Longitud: " + String(lonConverter.f, 6));
        logMessage(3, "📍 Altitud: " + String(altConverter.f, 2) + "m");
        logMessage(3, "📍 Fecha: " + String(gps_year) + "/" + String(gps_month) + "/" + String(gps_day));
        logMessage(3, "📍 Hora: " + String(gps_hour) + ":" + String(gps_minute) + ":" + String(gps_second));
      }

        return true;  // Sale inmediatamente si obtiene datos válidos
      }

      #if ENABLE_WDT_DEFENSIVE_LOOPS
      esp_task_wdt_reset();
      delay(200);
      #else
      delay(200);
      #endif
    }

    logMessage(2, "🔄 Intento " + String(i + 1) + " de 50 para obtener GPS (sin fix, reintentando)");
  }

  logMessage(1, "❌ No se pudieron obtener coordenadas GPS después de 50 intentos");
  return false;
}

/**
 * Detiene el módulo GPS integrado
 * @return true si el GPS se detiene exitosamente
 */
bool stopGps() {
  logMessage(2, "🛰️  Deteniendo módulo GPS integrado");

  // Apagar GPS del módem usando comando AT
  if (!sendATCommand("+CGNSPWR=0", "OK", 5000)) {
    logMessage(1, "⚠️  Fallo al apagar GPS del módem usando comando AT");
  }

  // Deshabilitar GPS usando función de TinyGSM
  if (modem.disableGPS()) {
    gpsEnabled = false;
    logMessage(2, "✅ GPS deshabilitado correctamente");
    
    // Limpiar variables GPS
    gps_latitude = 0;
    gps_longitude = 0;
    gps_speed = 0;
    gps_altitude = 0;
    gps_vsat = 0;
    gps_usat = 0;
    gps_accuracy = 0;
    gps_year = 0;
    gps_month = 0;
    gps_day = 0;
    gps_hour = 0;
    gps_minute = 0;
    gps_second = 0;
    
    // Limpiar convertidores
    latConverter.f = 0;
    lonConverter.f = 0;
    altConverter.f = 0;
    
    logMessage(3, "🧹 Variables GPS limpiadas");
    return true;
  }

  logMessage(0, "❌ Fallo deshabilitando GPS");
  gpsEnabled = false; // Marcar como deshabilitado aunque haya fallado
  return false;
}

/**
 * Obtiene información de la tarjeta SIM y calidad de señal
 */
void getIccid() {
  logMessage(2, "📱 Obteniendo información de la tarjeta SIM");

  flushPortSerial();

  for (int i = 0; i < 5; i++) {
    iccidsim0 = modem.getSimCCID();
    signalsim0 = modem.getSignalQuality();
    delay(1000);
  }

  logMessage(2, "📱 ICCID: " + iccidsim0);
  logMessage(2, "📶 Calidad de señal: " + String(signalsim0));

  // Evaluar calidad de señal
  if (signalsim0 >= 20) {
    logMessage(2, "✅ Señal excelente");
  } else if (signalsim0 >= 15) {
    logMessage(2, "✅ Señal buena");
  } else if (signalsim0 >= 10) {
    logMessage(1, "⚠️  Señal regular");
  } else {
    logMessage(0, "❌ Señal débil - problemas de conectividad esperados");
  }
}

/**
 * Prepara y encripta los datos para envío
 * @param data - Estructura de datos de sensores
 * @return String con datos encriptados
 */
String dataSend(sensordata_type* data) {
  logMessage(2, "📦 Preparando datos para envío");

  // Obtener información de la SIM
  Serial.println("📱 ICCID: " + iccidsim0);
  Serial.println("📶 Calidad de señal: " + String(signalsim0));
  data->H_rsi = (byte)signalsim0;

  // Convertir ICCID a formato hexadecimal
  logMessage(3, "🔄 Convirtiendo ICCID a formato hexadecimal");
  const int longitudIccidsim0 = iccidsim0.length();
  char iccidHex[longitudIccidsim0];

  for (int i = 0; i < longitudIccidsim0; i++) {
    iccidHex[i] = iccidsim0.charAt(i);
  }

  if (modemConfig.enableDebug) {
    Serial.println("------ICCID hex------");
    for (int i = 0; i < longitudIccidsim0; i++) {
      Serial.print("0x");
      if (iccidHex[i] < 16) Serial.print("0");
      Serial.print(iccidHex[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    Serial.println("Longitud ICCID: " + longitudIccidsim0);
  }

  // Preparar datos de sensores
  logMessage(3, "🔄 Preparando datos de sensores");
  
  // 🆕 FIX-004: Tamaño fijo de 46 bytes (40 sensores + 6 health data)
  // No usamos sizeof() debido a problemas de padding del compilador
  const int STRUCT_SIZE = 46;
  const int SIZEOF_CHECK = sizeof(sensordata_type);
  
  // Validación de seguridad
  if (modemConfig.enableDebug) {
    Serial.print("sizeof(sensordata_type) = ");
    Serial.println(SIZEOF_CHECK);
    Serial.print("STRUCT_SIZE usado = ");
    Serial.println(STRUCT_SIZE);
    if (SIZEOF_CHECK != STRUCT_SIZE) {
      Serial.println("⚠️  ADVERTENCIA: sizeof discrepancia - usando 46 bytes fijos");
    }
  }
  
  char dataBufferSensor[STRUCT_SIZE];
  memcpy(dataBufferSensor, data, STRUCT_SIZE);

  if (modemConfig.enableDebug) {
    Serial.println("------Sensor data hex------");
    for (int i = 0; i < STRUCT_SIZE; i++) {
      Serial.print("0x");
      if (dataBufferSensor[i] < 16) Serial.print("0");
      Serial.print(dataBufferSensor[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    Serial.print("Tamaño estructura: ");
    Serial.println(STRUCT_SIZE);
  }

  // Construir cadena completa para envío
  logMessage(3, "🔄 Construyendo cadena de envío");
  int longcadena = longitudIccidsim0 + STRUCT_SIZE + 2;
  char cadena[longcadena];

  memcpy(cadena, iccidHex, sizeof(iccidHex));
  memcpy(cadena + sizeof(iccidHex), dataBufferSensor, STRUCT_SIZE);

  // Agregar CRC16 para verificación de integridad
  size_t len = longcadena - 2;
  len = append_crc16_to_char_array(cadena, len, longcadena);

  if (modemConfig.enableDebug) {
    Serial.println("------Cadena envío hex------");
    for (int i = 0; i < longcadena; i++) {
      Serial.print("0x");
      if (cadena[i] < 16) Serial.print("0");
      Serial.print(cadena[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    Serial.println("Longitud final: " + longcadena);
  }

  // Encriptar datos
  logMessage(2, "🔐 Encriptando datos con AES");
  String cadenaEncriptada = encryptChar(cadena, longcadena);

  logMessage(2, "✅ Datos preparados: " + String(cadenaEncriptada.length()) + " bytes encriptados");
  return cadenaEncriptada;
}

/**
 * Inicia la comunicación GSM básica
 */
void startGsm() {
  logMessage(2, "📱 Iniciando comunicación GSM");

  int retry = 0;
  const int maxRetries = 3;
  const int maxTotalAttempts = 15; // 🆕 FIX-003: Límite total de intentos
  int totalAttempts = 0;           // 🆕 FIX-003: Contador de intentos totales

  while (!modem.testAT(1000)) {
    esp_task_wdt_reset(); // 🆕 FIX-003: Feed watchdog en cada iteración
    flushPortSerial();
    logMessage(3, "🔄 Esperando respuesta del módem...");

    if (retry++ > maxRetries) {
      modemPwrKeyPulse();
      sendATCommand("+CPIN?", "READY", 15000);
      retry = 0;
      logMessage(2, "🔄 Reintentando inicio del módem");
    }
    
    // 🆕 FIX-003: Salir si excede intentos totales
    if (++totalAttempts > maxTotalAttempts) {
      logMessage(0, "❌ ERROR: Módem no responde después de 15 intentos");
      return;
    }
  }

  logMessage(2, "✅ Comunicación GSM establecida");
  
  // Encender RF (Radio Frecuencia) del módem
  logMessage(2, "📡 Encendiendo RF del módem");
  
  // Activar funcionalidad completa del módem (RF ON)
  if (sendATCommand("+CFUN=1", "OK", 10000)) {
    esp_task_wdt_reset(); // 🆕 FIX-003: Feed después de comando largo
    logMessage(2, "✅ RF del módem activada correctamente");
  } else {
    logMessage(1, "⚠️  Error al activar RF, reintentando...");
    
    // Reintentar con comando alternativo
    if (sendATCommand("+CFUN=1,1", "OK", 10000)) {
      esp_task_wdt_reset(); // 🆕 FIX-003: Feed después de comando largo
      logMessage(2, "✅ RF del módem activada con reinicio");
    } else {
      logMessage(0, "❌ Fallo crítico al activar RF del módem");
    }
  }
  
  // 🆕 JAMR4 FIX-1: Esperar estabilización de la RF (fragmentado para watchdog)
  for (int i = 0; i < 4; i++) {
    delay(500);
    esp_task_wdt_reset();
  }
  
  // Verificar estado de la RF
  if (sendATCommand("+CFUN?", "+CFUN: 1", 5000)) {
    logMessage(2, "✅ RF verificada - módem completamente funcional");
  } else {
    logMessage(1, "⚠️  No se pudo verificar estado de RF");
  }
}

/**
 * Guarda datos en el buffer local con gestión inteligente
 * @param data - Datos a guardar
 */
void guardarDato(String data) {
  logMessage(2, "💾 Guardando dato en buffer local");

  std::vector<String> lineas;

  // Leer líneas existentes
  if (LittleFS.exists(ARCHIVO_BUFFER)) {
    File f = LittleFS.open(ARCHIVO_BUFFER, "r");
    while (f.available()) {
      String linea = f.readStringUntil('\n');
      linea.trim();
      if (linea.length() > 0) lineas.push_back(linea);
    }
    f.close();
  }

  // Contar datos no enviados
  int noEnviadas = 0;
  for (String l : lineas) {
    if (!l.startsWith("#ENVIADO")) noEnviadas++;
  }

  // Gestión inteligente del buffer
  if (noEnviadas >= MAX_LINEAS) {
    logMessage(1, "⚠️  Buffer lleno, eliminando dato más antiguo");

    // Eliminar el dato más antiguo NO enviado
    for (size_t i = 0; i < lineas.size(); i++) {
      if (!lineas[i].startsWith("#ENVIADO")) {
        lineas.erase(lineas.begin() + i);
        logMessage(3, "🗑️  Dato antiguo eliminado del buffer");
        break;
      }
    }
  }

  // Agregar nuevo dato
  lineas.push_back(data);

  // Reescribir archivo
  File f = LittleFS.open(ARCHIVO_BUFFER, "w");
  for (String l : lineas) f.println(l);
  f.close();

  logMessage(2, "✅ Dato guardado en buffer. Total en buffer: " + String(lineas.size()) + " líneas");
}

/**
 * Envía datos pendientes del buffer con gestión de errores mejorada
 */
void enviarDatos() {
  logMessage(2, "📤 Iniciando envío de datos del buffer");

  if (!ensureCommunicationBudget("enviarDatos_begin")) {
    return;
  }

  if (!LittleFS.exists(ARCHIVO_BUFFER)) {
    logMessage(2, "ℹ️  No hay archivo de buffer para enviar");
    return;
  }

  if (!tcpOpenSafe()) {
    logMessage(0, "❌ No se pudo abrir conexión TCP para envío");
    return;
  }
  
  updateCheckpoint(CP_TCP_OPEN); // 🆕 FIX-004: Socket TCP abierto

  File fin = LittleFS.open(ARCHIVO_BUFFER, "r");
  if (!fin) {
    logMessage(0, "❌ No se pudo abrir archivo de buffer para lectura");
    tcpClose();
    return;
  }

  fin.setTimeout(50);  // Evitar bloqueos largos

  // Archivo temporal para operación atómica
  const char* TMP = "/buffer.tmp";
  File fout = LittleFS.open(TMP, "w");
  if (!fout) {
    logMessage(0, "❌ No se pudo crear archivo temporal");
    fin.close();
    tcpClose();
    return;
  }

  bool conexion_ok = true;
  int datosEnviados = 0;
  int datosFallidos = 0;

  while (fin.available()) {
    if (!ensureCommunicationBudget("enviarDatos_loop")) {
      logMessage(1, "[FIX-6] Ciclo de envío detenido por presupuesto global agotado");
      break;
    }

    String linea = fin.readStringUntil('\n');
    linea.trim();
    if (linea.length() == 0) continue;  // Saltar líneas vacías

    if (linea.startsWith("#ENVIADO")) {
      fout.println(linea);  // Conservar marca de enviado
      continue;
    }

    if (conexion_ok) {
      unsigned long timeout = getAdaptiveTimeout();
      if (tcpSendData(linea, timeout)) {
        logMessage(2, "✅ Enviado: " + linea.substring(0, 50) + "...");
        fout.println("#ENVIADO " + linea);  // Marcar como enviado
        datosEnviados++;
      } else {
        logMessage(1, "❌ Falló envío: " + linea.substring(0, 50) + "...");
        fout.println(linea);  // Conservar sin marcar
        conexion_ok = false;  // No intentar más en esta pasada
        datosFallidos++;
      }
    } else {
      // Conexión caída: no seguir intentando
      fout.println(linea);
    }
  }

  fin.close();
  fout.close();
  tcpClose();

  // Reemplazo atómico del archivo original
  LittleFS.remove(ARCHIVO_BUFFER);
  LittleFS.rename(TMP, ARCHIVO_BUFFER);

  logMessage(2, "📊 Resumen de envío: " + String(datosEnviados) + " enviados, " + String(datosFallidos) + " fallidos");
}

/**
 * Limpia el buffer eliminando datos ya enviados
 */
void limpiarEnviados() {
  logMessage(2, "🧹 Limpiando buffer de datos enviados");

  std::vector<String> lineas;
  File f = LittleFS.open(ARCHIVO_BUFFER, "r");

  while (f.available()) {
    String l = f.readStringUntil('\n');
    l.trim();
    if (!l.startsWith("#ENVIADO")) {
      lineas.push_back(l);
    }
  }
  f.close();

  // Reescribir archivo solo con datos pendientes
  f = LittleFS.open(ARCHIVO_BUFFER, "w");
  for (String l : lineas) f.println(l);
  f.close();

  logMessage(2, "✅ Buffer limpio. Datos pendientes: " + String(lineas.size()) + " líneas");
}

/**
 * Espera un token específico en el stream con timeout
 * @param s - Stream a monitorear
 * @param token - Token a buscar
 * @param timeout_ms - Timeout en milisegundos
 * @return true si se encuentra el token
 */
static bool waitForToken(Stream& s, const String& token, uint32_t timeout_ms) {
  uint32_t start = millis();
  String buf;
  buf.reserve(256);

  while (millis() - start < timeout_ms) {
    while (s.available()) {
      char c = s.read();
      buf += c;

      // Limitar tamaño del buffer para evitar crecimiento descontrolado
      if (buf.length() > 512) buf.remove(0, buf.length() - 256);

      if (buf.indexOf(token) >= 0) return true;
    }

    esp_task_wdt_reset();       // 🆕 FIX-006: Alimentar watchdog mientras esperamos el token
    delay(1);                   // 🆕 FIX-006: Ceder CPU / evitar busy-wait
  }

  return false;  // Timeout
}

/**
 * Espera cualquiera de varios tokens con timeout
 * @param s - Stream a monitorear
 * @param okTokens - Array de tokens de éxito
 * @param okCount - Cantidad de tokens de éxito
 * @param errTokens - Array de tokens de error
 * @param errCount - Cantidad de tokens de error
 * @param timeout_ms - Timeout en milisegundos
 * @return 1=OK, -1=Error, 0=Timeout
 */
static int8_t waitForAnyToken(Stream& s,
                              const char* okTokens[], size_t okCount,
                              const char* errTokens[], size_t errCount,
                              uint32_t timeout_ms) {
  uint32_t start = millis();
  String buf;
  buf.reserve(512);

  while (millis() - start < timeout_ms) {
    while (s.available()) {
      char c = s.read();
      buf += c;

      if (buf.length() > 1024) buf.remove(0, buf.length() - 512);

      // Verificar tokens de error
      for (size_t i = 0; i < errCount; ++i) {
        if (buf.indexOf(errTokens[i]) >= 0) return -1;  // Error
      }

      // Verificar tokens de éxito
      for (size_t i = 0; i < okCount; ++i) {
        if (buf.indexOf(okTokens[i]) >= 0) return 1;  // OK
      }
    }

    esp_task_wdt_reset();       // 🆕 FIX-006: Mantener vivo el watchdog mientras esperamos respuesta
    delay(1);
  }

  return 0;  // Timeout
}

/**
 * Envía datos TCP con gestión robusta de errores
 * @param datos - Datos a enviar
 * @param timeout_ms - Timeout en milisegundos
 * @return true si el envío es exitoso
 */
bool tcpSendData(const String& datos, uint32_t timeout_ms) {
  if (!ensureCommunicationBudget("tcpSendData")) {
    return false;
  }

  logMessage(3, "📤 Enviando " + String(datos.length()) + " bytes por TCP");

  // Limpiar buffers de entrada
  flushPortSerial();
  while (SerialAT.available()) SerialAT.read();

  // Calcular longitud total (+2 por CRLF)
  const size_t len = datos.length() + 2;

  // Solicitar prompt para envío
  modem.sendAT(String("+CASEND=0,") + String(len));
  if (!waitForToken(SerialAT, ">", timeout_ms)) {
    logMessage(0, "❌ Timeout esperando prompt '>' para envío");
    return false;
  }

  // Enviar payload + CRLF
  modem.sendAT(datos);
  modem.sendAT("\r\n");

  // Esperar confirmación del módem
  const char* okTokens[] = { "CADATAIND: 0", "SEND OK", "OK" };
  const char* errTokens[] = { "SEND FAIL", "ERROR", "+CME ERROR", "+CMS ERROR" };

  int8_t result = waitForAnyToken(SerialAT,
                                  okTokens, sizeof(okTokens) / sizeof(okTokens[0]),
                                  errTokens, sizeof(errTokens) / sizeof(errTokens[0]),
                                  timeout_ms);

  if (result == 1) {
    logMessage(3, "✅ Datos TCP enviados exitosamente");
    return true;
  }

  if (result == -1) {
    logMessage(0, "❌ Error en envío TCP");
    return false;
  }

  logMessage(0, "❌ Timeout en envío TCP");
  return false;
}

/**
 * Inicializa el sistema de archivos LittleFS con reintentos
 * @param intentos - Número máximo de intentos
 * @param espera_ms - Tiempo de espera entre intentos
 * @return true si se monta exitosamente
 */
bool iniciarLittleFS(int intentos, uint32_t espera_ms) {
  logMessage(2, "💾 Iniciando sistema de archivos LittleFS");

  for (int i = 0; i < intentos; i++) {
    logMessage(3, "🔄 Intentando montar LittleFS (intento " + String(i + 1) + " de " + String(intentos) + ")");

    if (LittleFS.begin(true)) {
      logMessage(2, "✅ LittleFS montado correctamente");

      // Verificar espacio disponible
      size_t totalBytes = LittleFS.totalBytes();
      size_t usedBytes = LittleFS.usedBytes();
      size_t freeBytes = totalBytes - usedBytes;

      logMessage(2, "💾 Espacio total: " + String(totalBytes) + " bytes");
      logMessage(2, "💾 Espacio usado: " + String(usedBytes) + " bytes");
      logMessage(2, "💾 Espacio libre: " + String(freeBytes) + " bytes");

      return true;
    }

    logMessage(1, "⚠️  Falló el montaje, esperando " + String(espera_ms) + " ms antes de reintentar...");
    delay(espera_ms);
  }

  logMessage(0, "💥 No se pudo montar LittleFS después de " + String(intentos) + " intentos");
  return false;
}

/**
 * Calcula CRC16 para verificación de integridad de datos
 * @param data - Puntero a los datos
 * @param len - Longitud de los datos
 * @return Valor CRC16 calculado
 */
uint16_t crc16(const char* data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= (uint8_t)data[i];  // Cast a uint8_t para tratarlo como byte

    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;  // Low byte primero al enviar
}

/**
 * Agrega CRC16 al final de un array de caracteres
 * @param buf - Buffer donde agregar el CRC
 * @param len - Longitud actual del buffer
 * @param cap - Capacidad total del buffer
 * @return Nueva longitud del buffer
 */
size_t append_crc16_to_char_array(char* buf, size_t len, size_t cap) {
  if (len + 2 > cap) return len;

  uint16_t crc = crc16(buf, len);
  buf[len++] = (char)(crc & 0xFF);         // Low byte
  buf[len++] = (char)((crc >> 8) & 0xFF);  // High byte

  return len;
}