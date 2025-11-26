/**
 * @file gsmlte.h
 * @brief Sistema de comunicación LTE/GSM con selección automática de operadores - Definiciones e Interfaces
 * 
 * Este archivo contiene las definiciones, estructuras y declaraciones de funciones para el
 * sistema robusto de comunicación LTE/GSM que incluye:
 * 
 * **Características Principales:**
 * - 🔄 Selección secuencial de operadores (Altan, AT&T, Movistar, Telcel)
 * - 📡 Conexión automática con fallback inteligente entre operadores  
 * - 💾 Buffer local con gestión automática de espacio en LittleFS
 * - 🔐 Encriptación AES de datos sensibles
 * - 🛰️ GPS integrado del módem SIM7080G
 * - ⚡ Gestión eficiente de energía con apagado automático
 * - 📊 Sistema de logging estructurado con múltiples niveles
 * - ⏱️ Timeouts adaptativos basados en calidad de señal
 * 
 * **Hardware Soportado:**
 * - ESP32-S3 como microcontrolador principal
 * - Módem SIM7080G con soporte CAT-M/NB-IoT
 * - GPS integrado del módem
 * 
 * @author Elathia
 * @version 2.0
 * @date 2025-10-30
 * @since 1.0
 * 
 * @see gsmlte.cpp
 * @see type_def.h
 * @see cryptoaes.h
 * 
 * @note Requiere TinyGSM library para comunicación con el módem
 * @note Compatible con Arduino IDE y PlatformIO
 */

#ifndef GSMLTE_H
#define GSMLTE_H

#include <stdint.h>
#include "Arduino.h"
#include "type_def.h"

#define UART_BAUD 115200
#define PIN_DTR 25
#define PIN_TX 10
#define PIN_RX 11
#define PWRKEY_PIN 9
#define LED_PIN 12

#define SEND_RETRIES 3
#define SHORT_DELAY 500
#define LONG_DELAY 1500
#define MODEM_PWRKEY_DELAY 1200
#define MODEM_STABILIZE_DELAY 1000

#define DB_SERVER_IP "d04.elathia.ai"
//#define DB_SERVER_IP "sensores.lolaberries.com.mx"
#define TCP_PORT "12607"
#define NTP_SERVER_IP "200.23.51.102"

#define MODEM_NETWORK_MODE 38
#define CAT_M 1
#define NB_IOT 2
#define CAT_M_NB_IOT 3

#define TINY_GSM_MODEM_SIM7080
#define TINY_GSM_RX_BUFFER 1024
#define TINY_GSM_YIELD_MS 10
#define TINY_GSM_MODEM_HAS_GPS
#define SerialAT Serial1
#define SerialMon Serial
#define PDP_CONTEXT 1
#define APN "\"em\""

#define GPS_RETRIES 100
#define MAX_LINEAS 10

/**
 * @defgroup GSM_LTE_Structures Estructuras y Tipos de Datos
 * @brief Definiciones de estructuras para configuración y datos del sistema GSM/LTE
 * @{
 */

/**
 * @struct ModemConfig
 * @brief Estructura de configuración dinámica del módem
 * 
 * Contiene todos los parámetros configurables del módem para permitir
 * ajustes dinámicos sin recompilación del código.
 */
struct ModemConfig {
  String serverIP;          /**< Dirección IP del servidor de destino */
  String serverPort;        /**< Puerto TCP del servidor de destino */
  String apn;              /**< Access Point Name del operador */
  int networkMode;         /**< Modo de red (LTE/GSM) */
  int bandMode;            /**< Modo de banda (CAT-M/NB-IoT) */
  int maxRetries;          /**< Número máximo de reintentos */
  unsigned long baseTimeout; /**< Timeout base en milisegundos */
  bool enableDebug;        /**< Habilita logging detallado */
  bool showOperatorList;   /**< Muestra listado COPS antes de conectar */
};

// Estructura para información de operadores (definida en .cpp)
struct OperatorInfo;

/** @} */ // fin grupo GSM_LTE_Structures

/**
 * @defgroup GSM_LTE_Variables Variables Globales
 * @brief Variables de estado y configuración del sistema accesibles externamente
 * @{
 */

