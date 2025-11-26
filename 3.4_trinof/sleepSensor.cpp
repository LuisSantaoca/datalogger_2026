#include "config.h"         // Configuration settings
#include "sleepSensor.h"
#include "Wire.h"
#include "esp_system.h"  // Required for ESP system functions
#include "data_storage.h"

/**
 * @brief Prints the reason for waking up from deep sleep.
 */
uint8_t getWakeupReason() {
    return esp_sleep_get_wakeup_cause(); // Return the wakeup reason for further processing
}

/**
 * @brief Sets up the GPIO pins and initializes the I2C bus.
 */
void setupGPIO() {
    // Initial boot setup
    gpio_hold_dis(RS485_PWREN_PIN);
    gpio_hold_dis(MODEM_PWRKEY_PIN);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);  // Initialize I2C
    pinMode(RS485_PWREN_PIN, OUTPUT);
    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    digitalWrite(RS485_PWREN_PIN, HIGH);
//    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
//    delay(1200);
//    digitalWrite(MODEM_PWRKEY_PIN, LOW);
}

/**
 * @brief Puts the ESP32 into deep sleep mode.
 */
void sleepIOT() {
    // Prepare GPIO pins for sleep
    digitalWrite(RS485_PWREN_PIN, LOW);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(1200);
    digitalWrite(MODEM_PWRKEY_PIN, LOW);

    // Hold GPIO states during deep sleep
    gpio_hold_en(RS485_PWREN_PIN);
    gpio_hold_en(MODEM_PWRKEY_PIN);

    // Configure the ESP32 to wake up after a specified time
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    logMessage("✅🔋 INF: Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " seconds");
    logMessage("✅🔋 INF: Going to sleep now\n");

    // Enter deep sleep
    esp_deep_sleep_start();
}

/**
 * @brief Recovers the I2C bus in case of a bus lock.
 */
void i2cBusRecovery() {
    // Configure SDA and SCL pins for recovery
    pinMode(I2C_SDA_PIN, INPUT_PULLUP);  // SDA
    pinMode(I2C_SCL_PIN, OUTPUT);      // SCL

    // Generate up to 9 clock pulses to free the SDA line
    for (int i = 0; i < 9; i++) {
        digitalWrite(I2C_SCL_PIN, HIGH);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL_PIN, LOW);
        delayMicroseconds(5);
    }

    // Generate a STOP condition
    pinMode(I2C_SDA_PIN, OUTPUT);
    digitalWrite(I2C_SDA_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C_SDA_PIN, HIGH);
    delayMicroseconds(5);

    // Reinitialize the I2C bus
    Wire.end();
    delay(10);
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);  // Reinitialize I2C with the desired speed
}