#ifndef LTE_MODULE_H
#define LTE_MODULE_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include "config_data_lte.h"
#include "config_operadoras.h"

/** @brief Callback function type for COPS error handling */
typedef void (*COPSErrorCallback)(void);

struct SignalQuality {
    Operadora operadora;
    int rsrp;
    int rsrq;
    int rssi;
    int sinr;
    int score;
    bool valid;
};

class LTEModule {
public:
    /**
     * @brief Constructor
     * @param serial Reference to the hardware serial port
     */
    LTEModule(HardwareSerial& serial);

    /**
     * @brief Enable or disable debug output
     * @param enable true to enable debug, false to disable
     * @param debugSerial Pointer to Serial port for debug output (default Serial)
     */
    void setDebug(bool enable, Stream* debugSerial = &Serial);

    /**
     * @brief Set callback function to be called when COPS command fails
     * @param callback Function pointer to be called on COPS error
     */
    void setErrorCallback(COPSErrorCallback callback);

    /**
     * @brief Initialize the LTE module
     */
    void begin();

    /**
     * @brief Power on the SIM7080G module
     * @return true if successful, false otherwise
     */
    bool powerOn();

    /**
     * @brief Power off the SIM7080G module
     * @return true if successful, false otherwise
     */
    bool powerOff();

    /**
     * @brief Check if module is responding to AT commands
     * @return true if module responds, false otherwise
     */
    bool isAlive();

    /**
     * @brief Send AT command and wait for response
     * @param cmd Command to send
     * @param timeout Timeout in milliseconds
     * @return true if OK received, false otherwise
     */
    bool sendATCommand(const char* cmd, uint32_t timeout = LTE_AT_TIMEOUT_MS);

    /**
     * @brief Get ICCID from SIM card
     * @return ICCID string, empty if failed
     */
    String getICCID();

    /**
     * @brief Get ICCID from SIM card as bytes
     * @param buffer Buffer to store ICCID bytes
     * @param maxLen Maximum buffer length
     * @return Number of bytes written, 0 if failed
     */
    uint8_t getICCIDBytes(uint8_t* buffer, uint8_t maxLen);

    /**
     * @brief Configure network for specific operator
     * @param operadora Operator enum
     * @return true if configuration successful, false otherwise
     */
    bool configureOperator(Operadora operadora);

    /**
     * @brief Attach to network (CGATT)
     * @return true if attached successfully, false otherwise
     */
    bool attachNetwork();

    /**
     * @brief Activate PDP context
     * @return true if activated successfully, false otherwise
     */
    bool activatePDP();

    /**
     * @brief Deactivate PDP context
     * @return true if deactivated successfully, false otherwise
     */
    bool deactivatePDP();

    /**
     * @brief Detach from network
     * @return true if detached successfully, false otherwise
     */
    bool detachNetwork();

    /**
     * @brief Get network info (CPSI)
     * @return Network info string
     */
    String getNetworkInfo();

    /**
     * @brief Get current operator configuration (COPS)
     * @return Operator info string
     */
    String getCurrentOperator();

    /**
     * @brief Get current band configuration (CBANDCFG)
     * @return Band configuration string
     */
    String getCurrentBands();

    /**
     * @brief Scan for available networks (COPS=?)
     * @return List of available networks
     */
    String scanNetworks();

    /**
     * @brief Get SMS service status (CSMS)
     * @return SMS service status string
     */
    String getSMSService();

    /**
     * @brief Send SMS message
     * @param phoneNumber Destination phone number
     * @param message Message text to send
     * @return true if SMS sent successfully, false otherwise
     */
    bool sendSMS(const char* phoneNumber, const char* message);

    /**
     * @brief Send SMS message to configured number
     * @param message Message text to send
     * @return true if SMS sent successfully, false otherwise
     */
    bool sendSMS(const char* message);

    /**
     * @brief Parse CPSI response to extract signal quality
     * @param cpsiResponse CPSI response string
     * @return SignalQuality structure with parsed values
     */
    SignalQuality parseSignalQuality(String cpsiResponse);

    /**
     * @brief Get best operator based on signal quality tests
     * @return Operadora enum of best operator
     */
    Operadora getBestOperator();

    /**
     * @brief Get signal quality score for a specific operator
     * @param operadora Operator enum
     * @return Score value, -999 if invalid
     */
    int getOperatorScore(Operadora operadora);

    /**
     * @brief Test operator configuration completely
     * @param operadora Operator enum
     * @return true if test successful, false otherwise
     */
    bool testOperator(Operadora operadora);

    /**
     * @brief Open TCP connection to configured server
     * @return true if connection opened successfully, false otherwise
     */
    bool openTCPConnection();

    /**
     * @brief Close TCP connection
     * @return true if connection closed successfully, false otherwise
     */
    bool closeTCPConnection();

    /**
     * @brief Check if TCP connection is open
     * @return true if connected, false otherwise
     */
    bool isTCPConnected();

    /**
     * @brief Send data through TCP connection
     * @param data Data string to send
     * @return true if data sent successfully, false otherwise
     */
    bool sendTCPData(const char* data);

    /**
     * @brief Send data through TCP connection
     * @param data Arduino String to send
     * @return true if data sent successfully, false otherwise
     */
    bool sendTCPData(String data);

    /**
     * @brief Send binary data through TCP connection
     * @param data Pointer to data buffer
     * @param length Length of data to send
     * @return true if data sent successfully, false otherwise
     */
    bool sendTCPData(const uint8_t* data, size_t length);

private:
    HardwareSerial& _serial;
    bool _debugEnabled;
    Stream* _debugSerial;
    SignalQuality _signalQualities[NUM_OPERADORAS];
    COPSErrorCallback _copsErrorCallback;
    
    /**
     * @brief Print debug message if debug is enabled
     * @param msg Message to print
     */
    void debugPrint(const char* msg);

    /**
     * @brief Print debug message with value if debug is enabled
     * @param msg Message to print
     * @param value Value to print
     */
    void debugPrint(const char* msg, int value);
    
    /**
     * @brief Toggle PWRKEY pin to power on/off the module
     */
    void togglePWRKEY();

    /**
     * @brief Disable PSM (Power Saving Mode) to prevent first AT lost
     */
    void disablePSM();

    /**
     * @brief Wait for "OK" response from module
     * @param timeout Timeout in milliseconds
     * @return true if OK received, false otherwise
     */
    bool waitForOK(uint32_t timeout);

    /**
     * @brief Send AT command and get response
     * @param cmd Command to send
     * @param timeout Timeout in milliseconds
     * @return Response string
     */
    String sendATCommandWithResponse(const char* cmd, uint32_t timeout);

    /**
     * @brief Clear serial buffer
     */
    void clearBuffer();

    /**
     * @brief Reset modem and wait for SIM ready
     * @return true if SIM ready, false otherwise
     */
    bool resetModem();
};

#endif
