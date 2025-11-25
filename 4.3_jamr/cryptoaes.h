/**
 * @file cryptoaes.h
 * @brief Sistema de encriptación AES con codificación Base64
 * @author Elathia
 * @date 2025
 * @version 2.0
 * 
 * @warning Las claves AES están hardcodeadas - NO usar en producción sin cambiarlas
 * @details Proporciona encriptación/desencriptación AES para transmisión segura de datos.
 */

#ifndef cryptoaes_H
#define cryptoaes_H

#include <stdint.h>
#include "Arduino.h"

/**
 * @brief Encripta un string usando AES y lo codifica en Base64
 * @param inputText Texto a encriptar
 * @return String encriptado en Base64, vacío en caso de error
 * @warning Usa claves hardcodeadas
 */
String encryptString(String inputText);

/**
 * @brief Encripta un array de caracteres usando AES
 * @param cadena Array de caracteres a encriptar
 * @param longcadena Longitud del array
 * @return String encriptado en Base64, vacío en caso de error
 */
String encryptChar(char cadena[], int longcadena);

/**
 * @brief Desencripta un string en Base64 usando AES
 * @param encryptedBase64Text Texto encriptado en Base64
 * @return String desencriptado, vacío en caso de error
 */
String decrypt(String encryptedBase64Text);

#endif