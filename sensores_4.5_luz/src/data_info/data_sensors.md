# Módulo: data_sensors

## Descripción
El módulo data_sensors gestiona la lectura de diferentes tipos de sensores conectados al sistema.

## Componentes

### ADCSensorModule
- **Propósito**: Lectura de sensores analógicos mediante ADC
- **Funcionalidad Principal**:
  - Lectura de pines analógicos
  - Conversión ADC a valores físicos
  - Calibración de sensores analógicos
  - Promediado de múltiples lecturas

### I2CSensorModule
- **Propósito**: Comunicación con sensores I2C
- **Funcionalidad Principal**:
  - Inicialización de bus I2C
  - Lectura/escritura de registros I2C
  - Detección de dispositivos I2C
  - Gestión de múltiples sensores en el bus

### RS485Module
- **Propósito**: Comunicación con sensores RS485/Modbus
- **Funcionalidad Principal**:
  - Control de dirección TX/RX
  - Protocolo Modbus RTU
  - Lectura de registros Modbus
  - Control de tiempo de respuesta

## Archivos
- `ADCSensorModule.cpp/h`: Implementación de sensores ADC
- `I2CSensorModule.cpp/h`: Implementación de sensores I2C
- `RS485Module.cpp/h`: Implementación de sensores RS485
- `config_data_sensors.h`: Configuración de pines y parámetros

## Tipos de Sensores Soportados

### Sensores ADC
- Sensores de voltaje/corriente
- Sensores resistivos (temperatura, humedad)
- Potenciómetros
- Fotoresistores

### Sensores I2C
- Sensores ambientales (BME280, SHT3x)
- Acelerómetros (MPU6050)
- Magnetómetros
- RTCs

### Sensores RS485
- Medidores de energía
- Sensores industriales Modbus
- PLCs
- Controladores remotos

## Flujo de Operación

### ADC
1. Configurar pin como entrada analógica
2. Realizar múltiples lecturas
3. Promediar valores
4. Aplicar calibración
5. Convertir a unidades físicas

### I2C
1. Inicializar bus I2C
2. Escanear dirección del sensor
3. Configurar sensor (registros de control)
4. Leer registros de datos
5. Procesar datos según datasheet

### RS485
1. Configurar pin de dirección
2. Construir trama Modbus
3. Cambiar a modo TX
4. Enviar consulta
5. Cambiar a modo RX
6. Recibir respuesta
7. Validar CRC
8. Extraer datos
