/**
 * @file gsmlte.cpp
 * @brief Sistema de comunicación LTE/GSM con selección automática de operadores y envío de datos
 * 
 * Este módulo implementa un sistema robusto de comunicación LTE/GSM que incluye:
 * - Selección secuencial de operadores (Altan Redes, AT&T, Movistar, Telcel)
 * - Conexión automática y envío de datos con fallback entre operadores
 * - Gestión inteligente de buffer local para datos pendientes
 * - Configuración GPS integrada del módem
 * - Encriptación AES de datos sensibles
 * - Timeouts adaptativos basados en calidad de señal
 * - Logging detallado para debugging y monitoreo
 * 
 * @author Elathia
 * @version 2.0
 * @date 2025-10-30
 * @since 1.0
 * 
 * @see gsmlte.h
 * @see cryptoaes.h
 * @see sleepdev.h
 */

#include "gsmlte.h"
#include "sleepdev.h"
#include <TinyGsmClient.h>
#include <LittleFS.h>
#include <vector>
#include "cryptoaes.h"

/**
 * @defgroup GSM_LTE_Core Núcleo GSM/LTE
 * @brief Funciones y variables principales del sistema GSM/LTE
 * @{
 */

/** @brief Instancia global del módem GSM/LTE */
TinyGsm modem(SerialAT);

/** @brief Configuración global del módem */
ModemConfig modemConfig;

/**
 * @defgroup System_State Variables de Estado del Sistema
 * @brief Variables que mantienen el estado actual del sistema
 * @{
 */

/** @brief Indica si el módem ha sido inicializado correctamente */
bool modemInitialized = false;

/** @brief Indica si el GPS está habilitado y funcionando */
bool gpsEnabled = false;



/** @brief Contador de fallos consecutivos de conexión */
int consecutiveFailures = 0;

/** @} */ // fin grupo System_State

/**
 * @defgroup Data_Storage Variables de Almacenamiento de Datos
 * @brief Variables relacionadas con el almacenamiento y gestión de datos
 * @{
 */

/** @brief ICCID de la tarjeta SIM actual */
String iccidsim0 = "";

/** @brief Calidad de señal actual (0-31, mayor es mejor) */
int signalsim0 = 0;

/** @brief Ruta del archivo de buffer local en LittleFS */
const char* ARCHIVO_BUFFER = "/buffer.txt";

/** @} */ // fin grupo Data_Storage

/**
 * @defgroup Operator_Management Gestión de Operadores
 * @brief Estructuras y datos para la gestión automática de operadores de red
 * @{
 */

/**
 * @struct OperatorInfo
 * @brief Información completa de un operador de red móvil
 * 
 * Estructura que almacena toda la información relevante de un operador
 * incluyendo métricas de calidad de señal y estado de conexión.
 */
struct OperatorInfo {
  String name;        /**< Nombre comercial del operador */
  String code;        /**< Código MCC+MNC del operador (ej: "334030") */
  int rsrp;          /**< Reference Signal Received Power en dBm (-140 a -44) */
  int rsrq;          /**< Reference Signal Received Quality en dB (-20 a -3) */
  int rssi;          /**< Received Signal Strength Indicator en dBm */
  int snr;           /**< Signal to Noise Ratio en dB (-20 a 30) */
  bool connected;    /**< Estado actual de conexión con el operador */
  int score;         /**< Puntuación calculada para ranking (mayor = mejor) */
};

/**
 * @brief Lista dinámica de operadores móviles disponibles
 * 
 * Array dinámico que se llena automáticamente con los operadores encontrados
 * en la respuesta del comando AT+COPS=?. Solo contiene operadores que están
 * realmente disponibles en la zona.
 * 
 * @note Los valores de métricas se actualizan dinámicamente durante la evaluación
 */
std::vector<OperatorInfo> operators;

/** @brief Número total de operadores disponibles dinámicamente */
int NUM_OPERATORS = 0;

/**
 * @brief Array de códigos de operadores prioritarios para México
 * 
 * Lista de operadores preferidos que se priorizarán si están disponibles
 * en la respuesta COPS. El orden define la prioridad de conexión.
 */
const char* PREFERRED_OPERATORS[] = {};

/** @brief Número de operadores preferidos */
const int NUM_PREFERRED = sizeof(PREFERRED_OPERATORS) / sizeof(PREFERRED_OPERATORS[0]);

/** @} */ // fin grupo Operator_Management

/**
 * @defgroup GPS_System Sistema GPS
 * @brief Variables y estructuras para el manejo de coordenadas GPS
 * @{
 */

/**
 * @union FloatToBytes
 * @brief Unión para conversión entre float y array de bytes
 * 
 * Permite la conversión eficiente de valores float a bytes individuales
 * para su transmisión en el protocolo de comunicación.
 */
union FloatToBytes {
  float f;        /**< Valor como número de punto flotante */
  uint8_t b[4];   /**< Valor como array de 4 bytes */
};

/** @brief Convertidor para latitud GPS */
FloatToBytes latConverter;

/** @brief Convertidor para longitud GPS */
FloatToBytes lonConverter;

/** @brief Convertidor para altitud GPS */
FloatToBytes altConverter;

/**
 * @defgroup GPS_Variables Variables GPS Actuales
 * @brief Variables que almacenan los datos GPS más recientes
 * @{
 */

/** @brief Latitud GPS actual en grados decimales */
float gps_latitude = 0;

/** @brief Longitud GPS actual en grados decimales */
float gps_longitude = 0;

/** @brief Velocidad GPS actual en km/h */
float gps_speed = 0;

/** @brief Altitud GPS actual en metros sobre el nivel del mar */
float gps_altitude = 0;

/** @brief Número de satélites visibles */
int gps_vsat = 0;

/** @brief Número de satélites utilizados para el fix */
int gps_usat = 0;

/** @brief Precisión horizontal estimada en metros */
float gps_accuracy = 0;

/** @brief Año actual del GPS */
int gps_year = 0;

/** @brief Mes actual del GPS (1-12) */
int gps_month = 0;

/** @brief Día actual del GPS (1-31) */
int gps_day = 0;

/** @brief Hora actual del GPS (0-23) */
int gps_hour = 0;

/** @brief Minuto actual del GPS (0-59) */
int gps_minute = 0;

/** @brief Segundo actual del GPS (0-59) */
int gps_second = 0;

/** @} */ // fin grupo GPS_Variables
/** @} */ // fin grupo GPS_System
/** @} */ // fin grupo GSM_LTE_Core

/**
 * @defgroup Configuration_Functions Funciones de Configuración
 * @brief Funciones para inicialización y configuración del sistema
 * @{
 */

/**
 * @brief Inicializa la configuración por defecto del módem
 * 
 * Establece todos los parámetros de configuración necesarios para el funcionamiento
 * del módem, incluyendo servidor, APN, timeouts y modos de operación.
 * 
 * **Configuración establecida:**
 * - 🌐 Servidor y puerto TCP de destino
 * - 📡 APN y configuración de red
 * - ⏱️ Timeouts adaptativos optimizados
 * - 🔍 Modo debug habilitado por defecto
 * - 🔄 Número máximo de reintentos
 * 
 * @note Esta función debe llamarse antes de cualquier operación con el módem
 * @note Los valores se toman de las constantes definidas en gsmlte.h
 * 
 * @see ModemConfig
 * @see getAdaptiveTimeout()
 */
void initModemConfig() {
  modemConfig.serverIP = DB_SERVER_IP;
  modemConfig.serverPort = TCP_PORT;
  modemConfig.apn = APN;
  modemConfig.networkMode = MODEM_NETWORK_MODE;
  modemConfig.bandMode = CAT_M;
  modemConfig.maxRetries = SEND_RETRIES;
  modemConfig.baseTimeout = 10000;
  modemConfig.enableDebug = true;
  modemConfig.showOperatorList = true; 
  
  logMessage(2, "🔧 Configuración del módem inicializada");
}

/**
 * @ingroup Configuration_Functions
 * @brief Calcula timeout adaptativo basado en calidad de señal y historial de fallos
 * 
 * Implementa un algoritmo inteligente que ajusta los timeouts dinámicamente:
 * 
 * **Factores de ajuste:**
 * - 📶 **Señal excelente** (>20): Reduce timeout 40% (más agresivo)
 * - 📶 **Señal débil** (<10): Aumenta timeout 20% (más conservador)
 * - ❌ **Fallos consecutivos**: +1000ms por cada fallo previo
 * - ⚠️ **Límites**: Mínimo 3s, máximo 20s
 * 
 * @return Timeout calculado en milisegundos (3000-20000ms)
 * 
 * @note Se basa en signalsim0 (calidad de señal actual)
 * @note Considera consecutiveFailures (historial de errores)
 * @note Optimiza rendimiento vs confiabilidad automáticamente
 * 
 * @see signalsim0
 * @see consecutiveFailures
 * @see modemConfig.baseTimeout
 */
unsigned long getAdaptiveTimeout() {
  unsigned long baseTimeout = modemConfig.baseTimeout;
  
  if (signalsim0 > 20) {
    baseTimeout = (unsigned long)(baseTimeout * 0.6);
  } else if (signalsim0 < 10) {
    baseTimeout = (unsigned long)(baseTimeout * 1.2);
  }
  
  if (consecutiveFailures > 0) {
    baseTimeout += (consecutiveFailures * 1000);
  }
  
  if (baseTimeout > 20000) baseTimeout = 20000;
  if (baseTimeout < 3000) baseTimeout = 3000;
  
  return baseTimeout;
}

