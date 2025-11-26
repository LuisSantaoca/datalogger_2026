#include "simmodem.h"
#include <TinyGsmClient.h>
#include "timeData.h"
#include "sleepSensor.h"
#include "config.h"         // Configuration settings

#include <algorithm>
#include <vector>

// Modem mode constants
#define MODEM_MODE          38  // Preffered mode for the modem <mode> 2 Automatic, 13 GSM only, 38 LTE only, 51 GSM and LTE only
#define CAT_M               1   // CAT-M Preferred mode
#define NB_IOT              2   // NB-IoT Preferred mode
#define CAT_M_NB_IOT        3   // CAT-M and NB-IoT Preferred mode

// Timeout and delay constants
#define SHORT_DELAY           100     // Short delay (100 ms) *60 is minimum to avoid failure to read some data
#define ONE_S_DELAY           1000    // Long delay (1 second)
#define TWO_S_DELAY           1000    // Long delay (1 second)
#define MODEM_PWRKEY_DELAY    1500    // Delay for modem power-on (1.5 s)
#define MODEM_STABILIZE_DELAY 3000    // Delay for modem stabilization (3 s)

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

// Global variables for modem information
extern agrodata_type agrodata;  // Station data struct
String response;                // Variable to store modem responses

// Multi-operator support variables
std::vector<OperatorInfo> availableOperators;
int currentOperatorIndex = -1;
int consecutiveFailures = 0;
const int MAX_OPERATORS = 10;  // Maximum operators to store

// Preferred operator codes (empty for multi-network roaming SIM - uses signal strength priority)
// For single-country SIMs, add operator codes here in priority order
const char* PREFERRED_OPERATORS[] = {
    // Empty for roaming SIM - automatic signal-based selection
    // Example for Mexico: "334090" (AT&T), "334020" (Telcel), "334010" (Movistar)
};
const int NUM_PREFERRED = sizeof(PREFERRED_OPERATORS) / sizeof(PREFERRED_OPERATORS[0]);

// Modem power control function
void modemPwrKeyPulse() {
  digitalWrite(MODEM_PWRKEY_PIN, HIGH); // Set power pin LOW to modem
  delay(MODEM_PWRKEY_DELAY);            // Wait for the modem to power on
  digitalWrite(MODEM_PWRKEY_PIN, LOW);  // Set power pin HIGH to modem
  delay(MODEM_STABILIZE_DELAY);         // Wait for the modem to stabilize
}

void modemRestart() {
  logMessage("🏷️📞 INF: Restarting modem...");
  modemPwrKeyPulse();                 // Power off the modem
  delay(TWO_S_DELAY);                 // Wait for a while
  modemPwrKeyPulse();                 // Power on the modem
  delay(MODEM_STABILIZE_DELAY);       // Wait for the modem to initialize
  //logMessage("✅📞 INF: Modem restart complete.");
}

bool modemFullReset() {
  logMessage("🏷️📞 INF: Performing full modem reset...");
  
  // Disable all connections first
  sendATCommand("+CNACT=0,0", "OK", 2000, response);
  delay(1000);
  sendATCommand("+CFUN=0", "OK", 2000, response);
  delay(2000);
    // Power cycle the modem
  modemRestart();
  
  // Verify modem is responding
  int retries = 0;
  while (!modem.testAT(1000) && retries < 10) {
    logMessage("⚠️📞 WAR: Waiting for modem after reset, retry " + String(retries + 1) + "/10");
    delay(1000);
    retries++;
  }
  
  if (retries >= 10) {
    logMessage("❌📞 ERR: Modem not responding after full reset.");
    return false;
  }
  
  logMessage("✅📞 INF: Modem full reset successful.");
  return true;
}

// Scan for available operators
bool scanAvailableOperators(void) {
  logMessage("🏷️📱 INF: Scanning for available operators (may take 30-120s)...");
  
  availableOperators.clear();
  
  flushPortSerial();
  modem.sendAT("+COPS=?");
  
  unsigned long start = millis();
  unsigned long timeout = 120000;  // 120 seconds
  String copsResponse = "";
  bool foundCops = false;
  
  unsigned long lastLog = 0;
  while (millis() - start < timeout) {
    while (SerialAT.available()) {
      char c = SerialAT.read();
      copsResponse += c;
    }
    
    // Log progress every 10 seconds
    if (millis() - lastLog > 10000) {
      logMessage("⏳ Scanning... " + String((millis() - start) / 1000) + "s");
      lastLog = millis();
    }
    
    // Check if response complete
    if (copsResponse.indexOf("+COPS:") != -1 && copsResponse.indexOf("OK") != -1) {
      foundCops = true;
      break;
    }
    
    delay(100);
  }
  
  if (!foundCops) {
    logMessage("❌📱 ERR: Operator scan timeout after " + String(timeout/1000) + "s");
    return false;
  }
  
  logMessage("✅📱 INF: Operator scan complete.");
  parseCopsResponse(copsResponse);
  
  return availableOperators.size() > 0;
}

