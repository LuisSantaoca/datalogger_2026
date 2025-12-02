#ifndef sensores_H
#define sensores_H

#include <stdint.h>
#include "Arduino.h"
#include "type_def.h"


#define RS485_TX 16
#define RS485_RX 15
#define ADC_VOLT_BAT 13
#define RS485_BAUD 9600
#define RS485_CFG SWSERIAL_8N1


#define SONDA_DFROBOT_EC 1
#define SONDA_DFROBOT_EC_PH 2
#define SONDA_SEED_EC 3
#define TYPE_SONDA 3

#define AHT10_SEN 1
#define AHT20_SEN 2
#define TYPE_AHT 2



void setupSensores(sensordata_type* data);
bool readSondaRS485(byte* request, int requestLength, byte* response, int responseLength, const char* sondaName);
void read_dfrobot_sonda_ec();
void read_dfrobot_sonda_ecph();
void read_seed_sonda_ec();
void readAht10();
void readAht20();
void readBateria();


#endif