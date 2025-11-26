#include <wireless.h>
#include <LittleFS.h>
#include "data_storage.h"
#include "config.h"
#include "timeData.h"
#include "simmodem.h"

// External reference to global agrodata from main file
extern agrodata_type agrodata;

BLECharacteristic *pCharacteristic;
BLECharacteristic *pRequestCharacteristic;

#define CHUNK_SIZE 256 // bytes per BLE read
static String requestedPath = LOG_FILE_PATH;
static size_t requestedOffset = 0;
static size_t requestedLen = CHUNK_SIZE;
WebServer server(80);

// Helper function to read the last N lines from a file
String readLastNLines(const String& filePath, int numLines) {
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        return "Log file not found: " + filePath;
    }
    
    // Read entire file content
    String content = file.readString();
    file.close();
    
    if (content.length() == 0) {
        return "Log file is empty";
    }
    
    // Split content into lines
    int lineCount = 0;
    int lastNewlinePos = content.length();
    
    // Count lines from the end
    for (int i = content.length() - 1; i >= 0 && lineCount < numLines; i--) {
        if (content.charAt(i) == '\n') {
            lineCount++;
            if (lineCount == numLines) {
                return content.substring(i + 1);
            }
        }
    }
    
    // If we have fewer lines than requested, return all content
    return content;
}

// Helper function to get timezone-adjusted time
String getLocalTime() {
    // Get current hour and adjust for timezone
    int hour = (getHoraInt() + TIME_ZONE + 24) % 24;
    int minute = getMinutoInt();
    int second = getSegundoInt();
    
    // Format as HH:MM:SS with zero padding
    String localTime = "";
    if (hour < 10) localTime += "0";
    localTime += String(hour) + ":";
    if (minute < 10) localTime += "0";
    localTime += String(minute) + ":";
    if (second < 10) localTime += "0";
    localTime += String(second);
    
    return localTime;
}

// Helper function to generate HTML page with log viewer
String generateLogViewerHTML(const String& logContent, int currentLines) {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<title>Intelagro System Dashboard</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f5f5f5; }";
    html += ".header { background-color: #2c3e50; color: white; padding: 20px; border-radius: 5px; margin-bottom: 20px; }";
    html += ".info-section { background-color: white; padding: 15px; border-radius: 5px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
    html += ".log-controls { background-color: white; padding: 15px; border-radius: 5px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
    html += ".log-content { background-color: #2f2f2f; color: #f8f8f2; padding: 15px; border-radius: 5px; ";
    html += "font-family: 'Courier New', Consolas, monospace; font-size: 12px; line-height: 1.4; ";
    html += "white-space: pre-wrap; word-wrap: break-word; overflow-x: auto; max-height: 600px; overflow-y: auto; }";
    html += ".control-group { margin-bottom: 10px; display: flex; align-items: center; }";
    html += "label { display: inline-block; width: 150px; font-weight: bold; }";
    html += "input[type='number'] { padding: 5px; border: 1px solid #ddd; border-radius: 3px; width: 100px; }";
    html += "button { background-color: #3498db; color: white; padding: 8px 15px; border: none; border-radius: 3px; cursor: pointer; margin-left: 10px; }";
    html += "button.danger { background-color: #e74c3c; margin-left: 30px; }";    
    html += "button.danger:hover { background-color: #c0392b; }";
    html += "button:hover { background-color: #2980b9; }";
    html += ".file-links { margin-top: 15px; }";
    html += ".file-links a { color: #3498db; text-decoration: none; margin-right: 15px; }";
    html += ".file-links a:hover { text-decoration: underline; }";
    html += "</style></head><body>";
    
    // Header
    html += "<div class='header'>";
    html += "<h1>🌱 Intelagro System Dashboard</h1>";
    html += "</div>";
    
    // System Information
    html += "<div class='info-section'>";
    html += "<h2>📊 System Information</h2>";
    html += "<p><strong>Firmware Version:</strong> Intelagro " + String(FIRMWARE_VERSION) + "</p>";
    html += "<p><strong>🏷️🛠️ Chip ID:</strong> " + String(agrodata.station.chip_id, HEX) + "</p>";
    html += "<p><strong>🏷️📶 Signal Quality (RSSI):</strong> " + String(agrodata.station.rssi) + " dBm</p>";
    html += "<p><strong>Current Date:</strong> " + getFecha() + "</p>";
    html += "<p><strong>Current Time:</strong> " + getLocalTime() + " (UTC" + String(TIME_ZONE) + ")</p>";
    html += "</div>";
    
    // Log Controls
    html += "<div class='log-controls'>";
    html += "<h2>📋 System Log Viewer</h2>";
    html += "<form method='GET' action='/'>";
    html += "<div class='control-group'>";
    html += "<label for='lines'>Number of lines:</label>";
    html += "<input type='number' id='lines' name='lines' value='" + String(currentLines) + "' min='1' max='1000'>";
    html += "<button type='submit'>Update View</button>";
    html += "<button class='danger' type='button' onclick='if(confirm(\"Are you sure you want to clear the entire log file? This cannot be undone.\")) { window.location.href=\"/clearlog\"; }'>🗑️ Clear Log File</button>";
    html += "</div>";
    html += "</form>";
    html += "</div>";
    
    // Log Content
    html += "<div class='info-section'>";
    html += "<h2>📄 Last " + String(currentLines) + " Log Lines</h2>";
    html += "<div class='log-content'>" + logContent + "</div>";
    html += "</div>";
    
    // File Download Links
    html += "<div class='info-section'>";
    html += "<h2>📁 Available Files</h2>";
    html += "<ul>";
    
    // List available files
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        String fname = String(file.name());
        html += "<li><a href='/download?file=" + fname + "' target='_blank'>📄 " + fname + "</a></li>";
        file = root.openNextFile();
    }
    
    html += "</ul></div>";
    html += "</body></html>";
    
    return html;
}

