// =============================================================================
// ARCHIVO: sleepdev.h
// DESCRIPCIÓN: Cabecera del sistema de gestión de energía y modo deep sleep
// FUNCIONALIDADES:
//   - Definiciones de constantes para configuración de sleep
//   - Declaraciones de funciones de gestión de energía
//   - Configuración de parámetros del sistema
// =============================================================================

#ifndef SLEEPDEV_H
#define SLEEPDEV_H

// =============================================================================
// INCLUSIONES NECESARIAS
// =============================================================================
#include <stdint.h>    // Tipos de datos enteros de tamaño fijo
#include "Arduino.h"   // Funciones básicas de Arduino
#include "esp_task_wdt.h"  // 🆕 JAMR_3 FIX-001: Watchdog Timer

// =============================================================================
// CONSTANTES DE CONFIGURACIÓN DEL SISTEMA
// =============================================================================

// 🆕 JAMR_3 FIX-001: Configuración del Watchdog Timer
/**
 * TIMEOUT DEL WATCHDOG EN SEGUNDOS
 * 
 * Define el tiempo máximo que el sistema puede estar sin "alimentar" el watchdog
 * antes de que se ejecute un reset automático.
 * 
 * Valor: 120 segundos (2 minutos)
 * Justificación: Permite operaciones normales sin falsos positivos
 */
#define WATCHDOG_TIMEOUT_SEC 120

// =============================================================================
// 🆕 JAMR_3 FIX-004: SISTEMA DE DIAGNÓSTICO POSTMORTEM
// =============================================================================

/**
 * CHECKPOINTS DEL SISTEMA
 * 
 * Constantes que identifican puntos críticos en el flujo de ejecución.
 * Permiten rastrear dónde ocurrió un fallo cuando el watchdog resetea el sistema.
 * 
 * FLUJO NORMAL:
 * 1=BOOT → 2=GPIO_OK → 3=GPS_START → 4=GPS_FIX → 5=SENSORS_OK →
 * 6=MODEM_ON → 7=GSM_OK → 8=LTE_CONNECT → 9=LTE_OK →
 * 10=TCP_OPEN → 11=DATA_SENT → 12=SLEEP_READY
 * 
 * Si el sistema se cuelga, el último checkpoint guardado indica dónde falló.
 */
#define CP_BOOT          1   // Sistema iniciado (setup())
#define CP_GPIO_OK       2   // GPIO configurados correctamente
#define CP_GPS_START     3   // Iniciando GPS
#define CP_GPS_FIX       4   // GPS fix obtenido
#define CP_SENSORS_OK    5   // Sensores leídos correctamente
#define CP_MODEM_ON      6   // Modem encendido
#define CP_GSM_OK        7   // Modem respondiendo AT
#define CP_LTE_CONNECT   8   // Conectando a red LTE
#define CP_LTE_OK        9   // Conectado a red LTE
#define CP_TCP_OPEN      10  // Socket TCP abierto
#define CP_DATA_SENT     11  // Datos enviados exitosamente
#define CP_SLEEP_READY   12  // Listo para entrar a sleep (éxito completo)

/**
 * VARIABLES EN RTC MEMORY
 * 
 * Estas variables persisten durante deep sleep y resets por watchdog,
 * pero se pierden si se desconecta la alimentación completamente.
 * 
 * RTC_DATA_ATTR marca la variable para que persista durante resets.
 * IMPORTANTE: Se declaran como extern aquí y se definen en sleepdev.cpp
 */
extern RTC_DATA_ATTR uint8_t rtc_last_checkpoint;       // Último checkpoint alcanzado (0-12)
extern RTC_DATA_ATTR uint32_t rtc_timestamp_ms;         // Timestamp en millis cuando se actualizó
extern RTC_DATA_ATTR uint16_t rtc_boot_count;           // Contador total de boots
extern RTC_DATA_ATTR uint8_t rtc_crash_reason;          // Razón del último reset
extern RTC_DATA_ATTR uint32_t rtc_last_success_time;    // Época de última transmisión exitosa

// 🆕 FIX-12: Variables persistentes para filtro de media móvil de batería
extern RTC_DATA_ATTR float rtc_battery_history[5];      // Historial últimas 5 lecturas
extern RTC_DATA_ATTR uint8_t rtc_battery_index;         // Índice circular del historial
extern RTC_DATA_ATTR uint8_t rtc_battery_count;         // Contador de lecturas válidas

