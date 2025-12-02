#include "gsm_health.h"
#include "gsmlte.h"
#include <esp_task_wdt.h>

static uint8_t g_modem_timeouts_critical = 0; // cuenta timeouts en operaciones críticas (CNACT, CAOPEN, envío TCP)
static bool g_modem_recovery_attempted = false;
static const char* FIX8_DEFAULT_CONTEXT = "FIX-8";

modem_health_state_t g_modem_health_state = MODEM_HEALTH_OK;

void modemHealthReset() {
  g_modem_health_state = MODEM_HEALTH_OK;
  g_modem_timeouts_critical = 0;
  g_modem_recovery_attempted = false;
}

void modemHealthRegisterTimeout(const char* contextTag) {
  g_modem_timeouts_critical++;

  if (g_modem_timeouts_critical == 1) {
    logMessage(2, String("[FIX-8] Primer timeout crítico detectado en ") + contextTag);
    g_modem_health_state = MODEM_HEALTH_TRYING;
  } else if (g_modem_timeouts_critical >= 3 && g_modem_health_state != MODEM_HEALTH_FAILED) {
    logMessage(1, String("[FIX-8] Múltiples timeouts críticos (") + g_modem_timeouts_critical + ") en " + contextTag + ", posible estado zombie");
    g_modem_health_state = MODEM_HEALTH_ZOMBIE_DETECTED;
  }
}

bool modemHealthAttemptRecovery(const char* contextTag) {
  const char* tag = (contextTag && contextTag[0] != '\0') ? contextTag : FIX8_DEFAULT_CONTEXT;

  if (g_modem_recovery_attempted) {
    logMessage(1, String("[FIX-8] Recuperación profunda ya ejecutada; se mantiene estado de fallo en ") + tag);
    return false;
  }

  if (!ensureCommunicationBudget("FIX8_recovery_begin")) {
    logMessage(1, "[FIX-8] Sin presupuesto global para intento de recuperación profunda");
    return false;
  }

  g_modem_recovery_attempted = true;
  logMessage(1, String("[FIX-8] Intentando recuperación profunda tras detectar estado zombie en ") + tag);

  bool ok = true;

  if (isSocketOpen()) {
    logMessage(2, "[FIX-8] Cerrando socket antes de la recuperación profunda");
    ok = tcpClose() && ok;
    delay(200);
  }

  if (!sendATCommand("+CNACT=0,0", "OK", 10000)) {
    logMessage(1, "[FIX-8] Fallo desactivando PDP durante recuperación");
    ok = false;
  }

  if (!sendATCommand("+CFUN=0", "OK", 10000)) {
    logMessage(1, "[FIX-8] Fallo apagando RF durante recuperación");
    ok = false;
  }

  for (uint8_t i = 0; i < 6; ++i) {
    delay(250);
    esp_task_wdt_reset();
  }

  if (!sendATCommand("+CFUN=1", "OK", 10000)) {
    logMessage(1, "[FIX-8] Fallo reactivando RF durante recuperación");
    ok = false;
  }

  if (ok) {
    g_modem_health_state = MODEM_HEALTH_TRYING;
    g_modem_timeouts_critical = 0;
    logMessage(2, "[FIX-8] Recuperación profunda completada; se reintentará flujo de comunicación");
  } else {
    g_modem_health_state = MODEM_HEALTH_FAILED;
    logMessage(0, "[FIX-8] Recuperación profunda falló, marcando ciclo como fallo");
  }

  return ok;
}

void modemHealthMarkOk() {
  if (g_modem_health_state == MODEM_HEALTH_TRYING || g_modem_health_state == MODEM_HEALTH_ZOMBIE_DETECTED) {
    g_modem_health_state = MODEM_HEALTH_RECOVERED;
    logMessage(2, "[FIX-8] Módem recuperado tras timeouts previos");
  } else {
    g_modem_health_state = MODEM_HEALTH_OK;
  }
  g_modem_timeouts_critical = 0;
}

bool modemHealthShouldAbortCycle(const char* contextTag) {
  const char* tag = (contextTag && contextTag[0] != '\0') ? contextTag : FIX8_DEFAULT_CONTEXT;

  if (g_modem_health_state == MODEM_HEALTH_ZOMBIE_DETECTED && g_modem_timeouts_critical >= 3) {
    logMessage(1, String("[FIX-8] Estado zombie confirmado tras múltiples timeouts en ") + tag);

    if (modemHealthAttemptRecovery(tag)) {
      logMessage(2, "[FIX-8] Recuperación profunda ejecutada; continuando con presupuesto remanente");
      return false;
    }

    g_modem_health_state = MODEM_HEALTH_FAILED;
    logMessage(0, "[FIX-8] Recuperación fallida, se abortará ciclo");
    return true;
  }

  if (g_modem_health_state == MODEM_HEALTH_FAILED) {
    logMessage(0, String("[FIX-8] Ciclo ya marcado como fallo definitivo en ") + tag);
    return true;
  }

  return false;
}
