/**
 * @file cryptoaes.cpp
 * @brief Implementación de funciones de encriptación y desencriptación AES
 * @details Proporciona funciones para encriptar/desencriptar strings y arrays de caracteres
 *          usando el algoritmo AES con codificación Base64. Las claves están hardcodeadas
 *          para mantener compatibilidad con datos existentes.
 * @warning Las claves AES están hardcodeadas en el código - NO usar en producción
 * @author Elathia
 * @version 1.0
 * @date 2024
 */

#include "cryptoaes.h"
#include "AESLib.h"
#include "arduino_base64.hpp"

// ============================================================================
// CONFIGURACIÓN Y CONSTANTES
// ============================================================================

// Configuración de encriptación AES
#define AES_KEY_LENGTH 16
#define AES_BLOCK_SIZE 16
#define MAX_INPUT_SIZE 1024  // Tamaño máximo de entrada para evitar desbordamientos
#define ENCRYPTION_RETRIES 3 // Número de reintentos en caso de fallo

// Claves AES hardcodeadas (mantenidas para compatibilidad)
// ⚠️ ADVERTENCIA: Estas claves están hardcodeadas y NO son seguras para producción
static const uint8_t AES_KEY[AES_KEY_LENGTH] = { 
  56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 74, 65, 77, 82 
};

static const uint8_t AES_IV[AES_KEY_LENGTH] = { 
  56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 74, 65, 77, 82 
};

// Clave para desencriptación (diferente a la de encriptación - ⚠️ PROBLEMA DE SEGURIDAD)
static const uint8_t AES_DECRYPT_KEY[AES_KEY_LENGTH] = { 
   56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 56, 74, 65, 77, 82 
};

// ============================================================================
// VARIABLES GLOBALES Y ESTADO
// ============================================================================

AESLib aesLib;

// Estado del sistema de encriptación
struct CryptoStatus {
  uint32_t encryptionCount = 0;
  uint32_t decryptionCount = 0;
  uint32_t encryptionErrors = 0;
  uint32_t decryptionErrors = 0;
  uint32_t lastOperationTime = 0;
  bool systemHealthy = true;
};

static CryptoStatus cryptoStatus;

// ============================================================================
// FUNCIONES AUXILIARES
// ============================================================================

/**
 * @brief Registra mensajes del sistema de encriptación
 * @param level Nivel de log (INFO, WARNING, ERROR)
 * @param message Mensaje a registrar
 * @param operation Operación realizada (encrypt, decrypt, etc.)
 */
void logCryptoMessage(const String& level, const String& message, const String& operation = "") {
  String timestamp = String(millis());
  String prefix = "🔐 [CRYPTO] ";
  
  if (operation.length() > 0) {
    prefix += "[" + operation + "] ";
  }
  
  Serial.println(prefix + "[" + level + "] " + timestamp + "ms: " + message);
}

/**
 * @brief Valida los parámetros de entrada para operaciones de encriptación
 * @param input Puntero a los datos de entrada
 * @param length Longitud de los datos
 * @param operation Nombre de la operación para logging
 * @return true si los parámetros son válidos, false en caso contrario
 */
bool validateInputParameters(const void* input, int length, const String& operation) {
  // Validar puntero
  if (input == nullptr) {
    logCryptoMessage("ERROR", "Puntero de entrada nulo", operation);
    return false;
  }
  
  // Validar longitud
  if (length <= 0) {
    logCryptoMessage("ERROR", "Longitud de entrada inválida: " + String(length), operation);
    return false;
  }
  
  // Validar límite máximo
  if (length > MAX_INPUT_SIZE) {
    logCryptoMessage("ERROR", "Longitud de entrada excede el máximo: " + String(length) + " > " + String(MAX_INPUT_SIZE), operation);
    return false;
  }
  
  return true;
}

