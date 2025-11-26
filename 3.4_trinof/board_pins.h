#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include <SoftwareSerial.h> // Library for software-based serial communication

// ===================== UART (Console/Terminal/Debug) =====================
#define CONSOLE_BAUDRATE    115200u  // Baud rate for console serial communication

// ===================== UART (Modem) =====================
#define MODEM_UART_TX_PIN   GPIO_NUM_10
#define MODEM_UART_RX_PIN   GPIO_NUM_11
#define MODEM_PWRKEY_PIN    GPIO_NUM_9
#define MODEM_UART_BAUDRATE 115200u     // Baud rate for modem serial communication
#define MODEM_UART_CFG      SERIAL_8N1  // Configuration byte for modem uart communication

// ===================== I2C =============================
#define I2C_SDA_PIN         GPIO_NUM_6
#define I2C_SCL_PIN         GPIO_NUM_12
#define I2C_FREQ            100000u  // 100kHz

// ===================== LEDs ============================
//#define LED_PIN             GPIO_NUM_12

// ===================== RS485 ===========================
#define RS485_TX_PIN        GPIO_NUM_16
#define RS485_RX_PIN        GPIO_NUM_15
#define RS485_PWREN_PIN     GPIO_NUM_3
#define RS485_BAUDRATE      9600u
#define RS485_CFG           SWSERIAL_8N1  // Configuration byte for RS485 uart communication

// ===================== ADC =============================
#define ADC_VOLT_BAT_PIN    GPIO_NUM_13

// ===================== Other Peripherals ===============
// Add more as needed, e.g. for relays, buttons, etc.
    
// ===================== Usage Example ===================
// pinMode(LED_PIN, OUTPUT);
// digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? HIGH : LOW);
// Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);
// Serial2.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);

#endif // BOARD_PINS_H