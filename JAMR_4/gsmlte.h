// =============================================================================
// ARCHIVO: gsmlte.h
// DESCRIPCIÓN: Archivo de cabecera para el sistema de comunicación LTE/GSM
// FUNCIONALIDADES:
//   - Configuración de comunicación serial y pines del módem
//   - Definiciones para redes LTE (CAT-M/NB-IoT)
//   - Estructuras de configuración dinámica del módem
//   - Declaraciones de funciones del sistema de comunicación
//   - Sistema de logging y manejo de errores robusto
// =============================================================================

#ifndef GSMLTE_H
#define GSMLTE_H

#include <stdint.h>
#include "Arduino.h"
#include "type_def.h"

// =============================================================================
// CONFIGURACIÓN DE COMUNICACIÓN SERIAL
// =============================================================================

#define UART_BAUD 115200          // Velocidad de comunicación serial con el módem
#define PIN_DTR 25                // Pin DTR (Data Terminal Ready) para control del módem
#define PIN_TX 10                 // Pin de transmisión serial al módem
#define PIN_RX 11                 // Pin de recepción serial del módem
#define PWRKEY_PIN 9              // Pin para control de encendido/apagado del módem
#define LED_PIN 12                // Pin del LED indicador de estado

// =============================================================================
// CONFIGURACIÓN DE REINTENTOS Y TIMEOUTS
// =============================================================================

#define SEND_RETRIES 6            // Número máximo de reintentos para envío de datos
#define SHORT_DELAY 1000          // Delay corto en milisegundos
#define LONG_DELAY 3000           // Delay largo en milisegundos
#define MODEM_PWRKEY_DELAY 1200   // Tiempo de pulsación del pin PWRKEY (ms)
#define MODEM_STABILIZE_DELAY 3000 // Tiempo de estabilización del módem después de encendido (ms)

// =============================================================================
// CONFIGURACIÓN DEL SERVIDOR (CONFIGURABLE DINÁMICAMENTE)
// =============================================================================

#define DB_SERVER_IP "sensores.decisio.com.mx"  // IP del servidor de datos por defecto
#define TCP_PORT "12607"                        // Puerto TCP del servidor por defecto
#define NTP_SERVER_IP "200.23.51.102"          // IP del servidor NTP para sincronización de tiempo

// =============================================================================
// CONFIGURACIÓN DE RED LTE
// =============================================================================

#define MODEM_NETWORK_MODE 38     // Modo de red: 38 = CAT-M/NB-IoT
#define CAT_M 1                   // Modo CAT-M (LTE de baja potencia)
#define NB_IOT 2                  // Modo NB-IoT (Internet de las Cosas de banda estrecha)
#define CAT_M_NB_IOT 3            // Modo combinado CAT-M + NB-IoT

// =============================================================================
// CONFIGURACIÓN DEL MÓDEM SIM7080
// =============================================================================

#define TINY_GSM_MODEM_SIM7080    // Definir tipo de módem para la librería TinyGSM
#define TINY_GSM_RX_BUFFER 1024   // Tamaño del buffer de recepción
#define TINY_GSM_YIELD_MS 10      // Tiempo de yield para no bloquear el sistema
#define TINY_GSM_MODEM_HAS_GPS    // Indicar que el módem tiene GPS integrado
#define SerialAT Serial1           // Puerto serial para comunicación con el módem
#define SerialMon Serial           // Puerto serial para monitoreo/debug
#define PDP_CONTEXT 1             // Contexto PDP (Packet Data Protocol) a utilizar
#define APN "\"em\""              // APN (Access Point Name) de la red por defecto

// =============================================================================
// CONFIGURACIÓN GPS
// =============================================================================

#define GPS_RETRIES 100           // Número máximo de intentos para obtener fix GPS
#define MAX_LINEAS 10             // Número máximo de líneas en el buffer de datos

// =============================================================================
// FIX-3: PROTECCIÓN DEFENSIVA EN LOOPS CON MÓDEM
// =============================================================================

// Habilita lógica adicional para alimentar el watchdog y acotar timeouts
// en loops que interactúan con el módem/GPS. Mantener en true en producción
// una vez validado en campo.
#define ENABLE_WDT_DEFENSIVE_LOOPS true

// =============================================================================
// FIX-4: OPERADOR EFICIENTE MULTI-PERFIL
// =============================================================================

