#ifndef TYPE_DEF_H
#define TYPE_DEF_H

#include <Arduino.h>

// Struct for Sensor Data
typedef struct {
    unsigned long epoc;             // Epoch time (Unix timestamp)
    float temperatura_ambiente;     // Ambient temperature
    float humedad_relativa;         // Relative humidity
    float humedad_suelo;            // Soil humidity
    float temperatura_suelo;        // Soil temperature
    float conductividad_suelo;      // Soil conductivity
    float battery_voltage;          // Battery voltage
} sensordata_type;

// Struct for GPS Data
typedef struct {
    float lat;                      // Latitude in degrees
    float lon;                      // Longitude in degrees
    float speed;                    // Speed in m/s
    float alt;                      // Altitude in meters
    int   vsat;                     // Number of visible satellites
    int   usat;                     // Number of used satellites
    float accuracy;                 // GPS accuracy
    int   year;
    int   month;
    int   day;
    int   hour;
    int   min;
    int   sec;
    bool  level;                    // GPS level (true if valid, false if not)
    unsigned long epoc;             // Epoch time (Unix timestamp)
    uint8_t tries;                  // Number of retries to get GPS data
} gpsdata_type;

// Struct for Station data (SIM & Chip id's, Time, etc)
typedef struct {
        uint64_t chip_id;           // Unique chip ID
        char     sim_ccid[21];      // SIM card ICCID
        uint8_t  rssi;              // Signal strength (RSSI)
   unsigned long epoc;              // Epoch time (Unix timestamp)
} stationdata_type;

// Struct that contains all other structs
typedef struct {
    sensordata_type sensor;         // Sensor data
    gpsdata_type gps;               // GPS data
    stationdata_type station;       // Station data
} agrodata_type;

#endif // TYPE_DEF_H