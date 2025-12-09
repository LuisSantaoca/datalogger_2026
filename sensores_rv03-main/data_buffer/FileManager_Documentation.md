# FileManager - Documentación

## Descripción General

`FileManager` es una clase diseñada para gestionar operaciones de archivos en sistemas embebidos utilizando el sistema de archivos LittleFS. Proporciona funcionalidades para escribir, leer, marcar y eliminar líneas de texto de manera eficiente, con un sistema integrado de marcado de líneas procesadas.

## Características Principales

- ✅ Inicialización y gestión del sistema de archivos LittleFS
- ✅ Escritura de líneas de texto al archivo
- ✅ Lectura de todas las líneas o solo las no procesadas
- ✅ Sistema de marcado de líneas procesadas
- ✅ Eliminación selectiva de líneas procesadas
- ✅ Verificación de existencia y tamaño de archivos

---

## Arquitectura del Sistema

### Archivos del Proyecto

```
data_buffer/
├── config.h              # Configuración global del sistema
├── FileManager.h         # Declaración de la clase FileManager
├── FileManager.cpp       # Implementación de FileManager
├── data_buffer.ino       # Programa principal con pruebas
└── data/
    └── buffer.txt        # Archivo de almacenamiento de datos
```

---

## Configuración (config.h)

### Variables de Configuración

| Variable | Valor | Descripción |
|----------|-------|-------------|
| `BUFFER_FILE_PATH` | `"/buffer.txt"` | Ruta del archivo en el sistema LittleFS |
| `MAX_LINES_TO_READ` | `50` | Número máximo de líneas que se pueden leer |
| `SERIAL_BAUD_RATE` | `115200` | Velocidad de comunicación serial |
| `PROCESSED_MARKER` | `"[P]"` | Marcador para identificar líneas procesadas |

### Secciones de Configuración

```cpp
// CONFIGURACIÓN DE ARCHIVOS
#define BUFFER_FILE_PATH "/buffer.txt"
#define MAX_LINES_TO_READ 50
#define PROCESSED_MARKER "[P]"

// CONFIGURACIÓN DE COMUNICACIÓN SERIAL
#define SERIAL_BAUD_RATE 115200
```

---

## API de FileManager

### Constructor

```cpp
FileManager();
```

**Descripción:** Inicializa las variables internas del gestor de archivos.

**Ejemplo:**
```cpp
FileManager fileManager;
```

---

### Métodos Públicos

#### 1. Inicialización

```cpp
bool begin();
```

**Descripción:** Inicializa el sistema de archivos LittleFS.

**Retorna:** 
- `true` si la inicialización fue exitosa
- `false` en caso de error

**Ejemplo:**
```cpp
if (!fileManager.begin()) {
    Serial.println("Error al inicializar LittleFS");
    return;
}
```

---

#### 2. Operaciones Básicas de Escritura

```cpp
bool appendLine(const String& line);
```

**Descripción:** Agrega una nueva línea de texto al final del archivo.

**Parámetros:**
- `line`: Cadena de texto a agregar

**Retorna:** 
- `true` si la línea se agregó correctamente
- `false` en caso de error

**Ejemplo:**
```cpp
fileManager.appendLine("Sensor temperatura: 23.5");
fileManager.appendLine("Sensor humedad: 65.2");
```

---

#### 3. Operaciones de Lectura

##### 3.1 Leer Todas las Líneas

```cpp
bool readLines(String* lines, int maxLines, int& count);
```

**Descripción:** Lee todas las líneas del archivo (procesadas y no procesadas).

**Parámetros:**
- `lines`: Arreglo donde se almacenarán las líneas leídas
- `maxLines`: Número máximo de líneas a leer
- `count`: Referencia donde se almacenará el número de líneas leídas

**Retorna:** 
- `true` si la lectura fue exitosa
- `false` en caso de error

**Ejemplo:**
```cpp
String lines[MAX_LINES_TO_READ];
int lineCount = 0;

if (fileManager.readLines(lines, MAX_LINES_TO_READ, lineCount)) {
    for (int i = 0; i < lineCount; i++) {
        Serial.println(lines[i]);
    }
}
```