/** @} */ // fin grupo Configuration_Functions

/**
 * @defgroup Utility_Functions Funciones de Utilidad
 * @brief Funciones auxiliares para logging, estadísticas y diagnóstico
 * @{
 */

/**
 * @brief Sistema de logging con niveles y timestamps automáticos
 * 
 * Proporciona un sistema de registro unificado con diferentes niveles de severidad
 * y control automático de verbosidad basado en la configuración de debug.
 * 
 * **Niveles de Logging:**
 * - 🔴 **0 - ERROR**: Errores críticos (siempre se muestran)
 * - 🟡 **1 - WARN**: Advertencias importantes (siempre se muestran)  
 * - 🔵 **2 - INFO**: Información general (siempre se muestran)
 * - 🟣 **3 - DEBUG**: Información detallada (solo si enableDebug=true)
 * 
 * @param level Nivel de severidad del mensaje (0-3)
 * @param message Texto del mensaje a registrar
 * 
 * @note Los mensajes nivel 3 solo se muestran si modemConfig.enableDebug = true
 * @note Incluye timestamp automático en milisegundos desde inicio
 * @note Usa emojis para identificación visual rápida de niveles
 * 
 * @see modemConfig.enableDebug
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

String getSystemStats() {
  String stats = "=== ESTADÍSTICAS DEL SISTEMA ===\n";
  stats += "Módem inicializado: " + String(modemInitialized ? "Sí" : "No") + "\n";
  stats += "GPS habilitado: " + String(gpsEnabled ? "Sí" : "No") + "\n";
  stats += "Calidad de señal: " + String(signalsim0) + "\n";
  stats += "Fallos consecutivos: " + String(consecutiveFailures) + "\n";
  stats += "Timeout adaptativo actual: " + String(getAdaptiveTimeout()) + "ms\n";

  return stats;
}

/**
 * @ingroup GSM_LTE_Core
 * @brief Función principal de configuración e inicialización del sistema GSM/LTE
 * 
 * Ejecuta la secuencia completa de inicialización y envío de datos:
 * 
 * **Secuencia de Inicialización:**
 * 1. 🔧 Inicializa configuración del módem (timeouts, APN, etc.)
 * 2. 📡 Configura comunicación serial con el módem
 * 3. 📱 Inicia GSM básico y obtiene información de la SIM
 * 4. 💾 Monta sistema de archivos LittleFS para buffer local
 * 5. 🔐 Prepara y encripta los datos de sensores
 * 6. 💾 Guarda datos en buffer local como respaldo
 * 7. 🌐 Ejecuta estrategia de conexión LTE y envío de datos
 * 8. 🔋 Apaga módem para conservar energía
 * 
 * @param data Puntero a estructura con datos de sensores a transmitir
 * 
 * @note Esta función maneja automáticamente todos los errores críticos
 * @note Reinicia el ESP32 si falla el montaje de LittleFS
 * @note Actualiza contadores de fallos consecutivos
 * @note Marca el módem como inicializado al finalizar
 * 
 * @see sensordata_type
 * @see startLTE()
 * @see dataSend()
 * @see iniciarLittleFS()
 * 
 * @warning Función bloqueante - puede tardar varios minutos en completarse
 */
void setupModem(sensordata_type* data) {
  logMessage(2, "🚀 Iniciando configuración del módem LTE/GSM");

  initModemConfig();

  SerialMon.begin(115200);
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  startGsm();
  getIccid();

  if (!iniciarLittleFS(3, 200)) {
    logMessage(0, "🚫 Abortando ejecución por error de LittleFS");
    ESP.restart();
  }

  String cadenaEncriptada = dataSend(data);
  logMessage(2, "📦 Datos preparados y encriptados: " + String(cadenaEncriptada.length()) + " bytes");
  guardarDato(cadenaEncriptada);

  if (startLTE()) {
    logMessage(2, "✅ Conexión LTE y envío de datos completado exitosamente");
  } else {
    consecutiveFailures++;
    logMessage(1, "⚠️  Fallo en conexión/envío LTE con todos los operadores (intento " + String(consecutiveFailures) + ")");
  }

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
 * @ingroup GSM_LTE_Core
 * @brief Inicia conexión LTE con estrategia de selección secuencial de operadores
 * 
 * Implementa la nueva estrategia de conexión que prueba operadores secuencialmente
 * hasta lograr tanto la conexión como el envío exitoso de datos:
 * 
 * **Proceso:**
 * 1. Configura parámetros básicos del módem (modo de red, bandas LTE)
 * 2. Prueba cada operador en orden de prioridad: Altan → AT&T → Movistar → Telcel
 * 3. Para cada operador: conecta Y verifica envío de datos
 * 4. Si todos fallan, intenta modo automático como último recurso
 * 
 * **Orden de Prioridad:**
 * - 🥇 Altan Redes (Red Compartida - mejor cobertura)
 * - 🥈 AT&T (Buena cobertura internacional)
 * - 🥉 Movistar (Cobertura urbana sólida) 
 * - 🏃 Telcel (Fallback final)
 * 
 * @retval true Conexión y envío exitoso con algún operador
 * @retval false Todos los operadores fallaron (incluyendo modo automático)
 * 
 * @note Actualiza consecutiveFailures en caso de fallo total
 * @note Llama a limpiarEnviados() automáticamente tras envío exitoso
 * 
 * @see connectAndSendWithOperator()
 * @see operators[]
 * @see modemConfig
 * 
 * @warning Requiere configuración previa de modemConfig y módem inicializado
 */
bool startLTE() {
  logMessage(2, "🌐 Iniciando conexión LTE con estrategia secuencial por operador");

  if (!sendConfigCommand("+CNMP=" + String(modemConfig.networkMode))) {
    logMessage(0, "❌ Fallo configurando modo de red");
    return false;
  }

  if (!sendConfigCommand("+CMNB=" + String(modemConfig.bandMode))) {
    logMessage(0, "❌ Fallo configurando modo de banda");
    return false;
  }

  sendConfigCommand("+CBANDCFG=\"CAT-M\",1,2,3,4,5,8,12,13,14,18,19,20,25,26,27,28,66,85","OK",8000);
  sendConfigCommand("+CBANDCFG=\"NB-IOT\"");

  // Obtener listado de operadores disponibles (OBLIGATORIO para array dinámico)
  logMessage(2, "📋 Obteniendo operadores disponibles en la zona (obligatorio)...");
  if (!showAvailableOperators()) {
    logMessage(0, "❌ No se pudo obtener listado COPS. Sin operadores disponibles no se puede continuar.");
    return false;
  }

  // Verificar que tengamos operadores disponibles
  if (NUM_OPERATORS == 0) {
    logMessage(0, "❌ No hay operadores disponibles para probar");
    return false;
  }
  
  logMessage(2, "🔄 Probando operadores secuencialmente con evaluación dinámica");
  
  // NO evaluamos todos por adelantado porque la señal medida pertenece a la red actual,
  // no a la red que vamos a probar. En su lugar, probamos directamente.
  
  for (int i = 0; i < NUM_OPERATORS; i++) {
    OperatorInfo& op = operators[i];
    
    logMessage(2, "🔄 Intentando operador " + String(i + 1) + " de " + String(NUM_OPERATORS) + 
                  ": " + op.name + " (" + op.code + ")");
    
    // IMPORTANTE: Configurar APN ANTES de cada intento
    // Esto evita fallos intermitentes con diferentes operadores
    String pdpCommand = "+CGDCONT=1,\"IP\",\"" + modemConfig.apn + "\"";
    if (!sendATCommand(pdpCommand, "OK", 3000)) {
      logMessage(1, "⚠️  Fallo configurando APN para " + op.name + ", intentando siguiente");
      continue;
    }
    logMessage(3, "✅ APN pre-configurado para " + op.name);
    
    if (connectAndSendWithOperator(i)) {
      logMessage(2, "✅ Conexión y envío exitoso con " + op.name);
      flushPortSerial();
      return true;
    } else {
      logMessage(1, "⚠️  Falló conexión/envío con " + op.name + ", probando siguiente operador");
      
      // Desregistrar apropiadamente esperando CEREG
      deregisterFromNetwork();
      cleanPDPContext();
    }
  }

  logMessage(1, "⚠️  Todos los operadores fallaron, intentando modo automático");
  
  // Limpiar antes de modo automático
  cleanPDPContext();
  
  // Configurar APN primero
  String pdpCommand = "+CGDCONT=1,\"IP\",\"" + modemConfig.apn + "\"";
  if (!sendATCommand(pdpCommand, "OK", 3000)) {
    logMessage(0, "❌ Fallo configurando APN en modo automático");
    consecutiveFailures++;
    return false;
  }
  
  if (sendATCommand("+COPS=0", "OK", 15000)) {
    // Esperar registro en red
    if (waitForNetworkRegistration(30000)) {
      if (sendATCommand("+CNACT=0,1", "OK", 10000)) {
        delay(2000);
        
        // Verificar PDP activo con IP
        String autoIP = verifyPDPActive();
        if (autoIP != "" && autoIP != "0.0.0.0") {
          logMessage(2, "✅ Conectado en modo automático con IP asignada: " + autoIP);
          
          // Mostrar información CPSI en modo automático también
          logCpsiInfo("Modo Automático");
          
          // Nota: enviarDatos() gestiona TODO el ciclo TCP internamente
          // CRÍTICO: Validar el resultado del envío
          if (!enviarDatos()) {
            logMessage(1, "❌ Error enviando datos (TCP/Socket) en modo automático");
            logCpsiInfo("Fallo envío TCP modo automático");
            return false;
          }
          
          limpiarEnviados();
          consecutiveFailures = 0;
          logMessage(2, "✅ Datos enviados en modo automático");
          flushPortSerial();
          return true;
        } else {
          logMessage(1, "⚠️  Modo automático: PDP sin IP válida (" + (autoIP == "" ? "vacía" : autoIP) + ")");
          logCpsiInfo("Fallo IP modo automático");
        }
      }
    } else {
      logMessage(1, "⚠️  Modo automático: no se pudo registrar en red");
    }
  }

  logMessage(0, "❌ No se pudo conectar con ningún operador ni en modo automático");
  consecutiveFailures++;
  return false;
}

/**
 * Abre conexión TCP con manejo robusto de estados residuales
 * @return true si la conexión se abre exitosamente
 */
bool tcpOpen() {
  logMessage(2, "🔌 Abriendo conexión TCP a " + modemConfig.serverIP + ":" + modemConfig.serverPort);

  int maxAttempts = modemConfig.maxRetries;
  unsigned long timeout = getAdaptiveTimeout();

  for (int intento = 0; intento < maxAttempts; intento++) {
    logMessage(3, "🔄 Intento " + String(intento + 1) + " de " + String(maxAttempts));

    String openCommand = "+CAOPEN=0,0,\"TCP\",\"" + modemConfig.serverIP + "\"," + modemConfig.serverPort;

    if (sendATCommand(openCommand, "+CAOPEN: 0,0", timeout)) {
      logMessage(2, "✅ Socket TCP abierto exitosamente");
      return true;
    }

    // Cerrar socket residual antes del siguiente intento
    sendATCommand("+CACLOSE=0", "OK", 2000);
    logMessage(3, "🔄 Reintentando apertura de socket...");
    delay(500);
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


  unsigned long finalTimeout = (timeout > adaptiveTimeout) ? timeout : adaptiveTimeout;

  flushPortSerial();

  while (millis() - start < finalTimeout) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
    }
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

  String response = "";
  unsigned long start = millis();
  
  // Usar timeout especificado directamente para comandos de configuración rápidos
  // Solo usar timeout adaptativo si el timeout especificado es 0 o muy grande
  unsigned long finalTimeout = timeout;
  
  if (timeout == 0 || timeout > 15000) {
   
    unsigned long adaptiveTimeout = getAdaptiveTimeout();
    finalTimeout = adaptiveTimeout;
    logMessage(3, "🔄 Usando timeout adaptativo: " + String(finalTimeout) + "ms");
  } else {
    logMessage(3, "⏱️  Usando timeout especificado: " + String(finalTimeout) + "ms");
  }

  flushPortSerial();

  modem.sendAT(command);

  while (millis() - start < finalTimeout) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      if (modemConfig.enableDebug) {
        Serial.print(c);
      }
    }
  }

  if (response.indexOf(expectedResponse) != -1) {
    logMessage(3, "✅ Comando AT exitoso: " + command);
    return true;
  }

  logMessage(1, "⚠️  Comando AT falló: " + command + " (esperaba: " + expectedResponse + ")");
  return false;
}

