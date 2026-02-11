# Módulo BLE para Buffer de Datos

## Descripción
Módulo BLE para extraer datos del buffer de forma inalámbrica usando Bluetooth Low Energy.

## Características BLE

### Servicio UUID
`4fafc201-1fb5-459e-8fcc-c5c9c331914b`

### Características

1. **Lectura de Datos (Read)** - `beb5483e-36e1-4688-b7f5-ea07361b26a8`
   - Permisos: READ, NOTIFY
   - Función: Envía líneas del buffer al cliente

2. **Control (Write)** - `beb5483e-36e1-4688-b7f5-ea07361b26a9`
   - Permisos: WRITE
   - Función: Recibe comandos para controlar el buffer

3. **Estado (Status)** - `beb5483e-36e1-4688-b7f5-ea07361b26aa`
   - Permisos: READ, NOTIFY
   - Función: Informa sobre el estado del buffer

## Comandos Disponibles

Envía estos comandos a la característica de Control:

- `READ_ALL` - Lee y envía todas las líneas no procesadas
- `READ:n` - Lee y envía n líneas (ej: READ:10)
- `STATUS` - Solicita el estado actual del buffer
- `CLEAR` - Limpia todo el buffer
- `REMOVE_PROCESSED` - Elimina las líneas ya procesadas

## Uso

### Configuración en Arduino
```cpp
#include "BUFFERModule.h"
#include "BLEModule.h"

BUFFERModule fileManager;
BLEModule bleModule(fileManager);

void setup() {
    fileManager.begin();
    bleModule.begin("ESP32_Buffer");  // Nombre del dispositivo
}

void loop() {
    bleModule.update();  // Actualizar estado BLE
}
```

### Conexión desde App/Cliente BLE

1. Escanea dispositivos BLE
2. Busca "ESP32_Buffer"
3. Conéctate al dispositivo
4. Busca el servicio UUID: `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
5. Usa las características para:
   - Escribir comandos en CHAR_UUID_CONTROL
   - Leer datos desde CHAR_UUID_READ
   - Monitorear estado en CHAR_UUID_STATUS

### Ejemplo con nRF Connect (Android/iOS)

1. Conectar al dispositivo "ESP32_Buffer"
2. Ir a la característica de Control
3. Escribir "READ_ALL" para leer todos los datos
4. Habilitar notificaciones en la característica de Lectura
5. Recibir datos del buffer

### Ejemplo con Python (usando bleak)

```python
import asyncio
from bleak import BleakClient, BleakScanner

SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHAR_READ = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
CHAR_CONTROL = "beb5483e-36e1-4688-b7f5-ea07361b26a9"
CHAR_STATUS = "beb5483e-36e1-4688-b7f5-ea07361b26aa"

async def main():
    # Buscar dispositivo
    devices = await BleakScanner.discover()
    esp32 = next((d for d in devices if d.name == "ESP32_Buffer"), None)
    
    if esp32:
        async with BleakClient(esp32.address) as client:
            # Suscribirse a notificaciones
            def notification_handler(sender, data):
                print(f"Datos: {data.decode()}")
            
            await client.start_notify(CHAR_READ, notification_handler)
            
            # Enviar comando para leer datos
            await client.write_gatt_char(CHAR_CONTROL, b"READ_ALL")
            
            await asyncio.sleep(5)

asyncio.run(main())
```

## Formato de Respuestas

### Datos del Buffer
Formato: `índice:contenido`
Ejemplo: `0:Temperatura: 25.3 C`

### Estado del Buffer
Formato: `STATUS|SIZE:bytes|EXISTS:YES/NO`
Ejemplo: `STATUS|SIZE:156|EXISTS:YES`

## Notas Importantes

- El dispositivo debe ser un ESP32 con soporte BLE
- La biblioteca BLE debe estar instalada
- El buffer debe estar inicializado antes del módulo BLE
- Los datos se envían con un pequeño delay (50ms) entre líneas
- Las notificaciones permiten recibir datos automáticamente

## Dependencias

- ESP32 BLE Arduino library
- BUFFERModule
- LittleFS
