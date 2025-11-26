/**
 * @file timedata.cpp
 * @brief Implementación del sistema de gestión de tiempo con RTC DS3231
 * @author Elathia
 * @date 2025
 */

#include "timedata.h"
#include "RTClib.h"

const int RTC_INIT_RETRIES = 3;
const int RTC_READ_RETRIES = 3;
const int RTC_STABILIZE_DELAY = 2000;
const int RTC_RETRY_DELAY = 500;
const int RTC_BATTERY_CHECK_DELAY = 100;

RTC_DS3231 rtc;
bool rtcInitialized = false;
bool rtcBatteryOK = false;
int rtcInitErrors = 0;
int rtcReadErrors = 0;
unsigned long lastRtcRead = 0;

void logTimeMessage(int level, const String& message) {
  String timestamp = String(millis()) + "ms";
  String levelStr;
  
  switch (level) {
    case 0: levelStr = "❌ TIME_ERROR"; break;
    case 1: levelStr = "⚠️  TIME_WARN"; break;
    case 2: levelStr = "ℹ️  TIME_INFO"; break;
    case 3: levelStr = "🔍 TIME_DEBUG"; break;
    default: levelStr = "❓ TIME_UNKN"; break;
  }
  
  Serial.println("[" + timestamp + "] " + levelStr + ": " + message);
}

bool validateTimeValues(int year, int month, int day, int hour, int minute, int second) {
  if (year < 2020 || year > 2030) {
    logTimeMessage(1, "⚠️  Año fuera de rango: " + String(year));
    return false;
  }
  
  if (month < 1 || month > 12) {
    logTimeMessage(1, "⚠️  Mes fuera de rango: " + String(month));
    return false;
  }
  
  int maxDays = 31;
  if (month == 4 || month == 6 || month == 9 || month == 11) {
    maxDays = 30;
  } else if (month == 2) {
    maxDays = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 29 : 28;
  }
  
  if (day < 1 || day > maxDays) {
    logTimeMessage(1, "⚠️  Día fuera de rango para mes " + String(month) + ": " + String(day));
    return false;
  }
  
  if (hour < 0 || hour > 23) {
    logTimeMessage(1, "⚠️  Hora fuera de rango: " + String(hour));
    return false;
  }
  
  if (minute < 0 || minute > 59) {
    logTimeMessage(1, "⚠️  Minuto fuera de rango: " + String(minute));
    return false;
  }
  
  if (second < 0 || second > 59) {
    logTimeMessage(1, "⚠️  Segundo fuera de rango: " + String(second));
    return false;
  }
  
  return true;
}

bool checkRtcBattery() {
  if (rtc.lostPower()) {
    logTimeMessage(1, "⚠️  RTC perdió energía - Batería baja o desconectada");
    rtcBatteryOK = false;
    return false;
  }
  
  rtcBatteryOK = true;
  logTimeMessage(3, "🔋 RTC - Batería en buen estado");
  return true;
}