/**
 * @brief Envía comando AT de configuración con timeout optimizado
 * 
 * Versión optimizada para comandos de configuración que no requieren
 * mucho tiempo de espera (como CNMP, CMNB, CBANDCFG, etc.)
 * 
 * @param command Comando AT a enviar (sin prefijo "AT")  
 * @param expectedResponse Respuesta esperada del módem
 * @param timeout_ms Timeout en milisegundos (por defecto 2000ms)
 * @return true si se recibe la respuesta esperada
 */
bool sendConfigCommand(const String& command, const String& expectedResponse, unsigned long timeout_ms) {
  logMessage(3, "⚡ Enviando comando de configuración: " + command);

  String response = "";
  unsigned long start = millis();

  flushPortSerial();
  modem.sendAT(command);

  while (millis() - start < timeout_ms) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      if (modemConfig.enableDebug) {
        Serial.print(c);
      }
    }
    
 
    if (response.indexOf(expectedResponse) != -1) {
      logMessage(3, "⚡ Comando de configuración exitoso: " + command + " (tiempo: " + String(millis() - start) + "ms)");
      return true;
    }
  }

  logMessage(1, "⚠️  Comando de configuración falló: " + command + " (esperaba: " + expectedResponse + ")");
  return false;
}



/**
 * Parsea la respuesta del comando AT+CPSI? para extraer métricas de señal
 * @param cpsiResponse - Respuesta del comando CPSI
 * @param operatorInfo - Estructura donde almacenar los datos parseados
 * @return true si el parseo fue exitoso
 */
bool parseCpsiResponse(const String& cpsiResponse, OperatorInfo& operatorInfo) {
  logMessage(3, "🔍 Parseando respuesta CPSI: " + cpsiResponse);
  
  
  int startPos = cpsiResponse.indexOf("+CPSI:");
  if (startPos == -1) {
    logMessage(1, "⚠️  No se encontró +CPSI en la respuesta");
    return false;
  }
  
  String cpsiData = cpsiResponse.substring(startPos + 6);
  cpsiData.trim();
  
 
  int commaCount = 0;
  int lastComma = -1;
  String values[20]; 
  
  for (int i = 0; i < cpsiData.length(); i++) {
    if (cpsiData.charAt(i) == ',' || i == cpsiData.length() - 1) {
      if (commaCount < 20) {
        int endPos = (i == cpsiData.length() - 1) ? i + 1 : i;
        values[commaCount] = cpsiData.substring(lastComma + 1, endPos);
        values[commaCount].trim();
      }
      lastComma = i;
      commaCount++;
    }
  }
  
  if (commaCount < 13) {
    logMessage(1, "⚠️  Respuesta CPSI incompleta, elementos: " + String(commaCount));
    return false;
  }
  
  if (values[1].indexOf("Online") == -1) {
    logMessage(2, "ℹ️  Operador no está online: " + values[1]);
    operatorInfo.connected = false;
    return false;
  }
  
  operatorInfo.connected = true;

  if (commaCount >= 14) {
    operatorInfo.rsrp = values[10].toInt();
    operatorInfo.rsrq = values[11].toInt();
    operatorInfo.rssi = values[12].toInt();
    operatorInfo.snr = values[13].toInt();
  }
  
  logMessage(3, "📊 Métricas parseadas - RSRP: " + String(operatorInfo.rsrp) + 
                " dBm, RSRQ: " + String(operatorInfo.rsrq) + 
                " dB, RSSI: " + String(operatorInfo.rssi) + 
                " dBm, SNR: " + String(operatorInfo.snr) + " dB");
  
  return true;
}

/**
 * Calcula una puntuación para un operador basándose en las métricas de señal
 * @param operatorInfo - Información del operador
 * @return Puntuación calculada (mayor es mejor)
 */
int calculateOperatorScore(const OperatorInfo& operatorInfo) {
  if (!operatorInfo.connected) {
    return -1000; 
  }
  
  int score = 0;
  
  if (operatorInfo.rsrp >= -80) score += 50;
  else if (operatorInfo.rsrp >= -90) score += 40;
  else if (operatorInfo.rsrp >= -100) score += 30;
  else if (operatorInfo.rsrp >= -110) score += 20;
  else if (operatorInfo.rsrp >= -120) score += 10;
  
  if (operatorInfo.rsrq >= -8) score += 30;
  else if (operatorInfo.rsrq >= -12) score += 25;
  else if (operatorInfo.rsrq >= -15) score += 20;
  else if (operatorInfo.rsrq >= -18) score += 15;
  else score += 10;
  
  if (operatorInfo.snr >= 20) score += 20;
  else if (operatorInfo.snr >= 15) score += 15;
  else if (operatorInfo.snr >= 10) score += 12;
  else if (operatorInfo.snr >= 5) score += 8;
  else if (operatorInfo.snr >= 0) score += 5;
  
  logMessage(3, "🏆 Puntuación calculada para " + operatorInfo.name + ": " + String(score));
  return score;
}

/**
 * Obtiene información de señal usando AT+CPSI? con timeout
 * @param response - String donde almacenar la respuesta
 * @param timeout_ms - Timeout en milisegundos
 * @return true si se obtuvo la respuesta exitosamente
 */
bool getCpsiResponse(String& response, unsigned long timeout_ms) {
  logMessage(3, "📡 Obteniendo información CPSI del módem");
  
  flushPortSerial();
  modem.sendAT("+CPSI?");
  
  unsigned long start = millis();
  response = "";
  
  while (millis() - start < timeout_ms) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
    }
    
    
    if (response.indexOf("+CPSI:") != -1 && response.indexOf("OK") != -1) {
      return true;
    }
    
    delay(10);
  }
  
  logMessage(1, "⚠️  Timeout obteniendo respuesta CPSI");
  return false;
}

/**
 * @brief Obtiene y muestra información CPSI completa en el log
 * 
 * Esta función obtiene la información completa de CPSI del módem y la muestra
 * de forma legible en el log, incluyendo información de operador, bandas,
 * métricas de señal y estado de conexión.
 * 
 * @param operatorName Nombre del operador actual (para contexto en el log)
 * @return true si se obtuvo y mostró la información exitosamente
 */
