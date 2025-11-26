#ifndef SENSORES_H
#define SENSORES_H

#include <stdint.h>
#include "Arduino.h"
#include "type_def.h"
#include "board_pins.h"

// Function declarations

/**
 * @brief Initializes the sensors and prepares them for data collection.
 */
void setupSensores();

/**
 * @brief Reads data from all connected sensors and stores it in the provided structure.
 * @param data Pointer to the structure where sensor data will be stored.
 */
void readSensorData(sensordata_type* data);

/**
 * @brief Prints sensor data to the serial monitor for debugging purposes.
 * @param data Pointer to the structure containing the sensor data to debug.
 */
//void debugData(sensordata_type* data);

#endif // SENSORES_H