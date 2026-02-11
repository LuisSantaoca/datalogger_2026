#ifndef CONFIG_DATA_LTE_H
#define CONFIG_DATA_LTE_H

#include <Arduino.h>


/** @brief UART TX pin connected to the module RX. */
static const uint8_t LTE_PIN_TX = 10;

/** @brief UART RX pin connected to the module TX. */
static const uint8_t LTE_PIN_RX = 11;

/** @brief Module PWRKEY control pin. */
static const uint8_t LTE_PWRKEY_PIN = 9;

/** @brief UART baud rate for AT commands. */
static const uint32_t LTE_SIM_BAUD = 115200UL;

/** @brief Default timeout for a single AT command transaction (ms). */
static const uint32_t LTE_AT_TIMEOUT_MS = 1500UL;

/** @brief Timeout to consider the modem ready after power-on (ms). */
static const uint32_t LTE_AT_READY_TIMEOUT_MS = 8000UL;

/** @brief Maximum number of power-on attempts. */
static const uint16_t LTE_POWER_ON_ATTEMPTS = 3U;

/** @brief True if PWRKEY is asserted with HIGH, false if asserted with LOW. */
static const bool LTE_PWRKEY_ACTIVE_HIGH = true;

/** @brief Delay before toggling PWRKEY (ms). */
static const uint32_t LTE_PWRKEY_PRE_DELAY_MS = 100UL;

/** @brief PWRKEY pulse width (ms). */
static const uint32_t LTE_PWRKEY_PULSE_MS = 1200UL;

/** @brief Delay after PWRKEY pulse (ms). */
static const uint32_t LTE_PWRKEY_POST_DELAY_MS = 3000UL;

/** @brief AT command used for normal power off. */
static const char LTE_POWER_OFF_COMMAND[] = "AT+CPOWD=1";

/** @brief Phone number for SMS tests (include country code). */
static const char SMS_PHONE_NUMBER[] = "+523327022768";

// ============ [FIX-V2 START] Pin de control MIC2288 ============
/**
 * @brief GPIO que controla EN del MIC2288 vía divisor R23/R24.
 * IO13 → R23(10k) → junction → R24(10k) → GND
 * junction → MIC2288 EN (pin 4)
 * Ven = 3.3V × (10k/20k) = 1.65V > umbral 1.2V
 * @note GPIO13 también es ADC_VOLT_BAT. hardPowerCycle() restaura a INPUT.
 */
static const uint8_t MODEM_EN_PIN = 13;

/** @brief Tiempo con VBAT cortado para descarga completa (ms) */
static const uint32_t FIX_V2_POWEROFF_DELAY_MS = 5000;

/** @brief Tiempo de estabilización tras restaurar VBAT (ms) */
static const uint32_t FIX_V2_STABILIZATION_MS = 1000;

/** @brief Intentos PWRKEY antes de activar hard power cycle */
static const uint16_t FIX_V2_PWRKEY_ATTEMPTS_BEFORE_HARD = 2;
// ============ [FIX-V2 END] ============

#define DB_SERVER_IP "d04.elathia.ai"
//#define DB_SERVER_IP "dp01.lolaberries.com.mx"
#define TCP_PORT " 12608"

#endif
