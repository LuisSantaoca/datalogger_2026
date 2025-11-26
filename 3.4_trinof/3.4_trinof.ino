#include <Arduino.h>        // Arduino core library
#include <Preferences.h>    // Library for persistent storage
#include "type_def.h"       // Type definitions for sensor and GPS data
#include "board_pins.h"     // Board pin definitions
#include "config.h"         // Configuration settings
#include "sleepSensor.h"    // Functions for managing sleep mode
#include "timeData.h"       // Functions for handling time-related data
#include "simmodem.h"       // GSM/LTE communication functions
#include "sensores.h"       // Sensor-related functions
#include "data_storage.h"   // Functions for saving and loading data to/from flas
#include "wireless.h"       // BLE functions for log file transfer

// Global Variables
agrodata_type agrodata = {
    .sensor = {
        .epoc = 0,
        .temperatura_ambiente = 0.0,
        .humedad_relativa = 0.0,
        .humedad_suelo = 0.0,
        .temperatura_suelo = 0.0,
        .conductividad_suelo = 0.0,
        .battery_voltage = 0.0
    },
    .gps = {
        .lat = 0.0,
        .lon = 0.0,
        .speed = 0.0,
        .alt = 0.0,
        .vsat = 0,
        .usat = 0,
        .accuracy = 0.0,
        .year = 0,
        .month = 0,
        .day = 0,
        .hour = 0,
        .min = 0,
        .sec = 0,
        .level = false,
        .epoc = 0,
        .tries = 0
    },
    .station = {
        .chip_id = 0,
        .sim_ccid = "102030405060708090", // Placeholder for SIM card ICCID
        .rssi = 0,
        .epoc = 0
    }
};

Preferences datarecord;               // Preferences object for persistent storage of GPS data
unsigned long epoc_rtcint = 0;        // Variable to store GPS epoch time
uint8_t     wakeup_reason = 99;       // Variable to store the wakeup reason
bool    send_station_data = false;    // Flag to indicate if station data should be sent
bool     send_sensor_data = false;    // Flag to indicate if sensor data should be sent

// Setup function, runs once at startup
void setup() {
  Serial.begin(CONSOLE_BAUDRATE);     // Initialize console serial communication at 115200 baud
  setupLFS();                         // Initialize LittleFS for file storage
  wakeup_reason = getWakeupReason();  // Get the wakeup reason
  logMessage("🏷️🛠️ INF: STARTING.... Wakeup reason: ", false);     // Print wakeup reason
  logMessage(String(wakeup_reason));
  logMessage("🏷️🛠️ Intelagro Firmware: " FIRMWARE_VERSION);
  setupGPIO();                        // Setup GPIO pins and I2C bus
  setupTimeData();
  epoc_rtcint = getEpochtimertcInt(); // Get the current epoch time from the external RTC
  uint8_t hour = (getHoraInt() + TIME_ZONE + 24) % 24; // Adjust hour for local time zone
  uint8_t minute = getMinutoInt();
  logMessage("🏷️⏰ Local Time: " + String(hour) + ":" + String(minute)); // Print local time
  if (LOGGING) {
    Serial.println("🏷️🛠️ INF: Set output messages to log file");
    Serial.print("INF: Current RTC: ");
    Serial.println(epoc_rtcint);        // Print the epoch time from the internal RTC
  }
  setupModem();                       // Initialize the modem
  setupSensores();                    // Initialize sensors
  if (wakeup_reason == 0) {
    setupWiFiAPAndServer();
  }
  //setupBLE();                         // Setup BLE for log file transfer
}

