/**
 * @file FeatureFlags.h
 * @brief Feature flags para control de fixes y features.
 * 
 * Cada fix/feature tiene un flag independiente que permite:
 * - Activar/desactivar cambios individualmente
 * - Rollback instantáneo poniendo el flag a 0
 * - El bloque #else siempre preserva el código original
 * 
 * Nomenclatura: ENABLE_FIX_Vn_DESCRIPCION o ENABLE_FEAT_Vn_DESCRIPCION
 * Valores: 1 = habilitado, 0 = rollback al comportamiento original
 */

#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

#include <Arduino.h>

// ============================================================
// FIX-V1: Estandarización de logging en LTEModule
// Reemplaza debugPrint() propietario por macros DEBUG_* de DebugConfig.h
// Archivos: LTEModule.cpp, LTEModule.h, AppController.cpp
// ============================================================
#define ENABLE_FIX_V1_LTE_STANDARD_LOGGING  1

// ============================================================
// FIX-V2: Hard power cycle del modem vía control de VBAT
// Agrega hardPowerCycle() como fallback cuando PWRKEY falla
// Archivos: LTEModule.cpp, LTEModule.h, config_data_lte.h
// ============================================================
#define ENABLE_FIX_V2_HARD_POWER_CYCLE      1

// ============================================================
// Función de diagnóstico: imprime flags activos al boot
// ============================================================
inline void printActiveFlags() {
    Serial.println(F("=== Feature Flags Activos ==="));

    #if ENABLE_FIX_V1_LTE_STANDARD_LOGGING
    Serial.println(F("  [ON]  FIX-V1: LTE Standard Logging"));
    #else
    Serial.println(F("  [OFF] FIX-V1: LTE Standard Logging"));
    #endif

    #if ENABLE_FIX_V2_HARD_POWER_CYCLE
    Serial.println(F("  [ON]  FIX-V2: Hard Power Cycle"));
    #else
    Serial.println(F("  [OFF] FIX-V2: Hard Power Cycle"));
    #endif

    Serial.println(F("============================="));
}

#endif // FEATURE_FLAGS_H
