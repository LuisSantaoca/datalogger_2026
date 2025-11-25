/**
 * @file cryptoaes.cpp
 * @brief Implementación del sistema de encriptación AES
 * @author Elathia
 * @date 2025
 * @warning Claves hardcodeadas - cambiar en producción
 */

#include "cryptoaes.h"
#include "AESLib.h"
#include "arduino_base64.hpp"

#define AES_KEY_LENGTH 16
#define AES_BLOCK_SIZE 16
#define MAX_INPUT_SIZE 1024
#define ENCRYPTION_RETRIES 3

static const uint8_t AES_KEY[AES_KEY_LENGTH] = { 
  56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 74, 65, 77, 82 
};

static const uint8_t AES_IV[AES_KEY_LENGTH] = { 
  56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 74, 65, 77, 82 
};

static const uint8_t AES_DECRYPT_KEY[AES_KEY_LENGTH] = { 
   56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 74, 65, 77, 82 
};

AESLib aesLib;

struct CryptoStatus {
  uint32_t encryptionCount = 0;
  uint32_t decryptionCount = 0;
  uint32_t encryptionErrors = 0;
  uint32_t decryptionErrors = 0;
  uint32_t lastOperationTime = 0;
  bool systemHealthy = true;
};

static CryptoStatus cryptoStatus;

void logCryptoMessage(const String& level, const String& message, const String& operation = "") {
  String timestamp = String(millis());
  String prefix = "🔐 [CRYPTO] ";
  
  if (operation.length() > 0) {
    prefix += "[" + operation + "] ";
  }
  
  Serial.println(prefix + "[" + level + "] " + timestamp + "ms: " + message);
}

bool validateInputParameters(const void* input, int length, const String& operation) {
  if (input == nullptr) {
    logCryptoMessage("ERROR", "Puntero de entrada nulo", operation);
    return false;
  }
  
  if (length <= 0) {
    logCryptoMessage("ERROR", "Longitud de entrada inválida: " + String(length), operation);
    return false;
  }
  
  if (length > MAX_INPUT_SIZE) {
    logCryptoMessage("ERROR", "Longitud de entrada excede el máximo: " + String(length) + " > " + String(MAX_INPUT_SIZE), operation);
    return false;
  }
  
  return true;
}

bool isCryptoSystemHealthy() {
  if (!cryptoStatus.systemHealthy) {
    logCryptoMessage("WARNING", "Sistema de encriptación en estado no saludable");
    return false;
  }
  
  if (AES_KEY == nullptr || AES_IV == nullptr) {
    logCryptoMessage("ERROR", "Claves AES no configuradas");
    cryptoStatus.systemHealthy = false;
    return false;
  }
  
  return true;
}

String getCryptoStats() {
  String stats = "🔐 ESTADÍSTICAS DEL SISTEMA DE ENCRIPTACIÓN:\n";
  stats += "   Encriptaciones exitosas: " + String(cryptoStatus.encryptionCount) + "\n";
  stats += "   Desencriptaciones exitosas: " + String(cryptoStatus.decryptionCount) + "\n";
  stats += "   Errores de encriptación: " + String(cryptoStatus.encryptionErrors) + "\n";
  stats += "   Errores de desencriptación: " + String(cryptoStatus.decryptionCount) + "\n";
  stats += "   Estado del sistema: " + String(cryptoStatus.systemHealthy ? "✅ SALUDABLE" : "❌ NO SALUDABLE") + "\n";
  stats += "   Tiempo de última operación: " + String(cryptoStatus.lastOperationTime) + "ms";
  
  return stats;
}