/**
 * RAZONES DE RESET
 * 
 * Códigos que identifican la causa del último reset del ESP32.
 * Se obtienen de esp_reset_reason() y se guardan para diagnóstico.
 */
#define RESET_UNKNOWN       0  // Razón desconocida
#define RESET_POWERON       1  // Encendido normal
#define RESET_SW            3  // Reset por software
#define RESET_DEEPSLEEP     5  // Despertar de deep sleep (normal)
#define RESET_TASK_WDT      7  // Watchdog Timer (crash detectado)
#define RESET_BROWNOUT      8  // Bajo voltaje crítico
#define RESET_PANIC         9  // Crash crítico de software

// =============================================================================
// 🆕 FIX-004: DECLARACIONES DE FUNCIONES DE DIAGNÓSTICO
// =============================================================================

/**
 * FUNCIÓN: updateCheckpoint()
 * 
 * DESCRIPCIÓN: Actualiza el checkpoint actual en RTC Memory
 * 
 * PARÁMETROS:
 *   checkpoint - Número de checkpoint (usar constantes CP_*)
 * 
 * FUNCIONALIDAD:
 *   - Guarda el checkpoint actual en RTC Memory
 *   - Actualiza timestamp con millis() actual
 *   - Persiste durante resets de watchdog
 * 
 * USO: Llamar en cada punto crítico del flujo:
 *   updateCheckpoint(CP_GPS_START);
 */
void updateCheckpoint(uint8_t checkpoint);

/**
 * FUNCIÓN: getCrashInfo()
 * 
 * DESCRIPCIÓN: Recupera información del último crash (si hubo)
 * 
 * RETORNO: true si hubo crash previo, false si boot normal
 * 
 * FUNCIONALIDAD:
 *   - Lee esp_reset_reason() para determinar tipo de reset
 *   - Si fue TASK_WDT, hubo crash por watchdog
 *   - Retorna información del último checkpoint alcanzado
 * 
 * USO: Llamar en setup() para detectar crashes:
 *   if (getCrashInfo()) {
 *     // Hubo crash, enviar diagnóstico
 *   }
 */
bool getCrashInfo();
void markCycleSuccess();

/**
 * FACTOR DE CONVERSIÓN DE MICROSEGUNDOS A SEGUNDOS

// =============================================================================
// CONSTANTES DE CONFIGURACIÓN DEL SISTEMA
// =============================================================================

/**
 * FACTOR DE CONVERSIÓN DE MICROSEGUNDOS A SEGUNDOS
 * 
 * El ESP32 requiere que el tiempo de wakeup se especifique en microsegundos,
 * pero es más conveniente trabajar en segundos para la configuración.
 * Este factor permite la conversión automática.
 * 
 * Valor: 1,000,000 (1 segundo = 1,000,000 microsegundos)
 */
#define uS_TO_S_FACTOR 1000000

/**
 * TIEMPO DE SLEEP EN SEGUNDOS
 * 
 * Define el intervalo de tiempo que el ESP32 permanecerá en modo deep sleep
 * antes de despertar automáticamente para realizar nuevas mediciones.
 * 
 * Valor actual: 600 segundos (10 minutos)
 * 
 * NOTAS:
 * - Este valor se multiplica por uS_TO_S_FACTOR para obtener microsegundos
 * - El tiempo real puede variar ligeramente debido a la precisión del timer
 * - Valores recomendados: entre 30 segundos y 3600 segundos (1 hora)
 * - Para mayor duración de batería, usar valores más altos
 * - Para monitoreo más frecuente, usar valores más bajos
 */
#define TIME_TO_SLEEP 1200

// =============================================================================
// DECLARACIONES DE FUNCIONES
// =============================================================================

/**
 * FUNCIÓN: setupGPIO()
 * 
 * DESCRIPCIÓN: Configura los pines GPIO y periféricos del sistema
 * 
 * FUNCIONALIDAD:
 * - Inicializa la comunicación serial para debugging
 * - Configura los pines de control de alimentación y periféricos
 * - Establece la comunicación I2C para sensores
 * - Libera el estado "hold" de los pines GPIO después de un deep sleep
 * 
 * PARÁMETROS: Ninguno
 * RETORNO: void
 * 
 * USO: Llamar al inicio del setup() después de un reset o despertar
 */
