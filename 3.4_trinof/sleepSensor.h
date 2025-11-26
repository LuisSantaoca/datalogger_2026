#ifndef SLEEP_SENSOR_H
#define SLEEP_SENSOR_H

#include <stdint.h>
#include "Arduino.h"
#include "board_pins.h"

// Function declarations

/**
 * @brief Sets up the GPIO pins and initializes the I2C bus.
 */
void setupGPIO();

/**
 * @brief Puts the ESP32 into deep sleep mode.
 * Configures GPIO pins and sets the wake-up timer.
 */
void sleepIOT();

/**
 * @brief Prints the reason for waking up from deep sleep.
 * Handles specific wake-up cases and performs necessary actions.
 */
uint8_t getWakeupReason();

/**
 * @brief Recovers the I2C bus in case of a bus lock.
 * Generates clock pulses and reinitializes the I2C bus.
 */
void i2cBusRecovery();

#endif // SLEEP_SENSOR_H