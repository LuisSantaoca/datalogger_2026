// =============================================================================
// ARCHIVO: timedata.cpp
// DESCRIPCIÓN: Sistema de gestión de tiempo y fecha para monitoreo ambiental
// FUNCIONALIDADES:
//   - Lectura de RTC DS3231 de alta precisión
//   - Validación robusta de datos de tiempo
//   - Sistema de reintentos automáticos
//   - Logging estructurado y manejo de errores
//   - Control inteligente de energía
//   - Verificación de integridad del RTC
// =============================================================================

#include "timedata.h"
#include "RTClib.h"

// =============================================================================
// CONSTANTES DE CONFIGURACIÓN
// =============================================================================

const int RTC_INIT_RETRIES = 3;           // Número máximo de intentos de inicialización
const int RTC_READ_RETRIES = 3;           // Número máximo de intentos de lectura
const int RTC_STABILIZE_DELAY = 2000;     // Tiempo de estabilización del RTC (ms)
const int RTC_RETRY_DELAY = 500;          // Delay entre reintentos (ms)
const int RTC_BATTERY_CHECK_DELAY = 100;  // Delay para verificación de batería (ms)

// =============================================================================
// INSTANCIAS DE OBJETOS
// =============================================================================

RTC_DS3231 rtc;                           // Módulo RTC DS3231 de alta precisión

// =============================================================================
// VARIABLES DE ESTADO
// =============================================================================

bool rtcInitialized = false;               // Estado de inicialización del RTC
bool rtcBatteryOK = false;                 // Estado de la batería del RTC
int rtcInitErrors = 0;                     // Contador de errores de inicialización
int rtcReadErrors = 0;                     // Contador de errores de lectura
unsigned long lastRtcRead = 0;             // Timestamp de la última lectura exitosa

// =============================================================================
// FUNCIONES DE LOGGING Y DIAGNÓSTICO
// =============================================================================

/**
 * Sistema de logging estructurado para el módulo de tiempo
 * Proporciona diferentes niveles de logging con timestamps y emojis
 * 
 * @param level - Nivel de log (0=Error, 1=Warning, 2=Info, 3=Debug)
 * @param message - Mensaje a loguear
 */
void logTimeMessage(int level, const String& message) {
  String timestamp = String(millis()) + "ms";
  String levelStr;
  
  switch (level) {
    case 0: levelStr = "❌ TIME_ERROR"; break;    // Errores críticos
    case 1: levelStr = "⚠️  TIME_WARN"; break;    // Advertencias
    case 2: levelStr = "ℹ️  TIME_INFO"; break;    // Información general
    case 3: levelStr = "🔍 TIME_DEBUG"; break;    // Debug detallado
    default: levelStr = "❓ TIME_UNKN"; break;    // Nivel desconocido
  }
  
  Serial.println("[" + timestamp + "] " + levelStr + ": " + message);
}

/**
 * Valida que los valores de tiempo estén en rangos razonables
 * Verifica que la fecha y hora sean coherentes y válidas
 * 
 * @param year - Año a validar
 * @param month - Mes a validar (1-12)
 * @param day - Día a validar (1-31)
 * @param hour - Hora a validar (0-23)
 * @param minute - Minuto a validar (0-59)
 * @param second - Segundo a validar (0-59)
 * @return true si todos los valores son válidos
 */
bool validateTimeValues(int year, int month, int day, int hour, int minute, int second) {
  // Validar año (rango razonable: 2020-2030)
  if (year < 2020 || year > 2030) {
    logTimeMessage(1, "⚠️  Año fuera de rango: " + String(year));
    return false;
  }
  
  // Validar mes (1-12)
  if (month < 1 || month > 12) {
    logTimeMessage(1, "⚠️  Mes fuera de rango: " + String(month));
    return false;
  }
  
  // Validar día (1-31, considerando el mes)
  int maxDays = 31;
  if (month == 4 || month == 6 || month == 9 || month == 11) {
    maxDays = 30;
  } else if (month == 2) {
    // Febrero: considerar años bisiestos
    maxDays = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) ? 29 : 28;
  }
  
  if (day < 1 || day > maxDays) {
    logTimeMessage(1, "⚠️  Día fuera de rango para mes " + String(month) + ": " + String(day));
    return false;
  }
  
  // Validar hora (0-23)
  if (hour < 0 || hour > 23) {
    logTimeMessage(1, "⚠️  Hora fuera de rango: " + String(hour));
    return false;
  }
  
  // Validar minuto (0-59)
  if (minute < 0 || minute > 59) {
    logTimeMessage(1, "⚠️  Minuto fuera de rango: " + String(minute));
    return false;
  }
  
  // Validar segundo (0-59)
  if (second < 0 || second > 59) {
    logTimeMessage(1, "⚠️  Segundo fuera de rango: " + String(second));
    return false;
  }
  
  return true;
}

