#include "sensores.h"
#include "timeData.h"
#include "sleepSensor.h"
#include "simmodem.h"
#include <Adafruit_AHT10.h>
#include "data_storage.h"

// Constants definitions
#define ADC_RESOLUTION      4095      // ADC resolution for analog readings
#define VOLTAGE_REF         3.3       // Reference voltage for ADC calculations
#define BATTERY_CALIBRATION 0.2       // Calibration offset for battery voltage
#define TEMP_DIVISOR        100.0     // Divisor for scaling temperature readings
#define HUMIDITY_DIVISOR    100.0     // Divisor for scaling humidity readings
#define EC_DIVISOR          1000.0    // Divisor for scaling electrical conductivity readings
#define MAX_RETRIES         10        // Maximum retries for sensor initialization

// Global variables
EspSoftwareSerial::UART puertosw;     // Software serial object for RS485 communication
Adafruit_AHT10 aht;

// Function to initialize sensors
void setupSensores() {
  // Initialize RS485 communication
  puertosw.begin(RS485_BAUDRATE, RS485_CFG, RS485_RX_PIN, RS485_TX_PIN);
  // Initialize AHT sensor.
  if (!aht.begin()) {
    logMessage("Failed to find AHT10 sensor!");
  }
}

// Function to read data from sensors
void readSensorData(sensordata_type* data) {
  uint8_t sensor_1[] = { 0x12, 0x04, 0x00, 0x00, 0x00, 0x03, 0xB2, 0xA8 }; // Commands for RS485 soil sensor
  uint8_t resp_sensor_1[11];            // Response buffer for RS485 soil sensor
  uint8_t index = 0;                    // Index for RS485 response buffer
  sensors_event_t humidity, temp;       // Variables to hold humidity and temperature events

  digitalWrite(RS485_PWREN_PIN, HIGH);  // Enable RS485 power
  // Send command to RS485 soil sensor
  for (uint8_t i = 0; i < (sizeof(sensor_1) / sizeof(sensor_1[0])); i++) {
    puertosw.write(sensor_1[i]);
  }
  delay(3000);       // Wait for the sensor to respond
  // Read response from RS485 soil sensor
  while (puertosw.available()) {
    resp_sensor_1[index] = puertosw.read();
    index++;
  }
  //digitalWrite(RS485_PWREN_PIN, LOW);  // Disable RS485 power
  // Parse soil sensor data
  data->temperatura_suelo = word(resp_sensor_1[3], resp_sensor_1[4]) / TEMP_DIVISOR;
  data->humedad_suelo = word(resp_sensor_1[5], resp_sensor_1[6]) / HUMIDITY_DIVISOR;
  data->conductividad_suelo = word(resp_sensor_1[7], resp_sensor_1[8]) / EC_DIVISOR;

  //aht.getEvent(&data->humedad_relativa, &data->temperatura_ambiente);  // Read temperature and humidity
  aht.getEvent(&humidity, &temp);  // Read temperature and humidity
  //logMessage("Humidity: " + String(humidity.relative_humidity) + "%, Temperature: " + String(temp.temperature) + "°C");
  data->humedad_relativa = humidity.relative_humidity; // Store relative humidity
  data->temperatura_ambiente = temp.temperature; // Store ambient temperature

  // Read battery voltage
  data->battery_voltage = (((analogRead(ADC_VOLT_BAT_PIN) * VOLTAGE_REF) / ADC_RESOLUTION) * 2) + BATTERY_CALIBRATION;

  // Set the epoch time from the internal RTC
  data->epoc = getEpochtimertcInt(); // Get the current
}
/*
// Function to print sensor data for debugging
void debugData(sensordata_type* data) {
  Serial.println("-----Debug----");
  Serial.println("uuid: " + uuid);
  Serial.println("epoc: " + epoc_string);
  Serial.println("temperatura_ambiente: " + String(data->temperatura_ambiente));
  Serial.println("humedad_relativa : " + String(data->humedad_relativa));
//  Serial.println("luz: " + String(data->luz));
//  Serial.println("radiación: " + String(data->radiacion));
//  Serial.println("presion_barometrica: " + String(data->presion_barometrica));
//  Serial.println("co2: " + String(data->co2));
  Serial.println("humedad_suelo: " + String(data->humedad_suelo));
  Serial.println("temperatura_suelo: " + String(data->temperatura_suelo));
  Serial.println("conductividad_suelo: " + String(data->conductividad_suelo));
//  Serial.println("ph_suelo: " + String(data->ph_suelo));
//  Serial.println("nitrogeno: " + String(data->nitrogeno));
//  Serial.println("fosforo: " + String(data->fosforo));
//  Serial.println("potasio: " + String(data->potasio));
//  Serial.println("viento: " + String(data->viento));
  Serial.println("latitud: " + latitud);
  Serial.println("logitud: " + longitud);
  Serial.println("altitud: " + altitud);
  Serial.println("battery voltage: " + String(data->battery_voltage));
  Serial.println("rssi: " + rssi);
  Serial.println("---------");
}*/