/** @brief ICCID de la tarjeta SIM actual */
extern String iccidsim0;

/** @brief Calidad de señal actual (0-31, mayor es mejor) */
extern int signalsim0;

/** @brief Indica si el módem ha sido inicializado correctamente */
extern bool modemInitialized;

/** @brief Indica si el GPS está habilitado y funcionando */
extern bool gpsEnabled;

/** @brief Contador de fallos consecutivos de conexión */
extern int consecutiveFailures;

/** @brief Número total de operadores disponibles dinámicamente */
extern int NUM_OPERATORS;

/** @} */ // fin grupo GSM_LTE_Variables



/**
 * @defgroup Configuration_Management Gestión de Configuración
 * @brief Funciones para inicialización y configuración del sistema
 * @{
 */

/**
 * @brief Inicializa la configuración por defecto del módem
 * 
 * Establece todos los parámetros necesarios para el funcionamiento del módem,
 * incluyendo servidor, APN, timeouts y modos de operación.
 */
void initModemConfig();

/**
 * @brief Calcula timeout adaptativo basado en calidad de señal y fallos
 * 
 * Implementa algoritmo inteligente que ajusta timeouts dinámicamente según
 * la calidad de señal actual y el historial de fallos consecutivos.
 * 
 * @return Timeout calculado en milisegundos (3000-20000ms)
 */
unsigned long getAdaptiveTimeout();

/** @} */ // fin grupo Configuration_Management

/**
 * @defgroup Logging_System Sistema de Logging
 * @brief Funciones para registro y diagnóstico del sistema
 * @{
 */

/**
 * @brief Sistema de logging estructurado con niveles y timestamps
 * 
 * Proporciona logging unificado con diferentes niveles de severidad
 * y control automático de verbosidad basado en configuración de debug.
 * 
 * @param level Nivel de severidad (0=Error, 1=Warning, 2=Info, 3=Debug)
 * @param message Mensaje a registrar
 */
void logMessage(int level, const String& message);

/**
 * @brief Obtiene estadísticas completas del sistema
 * @return String con estadísticas formateadas para diagnóstico
 */
String getSystemStats();

/** @} */ // fin grupo Logging_System

/**
 * @defgroup Modem_Communication Comunicación con el Módem
 * @brief Funciones de bajo nivel para comunicación con el módem SIM7080G
 * @{
 */

/**
 * @brief Limpia todos los buffers de comunicación serial del módem
 * 
 * Elimina datos residuales del buffer para evitar interferencias
 * en comandos posteriores.
 */
void flushPortSerial();

/**
 * @brief Envía comando AT y espera respuesta específica con timeout
 * 
 * Función principal para comunicación AT con el módem, incluye
 * manejo de timeouts adaptativos y logging de errores.
 * 
 * @param command Comando AT a enviar (sin prefijo "AT")
 * @param expectedResponse Respuesta esperada del módem
 * @param timeout Timeout base en milisegundos
 * @return true si se recibe la respuesta esperada
 */
bool sendATCommand(const String& command, const String& expectedResponse, unsigned long timeout);

/**
 * @brief Envía comando AT de configuración con timeout optimizado
 * 
 * Versión optimizada para comandos de configuración que no requieren
 * mucho tiempo de espera (como CNMP, CMNB, CBANDCFG, etc.)
 * 
 * @param command Comando AT a enviar (sin prefijo "AT")  
 * @param expectedResponse Respuesta esperada del módem (por defecto "OK")
 * @param timeout_ms Timeout en milisegundos (por defecto 2000ms)
 * @return true si se recibe la respuesta esperada
 */
bool sendConfigCommand(const String& command, const String& expectedResponse = "OK", unsigned long timeout_ms = 2000);

/**
 * @brief Lee respuesta del módem con timeout adaptativo
 * 
 * Función para leer respuestas completas del módem con timeout
 * y limpieza automática de buffers.
 * 
 * @param timeout Timeout base en milisegundos
 * @return Respuesta del módem como String
 */
String readResponse(unsigned long timeout);

