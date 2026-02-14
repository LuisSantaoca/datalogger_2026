#include "GPSModule.h"
#include <string.h>
#include <stdlib.h>

GPSModule::GPSModule(Stream& serial, uint8_t pwrKeyPin)
    : serial_(serial), pwrKeyPin_(pwrKeyPin) {}

bool GPSModule::powerOn(uint16_t attempts) {
  DEBUG_INFO(GPS, "Iniciando encendido del modulo GPS...");
  pinMode(pwrKeyPin_, OUTPUT);
  setPwrKeyIdle();
  DEBUG_VERBOSE(GPS, "Pin PWRKEY configurado como salida");

  // Verificar si ya esta encendido antes de togglear
  flushInput();
  if (waitAtReady(GPS_AT_READY_TIMEOUT_MS)) {
    DEBUG_INFO(GPS, "Modulo ya encendido y respondiendo AT OK");
    disablePSM();
    return true;
  }

  for (uint16_t i = 0; i < attempts; ++i) {
    DEBUG_VERBOSE(GPS, String("Intento de encendido ") + (i+1) + "/" + attempts);

    // Verificar antes de cada toggle para no apagar un modem que tardo en arrancar
    flushInput();
    if (waitAtReady(GPS_AT_READY_TIMEOUT_MS)) {
      DEBUG_INFO(GPS, "Modulo respondio antes de toggle, ya encendido");
      disablePSM();
      return true;
    }

    applyPwrKeySequence();
    flushInput();
    if (waitAtReady(GPS_AT_READY_TIMEOUT_MS)) {
      DEBUG_INFO(GPS, "Modulo encendido y respondiendo AT OK");
      disablePSM();
      return true;
    }
    DEBUG_WARN(GPS, String("Intento ") + (i+1) + " fallido, reintentando...");
  }

  DEBUG_ERROR(GPS, "Fallo al encender el modulo despues de todos los intentos");
  return false;
}

void GPSModule::disablePSM() {
  (void)sendCommand("AT+CPSMS=0");
  (void)waitForOk(1000);
  DEBUG_VERBOSE(GPS, "PSM deshabilitado");
}

bool GPSModule::powerOff() {
  DEBUG_INFO(GPS, "Apagando modulo GPS...");
  (void)gnssPowerOff();
  flushInput();
  (void)sendCommand(GPS_POWER_OFF_COMMAND);
  DEBUG_INFO(GPS, "Comando de apagado enviado, esperando confirmacion...");

  // Esperar URC "NORMAL POWER DOWN" (datasheet: Toff-on min 2s)
  char line[64];
  uint32_t start = millis();
  while (millis() - start < 5000) {
    if (readLine(line, sizeof(line), 500)) {
      if (strstr(line, "NORMAL POWER DOWN") != nullptr) {
        DEBUG_INFO(GPS, "Apagado confirmado (NORMAL POWER DOWN)");
        return true;
      }
    }
  }

  // Fallback: si no llego URC, esperar 3s para cumplir Toff-on >= 2s
  DEBUG_WARN(GPS, "No se recibio URC, esperando fallback 3s");
  delay(3000);
  return true;
}

bool GPSModule::getCoordinatesAndShutdown(GpsFix& fix, uint16_t retries) {
  DEBUG_INFO(GPS, "Iniciando obtencion de coordenadas GPS...");
  fix.hasFix = false;
  fix.latitude = 0.0f;
  fix.longitude = 0.0f;
  fix.altitude = 0.0f;

  if (!gnssPowerOn()) {
    DEBUG_ERROR(GPS, "No se pudo encender el GNSS");
    powerOff();
    return false;
  }
  DEBUG_INFO(GPS, "GNSS encendido, buscando satelites...");

  for (uint16_t i = 0; i < retries; ++i) {
    DEBUG_VERBOSE(GPS, String("Intento de lectura GPS ") + (i+1) + "/" + retries);
    if (requestCgnsinf(fix) && fix.hasFix) {
      DEBUG_INFO(GPS, "Fix GPS obtenido exitosamente!");
      DEBUG_INFO(GPS, String("  Latitud:  ") + fix.latitude + " deg");
      DEBUG_INFO(GPS, String("  Longitud: ") + fix.longitude + " deg");
      DEBUG_INFO(GPS, String("  Altitud:  ") + fix.altitude + " m");
      powerOff();
      return true;
    }
    DEBUG_VERBOSE(GPS, String("Sin fix, esperando ") + (GPS_GNSS_RETRY_DELAY_MS/1000.0f) + " segundos...");
    delay(GPS_GNSS_RETRY_DELAY_MS);
  }

  DEBUG_ERROR(GPS, String("No se obtuvo fix GPS despues de ") + retries + " intentos");
  powerOff();
  return false;
}