bool logCpsiInfo(const String& operatorName) {
  logMessage(2, "📊 Obteniendo información CPSI para " + operatorName);
  
  String cpsiResponse;
  if (!getCpsiResponse(cpsiResponse, 5000)) {
    logMessage(1, "⚠️  No se pudo obtener información CPSI para " + operatorName);
    return false;
  }
  
  // Mostrar respuesta CPSI completa en el log
  logMessage(2, "📡 CPSI COMPLETO para " + operatorName + ":");
  logMessage(2, "   " + cpsiResponse);
  
  // Intentar parsear para mostrar información estructurada
  OperatorInfo tempOp;
  tempOp.name = operatorName;
  
  if (parseCpsiResponse(cpsiResponse, tempOp)) {
    logMessage(2, "📊 MÉTRICAS DE SEÑAL - " + operatorName + ":");
    logMessage(2, "   📶 RSRP: " + String(tempOp.rsrp) + " dBm");
    logMessage(2, "   📈 RSRQ: " + String(tempOp.rsrq) + " dB");
    logMessage(2, "   📡 RSSI: " + String(tempOp.rssi) + " dBm");
    logMessage(2, "   🔊 SNR:  " + String(tempOp.snr) + " dB");
    logMessage(2, "   🔗 Estado: " + String(tempOp.connected ? "Conectado" : "Desconectado"));
    logMessage(2, "   🏆 Score: " + String(calculateOperatorScore(tempOp)));
  }
  
  return true;
}

/**
 * @brief Desregistra de la red correctamente esperando URC +CEREG: 0
 * 
 * SIM7080 puede tardar varios segundos en liberar la celda tras AT+COPS=2.
 * Esta función espera la confirmación +CEREG: 0 o hace reset rápido si no llega.
 * 
 * @return true si se desregistró exitosamente
 */
bool deregisterFromNetwork() {
  logMessage(2, "📴 Desregistrando de la red actual...");
  
  flushPortSerial();
  
  if (!sendATCommand("+COPS=2", "OK", 5000)) {
    logMessage(1, "⚠️  Fallo enviando AT+COPS=2");
    return false;
  }
  
  // Esperar URC +CEREG: 0 (no registrado)
  logMessage(3, "⏳ Esperando +CEREG: 0...");
  
  String response = "";
  unsigned long start = millis();
  bool deregistered = false;
  
  while (millis() - start < 5000) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      
      // Buscar +CEREG: 0 (no registrado)
      if (response.indexOf("+CEREG: 0") != -1) {
        deregistered = true;
        break;
      }
    }
    delay(10);
  }
  
  if (deregistered) {
    logMessage(2, "✅ Desregistro confirmado (+CEREG: 0)");
    delay(500);
    return true;
  }
  
  // Si no llegó CEREG: 0, hacer reset rápido del módulo de radio
  logMessage(1, "⚠️  +CEREG: 0 no recibido, ejecutando reset rápido (AT+CFUN=1,1)");
  
  sendATCommand("+CFUN=1,1", "OK", 10000);
  delay(3000); // Esperar a que el módulo se reinicie
  
  logMessage(2, "✅ Reset rápido completado");
  return true;
}

/**
 * @brief Limpia sesión PDP activa antes de cambiar operador
 * 
 * SIM7080 puede mantener sesiones PDP antiguas que causan errores.
 * Esta función desactiva cualquier contexto PDP residual.
 * 
 * @return true si se limpió exitosamente
 */
bool cleanPDPContext() {
  logMessage(3, "🧹 Limpiando contexto PDP residual...");
  
  // Verificar si hay PDP activo primero
  flushPortSerial();
  modem.sendAT("+CNACT?");
  delay(100);
  
  String response = "";
  unsigned long start = millis();
  
  while (millis() - start < 2000) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      
      if (response.indexOf("OK") != -1) {
        break;
      }
    }
  }
  
  // Solo desactivar si hay PDP activo (evita error 500)
  if (response.indexOf("+CNACT: 0,1") != -1) {
    logMessage(3, "📊 PDP activo detectado, desactivando...");
    sendATCommand("+CNACT=0,0", "OK", 5000);
    delay(1000);
  } else {
    logMessage(3, "ℹ️  PDP ya está inactivo, no se requiere limpieza");
  }
  
  logMessage(3, "✅ Contexto PDP limpiado");
  return true;
}

/**
 * @brief Espera registro en red verificando +CEREG
 * 
 * Espera hasta que el módem se registre en la red (CEREG: 1 o 5).
 * CEREG: 1 = registrado red local
 * CEREG: 5 = registrado roaming
 * 
 * @param timeout_ms Tiempo máximo de espera en milisegundos
 * @return true si se registró exitosamente
 */
bool waitForNetworkRegistration(unsigned long timeout_ms) {
  logMessage(2, "⏳ Esperando registro en red (+CEREG: 1 o 5)...");
  
  unsigned long start = millis();
  
  while (millis() - start < timeout_ms) {
    flushPortSerial();
    modem.sendAT("+CEREG?");
    delay(100);
    
    String response = "";
    unsigned long readStart = millis();
    
    while (millis() - readStart < 2000) {
      if (SerialAT.available()) {
        char c = SerialAT.read();
        response += c;
        
        if (response.indexOf("OK") != -1) {
          break;
        }
      }
    }
    
    // Buscar +CEREG: 0,1 (registrado) o +CEREG: 0,5 (roaming)
    if (response.indexOf("+CEREG: 0,1") != -1 || 
        response.indexOf("+CEREG: 0,5") != -1 ||
        response.indexOf("+CEREG: 1") != -1 || 
        response.indexOf("+CEREG: 5") != -1) {
      logMessage(2, "✅ Registrado en red exitosamente");
      return true;
    }
    
    delay(1000);
  }
  
  logMessage(1, "⚠️  Timeout esperando registro en red");
  return false;
}

/**
 * @brief Verifica que PDP esté activo con IP asignada
 * 
 * isNetworkConnected() puede dar falsos positivos.
 * Esta función verifica AT+CNACT? para confirmar IP asignada.
 * 
 * @return true si PDP está activo con IP válida
 */
String verifyPDPActive() {
  logMessage(3, "🔍 Verificando estado PDP...");
  
  flushPortSerial();
  modem.sendAT("+CNACT?");
  delay(100);
  
  String response = "";
  unsigned long start = millis();
  
  while (millis() - start < 3000) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      
      if (response.indexOf("OK") != -1) {
        break;
      }
    }
  }
  
  // Buscar +CNACT: 0,1,"<IP>" (PDP activo con IP)
  int cnactPos = response.indexOf("+CNACT: 0,1,\"");
  if (cnactPos != -1) {
    // Extraer IP
    int ipStart = cnactPos + 13;
    int ipEnd = response.indexOf('\"', ipStart);
    if (ipEnd != -1) {
      String ip = response.substring(ipStart, ipEnd);
      logMessage(2, "✅ PDP activo con IP: " + ip);
      return ip;  // Retornar la IP extraída
    }
  }
  
  logMessage(1, "⚠️  PDP no activo o sin IP asignada");
  return "";  // Retornar cadena vacía si falla
}

/**
 * @brief Limpia y reinicializa el array dinámico de operadores
 * 
 * Libera memoria del array actual y lo prepara para recibir nuevos operadores
 * desde la respuesta COPS.
 */
void clearOperators() {
  operators.clear();
  NUM_OPERATORS = 0;
  logMessage(3, "🧹 Array de operadores limpiado");
}

/**
 * @brief Agrega un operador al array dinámico
 * 
 * Crea una nueva entrada de operador con la información proporcionada
 * y la agrega al array dinámico.
 * 
 * @param longName Nombre completo del operador
 * @param shortName Nombre corto del operador
 * @param numeric Código numérico (MCC+MNC)
 * @param status Estado del operador (0=desconocido, 1=disponible, 2=actual, 3=prohibido)
 * @return Índice del operador agregado en el array
 */
int addOperator(const String& longName, const String& shortName, const String& numeric, int status) {
  OperatorInfo newOp;
  
  // Usar nombre corto si está disponible, sino el largo
  newOp.name = (shortName.length() > 0) ? shortName : longName;
  newOp.code = numeric;
  newOp.rsrp = 0;
  newOp.rsrq = 0;
  newOp.rssi = 0;
  newOp.snr = 0;
  newOp.connected = false;
  newOp.score = (status == 1) ? 100 : 10; // Operadores disponibles tienen mayor prioridad
  
  operators.push_back(newOp);
  NUM_OPERATORS = operators.size();
  
  logMessage(3, "➕ Operador agregado: " + newOp.name + " (" + numeric + ")");
  return NUM_OPERATORS - 1;
}

/**
 * @brief Evalúa calidad de señal de un operador específico usando AT+CESQ
 * 
 * Conecta temporalmente al operador y obtiene métricas LTE (RSRQ, RSRP, SNR)
 * para calcular su puntuación de calidad.
 * 
 * @param operatorIndex Índice del operador en el array operators[]
 * @return true si se evaluó exitosamente
 */