/**
 * @brief Inicia comunicación GSM básica con el módem
 * 
 * Establece comunicación inicial, verifica SIM y activa funcionalidad
 * de radio del módem.
 */
void startGsm();

/**
 * @brief Controla el pin de encendido/apagado del módem
 * 
 * Genera pulso en el pin PWRKEY para encender o apagar el módem
 * con timing optimizado.
 */
void modemPwrKeyPulse();

/** @} */ // fin grupo Modem_Communication

/**
 * @brief Limpia buffer eliminando datos ya enviados exitosamente
 * 
 * Reescribe archivo de buffer conservando solo datos pendientes
 * (elimina líneas marcadas con #ENVIADO).
 */
void limpiarEnviados();

/** @} */ // fin grupo Data_Management

/**
 * @defgroup Utility_Functions Funciones de Utilidad
 * @brief Funciones auxiliares para cálculos, validación y sistema de archivos
 * @{
 */

/**
 * @brief Calcula CRC16 Modbus para verificación de integridad
 * 
 * Implementa algoritmo CRC16 estándar para detección de errores
 * en transmisión de datos.
 * 
 * @param data Puntero a datos de entrada
 * @param length Longitud de los datos en bytes
 * @return Valor CRC16 calculado
 */
uint16_t crc16(const char* data, size_t length);

/**
 * @brief Agrega CRC16 al final de un buffer de datos
 * 
 * Calcula CRC16 de los datos existentes y lo añade al final
 * del buffer en formato little-endian.
 * 
 * @param buf Buffer de datos (modificado in-place)
 * @param len Longitud actual del buffer
 * @param cap Capacidad total del buffer
 * @return Nueva longitud con CRC16 incluido
 */
size_t append_crc16_to_char_array(char* buf, size_t len, size_t cap);

/**
 * @brief Inicializa sistema de archivos LittleFS con reintentos
 * 
 * Monta LittleFS con múltiples intentos y reporta espacio disponible.
 * Función crítica para funcionamiento del buffer local.
 * 
 * @param intentos Número máximo de intentos de montaje
 * @param espera_ms Tiempo de espera entre intentos
 * @return true si se monta exitosamente
 */
bool iniciarLittleFS(int intentos, uint32_t espera_ms);

/** @} */ // fin grupo Utility_Functions

/**
 * @defgroup LTE_Core Sistema LTE Principal
 * @brief Funciones principales para gestión de conexiones LTE y operadores
 * @{
 */

/**
 * @brief Función principal de configuración e inicialización del sistema GSM/LTE
 * 
 * Ejecuta secuencia completa: configuración → inicialización → envío de datos.
 * Maneja automáticamente todos los errores críticos y reinicia en caso necesario.
 * 
 * @param data Puntero a estructura con datos de sensores a transmitir
 * @warning Función bloqueante - puede tardar varios minutos
 */
void setupModem(sensordata_type* data);

/**
 * @brief Inicia conexión LTE con estrategia secuencial de operadores
 * 
 * Implementa nueva estrategia que prueba operadores secuencialmente
 * hasta lograr conexión Y envío exitoso: Altan → AT&T → Movistar → Telcel
 * 
 * @return true si conexión y envío exitosos con algún operador
 */
bool startLTE();

/**
 * @brief Conecta con operador específico e intenta enviar todos los datos
 * 
 * Ciclo completo: configuración → conexión → envío → verificación.
 * Actualiza métricas de señal y contadores de fallos automáticamente.
 * 
 * @param operatorIndex Índice del operador en array operators[] (0-3)
 * @return true si se conectó Y envió todos los datos exitosamente
 */
bool connectAndSendWithOperator(int operatorIndex);

/** @} */ // fin grupo LTE_Core



/**
 * @defgroup TCP_Communication Comunicación TCP
 * @brief Funciones para manejo de conexiones TCP con gestión robusta de errores
 * @{
 */

/**
 * @brief Abre conexión TCP al servidor con reintentos adaptativos
 * 
 * Utiliza timeouts adaptativos basados en calidad de señal y
 * realiza múltiples intentos según configuración maxRetries.
 * 
 * @return true si la conexión se establece exitosamente
 */
bool tcpOpen();

