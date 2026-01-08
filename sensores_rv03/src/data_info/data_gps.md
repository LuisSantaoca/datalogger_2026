# Módulo: data_gps

## Descripción
El módulo data_gps gestiona la comunicación con el módulo GPS del SIM7080G para obtener coordenadas geográficas.

## Componentes

### GPSModule
- **Propósito**: Control del subsistema GNSS del SIM7080G
- **Funcionalidad Principal**:
  - Encendido/apagado del módulo GPS mediante PWRKEY
  - Comunicación AT con el módulo
  - Obtención de coordenadas GPS (latitud, longitud, altitud)
  - Validación de fix GPS

## Archivos
- `GPSModule.cpp/h`: Implementación del control GPS
- `config_data_gps.h`: Configuración de pines y parámetros GPS

## Configuración (config_data_gps.h)
- `GPS_PIN_TX/RX`: Pines UART para comunicación
- `GPS_PWRKEY_PIN`: Pin de control de encendido
- `GPS_SIM_BAUD`: Velocidad de comunicación (115200)
- `GPS_AT_TIMEOUT_MS`: Timeout para comandos AT
- `GPS_GNSS_MAX_RETRIES`: Intentos máximos para obtener fix
- `GPS_GNSS_RETRY_DELAY_MS`: Delay entre intentos

## Funciones Principales

### powerOn()
Enciende el módulo GPS usando secuencia PWRKEY y valida respuesta AT.

### powerOff()
Apaga el subsistema GNSS y el módulo completo.

### getCoordinatesAndShutdown()
Obtiene fix GPS con coordenadas completas y apaga el módulo.

### getLatitudeAsString()
Obtiene solo la latitud como string con precisión de 6 decimales.

### getLongitudeAsString()
Obtiene solo la longitud como string con precisión de 6 decimales.

### getAltitudeAsString()
Obtiene solo la altitud como string con precisión de 2 decimales.

## Flujo de Operación
1. Aplicar secuencia PWRKEY para encender módulo
2. Esperar respuesta AT (módulo listo)
3. Encender subsistema GNSS (AT+CGNSPWR=1)
4. Solicitar información GNSS (AT+CGNSINF)
5. Parsear respuesta para extraer coordenadas
6. Validar fix GPS (campo 1 = 1)
7. Apagar GNSS y módulo
