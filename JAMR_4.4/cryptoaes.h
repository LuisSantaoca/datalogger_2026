/**
 * @file cryptoaes.h
 * @brief Cabecera para funciones de encriptación y desencriptación AES
 * @details Define la interfaz para el sistema de encriptación AES con codificación Base64
 * @warning Las claves AES están hardcodeadas en el código - NO usar en producción
 * @author Elathia
 * @version 1.0
 * @date 2024
 */

#ifndef cryptoaes_H
#define cryptoaes_H

#include <stdint.h>
#include "Arduino.h"

// ============================================================================
// FUNCIONES PRINCIPALES DE ENCRIPTACIÓN
// ============================================================================

/**
 * @brief Encripta un string usando AES y lo codifica en Base64
 * @param inputText Texto a encriptar
 * @return String encriptado en Base64, o string vacío en caso de error
 * @warning Esta función mantiene las claves hardcodeadas por compatibilidad
 */
String encryptString(String inputText);

/**
 * @brief Encripta un array de caracteres usando AES y lo codifica en Base64
 * @param cadena Array de caracteres a encriptar
 * @param longcadena Longitud del array
 * @return String encriptado en Base64, o string vacío en caso de error
 * @warning Esta función mantiene las claves hardcodeadas por compatibilidad
 */
String encryptChar(char cadena[], int longcadena);

// ============================================================================
// FUNCIONES DE DESENCRIPTACIÓN
// ============================================================================

/**
 * @brief Desencripta un string en Base64 usando AES
 * @param encryptedBase64Text Texto encriptado en Base64
 * @return String desencriptado, o string vacío en caso de error
 * @warning Esta función usa una clave diferente a la de encriptación (PROBLEMA DE SEGURIDAD)
 */
String decrypt(String encryptedBase64Text);

// ============================================================================
// FUNCIONES DE DIAGNÓSTICO Y MANTENIMIENTO
// ============================================================================

/**
 * @brief Obtiene estadísticas del sistema de encriptación
 * @return String con estadísticas del sistema
 */
String getCryptoStats();

/**
 * @brief Verifica la salud del sistema de encriptación
 * @return true si el sistema está funcionando correctamente
 */
bool verifyCryptoSystem();

/**
 * @brief Obtiene información detallada del estado del sistema de encriptación
 * @return String con información detallada del estado
 */
String getCryptoSystemInfo();

/**
 * @brief Reinicia los contadores de errores del sistema de encriptación
 */
void resetCryptoErrorCounters();

#endif