// Parse COPS response and build operator list
void parseCopsResponse(const String& copsResponse) {
  logMessage("🏷️📱 INF: Parsing operator list...");
  
  int startPos = copsResponse.indexOf("+COPS:");
  if (startPos == -1) {
    logMessage("⚠️📱 WAR: No operator data found.");
    return;
  }
  
  String copsData = copsResponse.substring(startPos + 6);
  int operatorCount = 0;
  int pos = 0;
  
  while (pos < copsData.length() && operatorCount < MAX_OPERATORS) {
    int parenStart = copsData.indexOf('(', pos);
    if (parenStart == -1) break;
    
    int parenEnd = copsData.indexOf(')', parenStart);
    if (parenEnd == -1) break;
    
    String operatorEntry = copsData.substring(parenStart + 1, parenEnd);
    
    // Parse fields: status,"long","short","numeric",tech
    String fields[5];
    int fieldCount = 0;
    int lastComma = -1;
    bool inQuotes = false;
    
    for (int i = 0; i < operatorEntry.length() && fieldCount < 5; i++) {
      char c = operatorEntry.charAt(i);
      
      if (c == '"') {
        inQuotes = !inQuotes;
      } else if (c == ',' && !inQuotes) {
        fields[fieldCount] = operatorEntry.substring(lastComma + 1, i);
        fields[fieldCount].replace("\"", "");
        fields[fieldCount].trim();
        fieldCount++;
        lastComma = i;
      }
    }
    
    // Get last field
    if (fieldCount < 5 && lastComma < operatorEntry.length() - 1) {
      fields[fieldCount] = operatorEntry.substring(lastComma + 1);
      fields[fieldCount].replace("\"", "");
      fields[fieldCount].trim();
      fieldCount++;
    }
    
    if (fieldCount >= 4) {
      int statusCode = fields[0].toInt();
      String longName = fields[1];
      String shortName = fields[2];
      String numeric = fields[3];
      
      // Add operator to list
      OperatorInfo op;
      op.name = (shortName.length() > 0) ? shortName : longName;
      op.code = numeric;
      op.rsrp = 0;
      op.rsrq = 0;
      op.rssi = 0;
      op.snr = 0;
      op.connected = false;
      op.score = (statusCode == 1) ? 100 : 10;  // Available operators get priority
      
      availableOperators.push_back(op);
      operatorCount++;
      
      logMessage("🏷️📱 INF: Found operator: " + op.name + " (" + numeric + ") - " + 
                 (statusCode == 1 ? "Available" : statusCode == 2 ? "Current" : "Other"));
    }
    
    pos = parenEnd + 1;
  }
  
  logMessage("🏷️📱 INF: Found " + String(availableOperators.size()) + " operators before sorting.");
  
  // Sort operators: by signal strength for roaming SIMs, preferred first for single-country
  std::sort(availableOperators.begin(), availableOperators.end(), 
            [](const OperatorInfo& a, const OperatorInfo& b) {
              // For roaming SIMs (empty preferred list), sort purely by availability and score
              if (NUM_PREFERRED == 0) {
                // Available operators first (status=1 gets score=100)
                if (a.score >= 100 && b.score < 100) return true;
                if (a.score < 100 && b.score >= 100) return false;
                
                // If both available or both unavailable, sort by score
                // Higher score = better signal (will be updated with CPSI later)
                return a.score > b.score;
              }
              
              // For single-country SIMs with preferred operators
              bool aPref = false, bPref = false;
              int aIndex = 999, bIndex = 999;
              
              for (int i = 0; i < NUM_PREFERRED; i++) {
                if (a.code == PREFERRED_OPERATORS[i]) { aPref = true; aIndex = i; }
                if (b.code == PREFERRED_OPERATORS[i]) { bPref = true; bIndex = i; }
              }
              
              if (aPref && !bPref) return true;
              if (!aPref && bPref) return false;
              if (aPref && bPref) return aIndex < bIndex;
              
              return a.score > b.score;
            });
  
  logMessage("✅📱 INF: Found " + String(availableOperators.size()) + " operators, sorted by " + 
             String(NUM_PREFERRED == 0 ? "signal strength" : "priority") + ".");
}

void setupModem() {
  SerialMon.begin(CONSOLE_BAUDRATE);
  SerialAT.begin(MODEM_UART_BAUDRATE, MODEM_UART_CFG, MODEM_UART_RX_PIN, MODEM_UART_TX_PIN);
}