bool evaluateOperatorSignal(int operatorIndex) {
  if (operatorIndex < 0 || operatorIndex >= NUM_OPERATORS) {
    return false;
  }
  
  OperatorInfo& op = operators[operatorIndex];
  logMessage(2, "📊 Evaluando señal de " + op.name + " (" + op.code + ")...");
  
  // Desregistrar de red actual apropiadamente
  deregisterFromNetwork();
  
  // Registrar con operador específico
  String copsCommand = "+COPS=1,2,\"" + op.code + "\"";
  if (!sendATCommand(copsCommand, "OK", 20000)) {
    logMessage(1, "⚠️  No se pudo registrar con " + op.name);
    op.connected = false;
    op.score = -1000;
    return false;
  }
  
  // Esperar registro en red
  delay(3000);
  
  bool registered = false;
  for (int i = 0; i < 10; i++) {
    if (modem.isNetworkConnected()) {
      registered = true;
      break;
    }
    delay(1000);
  }
  
  if (!registered) {
    logMessage(1, "⚠️  " + op.name + " no pudo registrarse en red");
    op.connected = false;
    op.score = -1000;
    return false;
  }
  
  op.connected = true;
  
  // Obtener métricas CESQ
  flushPortSerial();
  modem.sendAT("+CESQ");
  
  String response = "";
  unsigned long start = millis();
  
  while (millis() - start < 5000) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      
      if (response.indexOf("+CESQ:") != -1 && response.indexOf("OK") != -1) {
        break;
      }
    }
  }
  
  // Parsear respuesta CESQ
  int cesqPos = response.indexOf("+CESQ:");
  if (cesqPos != -1) {
    String cesqData = response.substring(cesqPos + 6);
    cesqData.trim();
    
    int values[6] = {-1, -1, -1, -1, -1, -1};
    int valueIndex = 0;
    int lastComma = 0;
    
    for (int i = 0; i < cesqData.length() && valueIndex < 6; i++) {
      if (cesqData[i] == ',' || cesqData[i] == '\r' || cesqData[i] == '\n') {
        String valueStr = cesqData.substring(lastComma, i);
        valueStr.trim();
        values[valueIndex++] = valueStr.toInt();
        lastComma = i + 1;
      }
    }
    
    if (valueIndex < 6 && lastComma < cesqData.length()) {
      String valueStr = cesqData.substring(lastComma);
      valueStr.trim();
      int endPos = valueStr.indexOf('\r');
      if (endPos == -1) endPos = valueStr.indexOf('\n');
      if (endPos != -1) valueStr = valueStr.substring(0, endPos);
      if (valueStr.length() > 0) values[valueIndex] = valueStr.toInt();
    }
    
    int rsrq = values[4];  // RSRQ (0-34)
    int rsrp = values[5];  // RSRP (0-97)
    
    if (rsrq != 255 && rsrq >= 0) {
      op.rsrq = rsrq;
    }
    if (rsrp != 255 && rsrp >= 0) {
      op.rsrp = rsrp;
    }
    
    // Calcular puntuación basada en RSRQ y RSRP
    op.score = 0;
    
    // RSRQ: 0-34, mayor es mejor
    if (rsrq >= 25) op.score += 100;      // Excelente
    else if (rsrq >= 20) op.score += 80;  // Muy buena
    else if (rsrq >= 15) op.score += 60;  // Buena
    else if (rsrq >= 10) op.score += 40;  // Regular
    else if (rsrq >= 5) op.score += 20;   // Débil
    else op.score += 10;                   // Muy débil
    
    // RSRP: 0-97, mayor es mejor (bonus adicional)
    if (rsrp >= 75) op.score += 50;       // Excelente
    else if (rsrp >= 60) op.score += 40;  // Muy buena
    else if (rsrp >= 45) op.score += 30;  // Buena
    else if (rsrp >= 30) op.score += 20;  // Regular
    else if (rsrp >= 15) op.score += 10;  // Débil
    
    logMessage(2, "📊 " + op.name + " - RSRQ: " + String(rsrq) + 
                  ", RSRP: " + String(rsrp) + ", Score: " + String(op.score));
  } else {
    logMessage(1, "⚠️  No se pudo obtener CESQ para " + op.name);
    op.score = 50;  // Score por defecto para operadores disponibles sin métricas
  }
  
  return true;
}

/**
 * @brief Evalúa todos los operadores disponibles y los ordena por calidad de señal
 * 
 * ⚠️ ADVERTENCIA: Esta función mide la señal de la red ACTUAL, no de la red destino.
 * La señal medida pertenece a la celda donde está registrado el módem actualmente,
 * NO a la señal que tendrá con el operador que vas a probar.
 * 
 * ❌ NO USAR en producción - solo para testing/debugging
 * ✅ Mejor estrategia: probar operadores directamente sin evaluación previa
 * 
 * @deprecated Esta función no proporciona información útil para selección de operador
 * 
 * Conecta con cada operador, obtiene métricas CESQ y calcula puntuaciones.
 * Ordena el array de mayor a menor puntuación para optimizar intentos de conexión.
 */
void evaluateAndSortOperators() {
  logMessage(1, "⚠️  ADVERTENCIA: evaluateAndSortOperators() mide señal de red actual, no de operadores destino");
  logMessage(2, "📊 Evaluando calidad de señal de todos los operadores...");
  
  if (NUM_OPERATORS == 0) {
    logMessage(1, "⚠️  No hay operadores para evaluar");
    return;
  }
  
  // Evaluar cada operador
  for (int i = 0; i < NUM_OPERATORS; i++) {
    evaluateOperatorSignal(i);
    delay(1000);  // Pequeña pausa entre evaluaciones
  }
  
  // Ordenar por score (mayor a menor)
  for (int i = 0; i < NUM_OPERATORS - 1; i++) {
    for (int j = i + 1; j < NUM_OPERATORS; j++) {
      if (operators[j].score > operators[i].score) {
        // Intercambiar operadores
        OperatorInfo temp = operators[i];
        operators[i] = operators[j];
        operators[j] = temp;
      }
    }
  }
  
  // Mostrar orden final
  logMessage(2, "🏆 Operadores ordenados por calidad de señal:");
  for (int i = 0; i < NUM_OPERATORS; i++) {
    String statusIcon = operators[i].connected ? "✅" : "❌";
    logMessage(2, "  " + String(i + 1) + ". " + statusIcon + " " + 
                  operators[i].name + " - Score: " + String(operators[i].score) + 
                  " (RSRQ: " + String(operators[i].rsrq) + ", RSRP: " + String(operators[i].rsrp) + ")");
  }
  
  // Desregistrar apropiadamente para empezar limpio
  deregisterFromNetwork();
  cleanPDPContext();
}

/**
 * @brief Ordena operadores por prioridad basándose en la lista de preferidos
 * 
 * Reorganiza el array de operadores poniendo primero los operadores preferidos
 * en el orden especificado, seguidos por el resto de operadores disponibles.
 */
void sortOperatorsByPriority() {
  std::vector<OperatorInfo> sortedOperators;
  std::vector<bool> used(operators.size(), false);
  
 
  for (int pref = 0; pref < NUM_PREFERRED; pref++) {
    for (int i = 0; i < operators.size(); i++) {
      if (!used[i] && operators[i].code == PREFERRED_OPERATORS[pref]) {
        sortedOperators.push_back(operators[i]);
        used[i] = true;
        logMessage(3, "📌 Operador prioritario: " + operators[i].name);
        break;
      }
    }
  }
  
  // Agregar primero operadores disponibles (score >= 50), luego los no disponibles
  for (int i = 0; i < operators.size(); i++) {
    if (!used[i] && operators[i].score >= 50) {
      sortedOperators.push_back(operators[i]);
      used[i] = true;
      logMessage(3, "📋 Operador disponible: " + operators[i].name);
    }
  }
  
  // Agregar operadores no disponibles al final
  for (int i = 0; i < operators.size(); i++) {
    if (!used[i]) {
      sortedOperators.push_back(operators[i]);
      used[i] = true;
      logMessage(3, "📋 Operador no disponible: " + operators[i].name);
    }
  }
  
  operators = sortedOperators;
  NUM_OPERATORS = operators.size();
  
  logMessage(2, "🔄 Operadores ordenados por prioridad: " + String(NUM_OPERATORS) + " operadores");
}

/**
 * @brief Obtiene y muestra listado completo de operadores disponibles (COPS)
 * 
 * Ejecuta el comando AT+COPS=? para obtener la lista de todos los operadores
 * disponibles en la zona, incluyendo su estado (disponible, actual, prohibido)
 * y muestra la información de forma estructurada en el log.
 * 
 * @return true si se obtuvo el listado exitosamente
 */
bool showAvailableOperators() {
  logMessage(2, "📋 Obteniendo listado de operadores disponibles (COPS)...");
  
  flushPortSerial();
  

  logMessage(2, "⏳ Ejecutando AT+COPS=? (puede tardar 30-120 segundos)...");
  
  modem.sendAT("+COPS=?");
  
  unsigned long start = millis();
  unsigned long timeout = 120000;
  String response = "";
  bool foundCops = false;
  
  while (millis() - start < timeout) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      
     
      if ((millis() - start) % 10000 < 100) {
        logMessage(2, "⏳ Buscando operadores... " + String((millis() - start) / 1000) + "s");
      }
    }
    
   
    if (response.indexOf("+COPS:") != -1 && response.indexOf("OK") != -1) {
      foundCops = true;
      break;
    }
    
    delay(100);
  }
  
  if (!foundCops) {
    logMessage(1, "⚠️  Timeout obteniendo listado COPS después de " + String(timeout/1000) + " segundos");
    return false;
  }
  
  logMessage(2, "✅ Listado COPS obtenido exitosamente");
  
 
  if (modemConfig.enableDebug) {
    logMessage(3, "📄 Respuesta COPS completa:");
    logMessage(3, response);
  }
  
  
  parseCopsResponse(response);
  
  return true;
}

/**
 * @brief Parsea la respuesta del comando AT+COPS=? y llena el array dinámico de operadores
 * 
 * Analiza la respuesta del comando COPS, extrae información de cada operador
 * disponible y llena el array dinámico. Solo incluye operadores disponibles.
 * 
 * @param copsResponse String con la respuesta completa del comando COPS
 */
