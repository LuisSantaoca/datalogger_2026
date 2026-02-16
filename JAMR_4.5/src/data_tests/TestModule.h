/**
 * @file TestModule.h
 * @brief Sistema de tests minimalista ejecutable via serial
 * @version FEAT-V8
 * @date 2026-01-28
 * 
 * Sistema de testing ligero con 4 tests críticos:
 * - TEST_CRC: Validación CRC16 con vectores conocidos
 * - TEST_BAT: FSM de batería (transiciones de estado)
 * - TEST_COUNT: Contadores ProductionDiag (incrementos)
 * - TEST_PARSE: Parsing de respuestas AT del módem
 * 
 * USO:
 *   1. Compilar firmware con ENABLE_FEAT_V8_TESTING = 1
 *   2. Abrir monitor serial (115200 baud)
 *   3. Escribir comando: TEST_CRC, TEST_BAT, TEST_COUNT, TEST_PARSE
 *   4. Ver resultado: ✓ PASS o ✗ FAIL con detalles
 * 
 * VENTAJAS:
 *   - Sin recompilar: tests disponibles en firmware producción
 *   - Ejecutable en campo via CoolTerm
 *   - Overhead mínimo cuando feature flag = 0
 */

#ifndef TEST_MODULE_H
#define TEST_MODULE_H

#include <Arduino.h>
#include "config_tests.h"

/**
 * @namespace TestModule
 * @brief Funciones de testing minimalista
 */
namespace TestModule {

/**
 * @brief Procesa comandos de test desde serial
 * @param serial Stream para entrada/salida
 * @return true si procesó un comando, false si no hay entrada
 */
bool processCommand(Stream* serial);

/**
 * @brief Imprime ayuda de comandos disponibles
 * @param serial Stream de salida
 */
void printHelp(Stream* serial);

/**
 * @brief Test 1: Validación CRC16 con vectores conocidos
 * @param serial Stream de salida
 */
void testCRC16(Stream* serial);

/**
 * @brief Test 2: FSM de batería (transiciones)
 * @param serial Stream de salida
 */
void testBatteryFSM(Stream* serial);

/**
 * @brief Test 3: Contadores ProductionDiag
 * @param serial Stream de salida
 */
void testCounters(Stream* serial);

/**
 * @brief Test 4: Parsing de respuestas AT
 * @param serial Stream de salida
 */
void testParsing(Stream* serial);

} // namespace TestModule

#endif // TEST_MODULE_H
