#include "data_storage.h"
#include "simmodem.h"       // GSM/LTE communication functions
#include "config.h"         // Configuration settings
#include <LittleFS.h>


bool saveGpsDataToFlash(Preferences& prefs, const agrodata_type& data) {
    prefs.begin(AGRO_NAMESPACE, false);
    size_t written = prefs.putBytes(AGRO_KEY, &data, sizeof(agrodata_type));
    prefs.end();
    return (written == sizeof(gpsdata_type));
}

bool loadDataFromFlash(Preferences& prefs, agrodata_type& data) {
    prefs.begin(AGRO_NAMESPACE, true);
    size_t read = prefs.getBytes(AGRO_KEY, &data, sizeof(agrodata_type));
    prefs.end();
    // Check if we read a full struct
    return (read == sizeof(agrodata_type) && data.gps.year > 2000 && data.gps.month > 0 && data.gps.day > 0);
}

void eraseGpsDataFromFlash(Preferences& prefs) {
    prefs.begin(AGRO_NAMESPACE, false);
    prefs.remove(AGRO_KEY);
    prefs.end();
}

String getLittleFSInfo() {
  size_t totalBytes = LittleFS.totalBytes();
  size_t usedBytes = LittleFS.usedBytes();
  size_t freeBytes = totalBytes - usedBytes;
  
  String info = "Total: " + String(totalBytes / 1024.0, 2) + " KB, ";
  info += "Used: " + String(usedBytes / 1024.0, 2) + " KB, ";
  info += "Free: " + String(freeBytes / 1024.0, 2) + " KB";
  
  return info;
}

bool setupLFS() {
  if (!LittleFS.begin()) {
    Serial.println("⚠️💾 WAR: LittleFS Mount Failed, attempting to format...");   // If mount fails, try formatting, print to console
    if (LittleFS.format()) {
      Serial.println("✅💾 INF: LittleFS formatted successfully, retrying mount...");
      if (LittleFS.begin()) {
        logMessage("✅💾 INF: LittleFS Mounted Successfully after format");
        logMessage("📊💾 INF: " + getLittleFSInfo());
        return true;
      } else {
        Serial.println("❌💾 ERR: LittleFS Mount Failed after format!");
        return false;
      }
    } else {
      Serial.println("❌💾 ERR: LittleFS format failed!");
      return false;
    }
  } else {
    logMessage("✅💾 INF: LittleFS Mounted Successfully");
    logMessage("📊💾 INF: " + getLittleFSInfo());
    return true;
  }
}

bool appendDataToSensorCSV(const agrodata_type* data) {
  File file = LittleFS.open(CSV_SENSOR, FILE_APPEND);
  if (!file) {
    logMessage("❌💾 ERR: Failed to open " + String(CSV_SENSOR) + " for appending!");
    return false;
  }
  // Write Sensors CSV line: $, chip_id (hex), sensor.epoch, temp_amb, hum_rel, hum_suelo, temp_suelo, cond_suelo, batt_volt, #
  file.printf("%llX,%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n",
    data->station.chip_id,
    data->sensor.epoc,
    data->sensor.temperatura_ambiente,
    data->sensor.humedad_relativa,
    data->sensor.humedad_suelo,
    data->sensor.temperatura_suelo,
    data->sensor.conductividad_suelo,
    data->sensor.battery_voltage
  );
  file.close();
  return true; // Return true if data is successfully appended
}

bool appendDataToStationCSV(const agrodata_type* data) {
  File file = LittleFS.open(CSV_STATION, FILE_APPEND);
  if (!file) {
    logMessage("❌💾 ERR: Failed to open " + String(CSV_STATION) + " for appending!");
    return false;
  }
  // Write Station CSV line: &, chip_id (hex), sim_ccid, rssi, gps.epoc, lat, lon, alt, #
  file.printf("%llX,%20s,%u,%lu,%.8f,%.8f,%.2f\n",
    data->station.chip_id,
    data->station.sim_ccid,
    data->station.rssi,
    data->gps.epoc,
    data->gps.lat,
    data->gps.lon,
    data->gps.alt
  );
  file.close();
  return true;
}

bool printCSVFile(const char* csvFile) {
  File file = LittleFS.open(csvFile, FILE_READ);
  if (!file) {
    logMessage("❌💾 ERR: Failed to open " + String(csvFile) + " for reading!");
    return false;
  }
  while (file.available()) {
    String line = file.readStringUntil('\n');
    Serial.println(line); // Example: print to serial
  }
  file.close();
  return true; // Return true if all data is successfully sent
}