// Controla la lógica de selección de operador eficiente. Dejar en true una vez
// validado; permite revertir rápidamente a startLTE único en caso necesario.
#define ENABLE_MULTI_OPERATOR_EFFICIENT true

// Presupuesto total por ciclo para intentar conexión LTE (en ms). Usado por la
// lógica multi-perfil para acotar consumo de energía en escenarios adversos.
static const uint32_t LTE_CONNECT_BUDGET_MS = 120000UL; // 120s máximo

// Perfil de operador/escenario: parametriza modo de red, bandas y APN.
struct OperatorProfile {
  const char* name;       // Nombre lógico del perfil
  int networkMode;        // Valor para +CNMP
  int bandMode;           // CAT_M, NB_IOT, etc.
  const char* apn;        // APN a emplear
  uint32_t maxTimeMs;     // Presupuesto máximo para este perfil
  uint8_t id;             // Identificador para tracking
};

// =============================================================================
// FIX-6: PRESUPUESTO GLOBAL DE CICLO DE COMUNICACIÓN
// =============================================================================

// Tiempo máximo permitido (ms) para todo el ciclo LTE/TCP antes de abortar.
// Ajustar tras pruebas de campo para balancear consumo vs. fiabilidad.
static const uint32_t COMM_CYCLE_BUDGET_MS = 150000UL; // 150s máximo

// Registra el inicio del ciclo (debe llamarse antes de cualquier intento LTE/TCP).
void resetCommunicationCycleBudget();

// Retorna milisegundos restantes antes de agotar el presupuesto global.
uint32_t remainingCommunicationCycleBudget();

// Verifica si aún queda presupuesto; si no, loguea y retorna false.
bool ensureCommunicationBudget(const char* contextTag);

// =============================================================================
// ESTRUCTURAS DE DATOS
// =============================================================================

/**
 * Estructura de configuración del módem
 * Permite configurar dinámicamente los parámetros del módem sin recompilar
 */
struct ModemConfig {
  String serverIP;                // IP del servidor de datos
  String serverPort;              // Puerto del servidor de datos
  String apn;                     // APN de la red móvil
  int networkMode;                // Modo de red (38=CAT-M/NB-IoT)
  int bandMode;                   // Modo de banda (1=CAT-M, 2=NB-IoT)
  int maxRetries;                 // Número máximo de reintentos
  unsigned long baseTimeout;      // Timeout base en milisegundos
  bool enableDebug;               // Habilitar/deshabilitar modo debug
};

// =============================================================================
// VARIABLES GLOBALES DEL SISTEMA
// =============================================================================

// Variables de estado del módem y comunicación
extern String iccidsim0;          // ICCID de la tarjeta SIM
extern int signalsim0;            // Calidad de señal actual
extern bool modemInitialized;     // Estado de inicialización del módem
extern bool gpsEnabled;           // Estado del GPS integrado
extern int consecutiveFailures;   // Contador de fallos consecutivos

// =============================================================================
// FIX-8: ESTADO DE SALUD DEL MÓDEM
// =============================================================================

// Estado de salud del módem en el ciclo actual
extern modem_health_state_t g_modem_health_state;

// =============================================================================
// FUNCIONES DE CONFIGURACIÓN DEL MÓDEM
// =============================================================================

/**
 * Inicializa la configuración por defecto del módem
 * Establece valores iniciales para todos los parámetros de configuración
 */
void initModemConfig();

/**
 * Configura parámetros del módem dinámicamente
 * Permite cambiar la configuración en tiempo de ejecución
 * 
 * @param serverIP - IP del servidor de datos
 * @param serverPort - Puerto del servidor de datos
 * @param apn - APN de la red móvil
 * @param networkMode - Modo de red (38=CAT-M/NB-IoT)
 * @param bandMode - Modo de banda (1=CAT-M, 2=NB-IoT)
 */
void configureModem(const String& serverIP, const String& serverPort, 
                   const String& apn, int networkMode, int bandMode);

/**
 * Habilita o deshabilita el modo debug
 * Controla el nivel de logging y mensajes de debug
 * 
 * @param enable - true para habilitar, false para deshabilitar
 */
void setDebugMode(bool enable);

// =============================================================================
// FUNCIONES DE TIMEOUT ADAPTATIVO
// =============================================================================