void parseCopsResponse(const String& copsResponse) {
  // Limpiar array anterior
  clearOperators();
  
  logMessage(2, "📋 PROCESANDO OPERADORES DISPONIBLES:");
  logMessage(2, "════════════════════════════════════");
  
  int startPos = copsResponse.indexOf("+COPS:");
  if (startPos == -1) {
    logMessage(1, "⚠️  No se encontró información de operadores en la respuesta");
    return;
  }
  
  String copsData = copsResponse.substring(startPos + 6);
  int operatorCount = 0;
  

  int pos = 0;
  while (pos < copsData.length()) {
    int parenStart = copsData.indexOf('(', pos);
    if (parenStart == -1) break;
    
    int parenEnd = copsData.indexOf(')', parenStart);
    if (parenEnd == -1) break;
    
    String operatorEntry = copsData.substring(parenStart + 1, parenEnd);
    
   
    String fields[5];
    int fieldCount = 0;
    int lastComma = -1;
    bool inQuotes = false;
    
    for (int i = 0; i < operatorEntry.length() && fieldCount < 5; i++) {
      char c = operatorEntry.charAt(i);
      
      if (c == '"') {
        inQuotes = !inQuotes;
      } else if (c == ',' && !inQuotes) {
        fields[fieldCount] = operatorEntry.substring(lastComma + 1, i);
        fields[fieldCount].replace("\"", ""); 
        fields[fieldCount].trim();
        fieldCount++;
        lastComma = i;
      }
    }
    
   
    if (fieldCount < 5 && lastComma < operatorEntry.length() - 1) {
      fields[fieldCount] = operatorEntry.substring(lastComma + 1);
      fields[fieldCount].replace("\"", "");
      fields[fieldCount].trim();
      fieldCount++;
    }
    
    if (fieldCount >= 4) {
      int statusCode = fields[0].toInt();
      String longName = fields[1];
      String shortName = fields[2];
      String numeric = fields[3];
      String technology = (fieldCount >= 5) ? getTechnologyText(fields[4]) : "N/A";
      String status = getOperatorStatusText(fields[0]);
      
      operatorCount++;
      logMessage(2, "🏢 Operador " + String(operatorCount) + ":");
      logMessage(2, "   📛 Nombre: " + longName + " (" + shortName + ")");
      logMessage(2, "   🔢 Código: " + numeric);
      logMessage(2, "   📶 Tecnología: " + technology);
      logMessage(2, "   📊 Estado: " + status);
      
      // Agregar TODAS las operadoras encontradas al array dinámico
      int index = addOperator(longName, shortName, numeric, statusCode);
      
      if (statusCode == 1) {
        logMessage(2, "   ✅ Agregado al array dinámico - DISPONIBLE (índice: " + String(index) + ")");
      } else {
        logMessage(2, "   📋 Agregado al array dinámico - NO DISPONIBLE (índice: " + String(index) + ")");
      }
      
      logMessage(2, "");
    }
    
    pos = parenEnd + 1;
  }
  
  logMessage(2, "📊 Total de operadores encontrados: " + String(operatorCount));
  logMessage(2, "📊 Operadores agregados al array: " + String(NUM_OPERATORS));
  
  // Ordenar operadores por prioridad
  if (NUM_OPERATORS > 0) {
    sortOperatorsByPriority();
    
    logMessage(2, "📋 ARRAY FINAL DE OPERADORES (ordenado por prioridad):");
    for (int i = 0; i < NUM_OPERATORS; i++) {
      logMessage(2, "   " + String(i + 1) + ". " + operators[i].name + " (" + operators[i].code + ")");
    }
  } else {
    logMessage(1, "⚠️  No se encontraron operadores");
  }
  
  logMessage(2, "════════════════════════════════════");
}

/**
 * @brief Convierte código de estado de operador a texto legible
 * @param statusCode Código de estado del operador
 * @return Texto descriptivo del estado
 */
String getOperatorStatusText(const String& statusCode) {
  int status = statusCode.toInt();
  
  switch (status) {
    case 0: return "🚫 Desconocido";
    case 1: return "✅ Disponible";
    case 2: return "🔴 Actual";
    case 3: return "❌ Prohibido";
    default: return "❓ Estado " + statusCode;
  }
}

/**
 * @brief Convierte código de tecnología de acceso a texto legible
 * @param techCode Código de tecnología
 * @return Texto descriptivo de la tecnología
 */
String getTechnologyText(const String& techCode) {
  int tech = techCode.toInt();
  
  switch (tech) {
    case 0: return "2G GSM";
    case 1: return "2G Compact";
    case 2: return "3G UTRAN";
    case 3: return "3G EGPRS"; 
    case 4: return "3G HSDPA";
    case 5: return "3G HSUPA";
    case 6: return "3G HSDPA+HSUPA";
    case 7: return "4G LTE";
    case 8: return "EC-GSM-IoT";
    case 9: return "NB-IoT";
    default: return "Tech " + techCode;
  }
}



/**
 * @ingroup Operator_Management
 * @brief Conecta con un operador específico e intenta enviar todos los datos pendientes
 * 
 * Esta función implementa el ciclo completo de conexión y envío para un operador específico:
 * 1. Configura el operador en el módem
 * 2. Establece el contexto PDP y activa la conexión de datos
 * 3. Espera a que se establezca la conexión de red
 * 4. Abre conexión TCP y envía todos los datos del buffer
 * 5. Verifica que todos los datos se hayan enviado exitosamente
 * 
 * @param operatorIndex Índice del operador en el array operators[] (0-3)
 * 
 * @retval true Si se conectó exitosamente Y todos los datos se enviaron
 * @retval false Si falló la conexión o quedaron datos sin enviar
 * 
 * @note La función actualiza automáticamente el contador consecutiveFailures
 * @note Registra métricas de calidad de señal si enableDebug está activo
 * 
 * @see operators[]
 * @see tcpOpen()
 * @see enviarDatos()
 * 
 * @warning Requiere que el módem esté previamente inicializado
 */
bool connectAndSendWithOperator(int operatorIndex) {
  if (operatorIndex < 0 || operatorIndex >= NUM_OPERATORS) {
    logMessage(0, "❌ Índice de operador inválido: " + String(operatorIndex));
    return false;
  }
  
  OperatorInfo& op = operators[operatorIndex];
  logMessage(2, "🔄 Conectando con " + op.name + " (" + op.code + ")...");
  
  // PASO 1: Limpiar contexto PDP residual
  if (!cleanPDPContext()) {
    logMessage(1, "⚠️  No se pudo limpiar PDP antes de " + op.name);
    logCpsiInfo("Fallo limpieza PDP previo a " + op.name);
  }
  
  // NOTA: APN ya está configurado en el bucle principal antes de llamar a esta función
  
  // PASO 2: Conectar al operador específico
  String copsCommand = "+COPS=1,2,\"" + op.code + "\"";
  if (!sendATCommand(copsCommand, "OK", 20000)) {
    logMessage(1, "⚠️  Fallo configurando operador " + op.name);
    logCpsiInfo("Fallo COPS con " + op.name);
    return false;
  }
  
  // PASO 3: Esperar registro en red verificando CEREG
  if (!waitForNetworkRegistration(30000)) {
    logMessage(1, "⚠️  " + op.name + " no se registró en la red");
    logCpsiInfo("Fallo CEREG con " + op.name);
    return false;
  }
  
  // PASO 4: Activar contexto PDP
  if (!sendATCommand("+CNACT=0,1", "OK", 10000)) {
    logMessage(0, "❌ Fallo activando contexto PDP para " + op.name);
    logCpsiInfo("Fallo activación PDP con " + op.name);
    return false;
  }
  
  // PASO 5: Verificar que PDP esté realmente activo con IP
  delay(2000); // Dar tiempo a que se asigne IP
  
  String assignedIP = verifyPDPActive();
  
  // CRÍTICO: Verificar que la IP sea válida (no vacía ni 0.0.0.0)
  if (assignedIP == "" || assignedIP == "0.0.0.0") {
    logMessage(1, "⚠️  IP inválida (" + (assignedIP == "" ? "vacía" : assignedIP) + ") para " + op.name);
    logCpsiInfo("Fallo PDP/IP con " + op.name);
    return false;
  }
  
  logMessage(2, "✅ Conectado exitosamente a " + op.name + " (IP: " + assignedIP + ")");

  logCpsiInfo(op.name);
  
  if (modemConfig.enableDebug) {
    // Obtener métricas CESQ actuales
    int cesqSignal = getSignalQualityFromCESQ();
    if (cesqSignal >= 0) {
      logMessage(3, "📶 Calidad de señal CESQ con " + op.name + ": " + String(cesqSignal));
    } else {
      logMessage(3, "⚠️  No se pudo obtener CESQ con " + op.name);
    }
  }
  
  logMessage(2, "📤 Intentando enviar datos con " + op.name);
  
  // Nota: enviarDatos() gestiona TODO el ciclo TCP internamente
  // CRÍTICO: Validar el resultado del envío
  if (!enviarDatos()) {
    logMessage(1, "❌ Error enviando datos (TCP/Socket) con " + op.name);
    logCpsiInfo("Fallo envío TCP con " + op.name);
    return false;
  }
  
  // Verificar si quedan datos pendientes en el buffer
  bool hayPendientes = false;
  if (LittleFS.exists(ARCHIVO_BUFFER)) {
    File f = LittleFS.open(ARCHIVO_BUFFER, "r");
    while (f.available()) {
      String linea = f.readStringUntil('\n');
      linea.trim();
      if (linea.length() > 0 && !linea.startsWith("#ENVIADO")) {
        hayPendientes = true;
        break;
      }
    }
    f.close();
  }
  
  if (!hayPendientes) {
    logMessage(2, "✅ Todos los datos enviados exitosamente con " + op.name);
    limpiarEnviados();
    consecutiveFailures = 0;
    return true;
  } else {
    logMessage(1, "⚠️  Algunos datos no se pudieron enviar con " + op.name);
    return false;
  }
}