// Request characteristic callback: expects "path:offset:len"
class RequestCallback : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        String req = pCharacteristic->getValue().c_str();
        int first = req.indexOf(':');
        int second = req.indexOf(':', first + 1);
        if (first > 0 && second > first) {
            requestedPath = req.substring(0, first);
            requestedOffset = req.substring(first + 1, second).toInt();
            requestedLen = req.substring(second + 1).toInt();
            if (requestedLen > CHUNK_SIZE) requestedLen = CHUNK_SIZE;
            Serial.printf("BLE log request: %s offset=%u len=%u\n", requestedPath.c_str(), requestedOffset, requestedLen);
        } else {
            Serial.println("BLE log request format error");
        }
    }
};

// Read callback: serve requested chunk
class LogReadCallback : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *pCharacteristic) override {
        File logFile = LittleFS.open(requestedPath, FILE_READ);
        uint8_t buffer[CHUNK_SIZE] = {0};
        size_t actualLen = 0;
        Serial.println("BLE log sending");
        if (logFile) {
            size_t fileSize = logFile.size();
            if (requestedOffset < fileSize) {
                logFile.seek(requestedOffset, SeekSet);
                actualLen = logFile.read(buffer, requestedLen);
            }
            logFile.close();
        }
        pCharacteristic->setValue(buffer, actualLen);
        Serial.printf("BLE sent %u bytes from offset %u\n", actualLen, requestedOffset);
    }
};

void setupBLE() {
    BLEDevice::init("AgroLogger");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Request characteristic for chunk info
    pRequestCharacteristic = pService->createCharacteristic(
        "abcdefab-1234-1234-1234-abcdefabc001",
        BLECharacteristic::PROPERTY_WRITE
    );
    pRequestCharacteristic->setCallbacks(new RequestCallback());

    // Log read characteristic for chunk data
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ
    );
    pCharacteristic->setCallbacks(new LogReadCallback());

    // Increase MTU for better throughput
    BLEDevice::setMTU(512);

    pService->start();
    BLEDevice::startAdvertising();
    Serial.println("✅ BLE Log Service started. Ready for client to request and read log chunks.");
}

void setupWiFiAPAndServer() {
    WiFi.softAP(AP_SSID, AP_PASS);
    IPAddress myIP = WiFi.softAPIP();
    logMessage("✅🌐 WiFi AP started. Connect to SSID: " + String(AP_SSID));
    //logMessage("✅🌐 Browse to http://" + myIP.toString());

    LittleFS.begin();
    server.on("/", HTTP_GET, []() {
        // Get the number of lines to display (default 50)
        int numLines = 50;
        if (server.hasArg("lines")) {
            numLines = server.arg("lines").toInt();
            if (numLines < 1) numLines = 1;
            if (numLines > 1000) numLines = 1000;
        }
        
        // Read the last N lines from the log file
        String logContent = readLastNLines(LOG_FILE_PATH, numLines);
        
        // Generate and send the HTML dashboard
        String html = generateLogViewerHTML(logContent, numLines);
        server.send(200, "text/html", html);
    });
    server.on("/download", HTTP_GET, []() {
        String fname = server.arg("file");
        File file = LittleFS.open("/" + fname, "r");
        if (!file) {
            server.send(404, "text/plain", "File not found");
            return;
        }
        server.streamFile(file, "text/plain");
        file.close();
    });
    server.on("/clearlog", HTTP_GET, []() {
        if (clearLogFile()) {
            String html = "<!DOCTYPE html><html><head>";
            html += "<meta http-equiv='refresh' content='2;url=/' />";
            html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#f5f5f5;}</style>";
            html += "</head><body>";
            html += "<h2>✅ Log file cleared successfully!</h2>";
            html += "<p>Redirecting to dashboard...</p>";
            html += "</body></html>";
            server.send(200, "text/html", html);
        } else {
            String html = "<!DOCTYPE html><html><head>";
            html += "<meta http-equiv='refresh' content='3;url=/' />";
            html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#f5f5f5;}</style>";
            html += "</head><body>";
            html += "<h2>❌ Failed to clear log file</h2>";
            html += "<p>Redirecting to dashboard...</p>";
            html += "</body></html>";
            server.send(500, "text/html", html);
        }
    });
    server.begin();
    logMessage("✅🌐 HTTP server started. Browse to http://" + myIP.toString());
}

void handleWiFiServer() {
    server.handleClient();
}