bool GPSModule::getLatitudeAsString(String& latStr, uint16_t retries) {
  DEBUG_INFO(GPS, "Obteniendo latitud GPS como string...");
  GpsFix fix;
  
  if (!gnssPowerOn()) {
    DEBUG_ERROR(GPS, "No se pudo encender el GNSS");
    powerOff();
    latStr = "";
    return false;
  }

  for (uint16_t i = 0; i < retries; ++i) {
    if (requestCgnsinf(fix) && fix.hasFix) {
      latStr = String(fix.latitude, 6);
      DEBUG_INFO(GPS, String("Latitud obtenida: ") + latStr + " deg");
      powerOff();
      return true;
    }
    delay(GPS_GNSS_RETRY_DELAY_MS);
  }

  latStr = "";
  powerOff();
  return false;
}

bool GPSModule::getLongitudeAsString(String& lonStr, uint16_t retries) {
  DEBUG_INFO(GPS, "Obteniendo longitud GPS como string...");
  GpsFix fix;
  
  if (!gnssPowerOn()) {
    DEBUG_ERROR(GPS, "No se pudo encender el GNSS");
    powerOff();
    lonStr = "";
    return false;
  }

  for (uint16_t i = 0; i < retries; ++i) {
    if (requestCgnsinf(fix) && fix.hasFix) {
      lonStr = String(fix.longitude, 6);
      DEBUG_INFO(GPS, String("Longitud obtenida: ") + lonStr + " deg");
      powerOff();
      return true;
    }
    delay(GPS_GNSS_RETRY_DELAY_MS);
  }

  lonStr = "";
  powerOff();
  return false;
}

bool GPSModule::getAltitudeAsString(String& altStr, uint16_t retries) {
  DEBUG_INFO(GPS, "Obteniendo altitud GPS como string...");
  GpsFix fix;
  
  if (!gnssPowerOn()) {
    DEBUG_ERROR(GPS, "No se pudo encender el GNSS");
    powerOff();
    altStr = "";
    return false;
  }

  for (uint16_t i = 0; i < retries; ++i) {
    if (requestCgnsinf(fix) && fix.hasFix) {
      altStr = String(fix.altitude, 2);
      DEBUG_INFO(GPS, String("Altitud obtenida: ") + altStr + " m");
      powerOff();
      return true;
    }
    delay(GPS_GNSS_RETRY_DELAY_MS);
  }

  altStr = "";
  powerOff();
  return false;
}

void GPSModule::setPwrKeyIdle() {
  bool idleLevel = GPS_PWRKEY_ACTIVE_HIGH ? LOW : HIGH;
  digitalWrite(pwrKeyPin_, idleLevel);
}

void GPSModule::applyPwrKeySequence() {
  bool activeLevel = GPS_PWRKEY_ACTIVE_HIGH ? HIGH : LOW;
  bool idleLevel = GPS_PWRKEY_ACTIVE_HIGH ? LOW : HIGH;

  DEBUG_VERBOSE(GPS, "Aplicando secuencia PWRKEY...");
  digitalWrite(pwrKeyPin_, idleLevel);
  delay(GPS_PWRKEY_PRE_DELAY_MS);

  DEBUG_VERBOSE(GPS, String("Pulso PWRKEY por ") + GPS_PWRKEY_PULSE_MS + " ms");
  digitalWrite(pwrKeyPin_, activeLevel);
  delay(GPS_PWRKEY_PULSE_MS);

  digitalWrite(pwrKeyPin_, idleLevel);
  DEBUG_VERBOSE(GPS, String("Esperando ") + GPS_PWRKEY_POST_DELAY_MS + " ms post-pulso");
  delay(GPS_PWRKEY_POST_DELAY_MS);
}