// Calculate adaptive timeout based on signal quality and failure history
unsigned long getAdaptiveTimeout(unsigned long baseTimeout) {
  unsigned long timeout = baseTimeout;
  
  // Adjust based on signal quality
  if (agrodata.station.rssi > 20) {
    timeout = (unsigned long)(timeout * 0.6);  // Good signal, reduce timeout
  } else if (agrodata.station.rssi < 10) {
    timeout = (unsigned long)(timeout * 1.2);  // Poor signal, increase timeout
  }
  
  // Add delay for consecutive failures
  if (consecutiveFailures > 0) {
    timeout += (consecutiveFailures * 1000);
  }
  
  // Clamp to reasonable limits
  if (timeout > 20000) timeout = 20000;
  if (timeout < 3000) timeout = 3000;
  
  return timeout;
}

String getSimCCIDDirect() {           // Helper function to get CCID directly and robustly
  int startIdx = -1;
  int endIdx = -1;
  if (sendATCommand("+CCID", "OK", SHORT_DELAY, response)) {
    //logMessage("🏷️ Raw CCID Response: " + response);
    // Find a line with only digits (CCID is usually 19-20 digits)
    for (int i = 0; i < response.length(); ++i) {
      if (isDigit(response[i])) {
        if (startIdx == -1) startIdx = i;
        endIdx = i;
      } else if (startIdx != -1 && !isDigit(response[i])) {
        break;
      }
    }
    if (startIdx != -1 && endIdx != -1 && (endIdx - startIdx + 1) >= 19) {
      return response.substring(startIdx, endIdx + 1);
    }
  } else {
    return "--"; // Return placeholder if not found
  }
}

// Get CPSI signal metrics for detailed signal analysis
bool getCpsiMetrics(OperatorInfo& operatorInfo) {
  logMessage("🏷️📶 INF: Getting CPSI signal metrics...");
  
  String cpsiResponse;
  if (!sendATCommand("+CPSI?", "OK", 5000, cpsiResponse)) {
    logMessage("⚠️📶 WAR: Failed to get CPSI response.");
    return false;
  }
  
  // Parse CPSI response
  int startPos = cpsiResponse.indexOf("+CPSI:");
  if (startPos == -1) {
    logMessage("⚠️📶 WAR: No CPSI data in response.");
    return false;
  }
  
  String cpsiData = cpsiResponse.substring(startPos + 6);
  cpsiData.trim();
  
  // Split by commas and extract metrics
  int commaCount = 0;
  int lastComma = -1;
  String values[20];
  
  for (int i = 0; i < cpsiData.length(); i++) {
    if (cpsiData.charAt(i) == ',' || i == cpsiData.length() - 1) {
      if (commaCount < 20) {
        int endPos = (i == cpsiData.length() - 1) ? i + 1 : i;
        values[commaCount] = cpsiData.substring(lastComma + 1, endPos);
        values[commaCount].trim();
      }
      lastComma = i;
      commaCount++;
    }
  }
  
  if (commaCount < 13) {
    logMessage("⚠️📶 WAR: Incomplete CPSI response.");
    return false;
  }
  
  // Check if online
  if (values[1].indexOf("Online") == -1) {
    operatorInfo.connected = false;
    return false;
  }
  
  operatorInfo.connected = true;
  
  // Extract signal metrics (typical positions for LTE)
  if (commaCount >= 14) {
    operatorInfo.rsrp = values[10].toInt();
    operatorInfo.rsrq = values[11].toInt();
    operatorInfo.rssi = values[12].toInt();
    operatorInfo.snr = values[13].toInt();
    
    logMessage("✅📶 INF: CPSI Metrics - RSRP: " + String(operatorInfo.rsrp) + 
               " dBm, RSRQ: " + String(operatorInfo.rsrq) + 
               " dB, RSSI: " + String(operatorInfo.rssi) + 
               " dBm, SNR: " + String(operatorInfo.snr) + " dB");
    
    // Calculate score based on metrics
    int score = 0;
    if (operatorInfo.rsrp >= -80) score += 50;
    else if (operatorInfo.rsrp >= -90) score += 40;
    else if (operatorInfo.rsrp >= -100) score += 30;
    else if (operatorInfo.rsrp >= -110) score += 20;
    else score += 10;
    
    if (operatorInfo.rsrq >= -8) score += 30;
    else if (operatorInfo.rsrq >= -12) score += 25;
    else if (operatorInfo.rsrq >= -15) score += 20;
    else score += 15;
    
    if (operatorInfo.snr >= 20) score += 20;
    else if (operatorInfo.snr >= 10) score += 12;
    else if (operatorInfo.snr >= 0) score += 5;
    
    operatorInfo.score = score;
    logMessage("🏷️🏆 INF: Signal quality score: " + String(score));
  }
  
  return true;
}

