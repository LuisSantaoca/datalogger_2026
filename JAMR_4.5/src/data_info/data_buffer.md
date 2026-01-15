# Módulo: data_buffer

## Descripción
El módulo data_buffer gestiona el almacenamiento temporal de datos y la comunicación Bluetooth Low Energy (BLE) con dispositivos externos.

## Componentes

### BLEModule
- **Propósito**: Manejo de comunicación BLE para transmitir datos del sensor
- **Funcionalidad Principal**:
  - Inicialización del servicio BLE
  - Transmisión de datos a través de características BLE
  - Gestión de conexiones de clientes BLE

### BUFFERModule
- **Propósito**: Almacenamiento temporal de datos de sensores
- **Funcionalidad Principal**:
  - Buffering de lecturas de sensores
  - Gestión de memoria circular para datos
  - Persistencia de datos en archivo buffer.txt
  - Prevención de pérdida de datos cuando no hay conexión

## Archivos
- `BLEModule.cpp/h`: Implementación de comunicación BLE
- `BUFFERModule.cpp/h`: Implementación del sistema de buffer
- `config_data_buffer.h`: Configuración del módulo (tamaño de buffer, parámetros BLE)
- `README_BLE.md`: Documentación específica de BLE
- `data/buffer.txt`: Archivo de almacenamiento persistente

## Flujo de Operación
1. Los datos del sensor se escriben en el buffer
2. El buffer almacena temporalmente los datos
3. Si hay conexión BLE, los datos se transmiten
4. Si no hay conexión, los datos se persisten en buffer.txt
5. Los datos se recuperan cuando se restablece la conexión
