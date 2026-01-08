/**
 * @file FeatureFlags.h
 * @brief Sistema de Feature Flags para compilación condicional
 * @version FEAT-V1
 * @date 2026-01-07
 * 
 * Este archivo centraliza todas las banderas para habilitar o deshabilitar
 * fixes y features en tiempo de compilación.
 * 
 * USO:
 *   1 = Habilitado (código nuevo activo)
 *   0 = Deshabilitado (comportamiento original)
 * 
 * CONVENCIÓN DE NOMBRES:
 *   ENABLE_FIX_Vn_DESCRIPCION  - Para correcciones de bugs
 *   ENABLE_FEAT_Vn_DESCRIPCION - Para nuevas funcionalidades
 * 
 * ROLLBACK:
 *   Para revertir un fix, cambiar su valor de 1 a 0 y recompilar.
 *   El código original se mantiene en los bloques #else.
 */

#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

#include <Arduino.h>

// ============================================================
// FIX FLAGS - Correcciones de bugs
// ============================================================

/**
 * FIX-V1: Skip Reset en configureOperator()
 * Sistema: LTE/Modem
 * Archivo: LTEModule.cpp, LTEModule.h, AppController.cpp
 * Descripción: Evita resetModem() cuando ya hay operadora guardada,
 *              reduciendo eventos PDP de 3+ a 1 por ciclo.
 * Estado: Pendiente implementación
 */
#define ENABLE_FIX_V1_SKIP_RESET_PDP    0

/**
 * FIX-V2: [Reservado]
 * Sistema: [Por definir]
 * Descripción: [Por definir]
 */
#define ENABLE_FIX_V2_PLACEHOLDER       0

/**
 * FIX-V3: [Reservado]
 * Sistema: [Por definir]
 * Descripción: [Por definir]
 */
#define ENABLE_FIX_V3_PLACEHOLDER       0

// ============================================================
// FEAT FLAGS - Nuevas funcionalidades
// ============================================================

/**
 * FEAT-V2: [Reservado]
 * Sistema: [Por definir]
 * Descripción: [Por definir]
 */
#define ENABLE_FEAT_V2_PLACEHOLDER      0

// ============================================================
// FUNCIÓN DE DEBUG: Imprimir flags activos
// ============================================================

/**
 * @brief Imprime el estado de todos los feature flags al Serial
 * 
 * Útil para diagnóstico: permite saber qué fixes están activos
 * en el firmware compilado sin revisar el código fuente.
 */
inline void printActiveFlags() {
    Serial.println(F("=== FEATURE FLAGS ==="));
    
    // FIX Flags
    #if ENABLE_FIX_V1_SKIP_RESET_PDP
    Serial.println(F("  [X] FIX-V1: Skip Reset PDP"));
    #else
    Serial.println(F("  [ ] FIX-V1: Skip Reset PDP"));
    #endif
    
    #if ENABLE_FIX_V2_PLACEHOLDER
    Serial.println(F("  [X] FIX-V2: Placeholder"));
    #else
    Serial.println(F("  [ ] FIX-V2: Placeholder"));
    #endif
    
    #if ENABLE_FIX_V3_PLACEHOLDER
    Serial.println(F("  [X] FIX-V3: Placeholder"));
    #else
    Serial.println(F("  [ ] FIX-V3: Placeholder"));
    #endif
    
    // FEAT Flags
    #if ENABLE_FEAT_V2_PLACEHOLDER
    Serial.println(F("  [X] FEAT-V2: Placeholder"));
    #else
    Serial.println(F("  [ ] FEAT-V2: Placeholder"));
    #endif
    
    Serial.println(F("====================="));
}

#endif // FEATURE_FLAGS_H
