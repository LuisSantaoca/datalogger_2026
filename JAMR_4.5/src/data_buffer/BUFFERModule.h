/**
 * @file BUFFERModule.h
 * @brief Gestor de buffer de datos con persistencia en LittleFS
 * @version 1.0.0
 * @date 2026-01-15
 * 
 * Proporciona almacenamiento persistente de líneas de texto para:
 * - Buffering de tramas cuando no hay conectividad
 * - Marcado de líneas procesadas/pendientes
 * - Limpieza automática de datos enviados
 * 
 * @see config_data_buffer.h para configuración de rutas
 */

#ifndef BUFFERMODULE_H
#define BUFFERMODULE_H

#include <LittleFS.h>

/**
 * @class BUFFERModule
 * @brief Gestor de archivos para almacenamiento persistente de datos
 */
class BUFFERModule {
  public:
    /**
     * @brief Constructor de la clase BUFFERModule
     * 
     * Inicializa las variables internas del gestor de archivos.
     */
    BUFFERModule();
    
    /**
     * Inicializa el sistema de archivos LittleFS.
     * @return true si la inicialización fue exitosa, false en caso contrario.
     */
    bool begin();
    
    /**
     * Agrega una nueva línea de texto al final del archivo.
     * @param line Cadena de texto a agregar al archivo.
     * @return true si la línea se agregó correctamente, false en caso contrario.
     */
    bool appendLine(const String& line);
    
    /**
     * Lee múltiples líneas del archivo y las almacena en un arreglo.
     * @param lines Arreglo de cadenas donde se almacenarán las líneas leídas.
     * @param maxLines Número máximo de líneas a leer.
     * @param count Referencia donde se almacenará el número real de líneas leídas.
     * @return true si la lectura fue exitosa, false en caso contrario.
     */
    bool readLines(String* lines, int maxLines, int& count);
    
    /**
     * Elimina todo el contenido del archivo, dejándolo vacío.
     * @return true si el archivo se limpió correctamente, false en caso contrario.
     */
    bool clearFile();
    
    /**
     * Verifica si el archivo existe en el sistema de archivos.
     * @return true si el archivo existe, false si no existe.
     */
    bool fileExists();
    
    /**
     * Obtiene el tamaño actual del archivo en bytes.
     * @return Tamaño del archivo en bytes, 0 si el archivo no existe o está vacío.
     */
    size_t getFileSize();
    
    /**
     * Lee solo las líneas que no han sido marcadas como procesadas.
     * @param lines Arreglo de cadenas donde se almacenarán las líneas no procesadas.
     * @param maxLines Número máximo de líneas a leer.
     * @param count Referencia donde se almacenará el número real de líneas leídas.
     * @return true si la lectura fue exitosa, false en caso contrario.
     */
    bool readUnprocessedLines(String* lines, int maxLines, int& count);
    
    /**
     * Marca una línea específica como procesada agregando el marcador al inicio.
     * @param lineNumber Número de línea a marcar (comenzando desde 0).
     * @return true si la línea se marcó correctamente, false en caso contrario.
     */
    bool markLineAsProcessed(int lineNumber);
    
    /**
     * Marca múltiples líneas como procesadas.
     * @param lineNumbers Arreglo con los números de líneas a marcar.
     * @param count Cantidad de líneas a marcar.
     * @return true si todas las líneas se marcaron correctamente, false en caso contrario.
     */
    bool markLinesAsProcessed(int* lineNumbers, int count);
    
    /**
     * Elimina todas las líneas que ya han sido marcadas como procesadas.
     * Útil para mantener el archivo limpio y reducir su tamaño.
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool removeProcessedLines();
    
  private:
    const char* filePath;      // Ruta del archivo en el sistema de archivos
    bool isInitialized;        // Indica si el sistema de archivos fue inicializado correctamente
};

#endif
