#include "ADCSensorModule.h"

bool ADCSensorModule::begin() {
  rawValue_ = 0;
  voltage_ = 0.0;
  value_ = 0.0;
  
  // Configurar el pin ADC
  pinMode(ADC_PIN, INPUT);
  
  // Configurar resolución ADC (12 bits para ESP32)
  analogReadResolution(12);
  
  // Configurar atenuación (0-3.3V)
  analogSetAttenuation(ADC_11db);
  
  return true;
}

bool ADCSensorModule::readSensor() {
  // Leer múltiples muestras y promediar
  uint32_t sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(ADC_PIN);
    delay(10); // Pequeña pausa entre lecturas
  }
  rawValue_ = sum / ADC_SAMPLES;
  
  // Convertir a voltaje (0-3.3V para ESP32)
  voltage_ = (rawValue_ * ADC_VREF) / ADC_RESOLUTION;
  
  // Aplicar fórmula: (voltaje + ajuste) * multiplicador
  value_ = ((voltage_ * 2) + ADC_ADJUSTMENT) * ADC_MULTIPLIER;
  
  return true;
}

uint16_t ADCSensorModule::getRawValue() const {
  return rawValue_;
}

float ADCSensorModule::getVoltage() const {
  return voltage_;
}

float ADCSensorModule::getValue() const {
  return value_;
}

String ADCSensorModule::getValueString() const {
  int intValue = (int)value_;
  return String(intValue);
}