/**
 * Configura y obtiene datos GPS del módem integrado
 * @param data - Estructura de datos donde almacenar coordenadas GPS
 */
void setupGpsSim(sensordata_type* data) {
  logMessage(2, "🛰️  Configurando GPS integrado del módem");

  SerialMon.begin(115200);
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  if (startGps()) {
    if (getGpsSim()) {
     
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


  if (stopGps()) {
    logMessage(2, "🔋 GPS detenido para ahorrar energía");
  } else {
    logMessage(1, "⚠️  Error al detener GPS, continuando...");
  }

 
  logMessage(2, "🔌 Apagando módem para máximo ahorro de energía");
  
 
  if (sendATCommand("+CFUN=0", "OK", 3000)) {
    logMessage(2, "✅ Funciones del módem desactivadas");
  } else {
    logMessage(1, "⚠️  Error al desactivar funciones del módem");
  }
  
  delay(500); 
  
  
  modemPwrKeyPulse();
  

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

  while (!modem.testAT(1000)) {
    flushPortSerial();
    logMessage(3, "🔄 Esperando respuesta del módem...");

    if (retry++ > maxRetries) {
      modemPwrKeyPulse();
      retry = 0;
      logMessage(2, "🔄 Reintentando inicio del módem");
    }
  }

  logMessage(2, "✅ Módem iniciado correctamente");

  
  modem.sendAT("+CFUN=0");
  modem.waitResponse();
  delay(1000);


  modem.disableGPS();
  delay(200);


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

 
  if (!sendATCommand("+CGNSPWR=1", "OK", 5000)) {
    logMessage(1, "⚠️  Fallo al encender GPS del módem");
    return false;
  }

  delay(500);

  for (int i = 0; i < 50; ++i) {
    if (modem.getGPS(&latConverter.f,
                     &lonConverter.f,
                     &gps_speed,
                     &altConverter.f,
                     &gps_vsat,
                     &gps_usat,
                     &gps_accuracy,
                     &gps_year,
                     &gps_month,
                     &gps_day,         
                     &gps_hour,        
                     &gps_minute,      
                     &gps_second)) {

      logMessage(2, "✅ Coordenadas GPS obtenidas en " + String(i + 1) + " intentos");

      if (modemConfig.enableDebug) {
        logMessage(3, "📍 Latitud: " + String(latConverter.f, 6));
        logMessage(3, "📍 Longitud: " + String(lonConverter.f, 6));
        logMessage(3, "📍 Altitud: " + String(altConverter.f, 2) + "m");
        logMessage(3, "📍 Fecha: " + String(gps_year) + "/" + String(gps_month) + "/" + String(gps_day));
        logMessage(3, "📍 Hora: " + String(gps_hour) + ":" + String(gps_minute) + ":" + String(gps_second));
      }

      return true;
    }

    delay(500);
    logMessage(2, "🔄 Intento " + String(i + 1) + " de 50 para obtener GPS");
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


  if (!sendATCommand("+CGNSPWR=0", "OK", 5000)) {
    logMessage(1, "⚠️  Fallo al apagar GPS del módem usando comando AT");
  }
  
  if (modem.disableGPS()) {
    gpsEnabled = false;
    logMessage(2, "✅ GPS deshabilitado correctamente");
    
   
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
 * Obtiene calidad de señal LTE usando comando AT+CESQ
 * Prioriza RSRP sobre RSRQ para mejor precisión
 * @return Valor normalizado de señal (0-31), -1 si error
 */
int getSignalQualityFromCESQ() {
  logMessage(3, "📡 Obteniendo calidad de señal con AT+CESQ");
  
  flushPortSerial();
  modem.sendAT("+CESQ");
  
  // Esperar un momento para que el módem procese
  delay(100);
  
  // Leer respuesta completa hasta encontrar "OK"
  // Esto maneja correctamente URCs, timestamps y respuestas intermedias del módem
  String response = "";
  unsigned long start = millis();
  bool foundOK = false;
  
  while (millis() - start < 5000) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      
      // Buscar "OK" seguido de nueva línea o fin de respuesta
      if (response.endsWith("OK\r\n") || response.endsWith("OK\n") || response.endsWith("OK")) {
        foundOK = true;
        break;
      }
    }
  }
  
  if (!foundOK) {
    logMessage(1, "⚠️  Timeout esperando respuesta OK de AT+CESQ");
    return -1;
  }
  
  // Parsear respuesta: +CESQ: rxlev,ber,rscp,ecno,rsrq,rsrp
  // Ejemplo: +CESQ: 99,99,255,255,12,20
  int cesqPos = response.indexOf("+CESQ:");
  if (cesqPos == -1) {
    logMessage(1, "⚠️  No se encontró +CESQ: en la respuesta");
    logMessage(3, "Respuesta recibida: " + response);
    return -1;
  }
  
  String cesqData = response.substring(cesqPos + 6);
  cesqData.trim();
  
  // Cortar antes de "OK" para evitar capturar basura
  int okPos = cesqData.indexOf("OK");
  if (okPos != -1) {
    cesqData = cesqData.substring(0, okPos);
    cesqData.trim();
  }
  
  // Extraer valores separados por comas
  int values[6] = {-1, -1, -1, -1, -1, -1};
  int valueIndex = 0;
  int lastComma = 0;
  
  for (int i = 0; i < cesqData.length() && valueIndex < 6; i++) {
    if (cesqData[i] == ',' || cesqData[i] == '\r' || cesqData[i] == '\n') {
      String valueStr = cesqData.substring(lastComma, i);
      valueStr.trim();
      values[valueIndex++] = valueStr.toInt();
      lastComma = i + 1;
    }
  }
  
  // Procesar último valor si no termina en coma
  if (valueIndex < 6 && lastComma < cesqData.length()) {
    String valueStr = cesqData.substring(lastComma);
    valueStr.trim();
    int endPos = valueStr.indexOf('\r');
    if (endPos == -1) endPos = valueStr.indexOf('\n');
    if (endPos != -1) valueStr = valueStr.substring(0, endPos);
    if (valueStr.length() > 0) values[valueIndex] = valueStr.toInt();
  }
  
  int rsrq = values[4];  // Índice 4 = RSRQ (0-34, 255=desconocido)
  int rsrp = values[5];  // Índice 5 = RSRP (0-97, 255=desconocido)
  
  logMessage(3, "📊 CESQ - RSRQ: " + String(rsrq) + ", RSRP: " + String(rsrp));
  
  int signalValue = -1;
  
  // PRIORIDAD 1: RSRP (mejor indicador de potencia de señal real)
  if (rsrp != 255 && rsrp >= 0 && rsrp <= 97) {
    // RSRP: 0-97 representa -140 dBm a -44 dBm (cada unidad = 1 dBm)
    // RSRP en dBm = RSRP_valor - 140
    int rsrp_dbm = rsrp - 140;
    
    // Convertir RSRP dBm a escala 0-31 (compatible con CSQ)
    // Rangos típicos LTE:
    // -80 dBm o mejor: excelente (31)
    // -90 dBm: muy buena (25)
    // -100 dBm: buena (18)
    // -110 dBm: regular (10)
    // -120 dBm o peor: pobre (0)
    signalValue = map(rsrp_dbm, -120, -80, 0, 31);
    signalValue = constrain(signalValue, 0, 31);
    
    logMessage(3, "📊 RSRP: " + String(rsrp) + " (" + String(rsrp_dbm) + " dBm) → CSQ: " + String(signalValue));
  } 
  // PRIORIDAD 2: RSRQ como fallback si RSRP no disponible
  else if (rsrq != 255 && rsrq >= 0 && rsrq <= 34) {
    // RSRQ: 0-34 representa -19.5 dB a -2 dB (cada unidad = 0.5 dB)
    // RSRQ en dB = (RSRQ_valor * 0.5) - 19.5
    // Rango real: RSRQ=0 → -19.5 dB, RSRQ=34 → -2 dB
    float rsrq_db = (rsrq * 0.5) - 19.5;
    
    // Convertir RSRQ dB a escala 0-31 usando rango real
    // -19.5 dB (pobre) = CSQ 0
    // -2 dB (excelente) = CSQ 31
    signalValue = map(rsrq_db * 10, -195, -20, 0, 31);
    signalValue = constrain(signalValue, 0, 31);
    
    logMessage(3, "📊 RSRQ: " + String(rsrq) + " (" + String(rsrq_db) + " dB) → CSQ: " + String(signalValue) + " [fallback]");
  }
  
  if (signalValue < 0) {
    logMessage(1, "⚠️  Valores CESQ inválidos o no disponibles (RSRP:" + String(rsrp) + " RSRQ:" + String(rsrq) + ")");
  }
  
  return signalValue;
}

/**
 * Obtiene información de la tarjeta SIM y calidad de señal
 */
void getIccid() {
  logMessage(2, "📱 Obteniendo información de la tarjeta SIM");

  flushPortSerial();

  for (int i = 0; i < 2; i++) {
    iccidsim0 = modem.getSimCCID();
    
    // Obtener calidad de señal usando AT+CESQ (métricas LTE)
    int cesqSignal = getSignalQualityFromCESQ();
    if (cesqSignal >= 0) {
      signalsim0 = cesqSignal;
    } else {
      // Si CESQ falla, usar 0 como valor por defecto
      signalsim0 = 0;
      logMessage(1, "⚠️  No se pudo obtener señal CESQ, usando valor 0");
    }
    
    if (i < 1 && (iccidsim0.length() == 0 || signalsim0 == 0)) {
      delay(500);
    }
  }

  logMessage(2, "📱 ICCID: " + iccidsim0);
  logMessage(2, "📶 Calidad de señal (normalizada): " + String(signalsim0));

 
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

  Serial.println("📱 ICCID: " + iccidsim0);
  Serial.println("📶 Calidad de señal: " + String(signalsim0));
  data->H_rsi = (byte)signalsim0;

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

  logMessage(3, "🔄 Preparando datos de sensores");
  const int STRUCT_SIZE = sizeof(sensordata_type);
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
    Serial.println("Tamaño estructura: " + STRUCT_SIZE);
  }

  logMessage(3, "🔄 Construyendo cadena de envío");
  int longcadena = longitudIccidsim0 + STRUCT_SIZE + 2;
  char cadena[longcadena];

  memcpy(cadena, iccidHex, sizeof(iccidHex));
  memcpy(cadena + sizeof(iccidHex), dataBufferSensor, sizeof(dataBufferSensor));

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
  const int maxRetries = 2; 

  while (!modem.testAT(500)) {
    flushPortSerial();
    logMessage(3, "🔄 Esperando respuesta del módem...");

    if (retry++ > maxRetries) {
      modemPwrKeyPulse();
      sendATCommand("+CPIN?", "READY", 8000); 
      retry = 0;
      logMessage(2, "🔄 Reintentando inicio del módem");
    }
  }

  logMessage(2, "✅ Comunicación GSM establecida");
  
  logMessage(2, "📡 Encendiendo RF del módem");
  
  if (sendATCommand("+CFUN=1", "OK", 5000)) {
    logMessage(2, "✅ RF del módem activada correctamente");
  } else {
    logMessage(1, "⚠️  Error al activar RF, reintentando...");
    
    if (sendATCommand("+CFUN=1,1", "OK", 5000)) {
      logMessage(2, "✅ RF del módem activada con reinicio");
    } else {
      logMessage(0, "❌ Fallo crítico al activar RF del módem");
    }
  }
  
  delay(500);
  
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

 
  if (LittleFS.exists(ARCHIVO_BUFFER)) {
    File f = LittleFS.open(ARCHIVO_BUFFER, "r");
    while (f.available()) {
      String linea = f.readStringUntil('\n');
      linea.trim();
      if (linea.length() > 0) lineas.push_back(linea);
    }
    f.close();
  }

  int noEnviadas = 0;
  for (String l : lineas) {
    if (!l.startsWith("#ENVIADO")) noEnviadas++;
  }

  if (noEnviadas >= MAX_LINEAS) {
    logMessage(1, "⚠️  Buffer lleno, eliminando dato más antiguo");

    for (size_t i = 0; i < lineas.size(); i++) {
      if (!lineas[i].startsWith("#ENVIADO")) {
        lineas.erase(lineas.begin() + i);
        logMessage(3, "🗑️  Dato antiguo eliminado del buffer");
        break;
      }
    }
  }

  lineas.push_back(data);
  File f = LittleFS.open(ARCHIVO_BUFFER, "w");
  for (String l : lineas) f.println(l);
  f.close();

  logMessage(2, "✅ Dato guardado en buffer. Total en buffer: " + String(lineas.size()) + " líneas");
}

/**
 * Envía datos pendientes del buffer con gestión de errores mejorada
 */
bool enviarDatos() {
  logMessage(2, "📤 Iniciando envío de datos del buffer");

  if (!LittleFS.exists(ARCHIVO_BUFFER)) {
    logMessage(2, "ℹ️  No hay archivo de buffer para enviar");
    return true;  // No hay datos = éxito
  }

  // CRÍTICO: Cerrar cualquier TCP residual antes de abrir nuevo
  tcpClose();
  delay(300);

  if (!tcpOpen()) {
    logMessage(0, "❌ No se pudo abrir conexión TCP para envío");
    return false;
  }

  File fin = LittleFS.open(ARCHIVO_BUFFER, "r");
  if (!fin) {
    logMessage(0, "❌ No se pudo abrir archivo de buffer para lectura");
    tcpClose();
    return false;
  }

  fin.setTimeout(50);  

 
  const char* TMP = "/buffer.tmp";
  File fout = LittleFS.open(TMP, "w");
  if (!fout) {
    logMessage(0, "❌ No se pudo crear archivo temporal");
    fin.close();
    tcpClose();
    return false;
  }

  bool conexion_ok = true;
  int datosEnviados = 0;
  int datosFallidos = 0;

  while (fin.available()) {
    String linea = fin.readStringUntil('\n');
    linea.trim();
    if (linea.length() == 0) continue;  

    if (linea.startsWith("#ENVIADO")) {
      fout.println(linea); 
      continue;
    }

    if (conexion_ok) {
      unsigned long timeout = getAdaptiveTimeout();
      if (tcpSendData(linea, timeout)) {
        logMessage(2, "✅ Enviado: " + linea.substring(0, 50) + "...");
        fout.println("#ENVIADO " + linea);  
        datosEnviados++;
      } else {
        logMessage(1, "❌ Falló envío: " + linea.substring(0, 50) + "...");
        fout.println(linea);  
        conexion_ok = false;  
        datosFallidos++;
      }
    } else {
   
      fout.println(linea);
    }
  }

  fin.close();
  fout.close();
  tcpClose();

  
  LittleFS.remove(ARCHIVO_BUFFER);
  LittleFS.rename(TMP, ARCHIVO_BUFFER);

  logMessage(2, "📊 Resumen de envío: " + String(datosEnviados) + " enviados, " + String(datosFallidos) + " fallidos");
  
  // Retornar éxito solo si todos los datos fueron enviados
  return (datosFallidos == 0 && datosEnviados > 0);
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

    
      if (buf.length() > 512) buf.remove(0, buf.length() - 256);

      if (buf.indexOf(token) >= 0) return true;
    }
    delay(1);  
  }

  return false;  
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

      for (size_t i = 0; i < errCount; ++i) {
        if (buf.indexOf(errTokens[i]) >= 0) return -1;
      }

  
      for (size_t i = 0; i < okCount; ++i) {
        if (buf.indexOf(okTokens[i]) >= 0) return 1;  
      }
    }
    delay(1);
  }

  return 0; 
}

