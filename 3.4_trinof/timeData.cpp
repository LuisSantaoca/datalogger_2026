#include <ESP32Time.h>
#include "timeData.h"
#include "RTClib.h"
#include "sleepSensor.h"
#include "data_storage.h"

// RTC objects
ESP32Time   rtcInt;      // Internal RTC
RTC_DS3231  rtcExt;      // DS3231 RTC module

// Task handle for time management
TaskHandle_t TaskTimePower;

/**
 * @brief Helper function to add zero-padding to numbers.
 * @param value The number to pad.
 * @return A string with the number padded to two digits.
 */
String zeroPad(int value) {
    if (value < 10) {
        return "0" + String(value);
    }
    return String(value);
}

/**
 * @brief Sets up the time data by initializing the RTC and setting the initial date/time.
 */
void setupTimeData() {
    int countRtc = 0;

    // Attempt to initialize the external RTC
    while (!rtcExt.begin()) {
        i2cBusRecovery();       // Recover the I2C bus if needed
        logMessage("ERR: Couldn't find RTC");
        delay(1000);
        countRtc++;
        if (countRtc > 10) {
            sleepIOT();         // Enter deep sleep
        }
    }
    setRtcIntfromRtcExt();     // Synchronize the internal RTC with data from the external RTC
    delay(100);
}

/**
 * @brief FreeRTOS task to manage time-related operations.
 * @param pvParameters Parameters for the task (unused).
 */
void timePower(void* pvParameters) {
    logMessage("INF: Task Time running on core ", false);
    logMessage(String(xPortGetCoreID()));
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ); // Initialize I2C communication
    if (!rtcExt.begin()) {
        logMessage("ERR: Couldn't find RTC");
        Serial.flush();
        sleepIOT();
    }
    setRtcIntfromRtcExt(); // Synchronize the RTC
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1);
    for (;;) {
        xLastWakeTime = xTaskGetTickCount();
        vTaskDelayUntil(&xLastWakeTime, xFrequency); // Delay until the next task cycle
    }
}

/**
 * @brief Sets the RTC date and time based on a formatted string.
 * @param fecha The date/time string in the format "YY-MM-DD HH:MM:SS".
 * @return True if the date/time was set successfully, false otherwise.
 */
bool setFechafromString(const String& fecha) {
    int se = fecha.substring(15, 17).toInt();
    int mi = fecha.substring(12, 14).toInt();
    int ho = fecha.substring(9, 11).toInt();
    int di = fecha.substring(6, 8).toInt();
    int me = fecha.substring(3, 5).toInt();
    int an = fecha.substring(0, 2).toInt();
    int an0 = an + 2000;

    if (an0 >= 2024) {
        rtcInt.setTime(se, mi, ho, di, me, an0);
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Sets the external RTC (rtcExt) date and time using a pointer to a gpsdata_type variable.
 * @param gps Pointer to a gpsdata_type structure containing the time information.
 * @return true if the RTC was set successfully, false otherwise.
 */
bool setRtcsfromGps(gpsdata_type& gps) {
    // Check for valid year (e.g., > 2020)
    if (gps.year < 2021) {
        return false;
    }
    // Create a DateTime object from gpsdata
    DateTime dt(gps.year, gps.month, gps.day, gps.hour, gps.min, gps.sec);
    //extern RTC_DS3231 rtcExt;
    rtcInt.setTime(dt.second(), dt.minute(), dt.hour(), dt.day(), dt.month(), dt.year());
    rtcExt.adjust(dt);
    return true;
}

/**
 * @brief Synchronizes the internal RTC with the external RTC.
 */
void setRtcIntfromRtcExt() {
    DateTime nowExt = rtcExt.now();
    rtcInt.setTime(nowExt.second(), nowExt.minute(), nowExt.hour(), nowExt.day(), nowExt.month(), nowExt.year());
}

/**
 * @brief Retrieves the current date as a formatted string.
 * @return The date in the format "YYYY-MM-DD".
 */
String getFecha() {
    return String(rtcInt.getYear()) + "-" +
           zeroPad(rtcInt.getMonth() + 1) + "-" +
           zeroPad(rtcInt.getDay());
}

/**
 * @brief Retrieves the current time as a formatted string.
 * @return The time in the format "HH:MM:SS".
 */
String getHora() {
    return zeroPad(rtcInt.getHour(true)) + ":" +
           zeroPad(rtcInt.getMinute()) + ":" +
           zeroPad(rtcInt.getSecond());
}

/**
 * @brief Retrieves the current hour as an integer.
 * @return The current hour.
 */
int getHoraInt() {
    return rtcInt.getHour(true);
}

/**
 * @brief Retrieves the current minute as an integer.
 * @return The current minute.
 */
int getMinutoInt() {
    return rtcInt.getMinute();
}

/**
 * @brief Retrieves the current second as an integer.
 * @return The current second.
 */
int getSegundoInt() {
    return rtcInt.getSecond();
}

/**
 * @brief Retrieves the current date and time as a hexadecimal string.
 * @return The date and time in hexadecimal format.
 */
String dateStrHex() {
    char hexBuffer[13];
    sprintf(hexBuffer, "%02X%02X%02X%02X%02X%02X",
            rtcInt.getYear() - 2000,
            rtcInt.getMonth() + 1,
            rtcInt.getDay(),
            rtcInt.getHour(true),
            rtcInt.getMinute(),
            rtcInt.getSecond());
    return String(hexBuffer);
}

/**
 * @brief Retrieves the current date as a hexadecimal string.
 * @return The date in hexadecimal format.
 */
String fechaHex() {
    char hexBuffer[7];
    sprintf(hexBuffer, "%02X%02X%02X",
            rtcInt.getDay(),
            rtcInt.getMonth() + 1,
            rtcInt.getYear() - 2000);
    return String(hexBuffer);
}

/**
 * @brief Returns the current epoch time (Unix timestamp) from the internal RTC.
 * @return The current epoch time as an unsigned long.
 */
unsigned long getEpochtimertcInt() {
    return rtcInt.getEpoch();
}