/**
 * Verifica el estado de la batería del RTC
 * Comprueba si el módulo RTC tiene batería de respaldo funcional
 * 
 * @return true si la batería está en buen estado
 */
bool checkRtcBattery() {
  // El DS3231 tiene un bit de estado de batería
  // Si la batería está baja, el bit se activa
  if (rtc.lostPower()) {
    logTimeMessage(1, "⚠️  RTC perdió energía - Batería baja o desconectada");
    rtcBatteryOK = false;
    return false;
  }
  
  rtcBatteryOK = true;
  logTimeMessage(3, "🔋 RTC - Batería en buen estado");
  return true;
}

// =============================================================================
// FUNCIÓN PRINCIPAL DE CONFIGURACIÓN DE TIEMPO
// =============================================================================

/**
 * Configura e inicializa el sistema de tiempo con manejo robusto de errores
 * Implementa inicialización del RTC con reintentos automáticos y validación
 * 
 * @param data - Estructura de datos donde almacenar la información de tiempo
 * @return true si la inicialización es exitosa
 */
bool setupTimeData(sensordata_type* data) {
  logTimeMessage(2, "🕐 Iniciando configuración del sistema de tiempo");
  
  // =============================================================================
  // CONTROL DE ENERGÍA Y ESTABILIZACIÓN
  // =============================================================================
  
  // Activar fuente de alimentación para el RTC
  digitalWrite(GPIO_NUM_7, LOW);   // Desactivar modo de bajo consumo
  digitalWrite(GPIO_NUM_3, HIGH);  // Activar fuente de alimentación
  
  logTimeMessage(3, "⚡ Energía activada para RTC");
  
  // Esperar estabilización del sistema
  delay(RTC_STABILIZE_DELAY);
  
  // =============================================================================
  // INICIALIZACIÓN DEL RTC CON REINTENTOS
  // =============================================================================
  
  bool rtcInitSuccess = false;
  
  for (int attempt = 1; attempt <= RTC_INIT_RETRIES; attempt++) {
    logTimeMessage(3, "🔄 RTC - Intento de inicialización " + String(attempt) + " de " + String(RTC_INIT_RETRIES));
    
    // Intentar inicializar el RTC
    if (rtc.begin()) {
      rtcInitialized = true;
      rtcInitSuccess = true;
      logTimeMessage(2, "✅ RTC inicializado exitosamente en intento " + String(attempt));
      break;
    } else {
      rtcInitErrors++;
      logTimeMessage(1, "⚠️  RTC - Fallo en inicialización, intento " + String(attempt));
      
      // Si no es el último intento, esperar antes del siguiente
      if (attempt < RTC_INIT_RETRIES) {
        delay(RTC_RETRY_DELAY * attempt); // Delay progresivo
      }
    }
  }
  
  if (!rtcInitSuccess) {
    logTimeMessage(0, "❌ RTC - Fallo en inicialización después de " + String(RTC_INIT_RETRIES) + " intentos");
    
    // Restaurar modo de energía
    digitalWrite(GPIO_NUM_7, HIGH);  // Reactivar modo bajo consumo
    digitalWrite(GPIO_NUM_3, LOW);   // Desactivar fuente de alimentación
    
    return false;
  }
  
  // =============================================================================
  // VERIFICACIÓN DE BATERÍA Y ESTADO
  // =============================================================================
  
  delay(RTC_BATTERY_CHECK_DELAY); // Pequeña pausa para estabilización
  checkRtcBattery();
  
  // =============================================================================
  // LECTURA DE TIEMPO CON VALIDACIÓN
  // =============================================================================
  
  bool timeReadSuccess = false;
  DateTime now;
  
  for (int attempt = 1; attempt <= RTC_READ_RETRIES; attempt++) {
    logTimeMessage(3, "🔄 RTC - Intento de lectura " + String(attempt) + " de " + String(RTC_READ_RETRIES));
    
    // Leer tiempo actual del RTC
    now = rtc.now();
    
    // Validar que la lectura sea exitosa (año > 2000 indica lectura válida)
    if (now.year() > 2000) {
      // Validar que los valores estén en rangos razonables
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
    
    // Si no es el último intento, esperar antes del siguiente
    if (attempt < RTC_READ_RETRIES) {
      delay(RTC_RETRY_DELAY * attempt); // Delay progresivo
    }
  }
  
  if (!timeReadSuccess) {
    logTimeMessage(0, "❌ RTC - Fallo en lectura de tiempo después de " + String(RTC_READ_RETRIES) + " intentos");
    
    // Restaurar modo de energía
    digitalWrite(GPIO_NUM_7, HIGH);  // Reactivar modo bajo consumo
    digitalWrite(GPIO_NUM_3, LOW);   // Desactivar fuente de alimentación
    
    return false;
  }
  
  // =============================================================================
  // ALMACENAMIENTO DE DATOS EN ESTRUCTURA
  // =============================================================================
  
  // Almacenar fecha y hora en la estructura de datos
  data->H_ano = (byte)(now.year() >> 8);             // Byte alto del año
  data->L_ano = (byte)(now.year() & 0xFF);           // Byte bajo del año
  data->H_mes = (byte)now.month();                   // Mes (1-12)
  data->H_dia = (byte)now.day();                     // Día (1-31)
  data->H_hora = (byte)now.hour();                   // Hora (0-23)
  data->H_minuto = (byte)now.minute();               // Minuto (0-59)
  data->H_segundo = (byte)now.second();              // Segundo (0-59)
  
  // Actualizar estado del sistema
  lastRtcRead = millis();
  
  // =============================================================================
  // LOGGING Y DEBUGGING
  // =============================================================================
  
  // Imprimir información de tiempo en formato legible
  logTimeMessage(2, "📅 Fecha y hora configuradas:");
  logTimeMessage(2, "   " + String(now.year()) + "/" + 
                String(now.month(), DEC) + "/" + 
                String(now.day(), DEC) + " " +
                String(now.hour(), DEC) + ":" + 
                String(now.minute(), DEC) + ":" + 
                String(now.second(), DEC));
  
  // Mostrar información de estado del RTC
  logTimeMessage(2, "📊 Estado del RTC:");
  logTimeMessage(2, "   Inicializado: " + String(rtcInitialized ? "✅ SI" : "❌ NO"));
  logTimeMessage(2, "   Batería: " + String(rtcBatteryOK ? "✅ OK" : "⚠️  BAJA"));
  logTimeMessage(2, "   Errores de init: " + String(rtcInitErrors));
  logTimeMessage(2, "   Errores de lectura: " + String(rtcReadErrors));
  
  // =============================================================================
  // FINALIZACIÓN Y RESTAURACIÓN DE ENERGÍA
  // =============================================================================
  
  // Restaurar modo de energía
  digitalWrite(GPIO_NUM_7, HIGH);  // Reactivar modo bajo consumo
  digitalWrite(GPIO_NUM_3, LOW);   // Desactivar fuente de alimentación
  
  logTimeMessage(2, "✅ Sistema de tiempo configurado exitosamente");
  return true;
}

// =============================================================================
// FUNCIONES AUXILIARES Y DE DIAGNÓSTICO
// =============================================================================

/**
 * Obtiene el estado actual del sistema de tiempo
 * Proporciona información completa del estado del RTC
 * 
 * @return String con estadísticas del sistema de tiempo
 */
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

/**
 * Resetea los contadores de errores del sistema de tiempo
 * Útil para mantenimiento y recuperación del sistema
 */
void resetTimeErrorCounters() {
  rtcInitErrors = 0;
  rtcReadErrors = 0;
  logTimeMessage(2, "🔄 Contadores de error del sistema de tiempo reseteados");
}

/**
 * Obtiene la fecha y hora actual en formato String legible
 * Útil para logging y debugging
 * 
 * @return String con fecha y hora formateada
 */
String getCurrentTimeString() {
  if (!rtcInitialized) {
    return "RTC no inicializado";
  }
  
  // Activar energía temporalmente
  digitalWrite(GPIO_NUM_7, LOW);
  digitalWrite(GPIO_NUM_3, HIGH);
  delay(50);
  
  DateTime now = rtc.now();
  
  // Restaurar modo de energía
  digitalWrite(GPIO_NUM_7, HIGH);
  digitalWrite(GPIO_NUM_3, LOW);
  
  return String(now.year()) + "/" + 
         String(now.month(), DEC) + "/" + 
         String(now.day(), DEC) + " " +
         String(now.hour(), DEC) + ":" + 
         String(now.minute(), DEC) + ":" + 
         String(now.second(), DEC);
}