// Connect with specific operator
bool connectWithOperator(int operatorIndex) {
  if (operatorIndex < 0 || operatorIndex >= availableOperators.size()) {
    logMessage("❌📞 ERR: Invalid operator index: " + String(operatorIndex));
    return false;
  }
  
  OperatorInfo& op = availableOperators[operatorIndex];
  logMessage("🏷️📞 INF: Attempting connection with " + op.name + " (" + op.code + ")...");
  
  // Select operator
  String copsCommand = "+COPS=1,2,\"" + op.code + "\"";
  if (!sendATCommand(copsCommand, "OK", 15000, response)) {
    logMessage("⚠️📞 WAR: Failed to select operator " + op.name);
    return false;
  }
  
  delay(2000);
  
  // Configure APN
  if (!sendATCommand("+CGDCONT=1,\"IP\",\"" APN "\"", "OK", 2000, response)) {
    logMessage("⚠️📞 WAR: Failed to set APN for " + op.name);
    return false;
  }
  
  // Wait for network registration
  logMessage("🏷️📞 INF: Waiting for network registration with " + op.name + "...");
  unsigned long regStart = millis();
  int regTries = 0;
  int maxTries = 60;  // 60 seconds
  
  while (regTries < maxTries) {
    if (checkCellularNetwork()) {
      // Get signal quality
      agrodata.station.rssi = modem.getSignalQuality();
      logMessage("✅📞 INF: Registered with " + op.name + " (RSSI: " + 
                 String(agrodata.station.rssi) + " dBm) after " + 
                 String((millis() - regStart) / 1000) + "s");
      
      // Get detailed CPSI metrics
      getCpsiMetrics(op);
      
      // Activate PDP context
      delay(2000);
      logMessage("🏷️📡 INF: Activating PDP context...");
      
      // Check if already active
      if (sendATCommand("+CNACT?", "OK", 2000, response)) {
        if (response.indexOf(",ACTIVE") != -1) {
          logMessage("✅📡 INF: PDP context already active.");
          currentOperatorIndex = operatorIndex;
          consecutiveFailures = 0;
          return true;
        }
      }
      
      // Activate
      if (!sendATCommand("+CNACT=0,1", ",ACTIVE", 5000, response)) {
        logMessage("⚠️📡 WAR: PDP activation failed, retrying...");
        delay(2000);
        if (!sendATCommand("+CNACT=0,1", "OK", 5000, response)) {
          logMessage("❌📡 ERR: PDP activation failed for " + op.name);
          return false;
        }
      }
      
      logMessage("✅📡 INF: PDP context activated successfully.");
      currentOperatorIndex = operatorIndex;
      consecutiveFailures = 0;
      return true;
    }
    
    regTries++;
    delay(1000);
    
    // Log progress every 10 attempts
    if (regTries % 10 == 0) {
      int rssi = modem.getSignalQuality();
      logMessage("🏷️📶 INF: Waiting... RSSI: " + String(rssi) + " (" + String(regTries) + "/" + String(maxTries) + ")");
    }
  }
  
  logMessage("❌📞 ERR: Registration timeout with " + op.name + " after " + String(maxTries) + "s");
  return false;
}

