/**
 * @file CycleTiming.h
 * @brief Instrumentación de tiempos para análisis de rendimiento
 * @version FEAT-V2
 * @date 2026-01-08
 * 
 * Este archivo proporciona macros y estructuras para medir el tiempo
 * de ejecución de cada fase del ciclo de operación.
 * 
 * USO:
 *   TIMING_RESET()           - Iniciar medición de ciclo
 *   TIMING_START(fase)       - Marcar inicio de una fase
 *   TIMING_END(fase)         - Marcar fin y registrar duración
 *   TIMING_PRINT_SUMMARY()   - Imprimir resumen de tiempos
 * 
 * OVERHEAD:
 *   Cuando ENABLE_FEAT_V2_CYCLE_TIMING = 0, todas las macros
 *   se expanden a nada (zero overhead).
 */

#ifndef CYCLE_TIMING_H
#define CYCLE_TIMING_H

#include <Arduino.h>
#include "FeatureFlags.h"

// ============================================================
// ESTRUCTURA DE TIEMPOS
// ============================================================

/**
 * @brief Estructura para almacenar tiempos de cada fase del ciclo
 */
struct CycleTiming {
    unsigned long cycleStart;       // Timestamp inicio de ciclo
    unsigned long bleTime;          // Tiempo en modo BLE
    unsigned long sensorsTime;      // Lectura de sensores
    unsigned long nvsGpsTime;       // Recuperar GPS de NVS
    unsigned long gpsTime;          // Adquisición GPS
    unsigned long iccidTime;        // Obtener ICCID
    unsigned long buildFrameTime;   // Construcción de trama
    unsigned long bufferWriteTime;  // Escritura a buffer
    unsigned long sendLteTime;      // Ciclo LTE completo
    unsigned long ltePowerOn;       // Encender modem
    unsigned long lteConfig;        // Configurar operadora
    unsigned long lteAttach;        // Attach a red
    unsigned long ltePdp;           // Activar PDP
    unsigned long lteTcp;           // Abrir TCP
    unsigned long lteSend;          // Enviar datos
    unsigned long lteClose;         // Cerrar y apagar
    unsigned long compactBufferTime;// Compactar buffer
    unsigned long cycleTotal;       // Ciclo completo
};

// ============================================================
// MACROS DE INSTRUMENTACIÓN
// ============================================================

#if ENABLE_FEAT_V2_CYCLE_TIMING

// Variables temporales para cada fase (declaradas en scope local)
#define TIMING_VAR(phase) _timing_##phase##_start

/**
 * @brief Reinicia todos los contadores e inicia el ciclo
 * @param timing Referencia a estructura CycleTiming
 */
#define TIMING_RESET(timing) \
    do { \
        memset(&(timing), 0, sizeof(timing)); \
        (timing).cycleStart = millis(); \
    } while(0)

/**
 * @brief Marca el inicio de una fase
 * @param timing Referencia a estructura CycleTiming (no usado, para consistencia)
 * @param phase Nombre de la fase (sin comillas)
 */
#define TIMING_START(timing, phase) \
    unsigned long TIMING_VAR(phase) = millis()

/**
 * @brief Marca el fin de una fase y guarda la duración
 * @param timing Referencia a estructura CycleTiming
 * @param phase Nombre de la fase (debe coincidir con campo de CycleTiming: sensorsTime -> sensors)
 */
#define TIMING_END(timing, phase) \
    do { \
        (timing).phase##Time = millis() - TIMING_VAR(phase); \
        Serial.print(F("[TIMING] ")); \
        Serial.print(F(#phase)); \
        Serial.print(F(": ")); \
        Serial.print((timing).phase##Time); \
        Serial.println(F(" ms")); \
    } while(0)

/**
 * @brief Marca el fin de una fase sin imprimir (para sub-fases)
 * @param timing Referencia a estructura CycleTiming
 * @param phase Nombre de la fase
 */
#define TIMING_END_SILENT(timing, phase) \
    do { \
        (timing).phase##Time = millis() - TIMING_VAR(phase); \
    } while(0)

/**
 * @brief Calcula y guarda el tiempo total del ciclo
 * @param timing Referencia a estructura CycleTiming
 */
#define TIMING_FINALIZE(timing) \
    do { \
        (timing).cycleTotal = millis() - (timing).cycleStart; \
    } while(0)

/**
 * @brief Imprime resumen completo de tiempos
 * @param timing Referencia a estructura CycleTiming
 */
#define TIMING_PRINT_SUMMARY(timing) printTimingSummary(timing)

#else // ENABLE_FEAT_V2_CYCLE_TIMING == 0

// Macros vacías cuando está deshabilitado (zero overhead)
#define TIMING_RESET(timing)
#define TIMING_START(timing, phase)
#define TIMING_END(timing, phase)
#define TIMING_END_SILENT(timing, phase)
#define TIMING_FINALIZE(timing)
#define TIMING_PRINT_SUMMARY(timing)

#endif // ENABLE_FEAT_V2_CYCLE_TIMING

// ============================================================
// FUNCIÓN DE RESUMEN
// ============================================================

#if ENABLE_FEAT_V2_CYCLE_TIMING

/**
 * @brief Imprime un resumen formateado de todos los tiempos medidos
 * @param timing Referencia a estructura CycleTiming
 */
inline void printTimingSummary(const CycleTiming& timing) {
    Serial.println(F(""));
    Serial.println(F("╔══════════════════════════════════════╗"));
    Serial.println(F("║       CYCLE TIMING SUMMARY           ║"));
    Serial.println(F("╠══════════════════════════════════════╣"));
    
    Serial.print(F("║  BLE:          "));
    Serial.printf("%6lu", timing.bleTime);
    Serial.println(F(" ms         ║"));
    
    Serial.print(F("║  Sensors:      "));
    Serial.printf("%6lu", timing.sensorsTime);
    Serial.println(F(" ms         ║"));
    
    Serial.print(F("║  NVS GPS:      "));
    Serial.printf("%6lu", timing.nvsGpsTime);
    Serial.println(F(" ms         ║"));
    
    Serial.print(F("║  GPS:          "));
    Serial.printf("%6lu", timing.gpsTime);
    Serial.println(F(" ms         ║"));
    
    Serial.print(F("║  ICCID:        "));
    Serial.printf("%6lu", timing.iccidTime);
    Serial.println(F(" ms         ║"));
    
    Serial.print(F("║  Build Frame:  "));
    Serial.printf("%6lu", timing.buildFrameTime);
    Serial.println(F(" ms         ║"));
    
    Serial.print(F("║  Buffer Write: "));
    Serial.printf("%6lu", timing.bufferWriteTime);
    Serial.println(F(" ms         ║"));
    
    Serial.print(F("║  Send LTE:     "));
    Serial.printf("%6lu", timing.sendLteTime);
    Serial.println(F(" ms         ║"));
    
    Serial.print(F("║  Compact:      "));
    Serial.printf("%6lu", timing.compactBufferTime);
    Serial.println(F(" ms         ║"));
    
    Serial.println(F("╠══════════════════════════════════════╣"));
    
    Serial.print(F("║  CYCLE TOTAL:  "));
    Serial.printf("%6lu", timing.cycleTotal);
    Serial.print(F(" ms ("));
    Serial.printf("%.1f", timing.cycleTotal / 1000.0);
    Serial.println(F(" s) ║"));
    
    Serial.println(F("╚══════════════════════════════════════╝"));
    Serial.println(F(""));
}

#endif // ENABLE_FEAT_V2_CYCLE_TIMING

#endif // CYCLE_TIMING_H