bool setupTimeData(sensordata_type* data) {
  logTimeMessage(2, "🕐 Iniciando configuración del sistema de tiempo");
  
  digitalWrite(GPIO_NUM_7, LOW);
  digitalWrite(GPIO_NUM_3, HIGH);
  
  logTimeMessage(3, "⚡ Energía activada para RTC");
  delay(RTC_STABILIZE_DELAY);
  
  bool rtcInitSuccess = false;
  
  for (int attempt = 1; attempt <= RTC_INIT_RETRIES; attempt++) {
    logTimeMessage(3, "🔄 RTC - Intento de inicialización " + String(attempt) + " de " + String(RTC_INIT_RETRIES));
    
    if (rtc.begin()) {
      rtcInitialized = true;
      rtcInitSuccess = true;
      logTimeMessage(2, "✅ RTC inicializado exitosamente en intento " + String(attempt));
      break;
    } else {
      rtcInitErrors++;
      logTimeMessage(1, "⚠️  RTC - Fallo en inicialización, intento " + String(attempt));
      
      if (attempt < RTC_INIT_RETRIES) {
        delay(RTC_RETRY_DELAY * attempt);
      }
    }
  }
  
  if (!rtcInitSuccess) {
    logTimeMessage(0, "❌ RTC - Fallo en inicialización después de " + String(RTC_INIT_RETRIES) + " intentos");
    digitalWrite(GPIO_NUM_7, HIGH);
    digitalWrite(GPIO_NUM_3, LOW);
    return false;
  }
  
  delay(RTC_BATTERY_CHECK_DELAY);
  checkRtcBattery();
  
  bool timeReadSuccess = false;
  DateTime now;
  
  for (int attempt = 1; attempt <= RTC_READ_RETRIES; attempt++) {
    logTimeMessage(3, "🔄 RTC - Intento de lectura " + String(attempt) + " de " + String(RTC_READ_RETRIES));
    
    now = rtc.now();
    
    if (now.year() > 2000) {
      if (validateTimeValues(now.year(), now.month(), now.day(), 
                           now.hour(), now.minute(), now.second())) {
        timeReadSuccess = true;
        logTimeMessage(2, "✅ Tiempo leído exitosamente en intento " + String(attempt));
        break;
      } else {
        logTimeMessage(1, "⚠️  RTC - Valores de tiempo inválidos en intento " + String(attempt));
      }
    } else {
      logTimeMessage(1, "⚠️  RTC - Lectura de tiempo fallida en intento " + String(attempt));
    }
    
    rtcReadErrors++;
    
    if (attempt < RTC_READ_RETRIES) {
      delay(RTC_RETRY_DELAY * attempt);
    }
  }
  
  if (!timeReadSuccess) {
    logTimeMessage(0, "❌ RTC - Fallo en lectura de tiempo después de " + String(RTC_READ_RETRIES) + " intentos");
    digitalWrite(GPIO_NUM_7, HIGH);
    digitalWrite(GPIO_NUM_3, LOW);
    return false;
  }
  
  data->H_ano = (byte)(now.year() >> 8);
  data->L_ano = (byte)(now.year() & 0xFF);
  data->H_mes = (byte)now.month();
  data->H_dia = (byte)now.day();
  data->H_hora = (byte)now.hour();
  data->H_minuto = (byte)now.minute();
  data->H_segundo = (byte)now.second();
  
  lastRtcRead = millis();
  
  logTimeMessage(2, "📅 Fecha y hora configuradas:");
  logTimeMessage(2, "   " + String(now.year()) + "/" + 
                String(now.month(), DEC) + "/" + 
                String(now.day(), DEC) + " " +
                String(now.hour(), DEC) + ":" + 
                String(now.minute(), DEC) + ":" + 
                String(now.second(), DEC));
  
  logTimeMessage(2, "📊 Estado del RTC:");
  logTimeMessage(2, "   Inicializado: " + String(rtcInitialized ? "✅ SI" : "❌ NO"));
  logTimeMessage(2, "   Batería: " + String(rtcBatteryOK ? "✅ OK" : "⚠️  BAJA"));
  logTimeMessage(2, "   Errores de init: " + String(rtcInitErrors));
  logTimeMessage(2, "   Errores de lectura: " + String(rtcReadErrors));
  
  digitalWrite(GPIO_NUM_7, HIGH);
  digitalWrite(GPIO_NUM_3, LOW);
  
  logTimeMessage(2, "✅ Sistema de tiempo configurado exitosamente");
  return true;
}
String getTimeSystemStats() {
  String stats = "=== ESTADÍSTICAS DEL SISTEMA DE TIEMPO ===\n";
  
  stats += "RTC inicializado: " + String(rtcInitialized ? "SI" : "NO") + "\n";
  stats += "Batería RTC: " + String(rtcBatteryOK ? "OK" : "BAJA") + "\n";
  stats += "Errores de inicialización: " + String(rtcInitErrors) + "\n";
  stats += "Errores de lectura: " + String(rtcReadErrors) + "\n";
  stats += "Última lectura: " + String(lastRtcRead) + "ms\n";
  
  if (rtcInitialized) {
    DateTime now = rtc.now();
    stats += "Tiempo actual: " + String(now.year()) + "/" + 
             String(now.month()) + "/" + String(now.day()) + " " +
             String(now.hour()) + ":" + String(now.minute()) + ":" + 
             String(now.second()) + "\n";
  }
  
  return stats;
}

/**
 * Verifica la integridad del sistema de tiempo
 * Realiza una lectura de prueba para validar el funcionamiento
 * 
 * @return true si el sistema funciona correctamente
 */
bool verifyTimeSystem() {
  if (!rtcInitialized) {
    logTimeMessage(1, "⚠️  RTC no inicializado");
    return false;
  }
  
  // Activar energía temporalmente para verificación
  digitalWrite(GPIO_NUM_7, LOW);
  digitalWrite(GPIO_NUM_3, HIGH);
  delay(100); // Pausa mínima
  
  DateTime now = rtc.now();
  bool isValid = (now.year() > 2000);
  
  // Restaurar modo de energía
  digitalWrite(GPIO_NUM_7, HIGH);
  digitalWrite(GPIO_NUM_3, LOW);
  
  if (isValid) {
    logTimeMessage(2, "✅ Verificación del sistema de tiempo exitosa");
  } else {
    logTimeMessage(1, "⚠️  Verificación del sistema de tiempo fallida");
  }
  
  return isValid;
}

void resetTimeErrorCounters() {
  rtcInitErrors = 0;
  rtcReadErrors = 0;
  logTimeMessage(2, "🔄 Contadores de error del sistema de tiempo reseteados");
}

String getCurrentTimeString() {
  if (!rtcInitialized) {
    return "RTC no inicializado";
  }
  
  digitalWrite(GPIO_NUM_7, LOW);
  digitalWrite(GPIO_NUM_3, HIGH);
  delay(50);
  
  DateTime now = rtc.now();
  
  digitalWrite(GPIO_NUM_7, HIGH);
  digitalWrite(GPIO_NUM_3, LOW);
  
  return String(now.year()) + "/" + 
         String(now.month(), DEC) + "/" + 
         String(now.day(), DEC) + " " +
         String(now.hour(), DEC) + ":" + 
         String(now.minute(), DEC) + ":" + 
         String(now.second(), DEC);
}