/**
 * @brief Envía datos por TCP con gestión robusta de errores y confirmación
 * 
 * Implementa protocolo de envío con espera de confirmación del módem
 * y manejo de múltiples tokens de respuesta (éxito/error).
 * 
 * @param datos String con datos a enviar
 * @param timeout_ms Timeout en milisegundos para la operación
 * @return true si el envío es confirmado por el módem
 */
bool tcpSendData(const String& datos, uint32_t timeout_ms);



/**
 * @brief Cierra la conexión TCP activa de forma limpia
 * @return true si se cierra exitosamente
 */
bool tcpClose();



/** @} */ // fin grupo TCP_Communication



/**
 * @defgroup GPS_System Sistema GPS Integrado
 * @brief Funciones para manejo del GPS integrado del módem SIM7080G
 * @{
 */

/**
 * @brief Configuración completa del GPS: inicio → obtención → almacenamiento → apagado
 * 
 * Secuencia completa de GPS que incluye configuración del módem, obtención de
 * coordenadas con reintentos, conversión a bytes y apagado para ahorro de energía.
 * 
 * @param data Estructura donde almacenar coordenadas GPS obtenidas
 */
void setupGpsSim(sensordata_type* data);

/**
 * @brief Inicia y configura el módulo GPS integrado del módem
 * 
 * Activa GPS, configura modo GNSS y prepara el sistema para
 * obtención de coordenadas.
 * 
 * @return true si el GPS se inicia exitosamente
 */
bool startGps();

/**
 * @brief Detiene el módulo GPS y limpia variables relacionadas
 * 
 * Desactiva GPS del módem y resetea todas las variables GPS
 * para ahorrar energía.
 * 
 * @return true si el GPS se detiene exitosamente
 */
bool stopGps();

/**
 * @brief Obtiene coordenadas GPS válidas con sistema de reintentos
 * 
 * Realiza hasta 50 intentos para obtener fix GPS válido.
 * Almacena coordenadas en variables globales y convertidores de bytes.
 * 
 * @return true si se obtienen coordenadas válidas
 */
bool getGpsSim();

/** @} */ // fin grupo GPS_System

/**
 * @defgroup Data_Management Gestión de Datos
 * @brief Funciones para preparación, encriptación y almacenamiento de datos
 * @{
 */

/**
 * @brief Obtiene calidad de señal LTE usando comando AT+CESQ (única fuente de métricas)
 * 
 * Ejecuta comando AT+CESQ y extrae métricas LTE (RSRQ/RSRP).
 * Convierte valores a escala 0-31 compatible con getAdaptiveTimeout().
 * 
 * @return Valor normalizado de señal (0-31), -1 si error o no disponible
 * 
 * @note RSRQ (índice 4): 0-34, mayor es mejor (usado como métrica principal)
 * @note RSRP (índice 5): 0-97, usado como fallback si RSRQ no disponible
 * @note 255 = valor desconocido/no disponible
 * @note NO usa AT+CSQ - exclusivamente AT+CESQ para métricas LTE precisas
 */
int getSignalQualityFromCESQ();

/**
 * @brief Obtiene información de la tarjeta SIM y métricas de señal
 * 
 * Lee ICCID de la SIM y evalúa calidad de señal actual usando exclusivamente AT+CESQ.
 * Actualiza variables globales iccidsim0 y signalsim0.
 * Si CESQ falla, signalsim0 se establece en 0.
 * 
 * @note NO usa AT+CSQ - solo AT+CESQ para métricas LTE reales
 */
void getIccid();

/**
 * @brief Prepara, encripta y formatea datos de sensores para transmisión
 * 
 * Proceso completo: ICCID → datos sensores → CRC16 → encriptación AES.
 * Genera payload final listo para envío por TCP.
 * 
 * @param data Puntero a estructura con datos de sensores
 * @return String con datos encriptados listos para envío
 */
String dataSend(sensordata_type* data);

/**
 * @brief Guarda datos en buffer local con gestión inteligente de espacio
 * 
 * Almacena datos en LittleFS con gestión automática de espacio.
 * Elimina datos más antiguos si se alcanza el límite máximo.
 * 
 * @param data String con datos a almacenar en buffer
 */
