#ifndef FORMATMODULE_H
#define FORMATMODULE_H

#include <Arduino.h>
#include "config_data_format.h"

/**
 * @file FORMATModule.h
 * @brief Construcción de trama y codificación Base64 sin librerías.
 */

/**
 * @class FormatModule
 * @brief Módulo para formar la trama:
 * "$,<iccid>,<epoch>,<lat>,<lng>,<alt>,<var1>,<var2>,<var3>,<var4>,<var5>,<var6>,<var7>,#"
 */
class FormatModule {
 public:
  /**
   * @brief Crea una instancia del módulo.
   */
  FormatModule();

  /**
   * @brief Reinicia todos los campos a cero.
   */
  void reset();

  /**
   * @brief Asigna ICCID (se rellena con ceros a la izquierda).
   * @param iccid Cadena con dígitos (hasta 20 caracteres).
   */
  void setIccid(const char* iccid);

  /**
   * @brief Asigna epoch (se rellena con ceros a la izquierda).
   * @param epoch Epoch como cadena de texto.
   */
  void setEpoch(const char* epoch);

  /**
   * @brief Asigna latitud (formato fijo 000.000000).
   * @param lat Latitud como cadena de texto.
   */
  void setLat(const char* lat);

  /**
   * @brief Asigna longitud (formato fijo 000.000000).
   * @param lng Longitud como cadena de texto.
   */
  void setLng(const char* lng);

  /**
   * @brief Asigna altitud (se rellena con ceros a la izquierda).
   * @param alt Altitud como cadena de texto.
   */
  void setAlt(const char* alt);

  /**
   * @brief Asigna una variable var1..var7.
   * @param index Índice 0..6.
   * @param value Valor como cadena de texto.
   * @return true si el índice es válido.
   */
  bool setVar(uint8_t index, const char* value);

  /**
   * @brief Asigna la variable var1.
   * @param value Valor como cadena de texto.
   */
  void setVar1(const char* value);

  /**
   * @brief Asigna la variable var2.
   * @param value Valor como cadena de texto.
   */
  void setVar2(const char* value);

  /**
   * @brief Asigna la variable var3.
   * @param value Valor como cadena de texto.
   */
  void setVar3(const char* value);

  /**
   * @brief Asigna la variable var4.
   * @param value Valor como cadena de texto.
   */
  void setVar4(const char* value);

  /**
   * @brief Asigna la variable var5.
   * @param value Valor como cadena de texto.
   */
  void setVar5(const char* value);

  /**
   * @brief Asigna la variable var6.
   * @param value Valor como cadena de texto.
   */
  void setVar6(const char* value);

  /**
   * @brief Asigna la variable var7.
   * @param value Valor como cadena de texto.
   */
  void setVar7(const char* value);

  /**
   * @brief Construye la trama en un buffer provisto por el usuario.
   * @param outBuffer Buffer destino.
   * @param outSize Tamaño del buffer destino.
   * @return true si cupo en el buffer.
   */
  bool buildFrame(char* outBuffer, size_t outSize) const;

  /**
   * @brief Codifica la trama a Base64 en un buffer provisto por el usuario.
   * @param outBuffer Buffer destino para Base64.
   * @param outSize Tamaño del buffer destino.
   * @return true si cupo en el buffer.
   */
  bool buildFrameBase64(char* outBuffer, size_t outSize) const;

  /**
   * @brief Codifica un bloque de bytes a Base64 (sin librerías).
   * @param data Datos de entrada.
   * @param dataLen Longitud de datos.
   * @param outBuffer Buffer destino.
   * @param outSize Tamaño del buffer destino.
   * @return Cantidad de caracteres escritos (sin '\0'). 0 si no cupo.
   */
  static size_t encodeBase64(const uint8_t* data,
                            size_t dataLen,
                            char* outBuffer,
                            size_t outSize);

 private:
  /**
   * @brief ICCID relleno a 20 caracteres (más '\0').
   */
  char iccid_[ICCID_LEN + 1];

  /**
   * @brief Epoch relleno a 10 caracteres (más '\0').
   */
  char epoch_[EPOCH_LEN + 1];

  /**
   * @brief Latitud en formato fijo (más '\0').
   */
  char lat_[COORD_LEN + 1];

  /**
   * @brief Longitud en formato fijo (más '\0').
   */
  char lng_[COORD_LEN + 1];

  /**
   * @brief Altitud rellena a 4 caracteres (más '\0').
   */
  char alt_[ALT_LEN + 1];

  /**
   * @brief Variables var1..var7 rellenas a 4 caracteres (más '\0').
   */
  char vars_[VAR_COUNT][VAR_LEN + 1];

  static void fillZeros(char* dst, uint8_t width);
  static void copyRightAligned(char* dst, uint8_t width, const char* src);
  static void copyCoordAligned(char* dst, uint8_t width, const char* src);
};

#endif
