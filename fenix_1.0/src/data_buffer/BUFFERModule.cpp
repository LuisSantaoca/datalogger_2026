/**
 * @file BUFFERModule.cpp
 * @brief Implementación del gestor de archivos LittleFS
 */

#include "BUFFERModule.h"
#include "config_data_buffer.h"

BUFFERModule::BUFFERModule() {
    filePath = BUFFER_FILE_PATH;
    isInitialized = false;
}

bool BUFFERModule::begin() {
    if (!LittleFS.begin()) {
        return false;
    }
    isInitialized = true;
    return true;
}

bool BUFFERModule::appendLine(const String& line) {
    if (!isInitialized) {
        return false;
    }
    
    File file = LittleFS.open(filePath, "a");
    if (!file) {
        return false;
    }
    
    file.println(line);
    file.close();
    return true;
}

bool BUFFERModule::readLines(String* lines, int maxLines, int& count) {
    if (!isInitialized) {
        return false;
    }
    
    count = 0;
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        return false;
    }
    
    while (file.available() && count < maxLines) {
        lines[count] = file.readStringUntil('\n');
        lines[count].trim();  // FIX-V13: eliminar \r residual
        count++;
    }

    file.close();
    return true;
}

bool BUFFERModule::clearFile() {
    if (!isInitialized) {
        return false;
    }
    
    if (LittleFS.exists(filePath)) {
        return LittleFS.remove(filePath);
    }
    return true;
}

bool BUFFERModule::fileExists() {
    if (!isInitialized) {
        return false;
    }
    return LittleFS.exists(filePath);
}

size_t BUFFERModule::getFileSize() {
    if (!isInitialized) {
        return 0;
    }
    
    if (!fileExists()) {
        return 0;
    }
    
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        return 0;
    }
    
    size_t size = file.size();
    file.close();
    return size;
}

bool BUFFERModule::readUnprocessedLines(String* lines, int maxLines, int& count) {
    if (!isInitialized) {
        return false;
    }
    
    count = 0;
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        return false;
    }
    
    while (file.available() && count < maxLines) {
        String line = file.readStringUntil('\n');
        line.trim();  // FIX-V13: eliminar \r residual
        // Solo agregar líneas que no tengan el marcador de procesado
        if (!line.startsWith(PROCESSED_MARKER)) {
            lines[count] = line;
            count++;
        }
    }
    
    file.close();
    return true;
}

bool BUFFERModule::markLineAsProcessed(int lineNumber) {
    if (!isInitialized || lineNumber < 0) {
        return false;
    }
    
    // Leer todas las líneas del archivo
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        return false;
    }
    
    String allLines[MAX_LINES_TO_READ];
    int totalLines = 0;
    
    while (file.available() && totalLines < MAX_LINES_TO_READ) {
        allLines[totalLines] = file.readStringUntil('\n');
        allLines[totalLines].trim();  // FIX-V13: eliminar \r residual
        totalLines++;
    }
    file.close();

    // Verificar que el número de línea sea válido
    if (lineNumber >= totalLines) {
        return false;
    }
    
    // Marcar la línea si no está ya marcada
    if (!allLines[lineNumber].startsWith(PROCESSED_MARKER)) {
        allLines[lineNumber] = String(PROCESSED_MARKER) + allLines[lineNumber];
    }
    
    // Reescribir el archivo con la línea marcada
    if (!clearFile()) {
        return false;
    }
    
    file = LittleFS.open(filePath, "w");
    if (!file) {
        return false;
    }
    
    for (int i = 0; i < totalLines; i++) {
        file.println(allLines[i]);
    }
    
    file.close();
    return true;
}

bool BUFFERModule::markLinesAsProcessed(int* lineNumbers, int count) {
    if (!isInitialized || count <= 0) {
        return false;
    }
    
    // Leer todas las líneas del archivo
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        return false;
    }
    
    String allLines[MAX_LINES_TO_READ];
    int totalLines = 0;
    
    while (file.available() && totalLines < MAX_LINES_TO_READ) {
        allLines[totalLines] = file.readStringUntil('\n');
        allLines[totalLines].trim();  // FIX-V13: eliminar \r residual
        totalLines++;
    }
    file.close();

    // Marcar todas las líneas indicadas
    for (int i = 0; i < count; i++) {
        int lineNum = lineNumbers[i];
        if (lineNum >= 0 && lineNum < totalLines) {
            if (!allLines[lineNum].startsWith(PROCESSED_MARKER)) {
                allLines[lineNum] = String(PROCESSED_MARKER) + allLines[lineNum];
            }
        }
    }
    
    // Reescribir el archivo con las líneas marcadas
    if (!clearFile()) {
        return false;
    }
    
    file = LittleFS.open(filePath, "w");
    if (!file) {
        return false;
    }
    
    for (int i = 0; i < totalLines; i++) {
        file.println(allLines[i]);
    }
    
    file.close();
    return true;
}

bool BUFFERModule::removeProcessedLines() {
    if (!isInitialized) {
        return false;
    }
    
    // Leer todas las líneas del archivo
    File file = LittleFS.open(filePath, "r");
    if (!file) {
        return false;
    }
    
    String unprocessedLines[MAX_LINES_TO_READ];
    int unprocessedCount = 0;
    
    while (file.available() && unprocessedCount < MAX_LINES_TO_READ) {
        String line = file.readStringUntil('\n');
        line.trim();  // FIX-V13: eliminar \r residual
        // Solo guardar líneas sin el marcador
        if (!line.startsWith(PROCESSED_MARKER)) {
            unprocessedLines[unprocessedCount] = line;
            unprocessedCount++;
        }
    }
    file.close();
    
    // Reescribir el archivo solo con las líneas no procesadas
    if (!clearFile()) {
        return false;
    }
    
    if (unprocessedCount == 0) {
        return true; // No hay líneas sin procesar, archivo ya limpio
    }
    
    file = LittleFS.open(filePath, "w");
    if (!file) {
        return false;
    }
    
    for (int i = 0; i < unprocessedCount; i++) {
        file.println(unprocessedLines[i]);
    }
    
    file.close();
    return true;
}
