#include "FileManager.h"
#include "config.h"

FileManager fileManager;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(1000);
    
    Serial.println("Iniciando sistema LittleFS...");
    
    if (!fileManager.begin()) {
        Serial.println("Error al inicializar LittleFS");
        return;
    }
    Serial.println("LittleFS inicializado correctamente");
    
    testFileOperations();
    fileManager.clearFile();
}

void loop() {
    
}

void testFileOperations() {
    Serial.println("\n========================================");
    Serial.println("  PRUEBAS DEL SISTEMA DE ARCHIVOS");
    Serial.println("========================================");
    
    // Test 1: Verificar existencia del archivo
    Serial.println("\n[TEST 1] Verificar existencia del archivo");
    if (fileManager.fileExists()) {
        Serial.print("✓ Archivo existe. Tamaño: ");
        Serial.print(fileManager.getFileSize());
        Serial.println(" bytes");
    } else {
        Serial.println("○ Archivo no existe. Se creará al escribir.");
    }
    
    // Test 2: Escribir líneas al archivo
    Serial.println("\n[TEST 2] Escribir líneas al archivo");
    fileManager.appendLine("Sensor temperatura: 23.5");
    fileManager.appendLine("Sensor humedad: 65.2");
    fileManager.appendLine("Sensor presión: 1013.25");
    fileManager.appendLine("Sensor luz: 450");
    fileManager.appendLine("Estado sistema: OK");
    Serial.println("✓ 5 líneas escritas correctamente");
    
    // Test 3: Leer todas las líneas
    Serial.println("\n[TEST 3] Leer todas las líneas del archivo");
    String lines[MAX_LINES_TO_READ];
    int lineCount = 0;
    
    if (fileManager.readLines(lines, MAX_LINES_TO_READ, lineCount)) {
        Serial.print("✓ Líneas totales leídas: ");
        Serial.println(lineCount);
        for (int i = 0; i < lineCount; i++) {
            Serial.print("  [");
            Serial.print(i);
            Serial.print("] ");
            Serial.println(lines[i]);
        }
    } else {
        Serial.println("✗ Error al leer el archivo");
    }
    
    // Test 4: Leer solo líneas no procesadas
    Serial.println("\n[TEST 4] Leer líneas no procesadas");
    int unprocessedCount = 0;
    String unprocessedLines[MAX_LINES_TO_READ];
    
    if (fileManager.readUnprocessedLines(unprocessedLines, MAX_LINES_TO_READ, unprocessedCount)) {
        Serial.print("✓ Líneas sin procesar: ");
        Serial.println(unprocessedCount);
        for (int i = 0; i < unprocessedCount; i++) {
            Serial.print("  [");
            Serial.print(i);
            Serial.print("] ");
            Serial.println(unprocessedLines[i]);
        }
    } else {
        Serial.println("✗ Error al leer líneas no procesadas");
    }
    
    // Test 5: Marcar líneas individuales como procesadas
    Serial.println("\n[TEST 5] Marcar líneas individuales como procesadas");
    if (fileManager.markLineAsProcessed(0)) {
        Serial.println("✓ Línea 0 marcada como procesada");
    } else {
        Serial.println("✗ Error al marcar línea 0");
    }
    
    if (fileManager.markLineAsProcessed(2)) {
        Serial.println("✓ Línea 2 marcada como procesada");
    } else {
        Serial.println("✗ Error al marcar línea 2");
    }
    
    // Test 6: Verificar marcado leyendo todas las líneas
    Serial.println("\n[TEST 6] Verificar líneas marcadas");
    if (fileManager.readLines(lines, MAX_LINES_TO_READ, lineCount)) {
        Serial.println("✓ Estado actual de todas las líneas:");
        for (int i = 0; i < lineCount; i++) {
            Serial.print("  [");
            Serial.print(i);
            Serial.print("] ");
            Serial.println(lines[i]);
        }
    }
    
    // Test 7: Leer solo líneas no procesadas después del marcado
    Serial.println("\n[TEST 7] Leer líneas no procesadas después del marcado");
    if (fileManager.readUnprocessedLines(unprocessedLines, MAX_LINES_TO_READ, unprocessedCount)) {
        Serial.print("✓ Líneas sin procesar: ");
        Serial.println(unprocessedCount);
        for (int i = 0; i < unprocessedCount; i++) {
            Serial.print("  [");
            Serial.print(i);
            Serial.print("] ");
            Serial.println(unprocessedLines[i]);
        }
    }
    
    // Test 8: Marcar múltiples líneas como procesadas
    Serial.println("\n[TEST 8] Marcar múltiples líneas como procesadas");
    int linesToMark[] = {1, 3};
    if (fileManager.markLinesAsProcessed(linesToMark, 2)) {
        Serial.println("✓ Líneas 1 y 3 marcadas como procesadas");
    } else {
        Serial.println("✗ Error al marcar múltiples líneas");
    }
    
    // Test 9: Verificar estado después de marcar múltiples
    Serial.println("\n[TEST 9] Estado después de marcar múltiples líneas");
    if (fileManager.readLines(lines, MAX_LINES_TO_READ, lineCount)) {
        for (int i = 0; i < lineCount; i++) {
            Serial.print("  [");
            Serial.print(i);
            Serial.print("] ");
            Serial.println(lines[i]);
        }
    }
    
    // Test 10: Eliminar líneas procesadas
    Serial.println("\n[TEST 10] Eliminar líneas procesadas del archivo");
    if (fileManager.removeProcessedLines()) {
        Serial.println("✓ Líneas procesadas eliminadas");
        Serial.print("  Tamaño del archivo: ");
        Serial.print(fileManager.getFileSize());
        Serial.println(" bytes");
    } else {
        Serial.println("✗ Error al eliminar líneas procesadas");
    }
    
    // Test 11: Verificar contenido final
    Serial.println("\n[TEST 11] Contenido final del archivo");
    if (fileManager.readLines(lines, MAX_LINES_TO_READ, lineCount)) {
        Serial.print("✓ Líneas restantes: ");
        Serial.println(lineCount);
        for (int i = 0; i < lineCount; i++) {
            Serial.print("  [");
            Serial.print(i);
            Serial.print("] ");
            Serial.println(lines[i]);
        }
    }
    
    Serial.println("\n========================================");
    Serial.println("  FIN DE PRUEBAS");
    Serial.println("========================================");
}