// Start LTE with multi-operator fallback
bool startLTEWithFallback(void) {
  logMessage("🏷️📞 ======== START LTE WITH MULTI-OPERATOR FALLBACK ========");
  
  // Step 1: Power on and initialize modem
  logMessage("🏷️📞 INF: Powering on modem...");
  modemPwrKeyPulse();
  
  if (!sendATCommand("+CFUN=1", "OK", 2000, response)) {
    logMessage("⚠️📞 WAR: CFUN=1 failed, response: " + response);
  }
  delay(1000);
  
  // Step 2: Wait for modem ready
  int atRetries = 0;
  while (!modem.testAT(1000) && atRetries < 10) {
    logMessage("⚠️📞 WAR: Modem not responding, retry " + String(atRetries + 1) + "/10");
    atRetries++;
    delay(1000);
    if (atRetries >= 5) {
      modemPwrKeyPulse();
      delay(2000);
    }
  }
  
  if (atRetries >= 10) {
    logMessage("❌📞 ERR: Modem failed to respond after retries.");
    agrodata.station.rssi = 255;
    return false;
  }
  logMessage("✅📞 INF: Modem responding.");
  
  // Step 3: Check SIM
  int simRetries = 0;
  while (!sendATCommand("+CPIN?", "READY", ONE_S_DELAY, response) && simRetries < 5) {
    logMessage("⚠️📞 WAR: SIM not ready, retry " + String(simRetries + 1) + "/5");
    simRetries++;
    delay(2000);
  }
  
  if (simRetries >= 5) {
    logMessage("❌📞 ERR: SIM Card failure.");
    agrodata.station.rssi = 255;
    return false;
  }
  logMessage("✅📞 INF: SIM card ready.");
  
  // Step 4: Get SIM info
  String ccid = getSimCCIDDirect();
  strncpy(agrodata.station.sim_ccid, ccid.c_str(), sizeof(agrodata.station.sim_ccid) - 1);
  agrodata.station.sim_ccid[sizeof(agrodata.station.sim_ccid) - 1] = '\0';
  logMessage("🏷️📞 INF: SIM CCID: " + String(agrodata.station.sim_ccid));
  
  // Step 5: Configure modem mode
  logMessage("🏷️📞 INF: Configuring modem (Mode: " + String(MODEM_MODE) + ", Cat: " + String(CAT_M) + ")...");
  if (!sendATCommand("+CNMP=" + String(MODEM_MODE), "OK", 2000, response)) {
    logMessage("⚠️📞 WAR: CNMP failed: " + response);
  }
  if (!sendATCommand("+CMNB=" + String(CAT_M), "OK", 2000, response)) {
    logMessage("⚠️📞 WAR: CMNB failed: " + response);
  }
  delay(1000);
  
  // Step 6: For roaming SIMs, try automatic mode first (faster and more reliable)
  if (NUM_PREFERRED == 0) {
    logMessage("🏷️📱 INF: Roaming SIM detected, trying automatic operator selection first...");
    
    if (sendATCommand("+COPS=0", "OK", 10000, response)) {
      logMessage("✅📱 INF: Automatic mode set, waiting for network registration...");
      
      int autoRetries = 0;
      unsigned long autoStart = millis();
      while (autoRetries < 30 && (millis() - autoStart < 30000)) {  // 30 seconds max
        if (checkCellularNetwork()) {
          agrodata.station.rssi = modem.getSignalQuality();
          logMessage("✅📞 INF: Automatic registration successful! RSSI: " + 
                     String(agrodata.station.rssi) + " dBm");
          
          // Activate PDP context
          delay(2000);
          if (sendATCommand("+CNACT?", "OK", 2000, response)) {
            if (response.indexOf(",ACTIVE") != -1) {
              logMessage("✅📡 INF: PDP context already active in automatic mode.");
              return true;
            }
          }
          
          if (sendATCommand("+CNACT=0,1", ",ACTIVE", 5000, response) || 
              sendATCommand("+CNACT=0,1", "OK", 5000, response)) {
            logMessage("✅📡 INF: PDP context activated in automatic mode.");
            consecutiveFailures = 0;
            return true;
          }
        }
        autoRetries++;
        delay(1000);
      }
      
      logMessage("⚠️📱 WAR: Automatic mode timeout, falling back to manual selection...");
      sendATCommand("+COPS=2", "OK", 5000, response);  // Deregister
      delay(2000);
    } else {
      logMessage("⚠️📱 WAR: Failed to set automatic mode, using manual selection...");
    }
  }
  
  // Step 7: Scan for operators (manual selection fallback)
  logMessage("🏷️📱 INF: Starting manual operator selection...");
  if (!scanAvailableOperators()) {
    logMessage("⚠️📱 WAR: Operator scan failed, trying default connection...");
    // Fallback to original single-operator method
    return startLTE();
  }
  
  // Step 8: Try each operator in priority order
  for (int i = 0; i < availableOperators.size(); i++) {
    logMessage("🏷️📞 INF: Trying operator " + String(i + 1) + "/" + String(availableOperators.size()) + ": " + 
               availableOperators[i].name);
    
    if (connectWithOperator(i)) {
      logMessage("✅📞 INF: Successfully connected with " + availableOperators[i].name + "!");
      return true;
    }
    
    logMessage("⚠️📞 WAR: Failed with " + availableOperators[i].name + ", trying next...");
    
    // Reset modem if this was a preferred operator that failed
    bool isPreferred = false;
    for (int p = 0; p < NUM_PREFERRED; p++) {
      if (availableOperators[i].code == PREFERRED_OPERATORS[p]) {
        isPreferred = true;
        break;
      }
    }
    
    if (isPreferred && i < availableOperators.size() - 1) {
      logMessage("🏷️🔄 INF: Performing quick modem reset before next operator...");
      sendATCommand("+CFUN=0", "OK", 2000, response);
      delay(2000);
      sendATCommand("+CFUN=1", "OK", 2000, response);
      delay(2000);
    }
  }
  
  // All operators failed
  logMessage("❌📞 ERR: All " + String(availableOperators.size()) + " operators failed.");
  agrodata.station.rssi = 255;
  consecutiveFailures++;
  return false;
}

// Get operator count
int getOperatorCount(void) {
  return availableOperators.size();
}

// Get operator info
OperatorInfo* getOperatorInfo(int index) {
  if (index < 0 || index >= availableOperators.size()) {
    return nullptr;
  }
  return &availableOperators[index];
}

