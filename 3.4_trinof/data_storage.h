#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include <Preferences.h>
#include <LittleFS.h>
#include "type_def.h"

/**
 * @brief Save GPS data from agrodata_type struct to Preferences (flash).
 * @param prefs Reference to a Preferences object (should be opened before use).
 * @param data The agrodata_type struct containing the GPS data to save.
 * @return true if data was saved successfully, false otherwise.
 */
bool saveGpsDataToFlash(Preferences& prefs, const agrodata_type& data);

/**
 * @brief Load GPS data into agrodata_type struct from Preferences (flash).
 * @param prefs Reference to a Preferences object (should be opened before use).
 * @param data Reference to an agrodata_type struct where loaded GPS data will be stored.
 * @return true if data was loaded successfully, false otherwise.
 */
bool loadDataFromFlash(Preferences& prefs, agrodata_type& data);

/**
 * @brief Erase saved GPS data from Preferences (flash).
 * @param prefs Reference to a Preferences object (should be opened before use).
 */
void eraseGpsDataFromFlash(Preferences& prefs);

/**
 * @brief Initialize and mount LittleFS filesystem.
 * @return true if mounting was successful, false otherwise.
 */
bool setupLFS();

/**
 * @brief Get LittleFS filesystem space information.
 * @return String with total, used, and free space information.
 */
String getLittleFSInfo();

/**
 * @brief Append a sensor data record to a CSV file in LittleFS.
 * @param data Pointer to the agrodata_type struct to append.
 * @return true if data was appended successfully, false otherwise.
 */
bool appendDataToSensorCSV(const agrodata_type* data);

/**
 * @brief Append a station data record to a CSV file in LittleFS.
 * @param data Pointer to the agrodata_type struct to append.
 * @return true if data was appended successfully, false otherwise.
 */
bool appendDataToStationCSV(const agrodata_type* data);

/**
 * @brief Read and send all sensor data records from the CSV file in LittleFS.
 *        This function reads each line and (for example) prints or sends it.
 * @param csvFile The CSV file path to send.
 * @return true if all data was read and sent successfully, false otherwise.
 */
bool printCSVFile(const char* csvFile);

/**
 * @brief Delete the sensor data CSV file from LittleFS.
 * @param csvFile The CSV file path to delete.
 * @return true if the file was deleted successfully, false otherwise.
 */
bool deleteCSVFile(const char* csvFile);

/**
 * @brief Send the contents of a CSV file over LTE using the modem (TCP).
 * @param startChar The start character for each line (e.g., "$", "&", "%").
 * @param csvFile The CSV file path to send.
 * @return
 *   3 - CSV file was empty
 *   2 - All lines sent successfully
 *   1 - Some lines failed to send
 *   0 - No lines sent
 */
uint8_t sendCSVDataByTCP(const char* startChar, const char* csvFile);

/**
 * @brief Log a message to Serial or to a log file in LittleFS, depending on LOG_TO_FILE flag.
 * @param msg The message to log.
 */
void logMessage(const String& msg, bool newLine = true);

/**
 * @brief Clear/delete the log file from LittleFS.
 * @return true if the file was cleared successfully, false otherwise.
 */
bool clearLogFile();

#endif // DATA_STORAGE_H