String encryptString(String inputText) {
  uint32_t startTime = millis();
  logCryptoMessage("INFO", "Iniciando encriptación de string", "encryptString");
  
  if (inputText.length() == 0) {
    logCryptoMessage("WARNING", "String de entrada vacío", "encryptString");
    return "";
  }
  
  if (!isCryptoSystemHealthy()) {
    cryptoStatus.encryptionErrors++;
    return "";
  }
  
  if (inputText.length() > MAX_INPUT_SIZE) {
    logCryptoMessage("ERROR", "String de entrada demasiado largo: " + String(inputText.length()), "encryptString");
    cryptoStatus.encryptionErrors++;
    return "";
  }
  
  try {
    uint8_t key_copy[AES_KEY_LENGTH];
    uint8_t iv_copy[AES_KEY_LENGTH];
    memcpy(key_copy, AES_KEY, AES_KEY_LENGTH);
    memcpy(iv_copy, AES_IV, AES_KEY_LENGTH);
    
    int bytesInputLength = inputText.length() + 1;
    uint8_t bytesInput[bytesInputLength];
    inputText.getBytes(bytesInput, bytesInputLength);
    
    int outputLength = aesLib.get_cipher_length(bytesInputLength);
    if (outputLength <= 0) {
      logCryptoMessage("ERROR", "Error al calcular longitud de salida encriptada", "encryptString");
      cryptoStatus.encryptionErrors++;
      return "";
    }
    
    uint8_t bytesEncrypted[outputLength];
    
    aesLib.set_paddingmode((paddingMode)0);
    
    bool encryptionSuccess = false;
    for (int attempt = 1; attempt <= ENCRYPTION_RETRIES && !encryptionSuccess; attempt++) {
      if (attempt > 1) {
        logCryptoMessage("WARNING", "Reintento " + String(attempt) + " de encriptación", "encryptString");
        memcpy(key_copy, AES_KEY, AES_KEY_LENGTH);
        memcpy(iv_copy, AES_IV, AES_KEY_LENGTH);
      }
      
      encryptionSuccess = aesLib.encrypt(bytesInput, bytesInputLength, bytesEncrypted, 
                                        key_copy, AES_KEY_LENGTH, iv_copy);
      
      if (!encryptionSuccess) {
        logCryptoMessage("WARNING", "Intento " + String(attempt) + " falló", "encryptString");
        delay(10);
      }
    }
    
    if (!encryptionSuccess) {
      logCryptoMessage("ERROR", "Encriptación falló después de " + String(ENCRYPTION_RETRIES) + " intentos", "encryptString");
      cryptoStatus.encryptionErrors++;
      return "";
    }
    
    char base64EncodedOutput[base64::encodeLength(outputLength)];
    base64::encode(bytesEncrypted, outputLength, base64EncodedOutput);
    
    cryptoStatus.encryptionCount++;
    cryptoStatus.lastOperationTime = millis() - startTime;
    
    logCryptoMessage("INFO", "Encriptación exitosa en " + String(cryptoStatus.lastOperationTime) + "ms", "encryptString");
    
    return String(base64EncodedOutput);
    
  } catch (...) {
    logCryptoMessage("ERROR", "Excepción durante la encriptación", "encryptString");
    cryptoStatus.encryptionErrors++;
    return "";
  }
}