bool startLTE() {
  logMessage("🏷️📞 ======== START LTE ========");
  
  // Step 1: Power on the modem
  //logMessage("🏷️📞 INF: Powering on modem...");
  //modemPwrKeyPulse();
  
  // Step 2: Enable full functionality
  if (!sendATCommand("+CFUN=1", "OK", 2000, response)) {
    logMessage("⚠️📞 WAR: CFUN=1 failed, response: " + response);
  }
  delay(1000);
  
  // Step 3: Wait for modem to respond to AT (with timeout)
  int atRetries = 0;
  while (!modem.testAT(1000) && atRetries < 10) {
    logMessage("⚠️📞 WAR: Modem not responding to AT, retry " + String(atRetries + 1) + "/10");
    atRetries++;
    delay(1000);
    if (atRetries >= 5) {
      logMessage("⚠️📞 WAR: Attempting modem power cycle...");
      modemPwrKeyPulse();
      delay(2000);
    }
  }
  
  if (atRetries >= 10) {
    logMessage("❌📞 ERR: Modem failed to respond to AT commands after retries.");
    agrodata.station.rssi = 255;
    return false;
  }
  logMessage("✅📞 INF: Modem responding to AT commands.");
  
  // Step 4: Check SIM card status
  int simRetries = 0;
  while (!sendATCommand("+CPIN?", "READY", ONE_S_DELAY, response) && simRetries < 5) {
    logMessage("⚠️📞 WAR: SIM not ready, retry " + String(simRetries + 1) + "/5, response: " + response);
    simRetries++;
    delay(2000);
  }
  
  if (simRetries >= 5) {
    logMessage("❌📞 ERR: SIM Card failure or not present after retries.");
    agrodata.station.rssi = 255;
    return false;
  }
  logMessage("✅📞 INF: SIM card ready.");
  
  // Step 5: Get and log SIM CCID
  String ccid = getSimCCIDDirect();
  strncpy(agrodata.station.sim_ccid, ccid.c_str(), sizeof(agrodata.station.sim_ccid) - 1);
  agrodata.station.sim_ccid[sizeof(agrodata.station.sim_ccid) - 1] = '\0';
  logMessage("🏷️📞 INF: SIM CCID: " + String(agrodata.station.sim_ccid));
  
  // Step 6: Configure modem mode and network preferences
  logMessage("🏷️📞 INF: Configuring modem for LTE (Mode: " + String(MODEM_MODE) + ", Cat: " + String(CAT_M) + ")...");
  if (!sendATCommand("+CNMP=" + String(MODEM_MODE), "OK", 2000, response)) {
    logMessage("⚠️📞 WAR: CNMP command failed, response: " + response);
  }
  if (!sendATCommand("+CMNB=" + String(CAT_M), "OK", 2000, response)) {
    logMessage("⚠️📞 WAR: CMNB command failed, response: " + response);
  }
  
  // Step 7: Set APN
  logMessage("🏷️📞 INF: Setting APN: " APN);
  if (!sendATCommand("+CGDCONT=1,\"IP\",\"" APN "\"", "OK", 2000, response)) {
    logMessage("⚠️📞 WAR: APN configuration failed, response: " + response);
  }
  delay(1000);
  
  // Step 8: Wait for network registration
  logMessage("🏷️📞 INF: Waiting for network registration...");
  int regTries = 0;
  int lastRssi = 0;
  unsigned long regStart = millis();
  
  while (regTries < 600) {
    // Check signal quality every 10 attempts
    if (regTries % 10 == 0) {
      int currentRssi = modem.getSignalQuality();
      if (currentRssi != lastRssi) {
        logMessage("🏷️📶 INF: Current RSSI: " + String(currentRssi) + " dBm (attempt " + String(regTries) + "/600)");
        lastRssi = currentRssi;
      }
    }
    
    if (checkCellularNetwork()) {
      agrodata.station.rssi = modem.getSignalQuality();
      logMessage("✅📞 INF: Network registered! Final RSSI: " + String(agrodata.station.rssi) + " dBm");
      logMessage("🏷️📞 INF: Registration took " + String((millis() - regStart) / 1000) + " seconds.");
      
      // Step 9: Activate PDP context
      delay(2000);
      logMessage("🏷️📡 INF: Activating PDP context...");
      
      // First check if already active
      if (sendATCommand("+CNACT?", "OK", 2000, response)) {
        //logMessage("🏷️📡 INF: Current PDP status: " + response);
        if (response.indexOf(",ACTIVE") != -1) {
          logMessage("✅📡 INF: PDP context already active.");
          return true;
        }
      }
      
      // Try to activate
      if (!sendATCommand("+CNACT=0,1", ",ACTIVE", 5000, response)) {
        logMessage("⚠️📡 WAR: PDP activation failed on first try, response: " + response);
        
        // Retry activation
        delay(2000);
        if (!sendATCommand("+CNACT=0,1", "OK", 5000, response)) {
          logMessage("❌📡 ERR: PDP activation failed after retry. Response: " + response);
          return false;
        }
      }
      
      logMessage("✅📡 INF: PDP context activated successfully.");
      delay(1000);
      return true;
    }
    
    regTries++;
    delay(ONE_S_DELAY);
  }
  
  // Network registration timeout
  agrodata.station.rssi = modem.getSignalQuality();
  logMessage("❌📡 ERR: Network registration timeout after 600 attempts (" + String((millis() - regStart) / 1000) + "s).");
  logMessage("❌📡 ERR: Final RSSI: " + String(agrodata.station.rssi) + " dBm");
  
  if (agrodata.station.rssi == 0 || agrodata.station.rssi == 99) {
    agrodata.station.rssi = 255;
    logMessage("❌📡 ERR: No signal detected. Consider antenna or location issues.");
  }
  
  return false;
}

