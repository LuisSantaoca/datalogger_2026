// =============================================================================
// ARCHIVO: sleepdev.cpp
// DESCRIPCIÓN: Implementación del sistema de gestión de energía y modo deep sleep
// FUNCIONALIDADES:
//   - Diagnóstico de causas de despertar del ESP32
//   - Configuración de GPIO y periféricos
//   - Gestión completa del modo deep sleep
//   - Control de pines de alimentación y periféricos
// =============================================================================

#include "sleepdev.h"
#include "Wire.h"
#include "esp_system.h"
#include "gsmlte.h"

// =============================================================================
// VARIABLES RTC PERSISTENTES (FIX-004)
// =============================================================================

RTC_DATA_ATTR uint8_t rtc_last_checkpoint = 0;     // Último checkpoint alcanzado
RTC_DATA_ATTR uint32_t rtc_timestamp_ms = 0;       // Timestamp del último checkpoint
RTC_DATA_ATTR uint16_t rtc_boot_count = 0;         // Contador de boots
RTC_DATA_ATTR uint8_t rtc_crash_reason = RESET_UNKNOWN;  // Motivo del último reset
RTC_DATA_ATTR uint32_t rtc_last_success_time = 0;  // Timestamp de última transmisión exitosa

// =============================================================================
// FUNCIONES DE DIAGNÓSTICO (FIX-004)
// =============================================================================

void updateCheckpoint(uint8_t checkpoint) {
  rtc_last_checkpoint = checkpoint;
  rtc_timestamp_ms = millis();

  // Almacenar timestamp de éxito cuando completamos ciclo
  if (checkpoint == CP_SLEEP_READY || checkpoint == CP_DATA_SENT) {
    rtc_last_success_time = rtc_timestamp_ms;
  }
}

bool getCrashInfo() {
  esp_reset_reason_t reason = esp_reset_reason();
  rtc_crash_reason = static_cast<uint8_t>(reason);

  // Incrementar boot count en cada arranque
  rtc_boot_count++;

  // Considerar crash si no venimos de deep sleep ni power-on controlado
  switch (reason) {
    case ESP_RST_DEEPSLEEP:
    case ESP_RST_SW:
    case ESP_RST_POWERON:
      return false;
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_BROWNOUT:
      return true;
    default:
      return false;
  }
}

void markCycleSuccess() {
  updateCheckpoint(CP_SLEEP_READY);
  rtc_crash_reason = RESET_DEEPSLEEP;
}