String encryptChar(char cadena[], int longcadena) {
  uint32_t startTime = millis();
  logCryptoMessage("INFO", "Iniciando encriptación de array de caracteres", "encryptChar");
  
  if (!validateInputParameters(cadena, longcadena, "encryptChar")) {
    cryptoStatus.encryptionErrors++;
    return "";
  }
  
  if (!isCryptoSystemHealthy()) {
    cryptoStatus.encryptionErrors++;
    return "";
  }
  
  try {
    uint8_t key_copy[AES_KEY_LENGTH];
    uint8_t iv_copy[AES_KEY_LENGTH];
    memcpy(key_copy, AES_KEY, AES_KEY_LENGTH);
    memcpy(iv_copy, AES_IV, AES_KEY_LENGTH);
    
    uint8_t bytesInput[longcadena];
    memcpy(bytesInput, cadena, longcadena);
    
    int outputLength = aesLib.get_cipher_length(longcadena);
    if (outputLength <= 0) {
      logCryptoMessage("ERROR", "Error al calcular longitud de salida encriptada", "encryptChar");
      cryptoStatus.encryptionErrors++;
      return "";
    }
    
    uint8_t bytesEncrypted[outputLength];
    
    aesLib.set_paddingmode((paddingMode)0);
    
    bool encryptionSuccess = false;
    for (int attempt = 1; attempt <= ENCRYPTION_RETRIES && !encryptionSuccess; attempt++) {
      if (attempt > 1) {
        logCryptoMessage("WARNING", "Reintento " + String(attempt) + " de encriptación", "encryptChar");
        memcpy(key_copy, AES_KEY, AES_KEY_LENGTH);
        memcpy(iv_copy, AES_IV, AES_KEY_LENGTH);
      }
      
      encryptionSuccess = aesLib.encrypt(bytesInput, longcadena, bytesEncrypted, 
                                        key_copy, AES_KEY_LENGTH, iv_copy);
      
      if (!encryptionSuccess) {
        logCryptoMessage("WARNING", "Intento " + String(attempt) + " falló", "encryptChar");
        delay(10);
      }
    }
    
    if (!encryptionSuccess) {
      logCryptoMessage("ERROR", "Encriptación falló después de " + String(ENCRYPTION_RETRIES) + " intentos", "encryptChar");
      cryptoStatus.encryptionErrors++;
      return "";
    }
    
    char base64EncodedOutput[base64::encodeLength(outputLength)];
    base64::encode(bytesEncrypted, outputLength, base64EncodedOutput);
    
    cryptoStatus.encryptionCount++;
    cryptoStatus.lastOperationTime = millis() - startTime;
    
    logCryptoMessage("INFO", "Encriptación exitosa en " + String(cryptoStatus.lastOperationTime) + "ms", "encryptChar");
    
    return String(base64EncodedOutput);
    
  } catch (...) {
    logCryptoMessage("ERROR", "Excepción durante la encriptación", "encryptChar");
    cryptoStatus.encryptionErrors++;
    return "";
  }
}

String decrypt(String encryptedBase64Text) {
  uint32_t startTime = millis();
  logCryptoMessage("INFO", "Iniciando desencriptación", "decrypt");
  
  if (encryptedBase64Text.length() == 0) {
    logCryptoMessage("WARNING", "String encriptado vacío", "decrypt");
    return "";
  }
  
  if (!isCryptoSystemHealthy()) {
    cryptoStatus.decryptionErrors++;
    return "";
  }
  
  try {
    uint8_t decrypt_key_copy[AES_KEY_LENGTH];
    uint8_t iv_copy[AES_KEY_LENGTH];
    memcpy(decrypt_key_copy, AES_DECRYPT_KEY, AES_KEY_LENGTH);
    memcpy(iv_copy, AES_IV, AES_KEY_LENGTH);
    
    int originalBytesLength = base64::decodeLength(encryptedBase64Text.c_str());
    if (originalBytesLength <= 0) {
      logCryptoMessage("ERROR", "Error al decodificar Base64", "decrypt");
      cryptoStatus.decryptionErrors++;
      return "";
    }
    
    if (originalBytesLength > MAX_INPUT_SIZE) {
      logCryptoMessage("ERROR", "Longitud decodificada excede el máximo: " + String(originalBytesLength), "decrypt");
      cryptoStatus.decryptionErrors++;
      return "";
    }
    
    uint8_t encryptedBytes[originalBytesLength];
    uint8_t decryptedBytes[originalBytesLength];
    
    base64::decode(encryptedBase64Text.c_str(), encryptedBytes);
    
    aesLib.set_paddingmode((paddingMode)0);
    
    bool decryptionSuccess = false;
    for (int attempt = 1; attempt <= ENCRYPTION_RETRIES && !decryptionSuccess; attempt++) {
      if (attempt > 1) {
        logCryptoMessage("WARNING", "Reintento " + String(attempt) + " de desencriptación", "decrypt");
        memcpy(decrypt_key_copy, AES_DECRYPT_KEY, AES_KEY_LENGTH);
        memcpy(iv_copy, AES_IV, AES_KEY_LENGTH);
      }
      
      decryptionSuccess = aesLib.decrypt(encryptedBytes, originalBytesLength,
                                        decryptedBytes, decrypt_key_copy, AES_KEY_LENGTH, iv_copy);
      
      if (!decryptionSuccess) {
        logCryptoMessage("WARNING", "Intento " + String(attempt) + " falló", "decrypt");
        delay(10);
      }
    }
    
    if (!decryptionSuccess) {
      logCryptoMessage("ERROR", "Desencriptación falló después de " + String(ENCRYPTION_RETRIES) + " intentos", "decrypt");
      cryptoStatus.decryptionErrors++;
      return "";
    }
    
    String decryptedText = String((char*)decryptedBytes);
    
    cryptoStatus.decryptionCount++;
    cryptoStatus.lastOperationTime = millis() - startTime;
    
    logCryptoMessage("INFO", "Desencriptación exitosa en " + String(cryptoStatus.lastOperationTime) + "ms", "decrypt");
    
    return decryptedText;
    
  } catch (...) {
    logCryptoMessage("ERROR", "Excepción durante la desencriptación", "decrypt");
    cryptoStatus.decryptionErrors++;
    return "";
  }
}

