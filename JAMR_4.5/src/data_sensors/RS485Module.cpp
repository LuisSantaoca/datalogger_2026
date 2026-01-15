#include "RS485Module.h"
#include "config_data_sensors.h"

bool RS485Module::begin() {
  enablePow();
  serialPort_ = &Serial2;
  serialPort_->begin(RS485_BAUD, SERIAL_8N1, RS485_RX, RS485_TX);

  node_.begin(MODBUS_SLAVE_ID, *serialPort_);

  return true;
}

bool RS485Module::readSensor() {
  uint8_t result = node_.readHoldingRegisters(
      MODBUS_START_ADDRESS,
      MODBUS_REGISTER_COUNT
  );

  if (result != node_.ku8MBSuccess) {
    return false;
  }

  for (uint8_t i = 0; i < MODBUS_REGISTER_COUNT; i++) {
    registers_[i] = node_.getResponseBuffer(i);
  }

  return true;
}

uint16_t RS485Module::getRegister(uint8_t index) const {
  if (index >= MODBUS_REGISTER_COUNT) {
    return 0;
  }
  return registers_[index];
}

String RS485Module::getRegisterString(uint8_t index) const {
  if (index >= MODBUS_REGISTER_COUNT) {
    return "0";
  }
  return String(registers_[index]);
}

bool RS485Module::writeRegister(uint16_t address, uint16_t value) {
  uint8_t result = node_.writeSingleRegister(address, value);
  return (result == node_.ku8MBSuccess);
}

void RS485Module::enablePow() {
  pinMode(ENPOWER, OUTPUT);
  digitalWrite(ENPOWER, HIGH);
}

void RS485Module::disablePow() {
  digitalWrite(ENPOWER, LOW);
}