bool deleteCSVFile(const char* csvFile) {
  if (LittleFS.exists(csvFile)) {
    if (LittleFS.remove(csvFile)) {
      logMessage("✅💾 INF: CSV file deleted successfully.");
      return true;
    } else {
      logMessage("❌💾 ERR: Failed to delete CSV file.");
      return false;
    }
  } else {
    logMessage("❌💾 ERR: CSV file does not exist.");
    return false;
  }
}

uint8_t sendCSVDataByTCP(const char* startChar, const char* csvFile) {
  File file = LittleFS.open(csvFile, FILE_READ);
  if (!file) {
    logMessage("❌💾 ERR: Failed to open CSV " + String(csvFile) + " file for sending!");
    return 0;
  }
  if (file.size() == 0) {
    logMessage("⚠️💾 WAR: CSV file is empty, nothing to send.");
    file.close();
    return 3;
  }
  File tempFile = LittleFS.open("/temp.csv", FILE_WRITE);
  if (!tempFile) {
    file.close();
    logMessage("❌💾 ERR: Failed to open temp file!");
    return 0;
  }
  int totallines = 0;
  int failstosend = 0;
  bool stopSending = false;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.length() > 0) {
      totallines++;
      if (stopSending) {
        tempFile.println(line); // Just copy remaining lines
        continue;
      }
      String linetosend = String(startChar) + "," + String(totallines) + "," + line + ",#";
      if (tcpSendData(linetosend, SEND_RETRIES) && (failstosend < FAIL_TOLERANCE)) {
        logMessage("✅📡 INF: Sended line: " + linetosend);
        if (tcpReadData(ACK_CHAR)) {
          logMessage("✅📡 INF: Server ACK received for line " + String(totallines));
        } else {
          failstosend++;
          logMessage("❌📡 ERR: No ACK from server for line " + String(totallines));
          tempFile.println(line);
          if (failstosend > 3) stopSending = true;
        }
      } else {
        failstosend++;
        logMessage("❌📡 ERR: Failed to send line: " + linetosend);
        tempFile.println(line);
        if (failstosend > 3) stopSending = true;
      }
    }
  }
  file.close();
  tempFile.close();
  LittleFS.remove(csvFile);
  LittleFS.rename("/temp.csv", csvFile);
  if (totallines == 0) return 0;
  if (failstosend == 0) return 2;
  if (failstosend < totallines) return 1;
  return 0;
}

void logMessage(const String& msg, bool newLine) {
    if (LOGGING) {
        // Check log file size before appending
        if (LittleFS.exists(LOG_FILE_PATH)) {
            File checkFile = LittleFS.open(LOG_FILE_PATH, FILE_READ);
            if (checkFile) {
                size_t currentSize = checkFile.size();
                checkFile.close();
                
                // If file exceeds MAX_LOG_SIZE, rotate it
                if (currentSize >= MAX_LOG_SIZE) {
                    File oldLog = LittleFS.open(LOG_FILE_PATH, FILE_READ);
                    if (oldLog) {
                        // Calculate how much to keep (last 50% of max size)
                        size_t keepSize = MAX_LOG_SIZE / 2;
                        size_t skipSize = currentSize - keepSize;
                        
                        // Read the content we want to keep
                        oldLog.seek(skipSize, SeekSet);
                        String keepContent = oldLog.readString();
                        oldLog.close();
                        
                        // Find first newline to avoid partial line
                        int firstNewline = keepContent.indexOf('\n');
                        if (firstNewline > 0) {
                            keepContent = keepContent.substring(firstNewline + 1);
                        }
                        
                        // Write back the truncated content
                        File newLog = LittleFS.open(LOG_FILE_PATH, FILE_WRITE);
                        if (newLog) {
                            newLog.println("... [Log rotated - older entries removed] ...");
                            newLog.print(keepContent);
                            newLog.close();
                        }
                    }
                }
            }
        }
        
        // Append new log message
        File logFile = LittleFS.open(LOG_FILE_PATH, FILE_APPEND);
        if (logFile) {
            if (newLine)
                logFile.println(msg);
            else
                logFile.print(msg);
            logFile.close();
        }
    } else {
        if (newLine)
            Serial.println(msg);
        else
            Serial.print(msg);
    }
}

bool clearLogFile() {
    if (LittleFS.exists(LOG_FILE_PATH)) {
        if (LittleFS.remove(LOG_FILE_PATH)) {
            logMessage("✅💾 INF: Log file cleared successfully.");
            return true;
        } else {
            logMessage("❌💾 ERR: Failed to clear log file.");
            return false;
        }
    } else {
        logMessage("⚠️💾 WAR: Log file does not exist.");
        return true; // Consider it cleared if it doesn't exist
    }
}