void loop() {
  if (!loadDataFromFlash(datarecord, agrodata)) {
    logMessage("⚠️💾 WAR: No valid data in flash, writing current.");
    acquireAndStoreGpsData();
    saveGpsDataToFlash(datarecord, agrodata);
    ESP.restart();
  }

  logMessage("✅💾 INF: Successfully Data loaded from flash.");
  if (wakeup_reason == 0) {
    if (LOGGING) Serial.println("🛠️-Chip ID: " + String(agrodata.station.chip_id, HEX));
    agrodata.station.chip_id = ESP.getEfuseMac();
    logMessage("🏷️🛠️ Chip ID: " + String(agrodata.station.chip_id, HEX));
    soilsensor_calibration();
    agrodata.station.epoc = epoc_rtcint;
    agrodata.gps.tries = 0;
  }
   //Check if GPS data is older than TIME_GPS_UPDATE or if the wakeup reason is not set
  if (((epoc_rtcint - agrodata.gps.epoc) > TIME_GPS_UPDATE) || wakeup_reason == 0 || agrodata.gps.tries <= GPS_MAXTRIES) { // Check if GPS data is older than TIME_GPS_UPDATE hours or if the wakeup reason is not set
    agrodata.gps.epoc = epoc_rtcint;                                              // Update GPS epoch time
    send_station_data = true;                                                     // Set flag to send station data
    if (wakeup_reason == 00) logMessage("🏷️🛰️ Power ON, acquiring GPS data...");
    else if (agrodata.gps.tries != 0) {
      logMessage("⚠️🛰️ WAR: Previous GPS acquisition failed, trying again, attempt " + String(agrodata.gps.tries+1) + " of " + String(GPS_MAXTRIES) + ".");
    } else logMessage("✅🛰️ INF: GPS data is older than " + String(TIME_GPS_UPDATE) + " hrs, updating data.");
    acquireAndStoreGpsData();
  } else {
      if (agrodata.gps.tries >= GPS_MAXTRIES) logMessage("⚠️🛰️ WAR: GPS tries exceeded, will try on next cycle.");
      else logMessage("✅🛰️ INF: GPS data is newer than " + String(TIME_GPS_UPDATE) + " hrs.");
  }

  //saveGpsDataToFlash(datarecord, agrodata);              // Save data to flash memory
  //uint8_t hour = (getHoraInt() + TIME_ZONE + 24) % 24;  // Adjust hour for local time zone
  //uint8_t minute = getMinutoInt();
  int time_since_on = (epoc_rtcint - agrodata.station.epoc);    // Calculate time since last station turn on (minutes or hours)
  if (time_since_on >= SEND_EVERY_H) send_sensor_data = true;   // Set flag to send sensor data if time since last send exceeds SEND_EVERY_H

  logMessage("🏷️🛠️ INF: Reading sensors...");
  readSensorData(&agrodata.sensor);
  appendDataToSensorCSV(&agrodata);
  logMessage("🏷️🌐 INF: Time since last check: " + String(time_since_on));

  if (send_sensor_data || send_station_data) {
    logMessage("🏷️🌐 INF: Sending CSV file(s)...");
    if(startLTEWithFallback()) {
      if (tcpOpen()) {
        if (send_sensor_data) {
          if (sendCSVDataByTCP(START_CHAR_SENSOR, CSV_SENSOR) == 2)
            agrodata.station.epoc = epoc_rtcint;
        }
        if (send_station_data) {
          appendDataToStationCSV(&agrodata);
          sendCSVDataByTCP(START_CHAR_STATION, CSV_STATION);
        }
      } else {
        logMessage("❌🌐 ERR: Failed to open TCP.");
      }
    } else {
      logMessage("❌🌐 ERR: Failed to start LTE.");
    }
  }
  saveGpsDataToFlash(datarecord, agrodata);
  //Serial.println("🏷️💤 INF: Printing Log File...");
  //printCSVFile(LOG_FILE_PATH);
  sleepIOT();
}
/*---------------------------------- END MAIN ------------------------------------------------------------*/

// Convert sensor data to string
String convertStructToString(const agrodata_type* data) {
  char chip_id_str[13];
  snprintf(chip_id_str, sizeof(chip_id_str), "%llX", data->station.chip_id);
  String result = String(START_CHAR_CAL) + "," + String(chip_id_str) + "," +
                  String(data->station.epoc) + "," +
                  String(data->station.rssi) + "," +
                  String(data->station.sim_ccid) + "," +
                  String(data->sensor.humedad_suelo, 2) + "," +
                  String(data->sensor.temperatura_suelo, 2) + "," +
                  String(data->sensor.conductividad_suelo, 2) + "," +
                  String(data->sensor.battery_voltage, 2) + ",#";
  return result;
}

// Function to print GPS data for debugging
void printGpsData(const gpsdata_type& gpsdata) {
  Serial.print("lat:"); Serial.print(gpsdata.lat, 8); Serial.print("\t");
  Serial.print("lon:"); Serial.print(gpsdata.lon, 8); Serial.println();
  Serial.print("speed:"); Serial.print(gpsdata.speed); Serial.print("\t");
  Serial.print("altitude:"); Serial.print(gpsdata.alt); Serial.println();
  Serial.print("year:"); Serial.print(gpsdata.year);
  Serial.print(" month:"); Serial.print(gpsdata.month);
  Serial.print(" day:"); Serial.print(gpsdata.day);
  Serial.print(" hour:"); Serial.print(gpsdata.hour);
  Serial.print(" minutes:"); Serial.print(gpsdata.min);
  Serial.print(" second:"); Serial.print(gpsdata.sec); Serial.println();
  Serial.print("epoc:"); Serial.print(gpsdata.epoc);
  Serial.println();
}