/**
 * Calcula timeout adaptativo basado en condiciones del sistema
 * Ajusta automáticamente los timeouts según calidad de señal y fallos previos
 * 
 * @return Timeout en milisegundos
 */
unsigned long getAdaptiveTimeout();

// =============================================================================
// SISTEMA DE LOGGING MEJORADO
// =============================================================================

/**
 * Sistema de logging estructurado con niveles y timestamps
 * Proporciona logging organizado con diferentes niveles de detalle
 * 
 * @param level - Nivel de log (0=Error, 1=Warning, 2=Info, 3=Debug)
 * @param message - Mensaje a loguear
 */
void logMessage(int level, const String& message);

// =============================================================================
// FUNCIONES PRINCIPALES DEL MÓDEM
// =============================================================================

/**
 * Controla el pin de encendido del módem con timing optimizado
 * Implementa secuencia de encendido/apagado del módem
 */
void modemPwrKeyPulse();

/**
 * Reinicia el módem completamente
 * Apaga y vuelve a encender el módem para recuperación de errores
 */
void modemRestart();

/**
 * Configura e inicializa el módem LTE/GSM
 * Función principal que coordina toda la inicialización del sistema
 * 
 * @param data - Estructura de datos de sensores para envío
 */
void setupModem(sensordata_type* data);

/**
 * Establece conexión LTE con configuración adaptativa
 * Configura y conecta a la red LTE con manejo robusto de errores
 * 
 * @return true si la conexión es exitosa
 */
bool startLTE();

/**
 * Limpia todos los buffers de comunicación serial
 * Elimina datos residuales para evitar interferencias
 */
void flushPortSerial();

/**
 * Envía comando AT y espera respuesta específica
 * Implementa comunicación AT robusta con timeouts adaptativos
 * 
 * @param command - Comando AT a enviar
 * @param expectedResponse - Respuesta esperada
 * @param timeout - Timeout en milisegundos
 * @return true si se recibe la respuesta esperada
 */
bool sendATCommand(const String& command, const String& expectedResponse, unsigned long timeout);

/**
 * Lee respuesta del módem con timeout adaptativo
 * Lee datos del módem con timeout inteligente
 * 
 * @param timeout - Timeout base en milisegundos
 * @return Respuesta del módem como String
 */
String readResponse(unsigned long timeout);

// =============================================================================
// FUNCIONES DE COMUNICACIÓN TCP
// =============================================================================

/**
 * Verifica el estado actual del socket TCP
 * Consulta el estado del socket antes de operaciones críticas
 * 
 * @return true si hay un socket abierto
 */
bool isSocketOpen();

/**
 * Abre conexión TCP con validación de estado previa
 * Verifica estado del socket antes de intentar abrir nueva conexión
 * 
 * @return true si la conexión se abre exitosamente
 */
bool tcpOpenSafe();

/**
 * Abre conexión TCP con reintentos adaptativos
 * Establece conexión TCP al servidor con manejo robusto de errores
 * 
 * @return true si la conexión se abre exitosamente
 */
bool tcpOpen();

/**
 * Envía datos TCP con gestión robusta de errores
 * Transmite datos por TCP con validación y reintentos
 * 
 * @param datos - Datos a enviar
 * @param timeout_ms - Timeout en milisegundos
 * @return true si el envío es exitoso
 */
bool tcpSendData(const String& datos, uint32_t timeout_ms);

/**
 * Lee datos de la conexión TCP
 * Recibe datos del servidor por la conexión TCP establecida
 * 
 * @return true si la lectura es exitosa
 */
bool tcpReadData();

/**
 * Cierra la conexión TCP
 * Cierra la conexión TCP de manera limpia
 * 
 * @return true si se cierra exitosamente
 */
bool tcpClose();

// =============================================================================
// FUNCIONES DE UTILIDAD
// =============================================================================

/**
 * Busca texto específico en una cadena principal
 * Implementa búsqueda de subcadenas para validación de respuestas
 * 
 * @param textoPrincipal - Texto donde buscar
 * @param textoBuscado - Texto a buscar
 * @return true si se encuentra el texto
 */
bool findText(String textoPrincipal, String textoBuscado);

// =============================================================================
// FUNCIONES GPS
// =============================================================================

