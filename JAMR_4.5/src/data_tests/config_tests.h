/**
 * @file config_tests.h
 * @brief Configuración para sistema de tests minimalista
 * @version FEAT-V8
 * @date 2026-01-28
 */

#ifndef CONFIG_TESTS_H
#define CONFIG_TESTS_H

/** @brief Timeout para comandos de test (ms) */
constexpr uint32_t TEST_COMMAND_TIMEOUT = 500;

/** @brief Buffer para entrada serial */
constexpr uint8_t TEST_CMD_BUFFER_SIZE = 32;

#endif // CONFIG_TESTS_H