/**
 * Envía datos TCP con gestión robusta de errores
 * @param datos - Datos a enviar
 * @param timeout_ms - Timeout en milisegundos
 * @return true si el envío es exitoso
 */
bool tcpSendData(const String& datos, uint32_t timeout_ms) {
  logMessage(3, "📤 Enviando " + String(datos.length()) + " bytes por TCP");

  flushPortSerial();
  while (SerialAT.available()) SerialAT.read();

 
  const size_t len = datos.length() + 2;


  modem.sendAT(String("+CASEND=0,") + String(len));
  if (!waitForToken(SerialAT, ">", timeout_ms)) {
    logMessage(0, "❌ Timeout esperando prompt '>' para envío");
    return false;
  }


  modem.sendAT(datos);
  modem.sendAT("\r\n");

  
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
    crc ^= (uint8_t)data[i];  

    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x0001) {
        crc = (crc >> 1) ^ 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;  
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
  buf[len++] = (char)(crc & 0xFF);        
  buf[len++] = (char)((crc >> 8) & 0xFF);  

  return len;
}

/** @} */ // fin grupo Utility_Functions

/**
 * @mainpage Sistema GSM/LTE para Sensores Elathia
 * 
 * @section intro_sec Introducción
 * 
 * Este sistema implementa una solución robusta de comunicación GSM/LTE para dispositivos IoT
 * con las siguientes características principales:
 * 
 * @subsection features_subsec Características Principales
 * 
 * - 🔄 **Selección Automática de Operadores**: Prueba secuencialmente diferentes operadores
 * - 📡 **Conectividad Robusta**: Fallback automático entre Altan, AT&T, Movistar y Telcel
 * - 💾 **Buffer Inteligente**: Almacenamiento local con gestión automática de espacio
 * - 🔐 **Seguridad**: Encriptación AES de todos los datos sensibles
 * - 🛰️ **GPS Integrado**: Obtención automática de coordenadas del módem
 * - ⚡ **Eficiencia Energética**: Gestión inteligente de energía del módem
 * - 📊 **Monitoreo Avanzado**: Sistema de logging con múltiples niveles
 * 
 * @subsection workflow_subsec Flujo de Trabajo
 * 
 * 1. **Inicialización**: Configuración del módem y sistema de archivos
 * 2. **Preparación**: Encriptación y almacenamiento local de datos
 * 3. **Conexión**: Selección automática del mejor operador disponible
 * 4. **Transmisión**: Envío de todos los datos pendientes
 * 5. **Limpieza**: Eliminación de datos enviados exitosamente
 * 6. **Hibernación**: Apagado del módem para conservar energía
 * 
 * @subsection usage_subsec Uso Básico
 * 
 * ```cpp
 * #include "gsmlte.h"
 * 
 * sensordata_type data;
 * // ... llenar estructura con datos de sensores ...
 * 
 * setupModem(&data);  // Maneja todo automáticamente
 * ```
 * 
 * @author Elathia
 * @version 2.0
 * @date 2025-10-30
 */