void setupGPIO();

/**
 * FUNCIÓN: sleepIOT()
 * 
 * DESCRIPCIÓN: Prepara y ejecuta la entrada en modo deep sleep
 * 
 * FUNCIONALIDAD:
 * - Desactiva todos los periféricos para minimizar consumo
 * - Configura los pines de control en estado de bajo consumo
 * - Establece el timer de wakeup automático
 * - Activa el modo GPIO Hold para mantener estado durante sleep
 * - Inicia el modo deep sleep del ESP32
 * 
 * PARÁMETROS: Ninguno
 * RETORNO: void (no retorna hasta el próximo despertar)
 * 
 * USO: Llamar al final del loop principal cuando se complete el ciclo de trabajo
 * 
 * NOTA: Esta función NO retorna hasta que el ESP32 se despierte nuevamente
 */
void sleepIOT();

/**
 * FUNCIÓN: printWakeupReason()
 * 
 * DESCRIPCIÓN: Diagnostica y muestra la causa que provocó el despertar del ESP32
 * 
 * FUNCIONALIDAD:
 * - Lee el registro de causa de wakeup del sistema
 * - Identifica el tipo específico de evento que despertó al dispositivo
 * - Muestra un mensaje descriptivo en el monitor serial
 * - Facilita el debugging y monitoreo del sistema
 * 
 * CAUSAS POSIBLES:
 * - ESP_SLEEP_WAKEUP_TIMER: Despertar por timer (caso normal)
 * - ESP_SLEEP_WAKEUP_EXT0/EXT1: Despertar por señal externa
 * - ESP_SLEEP_WAKEUP_TOUCHPAD: Despertar por toque
 * - ESP_SLEEP_WAKEUP_ULP: Despertar por programa ULP
 * 
 * PARÁMETROS: Ninguno
 * RETORNO: void
 * 
 * USO: Llamar al inicio del setup() para diagnosticar el despertar
 */
void printWakeupReason();

// =============================================================================
// NOTAS DE IMPLEMENTACIÓN
// =============================================================================

/*
 * CONFIGURACIÓN DE PINES GPIO:
 * 
 * El sistema utiliza los siguientes pines para control de energía:
 * 
 * - GPIO_NUM_3: Control de alimentación de sensores
 *   * LOW = Sensores desactivados (modo sleep)
 *   * HIGH = Sensores activados (modo normal)
 * 
 * - GPIO_NUM_7: Control de modo bajo consumo
 *   * LOW = Modo normal
 *   * HIGH = Modo bajo consumo activado
 * 
 * - GPIO_NUM_9: Control de otros periféricos
 *   * LOW = Periféricos desactivados
 *   * HIGH = Periféricos activados
 * 
 * CONFIGURACIÓN I2C:
 * 
 * - SDA: GPIO_NUM_6 (pin de datos)
 * - SCL: GPIO_NUM_12 (pin de reloj)
 * - Frecuencia: 100 kHz (estándar para sensores)
 * 
 * GESTIÓN DE ENERGÍA:
 * 
 * El sistema implementa una secuencia completa de gestión de energía:
 * 
 * 1. Desactivación de periféricos (I2C, sensores)
 * 2. Configuración de pines en estado de bajo consumo
 * 3. Activación de GPIO Hold para mantener estado
 * 4. Configuración de timer de wakeup
 * 5. Entrada en deep sleep
 * 
 * RECUPERACIÓN POST-DESPERTAR:
 * 
 * Al despertar, el sistema debe:
 * 1. Diagnosticar la causa del despertar
 * 2. Liberar el estado GPIO Hold
 * 3. Reinicializar periféricos
 * 4. Reconfigurar sensores
 * 
 * OPTIMIZACIONES DE CONSUMO:
 * 
 * - Deep sleep reduce el consumo a ~10μA
 * - GPIO Hold mantiene estado sin consumo adicional
 * - Desactivación de I2C elimina consumo de sensores
 * - Timer interno consume mínimo durante sleep
 */

#endif