void resetCryptoErrorCounters() {
  logCryptoMessage("INFO", "Reiniciando contadores de errores", "resetErrors");
  cryptoStatus.encryptionErrors = 0;
  cryptoStatus.decryptionErrors = 0;
  cryptoStatus.systemHealthy = true;
}

bool verifyCryptoSystem() {
  logCryptoMessage("INFO", "Verificando salud del sistema de encriptación", "verifySystem");
  
  if (AES_KEY == nullptr || AES_IV == nullptr || AES_DECRYPT_KEY == nullptr) {
    logCryptoMessage("ERROR", "Claves AES no configuradas correctamente");
    cryptoStatus.systemHealthy = false;
    return false;
  }
  
  if (!aesLib.get_cipher_length(16)) {
    logCryptoMessage("ERROR", "Sistema AES no responde correctamente");
    cryptoStatus.systemHealthy = false;
    return false;
  }
  
  if (cryptoStatus.encryptionErrors > 100 || cryptoStatus.decryptionErrors > 100) {
    logCryptoMessage("WARNING", "Demasiados errores acumulados - considerando sistema no saludable");
    cryptoStatus.systemHealthy = false;
    return false;
  }
  
  cryptoStatus.systemHealthy = true;
  logCryptoMessage("INFO", "Sistema de encriptación verificado como saludable");
  return true;
}

String getCryptoSystemInfo() {
  String info = "🔐 INFORMACIÓN DEL SISTEMA DE ENCRIPTACIÓN:\n";
  info += "   Estado general: " + String(cryptoStatus.systemHealthy ? "✅ SALUDABLE" : "❌ NO SALUDABLE") + "\n";
  info += "   Clave AES configurada: " + String(AES_KEY != nullptr ? "✅" : "❌") + "\n";
  info += "   IV AES configurado: " + String(AES_IV != nullptr ? "✅" : "❌") + "\n";
  info += "   Clave de desencriptación: " + String(AES_DECRYPT_KEY != nullptr ? "✅" : "❌") + "\n";
  info += "   Sistema AES respondiendo: " + String(aesLib.get_cipher_length(16) > 0 ? "✅" : "❌") + "\n";
  info += "   Tamaño máximo de entrada: " + String(MAX_INPUT_SIZE) + " bytes\n";
  info += "   Reintentos de operación: " + String(ENCRYPTION_RETRIES) + "\n";
  info += "   Tiempo de última operación: " + String(cryptoStatus.lastOperationTime) + "ms";
  
  return info;
}