##### 3.2 Leer Solo Líneas No Procesadas

```cpp
bool readUnprocessedLines(String* lines, int maxLines, int& count);
```

**Descripción:** Lee únicamente las líneas que no han sido marcadas como procesadas (sin el marcador `[P]`).

**Parámetros:**
- `lines`: Arreglo donde se almacenarán las líneas no procesadas
- `maxLines`: Número máximo de líneas a leer
- `count`: Referencia donde se almacenará el número de líneas leídas

**Retorna:** 
- `true` si la lectura fue exitosa
- `false` en caso de error

**Ejemplo:**
```cpp
String unprocessedLines[MAX_LINES_TO_READ];
int count = 0;

if (fileManager.readUnprocessedLines(unprocessedLines, MAX_LINES_TO_READ, count)) {
    Serial.print("Líneas pendientes: ");
    Serial.println(count);
    for (int i = 0; i < count; i++) {
        // Procesar líneas pendientes
        procesarDato(unprocessedLines[i]);
    }
}
```

---

#### 4. Sistema de Marcado de Líneas

##### 4.1 Marcar Línea Individual

```cpp
bool markLineAsProcessed(int lineNumber);
```

**Descripción:** Marca una línea específica como procesada agregando el marcador `[P]` al inicio.

**Parámetros:**
- `lineNumber`: Número de línea a marcar (comenzando desde 0)

**Retorna:** 
- `true` si la línea se marcó correctamente
- `false` en caso de error

**Ejemplo:**
```cpp
// Marcar la primera línea como procesada
if (fileManager.markLineAsProcessed(0)) {
    Serial.println("Línea 0 procesada");
}
```

##### 4.2 Marcar Múltiples Líneas

```cpp
bool markLinesAsProcessed(int* lineNumbers, int count);
```

**Descripción:** Marca múltiples líneas como procesadas en una sola operación.

**Parámetros:**
- `lineNumbers`: Arreglo con los números de líneas a marcar
- `count`: Cantidad de líneas a marcar

**Retorna:** 
- `true` si todas las líneas se marcaron correctamente
- `false` en caso de error

**Ejemplo:**
```cpp
// Marcar las líneas 0, 2 y 4 como procesadas
int lineasProcesadas[] = {0, 2, 4};
if (fileManager.markLinesAsProcessed(lineasProcesadas, 3)) {
    Serial.println("Líneas marcadas exitosamente");
}
```

##### 4.3 Eliminar Líneas Procesadas

```cpp
bool removeProcessedLines();
```

**Descripción:** Elimina todas las líneas marcadas como procesadas del archivo. Útil para mantener el archivo limpio y reducir su tamaño.

**Retorna:** 
- `true` si la operación fue exitosa
- `false` en caso de error

**Ejemplo:**
```cpp
// Limpiar el archivo eliminando líneas ya procesadas
if (fileManager.removeProcessedLines()) {
    Serial.println("Archivo limpiado");
    Serial.print("Nuevo tamaño: ");
    Serial.println(fileManager.getFileSize());
}
```

---

#### 5. Utilidades

##### 5.1 Verificar Existencia

```cpp
bool fileExists();
```

**Descripción:** Verifica si el archivo existe en el sistema de archivos.

**Retorna:** 
- `true` si el archivo existe
- `false` si no existe

**Ejemplo:**
```cpp
if (fileManager.fileExists()) {
    Serial.println("El archivo existe");
}
```

##### 5.2 Obtener Tamaño

```cpp
size_t getFileSize();
```

**Descripción:** Obtiene el tamaño actual del archivo en bytes.

**Retorna:** 
- Tamaño del archivo en bytes
- `0` si el archivo no existe o está vacío

**Ejemplo:**
```cpp
size_t size = fileManager.getFileSize();
Serial.print("Tamaño del archivo: ");
Serial.print(size);
Serial.println(" bytes");
```

##### 5.3 Limpiar Archivo

```cpp
bool clearFile();
```