// =============================================================================
// FUNCIÓN: printWakeupReason()
// DESCRIPCIÓN: Diagnostica y muestra la causa que provocó el despertar del ESP32
// PARÁMETROS: Ninguno
// RETORNO: void
// FUNCIONALIDAD: Identifica el tipo de evento que despertó al dispositivo
// =============================================================================
void printWakeupReason() {
  // Obtener la causa específica del despertar desde el registro del sistema
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  // Evaluar el tipo de causa de despertar mediante switch-case
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      // Despertar causado por señal externa usando RTC_IO (GPIO específico)
      Serial.println("Wakeup caused by external signal using RTC_IO");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      // Despertar causado por señal externa usando RTC_CNTL (múltiples GPIO)
      Serial.println("Wakeup caused by external signal using RTC_CNTL");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      // Despertar causado por timer interno (nuestro caso principal)
      Serial.println("Wakeup caused by timer");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      // Despertar causado por detección de toque en touchpad
      Serial.println("Wakeup caused by touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      // Despertar causado por programa ULP (Ultra Low Power)
      Serial.println("Wakeup caused by ULP program");
      break;
    default:
      // Caso por defecto: despertar no causado por deep sleep
      // Imprime el código numérico de la causa para debugging
      Serial.printf("Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
      break;
  }
}

// =============================================================================
// FUNCIÓN: setupGPIO()
// DESCRIPCIÓN: Configura los pines GPIO y periféricos del sistema
// PARÁMETROS: Ninguno
// RETORNO: void
// FUNCIONALIDAD: Inicializa comunicación serial, configura pines de control
//                y establece comunicación I2C
// =============================================================================
void setupGPIO() {
  // Inicializar comunicación serial para debugging y logging
  // Velocidad de 115200 baudios para transmisión rápida de datos
  Serial.begin(115200);

  // =============================================================================
  // DESACTIVACIÓN DE GPIO HOLD
  // =============================================================================
  // Liberar el estado "hold" de los pines GPIO para permitir control normal
  // Esto es necesario después de un deep sleep para recuperar control
  gpio_hold_dis(GPIO_NUM_7);  // Pin de control de modo bajo consumo
  gpio_hold_dis(GPIO_NUM_3);  // Pin de control de alimentación
  gpio_hold_dis(GPIO_NUM_9);  // Pin de control de periféricos

  // =============================================================================
  // CONFIGURACIÓN DE COMUNICACIÓN I2C
  // =============================================================================
  // Inicializar comunicación I2C con configuración específica:
  // - SDA: GPIO_NUM_6 (pin de datos)
  // - SCL: GPIO_NUM_12 (pin de reloj)
  // - Frecuencia: 100000 Hz (100 kHz) - velocidad estándar para sensores
  Wire.begin(GPIO_NUM_6, GPIO_NUM_12, 100000);

  // =============================================================================
  // CONFIGURACIÓN DE PINES DE CONTROL
  // =============================================================================
  // Configurar pines como salidas para control de periféricos
  pinMode(GPIO_NUM_7, OUTPUT);  // Control de modo bajo consumo
  pinMode(GPIO_NUM_3, OUTPUT);  // Control de alimentación de sensores
  pinMode(GPIO_NUM_9, OUTPUT);  // Control de otros periféricos

  // NOTA: Los valores iniciales de estos pines se establecen en sleepIOT()
  // para optimizar el consumo de energía durante el deep sleep
}

// =============================================================================
// FUNCIÓN: sleepIOT()
// DESCRIPCIÓN: Prepara y ejecuta la entrada en modo deep sleep
// PARÁMETROS: Ninguno
// RETORNO: void
// FUNCIONALIDAD: Desactiva periféricos, configura pines de control,
//                establece timer de wakeup y entra en deep sleep
// =============================================================================
void sleepIOT() {
  
  // =============================================================================
  // CONFIGURACIÓN DE PINES DE CONTROL PARA MÍNIMO CONSUMO
  // =============================================================================
  // Establecer estado de pines para optimizar consumo de energía:
  digitalWrite(GPIO_NUM_3, LOW);   // Desactivar alimentación de sensores
  digitalWrite(GPIO_NUM_7, HIGH);  // Activar modo bajo consumo
  digitalWrite(GPIO_NUM_9, LOW);   // Desactivar otros periféricos

  // =============================================================================
  // DESACTIVACIÓN DE PERIFÉRICOS
  // =============================================================================
  // Terminar comunicación I2C para reducir consumo de energía
  // Esto desconecta los sensores y otros dispositivos I2C
  Wire.end();

  // =============================================================================
  // ACTIVACIÓN DE GPIO HOLD
  // =============================================================================
  // Mantener el estado de los pines durante el deep sleep
  // Esto asegura que los pines mantengan su configuración
  // incluso cuando el procesador principal esté en sleep
  gpio_hold_en(GPIO_NUM_7);  // Mantener estado de modo bajo consumo
  gpio_hold_en(GPIO_NUM_3);  // Mantener estado de alimentación
  gpio_hold_en(GPIO_NUM_9);  // Mantener estado de periféricos

  // =============================================================================
  // CONFIGURACIÓN DE TIMER DE WAKEUP
  // =============================================================================
  // Configurar el timer interno para despertar automáticamente
  // TIME_TO_SLEEP: tiempo en segundos (definido en sleepdev.h)
  // uS_TO_S_FACTOR: factor de conversión a microsegundos
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  
  // Informar al usuario sobre la configuración del sleep
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " seconds");
  Serial.println("Going to sleep now");
  
  // =============================================================================
  // ENTRADA EN DEEP SLEEP
  // =============================================================================
  // Iniciar el modo deep sleep del ESP32
  // El dispositivo se despertará automáticamente según el timer configurado
  // o por otras causas configuradas (GPIO externo, etc.)
  esp_deep_sleep_start();
  
  // NOTA: El código después de esta línea NO se ejecutará hasta el próximo despertar
  // El ESP32 se reiniciará completamente al despertar
}
