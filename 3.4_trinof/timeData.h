#ifndef TIMEDATA_H
#define TIMEDATA_H

#include <stdint.h>
#include "Arduino.h"
#include "type_def.h"
#include "board_pins.h"

// Function declarations

/**
 * @brief Helper function to add zero-padding to numbers.
 * @param value The number to pad.
 * @return A string with the number padded to two digits.
 */
String zeroPad(int value);

/**
 * @brief Sets the RTC date and time based on a formatted string.
 * @param fecha The date/time string in the format "YY-MM-DD HH:MM:SS".
 * @return True if the date/time was set successfully, false otherwise.
 */
bool setFechafromString(const String& fecha);

/**
 * @brief Sets the external RTC (rtcExt) date and time using a pointer to a gpsdata_type variable.
 * @param gps Pointer to a gpsdata_type structure containing the time information.
 * @return true if the RTC was set successfully, false otherwise.
 */
bool setRtcsfromGps(gpsdata_type& gps);

/**
 * @brief Retrieves the current date as a formatted string.
 * @return The date in the format "YYYY-MM-DD".
 */
String getFecha();

/**
 * @brief Retrieves the current time as a formatted string.
 * @return The time in the format "HH:MM:SS".
 */
String getHora();

/**
 * @brief Retrieves the current hour as an integer.
 * @return The current hour.
 */
int getHoraInt();

/**
 * @brief Retrieves the current minute as an integer.
 * @return The current minute.
 */
int getMinutoInt();

/**
 * @brief Retrieves the current second as an integer.
 * @return The current second.
 */
int getSegundoInt();

/**
 * @brief Synchronizes the internal RTC with the external RTC.
 */
void setRtcIntfromRtcExt();

/**
 * @brief FreeRTOS task to manage time-related operations.
 * @param pvParameters Parameters for the task (unused).
 */
void timePower(void* pvParameters);

/**
 * @brief Sets up the time data by initializing the RTC and setting the initial date/time.
 */
void setupTimeData();

/**
 * @brief Updates the epoch time and return it.
 */
unsigned long getEpochtimertcInt();

/**
 * @brief Retrieves the current date and time as a hexadecimal string.
 * @return The date and time in hexadecimal format.
 */
String dateStrHex();

/**
 * @brief Retrieves the current date as a hexadecimal string.
 * @return The date in hexadecimal format.
 */
String fechaHex();

#endif // TIMEDATA_H