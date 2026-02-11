#include "I2CSensorModule.h"
#include "Wire.h"

bool I2CSensorModule::begin() {
  temperature_ = 0.0;
  humidity_ = 0.0;
  
  Wire.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
  
  return aht20_.begin();
}

bool I2CSensorModule::readSensor() {
  digitalWrite(GPIO_NUM_3, HIGH);
  delay(1000);
  temperature_ = aht20_.getTemperature();
  humidity_ = aht20_.getHumidity();
  

  if (isnan(temperature_) || isnan(humidity_)) {
    return false;
  }
  
  return true;
}

float I2CSensorModule::getTemperature() const {
  return temperature_;
}

float I2CSensorModule::getHumidity() const {
  return humidity_;
}

String I2CSensorModule::getTemperatureString() const {
  int value = (int)(temperature_ * 100);
  return String(value);
}

String I2CSensorModule::getHumidityString() const {
  int value = (int)(humidity_ * 100);
  return String(value);
}

bool I2CSensorModule::isConnected() {

  float testTemp = aht20_.getTemperature();
  return !isnan(testTemp);
}