/**
 * Configura y obtiene datos GPS del módem integrado
 * Inicializa el GPS y obtiene coordenadas para almacenamiento
 * 
 * @param data - Estructura de datos donde almacenar coordenadas GPS
 */
void setupGpsSim(sensordata_type* data);

/**
 * Inicia el módulo GPS integrado
 * Configura y habilita el GPS del módem
 * 
 * @return true si el GPS se inicia exitosamente
 */
bool startGps();

/**
 * Detiene el módulo GPS integrado
 * Deshabilita el GPS del módem y libera recursos
 * 
 * @return true si el GPS se detiene exitosamente
 */
bool stopGps();

/**
 * Obtiene coordenadas GPS con reintentos adaptativos
 * Lee coordenadas GPS con sistema robusto de reintentos
 * 
 * @return true si se obtienen coordenadas válidas
 */
bool getGpsSim();

/**
 * Obtiene información de la tarjeta SIM y calidad de señal
 * Lee ICCID y evalúa la calidad de la señal móvil
 */
void getIccid();

// =============================================================================
// FUNCIONES DE DATOS
// =============================================================================

/**
 * Prepara y encripta los datos para envío
 * Convierte datos de sensores a formato de envío con encriptación AES
 * 
 * @param data - Estructura de datos de sensores
 * @return String con datos encriptados
 */
String dataSend(sensordata_type* data);

/**
 * Inicia la comunicación GSM básica
 * Establece comunicación básica con el módem
 */
void startGsm();

/**
 * Guarda datos en el buffer local con gestión inteligente
 * Almacena datos en LittleFS con gestión de espacio y prioridades
 * 
 * @param data - Datos a guardar
 */
void guardarDato(String data);

/**
 * Envía datos pendientes del buffer con gestión de errores mejorada
 * Transmite datos almacenados localmente al servidor
 */
void enviarDatos();

/**
 * Limpia el buffer eliminando datos ya enviados
 * Mantiene solo datos pendientes de envío
 */
void limpiarEnviados();

// =============================================================================
// FUNCIONES DE COMUNICACIÓN AVANZADA
// =============================================================================

/**
 * Espera cualquiera de varios tokens con timeout
 * Implementa espera inteligente de múltiples respuestas posibles
 * 
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
                              uint32_t timeout_ms);

/**
 * Espera un token específico en el stream con timeout
 * Espera una respuesta específica del módem
 * 
 * @param s - Stream a monitorear
 * @param token - Token a buscar
 * @param timeout_ms - Timeout en milisegundos
 * @return true si se encuentra el token
 */
static bool waitForToken(Stream& s, const String& token, uint32_t timeout_ms);

// =============================================================================
// FUNCIONES DEL SISTEMA DE ARCHIVOS
// =============================================================================

/**
 * Inicializa el sistema de archivos LittleFS con reintentos
 * Monta el sistema de archivos con manejo robusto de errores
 * 
 * @param intentos - Número máximo de intentos
 * @param espera_ms - Tiempo de espera entre intentos
 * @return true si se monta exitosamente
 */
bool iniciarLittleFS(int intentos, uint32_t espera_ms);

// =============================================================================
// FUNCIONES DE VERIFICACIÓN DE INTEGRIDAD
// =============================================================================

/**
 * Calcula CRC16 para verificación de integridad de datos
 * Implementa algoritmo CRC16 para validación de datos transmitidos
 * 
 * @param data - Puntero a los datos
 * @param len - Longitud de los datos
 * @return Valor CRC16 calculado
 */
uint16_t crc16(const char* data, size_t len);

/**
 * Agrega CRC16 al final de un array de caracteres
 * Añade checksum CRC16 a los datos para verificación de integridad
 * 
 * @param buf - Buffer donde agregar el CRC
 * @param len - Longitud actual del buffer
 * @param cap - Capacidad total del buffer
 * @return Nueva longitud del buffer
 */
size_t append_crc16_to_char_array(char* buf, size_t len, size_t cap);

// =============================================================================
// FUNCIONES DE DIAGNÓSTICO Y ESTADÍSTICAS
// =============================================================================

/**
 * Obtiene estadísticas del sistema
 * Proporciona resumen completo del estado del sistema de comunicación
 * 
 * @return String con estadísticas formateadas
 */
String getSystemStats();

#endif