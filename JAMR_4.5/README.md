# Sensores RV03

## Estructura del Proyecto

```
sensores_rv03/
 sensores_rv03.ino          # Archivo principal del proyecto
 AppController.cpp           # Controlador principal de la aplicación
 AppController.h             # Interfaz del controlador
 README.md                   # Documentación del proyecto

 data/                       # Carpeta de datos
    buffer.txt             # Archivo de buffer

 src/                        # Código fuente de los módulos
    
     data_buffer/            # Módulo de buffer de datos
        BLEModule.cpp      # Implementación BLE
        BLEModule.h        # Interfaz BLE
        BUFFERModule.cpp   # Implementación del buffer
        BUFFERModule.h     # Interfaz del buffer
        config_data_buffer.h   # Configuración del módulo
        README_BLE.md      # Documentación BLE
    
     data_format/            # Módulo de formato de datos
        FORMATModule.cpp   # Implementación del formateo
        FORMATModule.h     # Interfaz del formateo
        config_data_format.h   # Configuración del módulo
    
     data_gps/               # Módulo GPS
        GPSModule.cpp      # Implementación GPS
        GPSModule.h        # Interfaz GPS
        config_data_gps.h  # Configuración del módulo
    
     data_lte/               # Módulo LTE
        LTEModule.cpp      # Implementación LTE
        LTEModule.h        # Interfaz LTE
        config_data_lte.h  # Configuración del módulo
        config_operadoras.h    # Configuración de operadoras
    
     data_sensors/           # Módulo de sensores
        ADCSensorModule.cpp    # Implementación sensores ADC
        ADCSensorModule.h      # Interfaz sensores ADC
        I2CSensorModule.cpp    # Implementación sensores I2C
        I2CSensorModule.h      # Interfaz sensores I2C
        RS485Module.cpp        # Implementación RS485
        RS485Module.h          # Interfaz RS485
        config_data_sensors.h  # Configuración del módulo
    
     data_sleepwakeup/       # Módulo de sleep/wakeup
        SLEEPModule.cpp    # Implementación sleep
        SLEEPModule.h      # Interfaz sleep
        config_data_sleepwakeup.h  # Configuración del módulo
    
     data_time/              # Módulo de tiempo/RTC
        RTCModule.cpp      # Implementación RTC
        RTCModule.h        # Interfaz RTC
        config_data_time.h # Configuración del módulo
    
     data_info/              # Documentación de módulos
        data_buffer.md     # Documentación del módulo buffer
        data_format.md     # Documentación del módulo format
        data_gps.md        # Documentación del módulo GPS
        data_lte.md        # Documentación del módulo LTE
        data_sensors.md    # Documentación del módulo sensors
        data_sleepwakeup.md    # Documentación del módulo sleep
        data_time.md       # Documentación del módulo time
    
     memoria_de_proyecto/    # Memoria del proyecto
         MEMORIA_SENSORES_RV03.md   # Documentación completa del proyecto
```

## Descripción de Módulos

- **data_buffer**: Gestión del buffer de datos y comunicación BLE
- **data_format**: Formato y procesamiento de datos
- **data_gps**: Gestión del módulo GPS/GNSS
- **data_lte**: Comunicación LTE/celular
- **data_sensors**: Lectura de sensores (ADC, I2C, RS485)
- **data_sleepwakeup**: Gestión de modos de bajo consumo
- **data_time**: Gestión de tiempo real (RTC)
- **data_info**: Documentación detallada de cada módulo
- **memoria_de_proyecto**: Documentación completa del proyecto

## Archivos Principales

- **sensores_rv03.ino**: Sketch principal de Arduino
- **AppController.cpp/h**: Controlador central que coordina todos los módulos
- **data/buffer.txt**: Almacenamiento persistente de datos

## Documentación

Para información detallada de cada módulo, consultar los archivos en src/data_info/:
- [Buffer y BLE](src/data_info/data_buffer.md)
- [Formato de Datos](src/data_info/data_format.md)
- [GPS/GNSS](src/data_info/data_gps.md)
- [LTE/Celular](src/data_info/data_lte.md)
- [Sensores](src/data_info/data_sensors.md)
- [Sleep/Wakeup](src/data_info/data_sleepwakeup.md)
- [RTC/Tiempo](src/data_info/data_time.md)

Para la memoria completa del proyecto, ver [MEMORIA_SENSORES_RV03.md](src/memoria_de_proyecto/MEMORIA_SENSORES_RV03.md)
