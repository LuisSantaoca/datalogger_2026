#ifndef CONFIG_OPERADORAS_H
#define CONFIG_OPERADORAS_H

#include <Arduino.h>

enum Operadora {
    TELCEL = 0,
    ATT = 1,
    ATT2 = 2,
    MOVISTAR = 3,
    ALTAN = 4
};

struct OperadoraConfig {
    const char* nombre;
    const char* mcc_mnc;
    const char* apn;
    const char* cnmp;
    const char* cmnb;
    const char* bandas;
};

static const OperadoraConfig OPERADORAS[] = {
    
    {
        "TELCEL",
        "334020",
        "em",
        "AT+CNMP=38",
        "AT+CMNB=1",
        "1,2,3,4,5,8,12,13,18,19,20,26,28"
    },
    {
        "AT&T MEXICO (334090)",
        "334090",
        "em",
        "AT+CNMP=38",
        "AT+CMNB=1",
        "1,2,3,4,5,8,12,13,18,19,20,26,28"
    },
    {
        "AT&T MEXICO (334050)",
        "334050",
        "em",
        "AT+CNMP=38",
        "AT+CMNB=1",
        "1,2,3,4,5,8,12,13,18,19,20,26,28"
    },
    {
        "MOVISTAR",
        "334030",
        "em",
        "AT+CNMP=38",
        "AT+CMNB=1",
        "1,2,3,4,5,8,12,13,18,19,20,26,28"
    },
    {
        "ALTAN",
        "334140",
        "em",
        "AT+CNMP=38",
        "AT+CMNB=1",
        "1,2,3,4,5,8,12,13,18,19,20,26,28"
    }
};


static const uint8_t NUM_OPERADORAS = 5;

#endif