**Descripción:** Elimina todo el contenido del archivo, dejándolo vacío.

**Retorna:** 
- `true` si el archivo se limpió correctamente
- `false` en caso de error

**Ejemplo:**
```cpp
if (fileManager.clearFile()) {
    Serial.println("Archivo limpiado completamente");
}
```

---

## Flujo de Trabajo Típico

### Escenario 1: Recolección y Procesamiento de Datos

```cpp
void loop() {
    // 1. Recolectar datos de sensores
    String dato = "Temperatura: " + String(leerTemperatura());
    fileManager.appendLine(dato);
    
    // 2. Leer líneas no procesadas
    String lineas[MAX_LINES_TO_READ];
    int count = 0;
    fileManager.readUnprocessedLines(lineas, MAX_LINES_TO_READ, count);
    
    // 3. Procesar cada línea
    int lineasProcesadas[MAX_LINES_TO_READ];
    int numProcesadas = 0;
    
    for (int i = 0; i < count; i++) {
        if (enviarDatosAServidor(lineas[i])) {
            lineasProcesadas[numProcesadas++] = i;
        }
    }
    
    // 4. Marcar líneas procesadas
    if (numProcesadas > 0) {
        fileManager.markLinesAsProcessed(lineasProcesadas, numProcesadas);
    }
    
    // 5. Limpiar periódicamente (cada hora, por ejemplo)
    if (debeLimpiarArchivo()) {
        fileManager.removeProcessedLines();
    }
    
    delay(1000);
}
```

### Escenario 2: Sistema de Buffer Offline

```cpp
void gestionarBufferOffline() {
    // Cuando hay conexión
    if (hayConexion()) {
        String pendientes[MAX_LINES_TO_READ];
        int count = 0;
        
        // Leer datos pendientes
        if (fileManager.readUnprocessedLines(pendientes, MAX_LINES_TO_READ, count)) {
            int enviadas[MAX_LINES_TO_READ];
            int numEnviadas = 0;
            
            // Enviar al servidor
            for (int i = 0; i < count; i++) {
                if (enviarAlServidor(pendientes[i])) {
                    enviadas[numEnviadas++] = i;
                }
            }
            
            // Marcar como enviadas
            fileManager.markLinesAsProcessed(enviadas, numEnviadas);
            
            // Limpiar buffer
            fileManager.removeProcessedLines();
        }
    } else {
        // Sin conexión: almacenar localmente
        String datos = recolectarDatos();
        fileManager.appendLine(datos);
    }
}
```

---

## Formato de Líneas en el Archivo

### Línea No Procesada
```
Sensor temperatura: 23.5
```

### Línea Procesada
```
[P]Sensor temperatura: 23.5
```

El marcador `[P]` se agrega automáticamente al inicio de la línea cuando se marca como procesada.

---

## Consideraciones de Uso

### Limitaciones

1. **Número máximo de líneas:** Definido por `MAX_LINES_TO_READ` (50 por defecto)
2. **Tamaño del archivo:** Limitado por el espacio disponible en LittleFS
3. **Operaciones de marcado:** Requieren reescribir todo el archivo (puede ser lento con archivos grandes)

### Mejores Prácticas

1. **Limpieza periódica:** Ejecutar `removeProcessedLines()` regularmente para mantener el archivo pequeño
2. **Verificar inicialización:** Siempre verificar que `begin()` retorne `true` antes de usar otras funciones
3. **Gestión de memoria:** Considerar el tamaño de `MAX_LINES_TO_READ` según la RAM disponible
4. **Procesamiento por lotes:** Usar `markLinesAsProcessed()` para marcar múltiples líneas en una sola operación

### Ejemplo de Gestión Óptima

```cpp
void gestionarArchivoOptimo() {
    // Leer líneas pendientes
    String lineas[MAX_LINES_TO_READ];
    int count = 0;
    fileManager.readUnprocessedLines(lineas, MAX_LINES_TO_READ, count);
    
    // Si hay muchas líneas, procesar por lotes
    if (count > 0) {
        procesarLotes(lineas, count);
        
        // Limpiar si el archivo es muy grande
        if (fileManager.getFileSize() > 10000) { // 10KB
            fileManager.removeProcessedLines();
        }
    }
}
```