void guardarDato(String data);

/**
 * @brief Envía todos los datos pendientes del buffer con gestión de errores
 * 
 * Lee buffer local y envía datos línea por línea. Marca datos enviados
 * exitosamente y conserva los que fallan para reintento posterior.
 * 
 * IMPORTANTE: Esta función gestiona TODO el ciclo de vida TCP internamente
 * (open, send, close). Cierra cualquier TCP residual antes de abrir nuevo.
 * 
 * @return true si el envío fue completamente exitoso (sin fallos)
 * @return false si hubo errores de conexión TCP o envío
 */
bool enviarDatos();

/** @} */ // fin grupo Data_Management

/**
 * @brief Espera múltiples tokens con timeout para comunicación robusta
 * 
 * Función interna para esperar respuestas del módem con múltiples
 * posibles tokens de éxito o error.
 * 
 * @param s Stream de comunicación a monitorear
 * @param okTokens Array de tokens que indican éxito
 * @param okCount Número de tokens de éxito
 * @param errTokens Array de tokens que indican error
 * @param errCount Número de tokens de error
 * @param timeout_ms Timeout total en milisegundos
 * @return 1=Éxito, -1=Error, 0=Timeout
 */
static int8_t waitForAnyToken(Stream& s,
                              const char* okTokens[], size_t okCount,
                              const char* errTokens[], size_t errCount,
                              uint32_t timeout_ms);

/**
 * @defgroup Operator_Management Gestión Automática de Operadores
 * @brief Funciones para evaluación, selección y gestión de operadores de red
 * @{
 */

/**
 * @brief Parsea respuesta AT+CPSI? para extraer métricas de señal detalladas
 * 
 * Analiza respuesta del comando CPSI y extrae RSRP, RSRQ, RSSI, SNR
 * para evaluación de calidad de operador.
 * 
 * @param cpsiResponse String con respuesta del comando CPSI
 * @param operatorInfo Estructura donde almacenar métricas parseadas
 * @return true si el parseo es exitoso
 */
bool parseCpsiResponse(const String& cpsiResponse, struct OperatorInfo& operatorInfo);

/**
 * @brief Calcula puntuación de operador basándose en métricas de señal
 * 
 * Algoritmo de scoring que evalúa RSRP, RSRQ y SNR para generar
 * puntuación comparable entre operadores.
 * 
 * @param operatorInfo Estructura con información del operador
 * @return Puntuación calculada (mayor es mejor, -1000 si desconectado)
 */
int calculateOperatorScore(const struct OperatorInfo& operatorInfo);

/**
 * @brief Obtiene información CPSI del módem con timeout controlado
 * 
 * Ejecuta comando AT+CPSI? y captura respuesta completa para
 * análisis posterior de métricas.
 * 
 * @param response String donde almacenar respuesta CPSI
 * @param timeout_ms Timeout para la operación
 * @return true si se obtiene respuesta válida
 */
bool getCpsiResponse(String& response, unsigned long timeout_ms);

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
bool logCpsiInfo(const String& operatorName);

/**
 * @brief Obtiene y muestra listado completo de operadores disponibles (COPS)
 * 
 * Ejecuta el comando AT+COPS=? para obtener la lista de todos los operadores
 * disponibles en la zona, incluyendo su estado (disponible, actual, prohibido)
 * y muestra la información de forma estructurada en el log.
 * 
 * @return true si se obtuvo el listado exitosamente
 */
bool showAvailableOperators();

/**
 * @brief Parsea la respuesta del comando AT+COPS=? y muestra operadores de forma legible
 * 
 * Analiza la respuesta del comando COPS y extrae información de cada operador
 * disponible, mostrándola de forma estructurada y fácil de leer.
 * 
 * @param copsResponse String con la respuesta completa del comando COPS
 */
void parseCopsResponse(const String& copsResponse);

/**
 * @brief Convierte código de estado de operador a texto legible
 * @param statusCode Código de estado del operador
 * @return Texto descriptivo del estado
 */
String getOperatorStatusText(const String& statusCode);

/**
 * @brief Convierte código de tecnología de acceso a texto legible
 * @param techCode Código de tecnología
 * @return Texto descriptivo de la tecnología
 */