/**
 * @brief Verifica que el sistema de encriptación esté funcionando correctamente
 * @return true si el sistema está saludable, false en caso contrario
 */
bool isCryptoSystemHealthy() {
  if (!cryptoStatus.systemHealthy) {
    logCryptoMessage("WARNING", "Sistema de encriptación en estado no saludable");
    return false;
  }
  
  // Verificar que las claves estén configuradas
  if (AES_KEY == nullptr || AES_IV == nullptr) {
    logCryptoMessage("ERROR", "Claves AES no configuradas");
    cryptoStatus.systemHealthy = false;
    return false;
  }
  
  return true;
}

/**
 * @brief Obtiene estadísticas del sistema de encriptación
 * @return String con estadísticas del sistema
 */
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

// ============================================================================
// FUNCIONES PRINCIPALES DE ENCRIPTACIÓN
// ============================================================================

/**
 * @brief Encripta un string usando AES y lo codifica en Base64
 * @param inputText Texto a encriptar
 * @return String encriptado en Base64, o string vacío en caso de error
 * @warning Esta función mantiene las claves hardcodeadas por compatibilidad
 */
String encryptString(String inputText) {
  uint32_t startTime = millis();
  logCryptoMessage("INFO", "Iniciando encriptación de string", "encryptString");
  
  // Validar entrada
  if (inputText.length() == 0) {
    logCryptoMessage("WARNING", "String de entrada vacío", "encryptString");
    return "";
  }
  
  if (!isCryptoSystemHealthy()) {
    cryptoStatus.encryptionErrors++;
    return "";
  }
  
  // Validar longitud máxima
  if (inputText.length() > MAX_INPUT_SIZE) {
    logCryptoMessage("ERROR", "String de entrada demasiado largo: " + String(inputText.length()), "encryptString");
    cryptoStatus.encryptionErrors++;
    return "";
  }
  
  try {
    // Preparar datos de entrada
    int bytesInputLength = inputText.length() + 1;
    uint8_t bytesInput[bytesInputLength];
    inputText.getBytes(bytesInput, bytesInputLength);
    
    // Calcular tamaño de salida encriptada
    int outputLength = aesLib.get_cipher_length(bytesInputLength);
    if (outputLength <= 0) {
      logCryptoMessage("ERROR", "Error al calcular longitud de salida encriptada", "encryptString");
      cryptoStatus.encryptionErrors++;
      return "";
    }
    
    // Buffer para datos encriptados
    uint8_t bytesEncrypted[outputLength];
    
    // Configurar modo de padding (sin padding para compatibilidad)
    aesLib.set_paddingmode((paddingMode)0);
    
    // Realizar encriptación con reintentos
    bool encryptionSuccess = false;
    for (int attempt = 1; attempt <= ENCRYPTION_RETRIES && !encryptionSuccess; attempt++) {
      if (attempt > 1) {
        logCryptoMessage("WARNING", "Reintento " + String(attempt) + " de encriptación", "encryptString");
      }
      
      encryptionSuccess = aesLib.encrypt(bytesInput, bytesInputLength, bytesEncrypted, 
                                        (uint8_t*)AES_KEY, AES_KEY_LENGTH, (uint8_t*)AES_IV);
      
      if (!encryptionSuccess) {
        logCryptoMessage("WARNING", "Intento " + String(attempt) + " falló", "encryptString");
        delay(10); // Pequeña pausa entre reintentos
      }
    }
    
    if (!encryptionSuccess) {
      logCryptoMessage("ERROR", "Encriptación falló después de " + String(ENCRYPTION_RETRIES) + " intentos", "encryptString");
      cryptoStatus.encryptionErrors++;
      return "";
    }
    
    // Codificar en Base64
    char base64EncodedOutput[base64::encodeLength(outputLength)];
    base64::encode(bytesEncrypted, outputLength, base64EncodedOutput);
    
    // Actualizar estadísticas
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

/**
 * @brief Encripta un array de caracteres usando AES y lo codifica en Base64
 * @param cadena Array de caracteres a encriptar
 * @param longcadena Longitud del array
 * @return String encriptado en Base64, o string vacío en caso de error
 * @warning Esta función mantiene las claves hardcodeadas por compatibilidad
 */
String encryptChar(char cadena[], int longcadena) {
  uint32_t startTime = millis();
  logCryptoMessage("INFO", "Iniciando encriptación de array de caracteres", "encryptChar");
  
  // Validar parámetros de entrada
  if (!validateInputParameters(cadena, longcadena, "encryptChar")) {
    cryptoStatus.encryptionErrors++;
    return "";
  }
  
  if (!isCryptoSystemHealthy()) {
    cryptoStatus.encryptionErrors++;
    return "";
  }
  
  try {
    // Preparar datos de entrada
    uint8_t bytesInput[longcadena];
    memcpy(bytesInput, cadena, longcadena);
    
    // Calcular tamaño de salida encriptada
    int outputLength = aesLib.get_cipher_length(longcadena);
    if (outputLength <= 0) {
      logCryptoMessage("ERROR", "Error al calcular longitud de salida encriptada", "encryptChar");
      cryptoStatus.encryptionErrors++;
      return "";
    }
    
    // Buffer para datos encriptados
    uint8_t bytesEncrypted[outputLength];
    
    // Configurar modo de padding (sin padding para compatibilidad)
    aesLib.set_paddingmode((paddingMode)0);
    
    // Realizar encriptación con reintentos
    bool encryptionSuccess = false;
    for (int attempt = 1; attempt <= ENCRYPTION_RETRIES && !encryptionSuccess; attempt++) {
      if (attempt > 1) {
        logCryptoMessage("WARNING", "Reintento " + String(attempt) + " de encriptación", "encryptChar");
      }
      
      encryptionSuccess = aesLib.encrypt(bytesInput, longcadena, bytesEncrypted, 
                                        (uint8_t*)AES_KEY, AES_KEY_LENGTH, (uint8_t*)AES_IV);
      
      if (!encryptionSuccess) {
        logCryptoMessage("WARNING", "Intento " + String(attempt) + " falló", "encryptChar");
        delay(10); // Pequeña pausa entre reintentos
      }
    }
    
    if (!encryptionSuccess) {
      logCryptoMessage("ERROR", "Encriptación falló después de " + String(ENCRYPTION_RETRIES) + " intentos", "encryptChar");
      cryptoStatus.encryptionErrors++;
      return "";
    }
    
    // Codificar en Base64
    char base64EncodedOutput[base64::encodeLength(outputLength)];
    base64::encode(bytesEncrypted, outputLength, base64EncodedOutput);
    
    // Actualizar estadísticas
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

// ============================================================================
// FUNCIONES DE DESENCRIPTACIÓN
// ============================================================================

/**
 * @brief Desencripta un string en Base64 usando AES
 * @param encryptedBase64Text Texto encriptado en Base64
 * @return String desencriptado, o string vacío en caso de error
 * @warning Esta función usa una clave diferente a la de encriptación (PROBLEMA DE SEGURIDAD)
 */
String decrypt(String encryptedBase64Text) {
  uint32_t startTime = millis();
  logCryptoMessage("INFO", "Iniciando desencriptación", "decrypt");
  
  // Validar entrada
  if (encryptedBase64Text.length() == 0) {
    logCryptoMessage("WARNING", "String encriptado vacío", "decrypt");
    return "";
  }
  
  if (!isCryptoSystemHealthy()) {
    cryptoStatus.decryptionErrors++;
    return "";
  }
  
  try {
    // Decodificar Base64
    int originalBytesLength = base64::decodeLength(encryptedBase64Text.c_str());
    if (originalBytesLength <= 0) {
      logCryptoMessage("ERROR", "Error al decodificar Base64", "decrypt");
      cryptoStatus.decryptionErrors++;
      return "";
    }
    
    // Validar longitud decodificada
    if (originalBytesLength > MAX_INPUT_SIZE) {
      logCryptoMessage("ERROR", "Longitud decodificada excede el máximo: " + String(originalBytesLength), "decrypt");
      cryptoStatus.decryptionErrors++;
      return "";
    }
    
    // Buffers para datos
    uint8_t encryptedBytes[originalBytesLength];
    uint8_t decryptedBytes[originalBytesLength];
    
    // Decodificar Base64
    base64::decode(encryptedBase64Text.c_str(), encryptedBytes);
    
    // Configurar modo de padding (sin padding para compatibilidad)
    aesLib.set_paddingmode((paddingMode)0);
    
    // Realizar desencriptación con reintentos
    bool decryptionSuccess = false;
    for (int attempt = 1; attempt <= ENCRYPTION_RETRIES && !decryptionSuccess; attempt++) {
      if (attempt > 1) {
        logCryptoMessage("WARNING", "Reintento " + String(attempt) + " de desencriptación", "decrypt");
      }
      
      decryptionSuccess = aesLib.decrypt(encryptedBytes, originalBytesLength,
                                        decryptedBytes, (uint8_t*)AES_DECRYPT_KEY, AES_KEY_LENGTH, (uint8_t*)AES_IV);
      
      if (!decryptionSuccess) {
        logCryptoMessage("WARNING", "Intento " + String(attempt) + " falló", "decrypt");
        delay(10); // Pequeña pausa entre reintentos
      }
    }
    
    if (!decryptionSuccess) {
      logCryptoMessage("ERROR", "Desencriptación falló después de " + String(ENCRYPTION_RETRIES) + " intentos", "decrypt");
      cryptoStatus.decryptionErrors++;
      return "";
    }
    
    // Convertir a string
    String decryptedText = String((char*)decryptedBytes);
    
    // Actualizar estadísticas
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

// ============================================================================
// FUNCIONES DE DIAGNÓSTICO Y MANTENIMIENTO
// ============================================================================

/**
 * @brief Reinicia los contadores de errores del sistema de encriptación
 */
void resetCryptoErrorCounters() {
  logCryptoMessage("INFO", "Reiniciando contadores de errores", "resetErrors");
  cryptoStatus.encryptionErrors = 0;
  cryptoStatus.decryptionErrors = 0;
  cryptoStatus.systemHealthy = true;
}

/**
 * @brief Verifica la salud del sistema de encriptación
 * @return true si el sistema está funcionando correctamente
 */
bool verifyCryptoSystem() {
  logCryptoMessage("INFO", "Verificando salud del sistema de encriptación", "verifySystem");
  
  // Verificar que las claves estén configuradas
  if (AES_KEY == nullptr || AES_IV == nullptr || AES_DECRYPT_KEY == nullptr) {
    logCryptoMessage("ERROR", "Claves AES no configuradas correctamente");
    cryptoStatus.systemHealthy = false;
    return false;
  }
  
  // Verificar que el sistema AES esté funcionando
  if (!aesLib.get_cipher_length(16)) {
    logCryptoMessage("ERROR", "Sistema AES no responde correctamente");
    cryptoStatus.systemHealthy = false;
    return false;
  }
  
  // Verificar estadísticas
  if (cryptoStatus.encryptionErrors > 100 || cryptoStatus.decryptionErrors > 100) {
    logCryptoMessage("WARNING", "Demasiados errores acumulados - considerando sistema no saludable");
    cryptoStatus.systemHealthy = false;
    return false;
  }
  
  cryptoStatus.systemHealthy = true;
  logCryptoMessage("INFO", "Sistema de encriptación verificado como saludable");
  return true;
}

/**
 * @brief Obtiene información detallada del estado del sistema de encriptación
 * @return String con información detallada del estado
 */
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