#pragma once

#include <Arduino.h>

/* RS485 */
#define RS485_TX 16
#define RS485_RX 15
#define RS485_BAUD 9600

/* Power sensor */
#define ENPOWER 3

/* Modbus */
#define MODBUS_SLAVE_ID 18
#define MODBUS_START_ADDRESS 0x0000
#define MODBUS_REGISTER_COUNT 4
#define MODBUS_TIMEOUT_MS 1200

/* I2C */
#define I2C_SDA 6
#define I2C_SCL 12
#define I2C_FREQ 100000

/* ADC */
#define ADC_VOLT_BAT 13
#define ADC_PIN ADC_VOLT_BAT
#define ADC_SAMPLES 10
#define ADC_MULTIPLIER 100