String getTechnologyText(const String& techCode);

/**
 * @brief Limpia y reinicializa el array dinámico de operadores
 * 
 * Libera memoria del array actual y lo prepara para recibir nuevos operadores
 * desde la respuesta COPS.
 */
void clearOperators();

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
int addOperator(const String& longName, const String& shortName, const String& numeric, int status);

/**
 * @brief Desregistra de la red correctamente esperando URC +CEREG: 0
 * 
 * SIM7080 puede tardar varios segundos en liberar la celda tras AT+COPS=2.
 * Esta función espera la confirmación +CEREG: 0 o hace reset rápido (AT+CFUN=1,1) si no llega.
 * 
 * @return true si se desregistró exitosamente
 * 
 * @note Espera hasta 5 segundos por +CEREG: 0
 * @note Si no llega, ejecuta AT+CFUN=1,1 para reset rápido del módulo de radio
 */
bool deregisterFromNetwork();

/**
 * @brief Limpia sesión PDP activa antes de cambiar operador
 * 
 * SIM7080 puede mantener sesiones PDP antiguas que causan errores.
 * Desactiva cualquier contexto PDP residual con AT+CNACT=0,0.
 * 
 * @return true si se limpió exitosamente
 */
bool cleanPDPContext();

/**
 * @brief Espera registro en red verificando +CEREG
 * 
 * Espera hasta que el módem se registre en la red.
 * - CEREG: 1 = registrado red local
 * - CEREG: 5 = registrado roaming
 * 
 * @param timeout_ms Tiempo máximo de espera en milisegundos
 * @return true si se registró exitosamente (CEREG: 1 o 5)
 * 
 * @note Verifica AT+CEREG? periódicamente
 */
bool waitForNetworkRegistration(unsigned long timeout_ms);

/**
 * @brief Verifica que PDP esté activo con IP asignada
 * 
 * isNetworkConnected() puede dar falsos positivos.
 * Esta función verifica AT+CNACT? para confirmar IP asignada.
 * Busca respuesta: +CNACT: 0,1,"<IP>"
 * 
 * @return String con la IP asignada, cadena vacía ("") si PDP no está activo o no hay IP
 */
String verifyPDPActive();

/**
 * @brief Evalúa calidad de señal de un operador específico usando AT+CESQ
 * 
 * Conecta temporalmente al operador y obtiene métricas LTE (RSRQ, RSRP)
 * para calcular su puntuación de calidad. Actualiza campos rsrq, rsrp y score.
 * 
 * @param operatorIndex Índice del operador en el array operators[]
 * @return true si se evaluó exitosamente
 * 
 * @note Usa deregisterFromNetwork() antes de evaluar
 * @note Espera hasta 10 segundos para registro en red
 */
bool evaluateOperatorSignal(int operatorIndex);

/**
 * @brief Evalúa todos los operadores disponibles y los ordena por calidad de señal
 * 
 * ⚠️ ADVERTENCIA CRÍTICA: Esta función mide la señal de la red ACTUAL,
 * NO de la red destino. La medición no es útil para seleccionar operadores.
 * 
 * ❌ NO USAR en producción - genera falsos positivos
 * ⚠️ La señal medida pertenece a la celda actual, no al operador que vas a probar
 * ✅ Mejor estrategia: probar operadores directamente sin evaluación previa
 * 
 * @deprecated No proporciona información útil para selección real de operador
 * @note Solo útil para debugging/testing de conectividad
 * @note Proceso puede tardar varios minutos (evaluación de cada operador)
 * @note Desregistra de red al finalizar para empezar limpio
 */
void evaluateAndSortOperators();

/**
 * @brief Ordena operadores por prioridad basándose en la lista de preferidos
 * 
 * Reorganiza el array de operadores poniendo primero los operadores preferidos
 * en el orden especificado, seguidos por el resto de operadores disponibles.
 * 
 * @deprecated Usar evaluateAndSortOperators() para ordenamiento basado en métricas reales
 */
void sortOperatorsByPriority();









/** @} */ // fin grupo Operator_Management

#endif // GSMLTE_H