bool tcpOpen() {
  logMessage("🏷️🌐 INF: Opening TCP connection to " DB_SERVER_IP ":" TCP_PORT "...");
  
  // First check if connection already exists and close it
  if (sendATCommand("+CASTATE?", "OK", 1000, response)) {
    if (response.indexOf("+CASTATE: 0,1") != -1) {
      logMessage("⚠️🌐 WAR: TCP connection 0 already exists, closing...");
      tcpClose();
      delay(1000);
    }
  }
  
  String command = "+CAOPEN=0,0,\"TCP\",\"" DB_SERVER_IP "\"," TCP_PORT;
  if (sendATCommand(command, "+CAOPEN: 0,0", 10000, response)) {
    logMessage("✅🌐 INF: TCP connection opened successfully.");
    return true;
  } else {
    logMessage("❌🌐 ERR: Failed to open TCP connection. Response: " + response);
    
    // Check network status
    if (sendATCommand("+CNACT?", "OK", 2000, response)) {
      logMessage("🏷️📡 INF: PDP context status: " + response);
      if (response.indexOf("ACTIVE") == -1) {
        logMessage("❌📡 ERR: PDP context not active. Network connection may be lost.");
      }
    }
    return false;
  }
}

bool tcpClose() {
  logMessage("🏷️🌐 INF: Closing TCP connection...");
  if (sendATCommand("+CACLOSE=0", "OK", 3000, response)) {
    logMessage("✅🌐 INF: TCP connection closed.");
    return true;
  } else {
    logMessage("⚠️🌐 WAR: TCP close response: " + response);
    return false;
  }
}

bool tcpReadData() {
  return sendATCommand("+CARECV=0,1", "OK", SHORT_DELAY, response); // Read data from TCP
}

bool tcpSendData(String datos, int retries) {
  //Clean string
  datos.replace("\r", "");
  datos.replace("\n", "");

  for (int attempt = 0; attempt < retries; attempt++) {
    if (sendATCommand("+CASEND=0," + String(datos.length() + 2), ">", 3000, response)) {
      if (sendATCommand(datos, "CADATAIND: 0", 3000, response)) {    // Send data and check response if receibed, server confirmed
        return true; // Return true if data is sent successfully
      }
    }
    logMessage("⚠️🌐 WAR: Attempt " + String(attempt + 1) + " failed. Retrying...");
    delay(ONE_S_DELAY); // Wait before retrying
  }
  logMessage("❌🌐 ERR: Failed to send data after " + String(retries) + " attempts.");
  return false; // Return false if all attempts fail
}

// Read data from TCP and check for acknowledgment character
bool tcpReadData(char ackChar) {
  if (sendATCommand("+CARECV=0,256", "OK", SHORT_DELAY, response)) {
    // Check if the acknowledgment character is present in the response
    if (response.indexOf(ackChar) != -1) {
      //logMessage("TCP acknowledgment received: " + String(ackChar));
      return true;
    } else {
      logMessage("❌🌐 ERR: TCP acknowledgment NOT received. Response: " + response);
      return false;
    }
  }
  logMessage("❌🌐 ERR: TCP read failed.");
  return false;
}

// Helper function to flush the serial port
void flushPortSerial() {
  while (SerialAT.available()) {
    char c = SerialAT.read(); // Read and discard all available data
  }
}

// Helper function to read response from the modem
String readResponse(unsigned long timeout) {
  unsigned long start = millis(); // Record the start time
  String response = "";

  flushPortSerial(); // Clear the serial buffer
  while (millis() - start < timeout) { // Wait until timeout
    while (SerialAT.available()) {
      char c = SerialAT.read(); // Read available data
      response += c;
    }
  }
  return response; // Return the response
}

// Helper function to send AT commands and wait for a specific response
bool sendATCommand(const String& command, const String& expectedResponse, unsigned long timeout, String &response) {
  response = "";
  unsigned long start = millis(); // Record the start time

  flushPortSerial(); // Clear the serial buffer
  //logMessage("Command:" + command); // Print the command being sent
  modem.sendAT(command); // Send the AT command
  while (millis() - start < timeout) { // Wait until timeout
    while (SerialAT.available()) {
      char c = SerialAT.read(); // Read available data
      response += c;
      if (response.indexOf(expectedResponse) != -1) { // Check if the expected response is received
        //logMessage(":\r" + response); // Print the response
        return true; // Return true if the response matches
      }
    }
  }
  return false; // Return false if the response does not match
}