void GPSModule::flushInput() {
  while (serial_.available() > 0) {
    (void)serial_.read();
  }
}

bool GPSModule::waitAtReady(uint32_t timeoutMs) {
  DEBUG_VERBOSE(GPS, "Esperando respuesta AT del modulo...");
  uint32_t start = millis();
  char line[64];
  uint16_t attempts = 0;

  while (millis() - start < timeoutMs) {
    flushInput();
    (void)sendCommand("AT");
    attempts++;
    DEBUG_VERBOSE(GPS, String("Envio comando AT (intento ") + attempts + ")");

    uint32_t t0 = millis();
    while (millis() - t0 < 800) {
      if (readLine(line, sizeof(line), 250)) {
        DEBUG_VERBOSE(GPS, String("Recibido: ") + line);
        if (strcmp(line, "OK") == 0) {
          DEBUG_VERBOSE(GPS, "Respuesta AT OK recibida");
          return true;
        }
      }
    }

    delay(300);
  }

  DEBUG_WARN(GPS, "Timeout esperando AT OK");
  return false;
}

bool GPSModule::sendCommand(const char* command) {
  if (command == nullptr) {
    DEBUG_ERROR(GPS, "Comando nulo enviado a sendCommand");
    return false;
  }
  DEBUG_VERBOSE(GPS, String("TX: ") + command);
  serial_.print(command);
  serial_.print("\r\n");
  return true;
}

bool GPSModule::readLine(char* buffer, size_t bufferSize, uint32_t timeoutMs) {
  if (buffer == nullptr || bufferSize < 2) {
    return false;
  }

  size_t idx = 0;
  uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    while (serial_.available() > 0) {
      char c = static_cast<char>(serial_.read());
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        if (idx == 0) {
          continue;
        }
        buffer[idx] = '\0';
        return true;
      }
      if (idx < bufferSize - 1) {
        buffer[idx++] = c;
      }
    }
  }

  buffer[0] = '\0';
  return false;
}

bool GPSModule::waitForOk(uint32_t timeoutMs) {
  char line[128];
  uint32_t start = millis();

  while (millis() - start < timeoutMs) {
    if (!readLine(line, sizeof(line), timeoutMs)) {
      continue;
    }
    DEBUG_VERBOSE(GPS, String("RX: ") + line);
    if (strcmp(line, "OK") == 0) {
      return true;
    }
    if (strcmp(line, "ERROR") == 0) {
      DEBUG_WARN(GPS, "Respuesta ERROR del modulo");
      return false;
    }
  }

  DEBUG_WARN(GPS, "Timeout esperando OK");
  return false;
}

bool GPSModule::gnssPowerOn() {
  DEBUG_VERBOSE(GPS, "Encendiendo GNSS (AT+CGNSPWR=1)...");
  flushInput();
  if (!sendCommand("AT+CGNSPWR=1")) {
    DEBUG_ERROR(GPS, "Fallo al enviar comando CGNSPWR=1");
    return false;
  }
  bool result = waitForOk(GPS_AT_TIMEOUT_MS);
  if (result) {
    DEBUG_VERBOSE(GPS, "GNSS encendido correctamente");
  } else {
    DEBUG_ERROR(GPS, "GNSS no respondio OK");
  }
  return result;
}

bool GPSModule::gnssPowerOff() {
  DEBUG_VERBOSE(GPS, "Apagando GNSS (AT+CGNSPWR=0)...");
  flushInput();
  if (!sendCommand("AT+CGNSPWR=0")) {
    DEBUG_ERROR(GPS, "Fallo al enviar comando CGNSPWR=0");
    return false;
  }
  bool result = waitForOk(GPS_AT_TIMEOUT_MS);
  if (result) {
    DEBUG_VERBOSE(GPS, "GNSS apagado correctamente");
  } else {
    DEBUG_WARN(GPS, "GNSS no respondio OK al apagado");
  }
  return result;
}