// Function to acquire and store GPS data
bool acquireAndStoreGpsData() {
  if (setupGpsSim()) {
    if (getGpsSim(agrodata.gps, GPS_FIX_RETRIES)) {   // Attempt to get GPS data up to n times
      agrodata.gps.tries = 0;                         // Reset retry count on successful GPS acquisition
      setRtcsfromGps(agrodata.gps);                   // Set the external RTC with GPS date/time data
      agrodata.gps.epoc = getEpochtimertcInt();       // Update the epoch time from the internal RTC
      printGpsData(agrodata.gps);                     // Print GPS data for debugging
      return true;                                    // Return true to indicate success
    }
  }
  agrodata.gps.tries++;         // Increment retry count
  agrodata.gps.lat = 0.0;       // Reset GPS data to indicate failure
  agrodata.gps.lon = 0.0;
  agrodata.gps.alt = 0.0;
  logMessage("❌🛰️ ERR: Failed to acquire GPS data after tries:" + String(agrodata.gps.tries) + "/" + String(GPS_MAXTRIES) + " max.");
  return false;
}

// Handle GPS retry logic
void handleGpstries() {
  if (agrodata.gps.tries >= GPS_MAXTRIES) { // If GPS tries exceed maximum
    agrodata.gps.tries = 0;
    if (agrodata.gps.epoc < 1577836800) {   // Verify if epoch time is before 2020 (1577836800 is Jan 1, 2020)
      logMessage("⚠️🛰️ WAR: GPS epoch time is outdated, setting to current RTC time.");
      agrodata.gps.epoc = epoc_rtcint;      // Set GPS epoch time to current RTC time
    }
    if (epoc_rtcint > agrodata.gps.epoc + TIME_GPS_UPDATE) {
      agrodata.gps.epoc = epoc_rtcint + (TIME_WAIT_GPS); // Increase GPS epoch time by TIME_WAIT_GPS hours to wait for new GPS data
    }
    logMessage("⚠️🛰️ WAR: GPS tries exceeded, will try on next cycle.");
  }
}

// Function to perform soil sensor calibration using maximum read count
void soilsensor_calibration() {
  logMessage("✅🌱 INF: Starting soil sensor calibration (max " + String(CALIBRATION_READS_MAX) + " reads)...");
  unsigned long lte_retry_start = millis();
  uint8_t calibration_reads = 0;
  uint8_t lte_failure_count = 0;
  bool lte_connected = false;

  // Try to start LTE for up to 30 minutes
  while (!lte_connected && (millis() - lte_retry_start < LTE_RETRY_TIMEOUT_MS)) {
    handleWiFiServer();
    logMessage("🏷️📞 INF: Attempting to start LTE (attempt " + String(lte_failure_count + 1) + ")...");
    
    // Perform modem reset every 3 failed attempts
    if (lte_failure_count > 0 && lte_failure_count % 3 == 0) {
      logMessage("🏷️📞 INF: Performing modem reset after " + String(lte_failure_count) + " failures...");
      if (!modemFullReset()) {
        logMessage("❌📞 ERR: Modem reset failed.");
      }
      delay(3000);
    }

    if (startLTE()) {
      lte_connected = true;
      logMessage("✅📞 INF: LTE connection established after " + String(lte_failure_count) + " failures.");
    } else {
      lte_failure_count++;
      logMessage("⚠️📞 WAR: LTE connection failed (" + String(lte_failure_count) + " total failures), retrying in 10 seconds...");
      unsigned long retry_wait = millis();
      while (millis() - retry_wait < 10000) {
        handleWiFiServer();
        delay(100);
      }
    }
  }

  if (!lte_connected) {
    logMessage("❌📞 ERR: LTE connection failed after 30 minutes and " + String(lte_failure_count) + " attempts. Continuing calibration with WiFi server only.");
  }

  // Continue with calibration regardless of LTE status
  while (calibration_reads < CALIBRATION_READS_MAX) {
    handleWiFiServer();
    calibration_reads++;
    logMessage("✅🌱 INF: Calibration reading: " + String(calibration_reads) + "/" + String(CALIBRATION_READS_MAX));
    agrodata.station.epoc = getEpochtimertcInt(); // Update epoch time
    if (lte_connected) {
      agrodata.station.rssi = getRSSI();            // Update RSSI value
    }
    readSensorData(&agrodata.sensor);
    
    // Send soil sensor data only if LTE is connected
    if (lte_connected) {
      String linetosend = convertStructToString(&agrodata);
      if (tcpOpen()) {
        if (tcpSendData(linetosend, SEND_RETRIES)) {
          if (!tcpReadData(ACK_CHAR)) {
            logMessage("❌🌐 ERR: No ACK from server for line ");
          }
        } else {
          logMessage("❌🌐 ERR: Failed to send line: ");
        }
        tcpClose();
      } else {
        logMessage("❌🌐 ERR: TCP connection failed during calibration.");
      }
    }
    
    // Wait between reads (skip wait after last read)
    if (calibration_reads < CALIBRATION_READS_MAX) {
      unsigned long interval_start = millis();
      while (millis() - interval_start < CALIBRATION_INTERVAL_MS) {
        handleWiFiServer();
        delay(100);
      }
    }
  }
  logMessage("✅🌱 INF: Soil sensor calibration finished after " + String(calibration_reads) + " reads. Continuing normal operation.");
}