// Function to stop GNSS on SIM7080 module
void stopGpsModem() {
  modem.disableGPS(); // Disable GPS function
  delay(3000); // Wait for a while
}

// Function to setup GPS on SIM7080 module
bool setupGpsSim(void) { 
  logMessage("🏷️🛰️ ======== INIT GPS ========");
  logMessage("🏷️📞 INF: Starting modem... ", false);
//  modemPwrKeyPulse(); // Power on the modem
  int retry = 0;
  while (!modem.testAT(1000)) {
    logMessage("#", false);
    if (retry++ > 3) {
      modemPwrKeyPulse();      // Send pulse to power key
      retry = 0;
      logMessage("\n⚠️📞 WAR: Retry start modem.");
    }
  }
  logMessage("✅📞 INF: Modem started!");
  modem.sendAT("+CFUN=0");
  modem.waitResponse();
  delay(3000);
  // When configuring GNSS, you need to stop GPS first
  modem.disableGPS();
  delay(500);
  // GNSS Work Mode Set GPS+BEIDOU [gps, glo, bd, gal, qzss]
  modem.sendAT("+CGNSMOD=1,0,1,0,0");
  modem.waitResponse();
  // minInterval = 1000,minDistance = 0,accuracy = 0 []
  //modem.sendAT("+SGNSCMD=2,1000,0,0");
  modem.sendAT("+SGNSCMD=1,0");
  modem.waitResponse();
  // Turn off GNSS.
  modem.sendAT("+SGNSCMD=0");
  modem.waitResponse();
  delay(500);
  // GPS function needs to be enabled for the first use
  if (modem.enableGPS() == false) {
    logMessage("❌📞🛰️ ERR: Modem enable gps function failed!!");
    return false; // Return false if enabling GPS fails
  }
  return true; // Return true if GPS is enabled successfully
}

// Function to get GPS position from SIM7080 module
bool getGpsSim(gpsdata_type &gpsdata, uint16_t GPS_RETRIES) {
  // Try to get GPS data up to n times
  for (int i = 0; i < GPS_RETRIES; ++i) {
    if (modem.getGPS(&gpsdata.lat, &gpsdata.lon, &gpsdata.speed, &gpsdata.alt, 
                     &gpsdata.vsat, &gpsdata.usat, &gpsdata.accuracy,
                     &gpsdata.year, &gpsdata.month, &gpsdata.day,
                     &gpsdata.hour, &gpsdata.min, &gpsdata.sec)) {
      logMessage("✅🛰️ INF: Successfully got GPS data!");
      modem.disableGPS(); 
      delay(3000);
      return true;                // Return true if GPS data is successfully retrieved
    } else {
        //logMessage(".");        // Print a dot to indicate an attempt
        delay(1000);              // Wait for 1 second before retrying
        if (i == GPS_RETRIES-1) { // If this is the last attempt
          logMessage("❌🛰️ ERR: Failed to get GPS data after " + String(GPS_RETRIES) + " attempts.");
          modem.disableGPS();     // Disable GPS to save power
          delay(3000);
          return false;           // Return false if GPS data retrieval fails
      }
    }
  }
}

/**
 * @brief Gets the RSSI (signal quality) from the modem.
 * @return The RSSI value as uint8_t.
 */
uint8_t getRSSI(void) {
    return modem.getSignalQuality();
}

/**
 * @brief Requests cellular network registration and verifies connection status.
 *        Uses AT+CGREG? and AT+CEREG? to check registration.
 * @return true if registered and connected, false otherwise.
 */
bool checkCellularNetwork(void) {
    // Check EPS (LTE) network registration
    if (!sendATCommand("+CEREG?", "OK", 1000, response)) {
        logMessage("❌📞 ERR: Failed to get CEREG status.");
        return false;
    }
    
    // Parse registration status
    // +CEREG: <n>,<stat>[,<tac>,<ci>,<AcT>]
    // stat: 0=not registered, 1=registered home, 2=searching, 3=denied, 4=unknown, 5=registered roaming
    if (response.indexOf("+CEREG:") != -1) {
        if (response.indexOf(",1") != -1) {
            logMessage("✅📞 INF: LTE registered (home network).");
            return true;
        } else if (response.indexOf(",5") != -1) {
            logMessage("✅📞 INF: LTE registered (roaming).");
            return true;
        } else if (response.indexOf(",2") != -1) {
            // Still searching, don't log error
            return false;
        } else if (response.indexOf(",3") != -1) {
            logMessage("❌📞 ERR: Network registration denied. Response: " + response);
            return false;
        } else if (response.indexOf(",0") != -1 || response.indexOf(",4") != -1) {
            // Not registered or unknown - normal during connection
            return false;
        }
    }
    
    return false;
}
