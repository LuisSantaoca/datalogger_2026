#include "gsm_comm_budget.h"
#include "gsmlte.h"
#include <esp_task_wdt.h>

static unsigned long g_cycle_start_ms = 0;
static bool g_cycle_budget_set = false;
static const char* FIX6_DEFAULT_CONTEXT = "contexto-desconocido";

void resetCommunicationCycleBudget() {
#if ENABLE_WDT_DEFENSIVE_LOOPS
  esp_task_wdt_reset();
#endif
  g_cycle_start_ms = millis();
  g_cycle_budget_set = true;
}

uint32_t remainingCommunicationCycleBudget() {
  if (!g_cycle_budget_set) {
    return COMM_CYCLE_BUDGET_MS;
  }

  unsigned long now = millis();
  unsigned long elapsed = now - g_cycle_start_ms;
  if (elapsed >= COMM_CYCLE_BUDGET_MS) {
    return 0;
  }
  return COMM_CYCLE_BUDGET_MS - elapsed;
}

bool ensureCommunicationBudget(const char* contextTag) {
  if (remainingCommunicationCycleBudget() > 0) {
    return true;
  }

  const char* tag = (contextTag && contextTag[0] != '\0') ? contextTag : FIX6_DEFAULT_CONTEXT;
  logMessage(1, String("[FIX-6] Presupuesto de ciclo agotado en ") + tag);
  return false;
}
