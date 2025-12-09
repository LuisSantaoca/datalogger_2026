# sensores_rv03

## Carpeta data_buffer

Sistema de gestión de archivos para ESP32/ESP8266 que utiliza LittleFS para almacenar datos de sensores.

### Estructura de Archivos

```
data_buffer/
├── config.h                        # Configuración global del sistema
├── FileManager.h                   # Declaración de la clase FileManager
├── FileManager.cpp                 # Implementación de FileManager
├── FileManager_Documentation.md    # Documentación detallada del sistema
├── data_buffer.ino                 # Programa principal con pruebas
└── data/
    └── buffer.txt                  # Archivo de almacenamiento de datos
```

### Descripción de Archivos

- **config.h**: Define constantes de configuración como la ruta del archivo buffer (`/buffer.txt`), límite de líneas a leer (50), velocidad serial (115200) y el marcador de líneas procesadas (`[P]`).

- **FileManager.h / FileManager.cpp**: Clase que gestiona el sistema de archivos LittleFS. Permite escribir líneas, leerlas, marcar líneas como procesadas y eliminarlas del archivo.

- **data_buffer.ino**: Programa principal que ejecuta pruebas del sistema FileManager, demostrando escritura, lectura, marcado y eliminación de líneas.

- **FileManager_Documentation.md**: Documentación técnica completa con arquitectura, referencia de API y ejemplos de uso.

- **data/buffer.txt**: Archivo donde se almacenan las líneas de datos de manera persistente en el sistema de archivos LittleFS

---

