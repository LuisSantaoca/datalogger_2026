#pragma once

#include "type_def.h"

// 🆕 FIX-11: Feature flag para mejoras en health recovery (Fase 3)
#ifndef ENABLE_FIX11_HEALTH_IMPROVEMENTS
#define ENABLE_FIX11_HEALTH_IMPROVEMENTS true
#endif

extern modem_health_state_t g_modem_health_state;

void modemHealthReset();
void modemHealthRegisterTimeout(const char* contextTag);
bool modemHealthAttemptRecovery(const char* contextTag);
void modemHealthMarkOk();
bool modemHealthShouldAbortCycle(const char* contextTag);