---

## Solución de Problemas

### El archivo no se crea

**Problema:** `fileExists()` retorna `false` después de intentar escribir.

**Solución:**
```cpp
// Verificar inicialización
if (!fileManager.begin()) {
    Serial.println("LittleFS no inicializado");
    // Formatear si es necesario
    LittleFS.format();
    fileManager.begin();
}
```

### Error al marcar líneas

**Problema:** `markLineAsProcessed()` retorna `false`.

**Causas posibles:**
- Número de línea inválido (negativo o mayor que el total de líneas)
- Archivo no inicializado
- Sin espacio en el sistema de archivos

**Solución:**
```cpp
// Verificar que el número de línea sea válido
String lineas[MAX_LINES_TO_READ];
int count = 0;
fileManager.readLines(lineas, MAX_LINES_TO_READ, count);

if (numLinea >= 0 && numLinea < count) {
    fileManager.markLineAsProcessed(numLinea);
}
```

### Archivo crece demasiado

**Problema:** El archivo ocupa mucho espacio.

**Solución:**
```cpp
// Limpiar periódicamente
unsigned long ultimaLimpieza = 0;
const unsigned long INTERVALO_LIMPIEZA = 3600000; // 1 hora

void loop() {
    if (millis() - ultimaLimpieza > INTERVALO_LIMPIEZA) {
        fileManager.removeProcessedLines();
        ultimaLimpieza = millis();
    }
}
```

---

## Ejemplos Completos

### Ejemplo 1: Logger de Sensores

```cpp
#include "FileManager.h"
#include "config.h"

FileManager fileManager;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    fileManager.begin();
}

void loop() {
    // Leer sensores
    float temp = analogRead(A0) * 0.1;
    float hum = analogRead(A1) * 0.1;
    
    // Guardar en archivo
    String dato = String(millis()) + "," + 
                  String(temp) + "," + 
                  String(hum);
    fileManager.appendLine(dato);
    
    delay(5000); // Cada 5 segundos
}
```

### Ejemplo 2: Sincronización con Servidor

```cpp
void sincronizarConServidor() {
    if (!WiFi.isConnected()) {
        return; // Esperar conexión
    }
    
    String datos[MAX_LINES_TO_READ];
    int count = 0;
    
    // Obtener datos pendientes
    if (fileManager.readUnprocessedLines(datos, MAX_LINES_TO_READ, count)) {
        int procesadas[MAX_LINES_TO_READ];
        int numProcesadas = 0;
        
        // Enviar cada línea
        for (int i = 0; i < count; i++) {
            if (httpPOST(datos[i])) {
                procesadas[numProcesadas++] = i;
            }
        }
        
        // Marcar exitosas
        if (numProcesadas > 0) {
            fileManager.markLinesAsProcessed(procesadas, numProcesadas);
        }
        
        // Limpiar si todo se envió
        if (numProcesadas == count) {
            fileManager.removeProcessedLines();
        }
    }
}
```

---

## Diagrama de Flujo del Sistema

```
┌─────────────────┐
│  Inicialización │
│  begin()        │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Escribir Datos  │
│ appendLine()    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Leer No         │
│ Procesadas      │
│ readUnproc...() │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Procesar Datos  │
│ (Lógica de      │
│  usuario)       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Marcar Como     │
│ Procesadas      │
│ markLinesAs...()│
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Limpiar Archivo │
│ (Periódico)     │
│ removeProc...() │
└─────────────────┘
```

---

## Conclusión

`FileManager` proporciona una solución robusta y eficiente para gestionar datos persistentes en sistemas embebidos. El sistema de marcado de líneas permite un control preciso sobre qué datos han sido procesados, facilitando la implementación de buffers offline, sistemas de sincronización y logging avanzado.

Para más información o reportar problemas, consulta el código fuente o las pruebas incluidas en `data_buffer.ino`.
