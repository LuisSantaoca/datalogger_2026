#pragma once

#include "type_def.h"

extern modem_health_state_t g_modem_health_state;

void modemHealthReset();
void modemHealthRegisterTimeout(const char* contextTag);
bool modemHealthAttemptRecovery(const char* contextTag);
void modemHealthMarkOk();
bool modemHealthShouldAbortCycle(const char* contextTag);
