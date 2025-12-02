#pragma once

#include <stdint.h>

void resetCommunicationCycleBudget();
uint32_t remainingCommunicationCycleBudget();
bool ensureCommunicationBudget(const char* contextTag);
