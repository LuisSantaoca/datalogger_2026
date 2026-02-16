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
  // FIX-V11: analogReadMilliVolts con calibracion eFuse (reemplaza calculo manual)
  uint32_t sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogReadMilliVolts(ADC_PIN);
    delay(10);
  }
  uint16_t avgMv = sum / ADC_SAMPLES;

  rawValue_ = avgMv;                          // mV promedio del pin
  voltage_ = avgMv / 1000.0f;                 // Voltaje en el pin (0-3.1V)
  value_ = (voltage_ * 2.0f) * ADC_MULTIPLIER; // ×2 divisor, ×100 para trama

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
