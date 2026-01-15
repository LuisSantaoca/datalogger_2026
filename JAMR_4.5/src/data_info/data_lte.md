# Módulo: data_lte

## Descripción
El módulo data_lte gestiona la conectividad celular LTE mediante el módulo SIM7080G para transmisión de datos.

## Componentes

### LTEModule
- **Propósito**: Control de comunicación LTE/celular
- **Funcionalidad Principal**:
  - Encendido/apagado del módulo SIM7080G
  - Registro en red celular
  - Transmisión TCP/IP de datos
  - Gestión de calidad de señal
  - Envío de SMS

## Archivos
- `LTEModule.cpp/h`: Implementación del control LTE
- `config_data_lte.h`: Configuración de pines y parámetros LTE
- `config_operadoras.h`: Configuración de operadoras celulares

## Configuración (config_data_lte.h)
- `LTE_PIN_TX/RX`: Pines UART para comunicación
- `LTE_PWRKEY_PIN`: Pin de control de encendido
- `LTE_SIM_BAUD`: Velocidad de comunicación (115200)
- `LTE_AT_TIMEOUT_MS`: Timeout para comandos AT
- `LTE_POWER_ON_ATTEMPTS`: Intentos de encendido
- `LTE_PWRKEY_PULSE_MS`: Duración del pulso PWRKEY
- `DB_SERVER_IP`: IP del servidor de datos
- `TCP_PORT`: Puerto TCP para conexión

## Funciones Principales

### begin()
Inicializa pines y puerto serial para comunicación.

### powerOn()
Enciende el módulo SIM7080G mediante secuencia PWRKEY.

### powerOff()
Apaga el módulo (primero por comando AT, luego PWRKEY si falla).

### isAlive()
Verifica si el módulo responde a comandos AT.

### getICCID()
Obtiene el ICCID de la tarjeta SIM.

### getSignalQuality()
Lee calidad de señal (RSRP, RSRQ, RSSI, SINR).

### sendTCPData()
Envía datos al servidor mediante conexión TCP.

## Flujo de Operación
1. Configurar pines y serial
2. Aplicar pulso PWRKEY para encender
3. Esperar respuesta AT
4. Configurar red celular (APN, operadora)
5. Registrarse en red
6. Establecer conexión TCP
7. Transmitir datos
8. Cerrar conexión
9. Apagar módulo si es necesario