bool GPSModule::requestCgnsinf(GpsFix& fix) {
  DEBUG_VERBOSE(GPS, "Solicitando informacion GNSS (AT+CGNSINF)...");
  flushInput();
  if (!sendCommand("AT+CGNSINF")) {
    DEBUG_ERROR(GPS, "Fallo al enviar comando CGNSINF");
    return false;
  }

  char line[220];
  uint32_t start = millis();

  while (millis() - start < GPS_AT_TIMEOUT_MS) {
    if (!readLine(line, sizeof(line), GPS_AT_TIMEOUT_MS)) {
      continue;
    }
    DEBUG_VERBOSE(GPS, String("RX: ") + line);
    if (strncmp(line, "+CGNSINF:", 9) == 0) {
      bool ok = parseCgnsinf(line, fix);
      (void)waitForOk(GPS_AT_TIMEOUT_MS);
      if (ok && fix.hasFix) {
        DEBUG_VERBOSE(GPS, "Respuesta CGNSINF parseada - Fix obtenido");
      } else if (ok && !fix.hasFix) {
        DEBUG_VERBOSE(GPS, "Respuesta CGNSINF parseada - Sin fix aun");
      } else {
        DEBUG_WARN(GPS, "Error al parsear CGNSINF");
      }
      return ok;
    }
    if (strcmp(line, "ERROR") == 0) {
      DEBUG_ERROR(GPS, "Respuesta ERROR al solicitar CGNSINF");
      return false;
    }
  }

  DEBUG_WARN(GPS, "Timeout esperando respuesta CGNSINF");
  return false;
}

bool GPSModule::isValidNumber(const char* s) const {
  if (s == nullptr || *s == '\0') {
    return false;
  }

  bool hasDigit = false;
  for (const char* p = s; *p != '\0'; ++p) {
    if (*p >= '0' && *p <= '9') {
      hasDigit = true;
      continue;
    }
    if (*p == '-' || *p == '+' || *p == '.') {
      continue;
    }
    return false;
  }
  return hasDigit;
}

bool GPSModule::parseCgnsinf(const char* line, GpsFix& fix) {
  DEBUG_VERBOSE(GPS, "Parseando respuesta CGNSINF...");
  const char* prefix = "+CGNSINF:";
  size_t prefixLen = strlen(prefix);

  if (strncmp(line, prefix, prefixLen) != 0) {
    DEBUG_ERROR(GPS, "Linea no comienza con +CGNSINF:");
    return false;
  }

  char temp[220];
  size_t n = strlen(line);
  if (n >= sizeof(temp)) {
    DEBUG_ERROR(GPS, "Linea CGNSINF demasiado larga");
    return false;
  }
  memcpy(temp, line, n + 1);

  char* p = temp + prefixLen;
  while (*p == ' ') {
    ++p;
  }

  uint8_t fieldIndex = 0;
  char* token = strtok(p, ",");

  const char* fixStatus = nullptr;
  const char* latStr = nullptr;
  const char* lonStr = nullptr;
  const char* altStr = nullptr;

  while (token != nullptr) {
    if (fieldIndex == 1) {
      fixStatus = token;
    } else if (fieldIndex == 3) {
      latStr = token;
    } else if (fieldIndex == 4) {
      lonStr = token;
    } else if (fieldIndex == 5) {
      altStr = token;
    }
    ++fieldIndex;
    token = strtok(nullptr, ",");
  }

  bool hasFix = (fixStatus != nullptr && atoi(fixStatus) == 1);

  if (hasFix && latStr && lonStr && altStr &&
      isValidNumber(latStr) && isValidNumber(lonStr) && isValidNumber(altStr)) {
    fix.hasFix = true;
    fix.latitude = static_cast<float>(atof(latStr));
    fix.longitude = static_cast<float>(atof(lonStr));
    fix.altitude = static_cast<float>(atof(altStr));
    return true;
  }

  fix.hasFix = false;
  fix.latitude = 0.0f;
  fix.longitude = 0.0f;
  fix.altitude = 0.0f;
  return true;
}
