#ifndef CONFIG_H
#define CONFIG_H

// Firmware version
#define FIRMWARE_VERSION "v2.2.1"   // Change this string for each release
#define LOGGING          false       // true or false to print log to file
//#define DUMP_AT_COMMANDS                // To see all AT commands on serial console, if wanted, uncomment the following line

// TCP connection constants used on simmodem.cpp and data_storage.cpp
#define DB_SERVER_IP    "d02.elathia.ai"  //* IP address for TCP connection
#define TCP_PORT        "11234"           // Port number for TCP connection
#define APN             "em"              // APN for cellular connection

// NTP server constants used on simmodem.cpp and timeData.cpp
#define NTP_SERVER_IP   "200.23.51.102"

// Time divisor constants
#define HOURS           3600u    // Seconds in an hour 
#define MINUTES         60u      // Seconds in a minute

// GPS and data sending intervals and retries constants used on agro.ino
#define TIME_GPS_UPDATE (12u * HOURS)   //* Time to update GPS data *12
#define GPS_FIX_RETRIES 300u            //* Maximum retries to adquire GPS data, each retry take 1 sec *300
#define GPS_MAXTRIES    3u              // Maximum tries to get GPS data before to wait next cycle
#define TIME_ZONE       -6              // Time zone offset in hours (e.g., UTC+6)
#define SEND_EVERY_H    (4u * HOURS)   //* Time to wait before sending data again *4
#define TIME_WAIT_GPS   (1u * HOURS)    //* Time to wait for next try GPS fix in hours, after reach GPS_MAXTRIES *1

// Calibration constants (milliseconds version)
#define CALIBRATION_DURATION_MS   (60u * MINUTES * 1000UL)    //* <minutes> in milliseconds *60
#define CALIBRATION_INTERVAL_MS   (5u  * MINUTES * 1000UL)    // <minutes> in milliseconds
#define CALIBRATION_READS_MAX     (CALIBRATION_DURATION_MS / CALIBRATION_INTERVAL_MS)   // Calculate maximum calibration reads
#define LTE_RETRY_TIMEOUT_MS      (30u * MINUTES * 1000UL)    // <minutes> timeout to retry LTE connection during calibration

// Flash memory and CSV file constants used on data_storage.cpp
#define AGRO_NAMESPACE      "agrodata"         // Namespace for storing agro data in flash memory
#define AGRO_KEY            "lastfix"          // Key for storing GPS data in flash memory
#define CSV_SENSOR          "/sensordata.csv"  // Sensor CSV file names
#define CSV_STATION         "/stationdata.csv" // Station CSV file names
#define LOG_FILE_PATH       "/systemdebug.log" // Log file path in LittleFS
#define MAX_LOG_SIZE        104800             // Maximum log file size in bytes (100 KB)
#define SEND_RETRIES        1u                 // Number of retries to send data over TCP
#define FAIL_TOLERANCE      5u                 // Number of failed attempts before stopping sending data
#define ACK_CHAR            '$'                // Acknowledgment character expected from server after sending each line
#define START_CHAR_SENSOR   "$"                // Start character for sensor data lines
#define START_CHAR_STATION  "&"                // Start character for station data lines
#define START_CHAR_CAL      "%"                // Start character for GPS data lines

// Sleep mode constants used on sleepSensor.cpp
#define uS_TO_S_FACTOR  1000000             // Conversion factor for microseconds to seconds
#define TIME_TO_SLEEP   (20 * MINUTES)      //* Time to sleep *20

// Other shared constants...
// Define BLE service and characteristic UUIDs
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "abcdefab-1234-1234-1234-abcdefabcdef"
#define AP_SSID             "AgroLogger"
#define AP_PASS             "agro1234"

#endif // CONFIG_H
