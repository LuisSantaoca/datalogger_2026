# tarjeta_sensores

## Project Overview

**sensorModelo06** is an Arduino-based environmental monitoring and data collection system designed for remote agricultural stations.  
It integrates multiple sensors, GPS, and GSM/LTE communication to collect, process, and securely transmit data to a central server.  
The system is optimized for low power consumption, using deep sleep modes and persistent storage to maximize battery life.

---

## Main Features

- **Sensor Data Acquisition:**  
  Reads soil and ambient sensors (temperature, humidity, conductivity, battery voltage) and stores them in a CSV file.

- **GPS Integration:**  
  Acquires GPS location, time, and satellite data, synchronizes RTC, and stores GPS info for each data record.

- **GSM/LTE Communication:**  
  Uses a SIM7080 modem to connect to the cellular network, send sensor and station data via TCP, and receive acknowledgments from the server.

- **Data Storage:**  
  Utilizes LittleFS for local CSV storage and Preferences for persistent flash storage of last GPS fix and station state.

- **Power Management:**  
  Employs deep sleep and wakeup logic to minimize energy usage, with configurable sleep intervals and wakeup reasons.

- **Calibration Routine:**  
  On power-on, runs a one-hour calibration routine for soil sensors, sending calibration data every 5 minutes.

- **Robust Error Handling:**  
  Retries GPS acquisition, network registration, and data transmission; stores unsent data for later retry.

---

## Data Formats

### Sensor Data (`/sensordata.csv`)
- **Format:**  
  `$,<chip_id_hex>,<sensor_epoch>,<temp_amb>,<hum_rel>,<hum_suelo>,<temp_suelo>,<cond_suelo>,<batt_volt>,#`
- **Sent:**  
  Every configured interval (e.g., every hour), or after calibration.

### Station Data (`/stationdata.csv`)
- **Format:**  
  `&,<chip_id_hex>,<sim_ccid>,<rssi>,<gps_epoch>,<lat>,<lon>,<alt>,#`
- **Sent:**  
  On power-on and every configured interval (e.g., every 24 hours).

### Calibration Data (`calibration_data.csv`)
- **Format:**  
  `%,<chip_id_hex>,<station_epoch>,<rssi>,<sim_ccid>,<hum_suelo>,<temp_suelo>,<cond_suelo>,<batt_volt>,#`
- **Sent:**  
  Every 5 minutes during the first hour after power-on.

---

## Main Code Structure

- **agro.ino:**  
  Main program loop. Handles setup, sensor reading, GPS acquisition, data transmission, calibration, and sleep management.

- **config.h:**  
  Centralized configuration for firmware version, network/APN settings, timing intervals, file paths, and protocol characters.

- **type_def.h:**  
  Data structures for sensor, GPS, and station data.

- **board_pins.h:**  
  Pin definitions for all hardware interfaces.

- **sleepSensor.h / sleepSensor.cpp:**  
  Functions for sleep mode, wakeup reason, and I2C bus recovery.

- **timeData.h / timeData.cpp:**  
  RTC and time management functions, including epoch calculation and formatting.

- **simmodem.h / simmodem.cpp:**  
  Modem control, AT command handling, TCP communication, and GPS setup/acquisition.

- **sensores.h / sensores.cpp:**  
  Sensor initialization and data reading.

- **data_storage.h / data_storage.cpp:**  
  Functions for saving/loading data to flash, appending to CSV, sending CSV data via TCP, and file management.

- **tcp_server.py:**  
  Example Python server for receiving data from the station, saving to CSV files, and sending acknowledgments.

---

## Usage

1. **Configure `config.h`:**  
   Set your APN, server IP, port, and timing intervals as needed.

2. **Deploy to ESP32:**  
   Flash the firmware to your ESP32-based station.

3. **Power On:**  
   On first boot, the station runs a calibration routine, then enters its normal data collection and transmission cycle.

4. **Server Setup:**  
   Run the provided `tcp_server.py` on your server/PC to receive and store incoming data.

5. **Data Transmission:**  
   The station wakes up at configured intervals, reads sensors, acquires GPS, connects to the network, and sends data.  
   Unsent data is retried and stored locally until successful transmission.

6. **Sleep Mode:**  
   After each cycle, the station enters deep sleep to conserve power.

---

## Customization

- **Add/Remove Sensors:**  
  Modify `sensores.cpp` and `type_def.h` to support additional sensors.

- **Change Data Intervals:**  
  Adjust timing constants in `config.h` for sleep, calibration, and data send intervals.

- **Change Data Format:**  
  Edit CSV formatting in `data_storage.cpp` and `agro.ino` as needed.

- **Server Integration:**  
  Adapt `tcp_server.py` for database integration or cloud upload.

---

## Troubleshooting

- **No Network:**  
  Check APN, SIM card, antenna, and signal strength.

- **No GPS Fix:**  
  Ensure clear sky view, check GPS antenna, and increase `GPS_FIX_RETRIES` if needed.

- **Data Not Sent:**  
  Check server IP/port, TCP connection, and acknowledgment handling.

- **Power Issues:**  
  Verify battery voltage readings and sleep mode configuration.

---

## License

This project is private and is copyright © IntelAgro. All rights reserved.

---