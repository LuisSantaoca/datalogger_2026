/**
 * @file crono.cpp
 * @brief Implementación del cronómetro para medición de tiempos
 * @author Elathia
 * @date 2025
 */

#include "crono.h"

Cronometro::Cronometro() {
  reiniciar();
}

void Cronometro::iniciar() {
  tiempoInicio = millis();
  corriendo = true;
}

void Cronometro::detener() {
  if (corriendo) {
    tiempoFin = millis();
    corriendo = false;
  }
}

unsigned long Cronometro::obtenerDuracion() {
  if (corriendo) {
    return millis() - tiempoInicio;
  } else {
    return tiempoFin - tiempoInicio;
  }
}

void Cronometro::reiniciar() {
  tiempoInicio = 0;
  tiempoFin = 0;
  corriendo = false;
}

bool Cronometro::estaCorriendo() {
  return corriendo;
}