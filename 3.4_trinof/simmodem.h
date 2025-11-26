#ifndef simmodem_H
#define simmodem_H

#include <stdint.h>
#include "Arduino.h"
#include "type_def.h"
#include "board_pins.h"
#include "data_storage.h"

// TinyGSM configuration
#define TINY_GSM_MODEM_SIM7080          // Define the modem type
#define TINY_GSM_RX_BUFFER      1024    // Buffer size for TinyGSM
#define TINY_GSM_YIELD_MS       10      // Yield time for TinyGSM
#define TINY_GSM_MODEM_HAS_GPS          // Enable GPS functionality for the modem

// Serial definitions
#define SerialAT    Serial1     // Serial port for AT commands
#define SerialMon   Serial      // Serial port for monitoring
#define GSM_PIN     ""          // GSM PIN (if required)

// Operator information structure
struct OperatorInfo {
    String name;        // Operator name
    String code;        // Operator numeric code (MCC+MNC)
    int rsrp;          // Reference Signal Received Power
    int rsrq;          // Reference Signal Received Quality
    int rssi;          // Received Signal Strength Indicator
    int snr;           // Signal to Noise Ratio
    bool connected;    // Connection status
    int score;         // Calculated quality score
};

// Function declarations

/**
 * @brief Powers on the modem by toggling the power pin.
 */
void modemPwrKeyPulse();

/**
 * @brief Restarts the modem by powering it off and on.
 */
void modemRestart();

/**
 * @brief Performs a full modem reset and reinitialization.
 * @return True if reset is successful, false otherwise.
 */
bool modemFullReset();

/**
 * @brief Initializes the modem module and prepares it for communication.
 */
void setupModem();

/**
 * @brief Starts the LTE connection and configures the modem.
 * @return True if the connection is successful, false otherwise.
 */
bool startLTE();

/**
 * @brief Flushes the serial port to clear any pending data.
 */
void flushPortSerial();

/**
 * @brief Sends an AT command to the modem and waits for a specific response.
 * @param command The AT command to send.
 * @param expectedResponse The expected response from the modem.
 * @param timeout The timeout period in milliseconds.
 * @return True if the expected response is received, false otherwise.
 */
bool sendATCommand(const String& command, const String& expectedResponse, unsigned long timeout, String &response);

/**
 * @brief Reads the response from the modem within a specified timeout.
 * @param timeout The timeout period in milliseconds.
 * @return The response string from the modem.
 */
String readResponse(unsigned long timeout);

/**
 * @brief Opens a TCP connection to the server.
 * @return True if the connection is successful, false otherwise.
 */
bool tcpOpen();

/**
 * @brief Sends data over the TCP connection.
 * @param data The data to send.
 * @param retries The number of retries if sending fails.
 * @return True if the data is sent successfully, false otherwise.
 */
bool tcpSendData(const String data, int retries);

/**
 * @brief Reads data from the TCP connection and checks for acknowledgment character.
 * @param ackChar The acknowledgment character to look for in the response.
 * @return True if acknowledgment is found, false otherwise.
 */
bool tcpReadData(char ackChar);

/**
 * @brief Closes the TCP connection.
 * @return True if the connection is closed successfully, false otherwise.
 */
bool tcpClose();

/**
 * @brief Retrieves the IMEI of the modem.
 * @return The IMEI as a string.
 */
String getImei();

/**
 * @brief Retrieves the ICCID of the SIM card.
 * @return The ICCID as a string.
 */
String getCcid();

/**
 * @brief Retrieves the signal quality of the modem.
 * @return The signal quality as a string.
 */
String getSignal();

/**
 * @brief Stops the GPS functionality on the SIM7080 module.
 * This function disables the GPS to save power or when GPS is not needed.
 */
void stopGpsModem();

/**
 * @brief Sets up the GPS on the SIM7080 module.
 * @return True if GPS setup is successful, false otherwise.
 */
bool setupGpsSim(void);

/**
 * @brief Gets the current GPS data.
 * @param gpsdata Reference to a gpsdata_type structure to store the GPS data.
 * @return True if GPS data is successfully retrieved, false otherwise.
 */
bool getGpsSim(gpsdata_type &gpsdata, uint16_t GPS_RETRIES);

/**
 * @brief Get SIM CCID directly from modem as a String.
 *        Parses modem response and returns the first line containing 19+ digits.
 * @return The CCID string if found, or an empty string if not found.
 */
String getSimCCIDDirect(void);

/**
 * @brief Gets the RSSI (signal quality) from the modem.
 * @return The RSSI value as uint8_t.
 */
uint8_t getRSSI(void);

/**
 * @brief Requests cellular network registration and verifies connection status.
 * @return true if registered and connected, false otherwise.
 */
bool checkCellularNetwork(void);

/**
 * @brief Scans for available operators and populates operator list.
 * @return true if operators were found, false otherwise.
 */
bool scanAvailableOperators(void);

/**
 * @brief Parses COPS response and builds operator list.
 * @param copsResponse The response from AT+COPS=? command.
 */
void parseCopsResponse(const String& copsResponse);

/**
 * @brief Gets CPSI metrics for signal quality analysis.
 * @param operatorInfo Reference to OperatorInfo structure to fill.
 * @return true if metrics were obtained successfully.
 */
bool getCpsiMetrics(OperatorInfo& operatorInfo);

/**
 * @brief Calculates adaptive timeout based on signal quality.
 * @param baseTimeout Base timeout in milliseconds.
 * @return Calculated timeout adjusted for current conditions.
 */
unsigned long getAdaptiveTimeout(unsigned long baseTimeout);

/**
 * @brief Attempts connection with a specific operator.
 * @param operatorIndex Index of operator in the list.
 * @return true if connection and registration successful.
 */
bool connectWithOperator(int operatorIndex);

/**
 * @brief Starts LTE with multi-operator fallback support.
 * @return true if any operator connects successfully.
 */
bool startLTEWithFallback(void);

/**
 * @brief Gets the number of available operators.
 * @return Number of operators found.
 */
int getOperatorCount(void);

/**
 * @brief Gets operator info by index.
 * @param index Operator index.
 * @return Pointer to OperatorInfo or nullptr if invalid.
 */
OperatorInfo* getOperatorInfo(int index);

#endif // SIMMODEM_H