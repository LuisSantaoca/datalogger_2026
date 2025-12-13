// =============================================================================
// ARCHIVO: JAMR_4.4.ino
// VERSIÓN: 4.4
// DESCRIPCIÓN: Programa principal para sistema de monitoreo ambiental con ESP32-S3
//              Basado en sensores_elathia_fix_gps (versión estable y probada)
// FUNCIONALIDADES:
//   - Lectura de sensores ambientales y de suelo
//   - Comunicación LTE/GSM para transmisión de datos
//   - Sistema de gestión de energía con modo deep sleep
//   - Almacenamiento local de datos con LittleFS
//   - GPS integrado para geolocalización
//   - Encriptación AES para seguridad de datos
// PROPOSITO: Firmware de desarrollo controlado para mejoras incrementales
// FECHA: 2025-10-27
// =============================================================================

// =============================================================================
// INCLUSIÓN DE ARCHIVOS DE CABECERA
// =============================================================================

#include "gsmlte.h"               // Sistema de comunicación LTE/GSM
#include "sleepdev.h"              // Gestión de energía y modo sleep
#include "sensores.h"              // Sistema de lectura de sensores
#include "crono.h"                 // Cronómetro para medición de tiempos
#include "timedata.h"              // Gestión de tiempo y fecha
#include "type_def.h"              // Definiciones de tipos de datos

// =============================================================================
// VARIABLES GLOBALES DEL SISTEMA
// =============================================================================

// Estructura principal para almacenar datos de sensores
sensordata_type sensordata;

// Cronómetro para medir tiempo total de funcionamiento del dispositivo
Cronometro cronometroDispositivo;

// 🆕 FIX-004: Flag para indicar si hubo crash en ciclo anterior
bool g_had_crash = false;

// 🆕 FIX-007: Etiqueta de versión para logs y health tracking
const char* FIRMWARE_VERSION_TAG = "v4.4.12 adc-bateria-fix";

// 🆕 REQ-004: Versionamiento semántico para payload
const uint8_t FIRMWARE_VERSION_MAJOR = 4;
const uint8_t FIRMWARE_VERSION_MINOR = 4;
const uint8_t FIRMWARE_VERSION_PATCH = 12;

// =============================================================================
// CONFIGURACIÓN DEL SISTEMA
// =============================================================================

// GPS del módem SIM habilitado por defecto
bool gps_sim_enabled = true;

// =============================================================================
// FUNCIÓN SETUP - INICIALIZACIÓN DEL SISTEMA
// =============================================================================

/**
 * Función de inicialización del sistema
 * Se ejecuta una sola vez al encender o resetear el ESP32
 */
void setup() {
  // Iniciar cronómetro para medir tiempo total de funcionamiento
  cronometroDispositivo.iniciar();
  
  // Inicializar comunicación serial para debugging
  Serial.begin(115200);
  
  // 🆕 FIX-12: Configurar ADC para lectura correcta de batería
  analogReadResolution(12);           // Resolución 12 bits (0-4095)
  analogSetAttenuation(ADC_11db);     // Atenuación 11dB (rango 0-3.3V)
  pinMode(ADC_VOLT_BAT, INPUT);       // Pin 13 como entrada ADC
  
  // Esperar estabilización del sistema
  delay(1000);
  
  // 🆕 FIX-004: Detectar crash del ciclo anterior
  Serial.println("🔍 Verificando estado del sistema...");
  g_had_crash = getCrashInfo();
  
  // Si hubo crash, imprimir información de diagnóstico
  if (g_had_crash) {
    Serial.println("📊 INFORMACIÓN DE DIAGNÓSTICO:");
    Serial.print("   Boot count: ");
    Serial.println(rtc_boot_count);
    Serial.print("   Último checkpoint: ");
    Serial.println(rtc_last_checkpoint);
    Serial.print("   Timestamp: ");
    Serial.print(rtc_timestamp_ms);
    Serial.println(" ms");
  }
  
  // 🆕 FIX-004: Marcar checkpoint de boot
  updateCheckpoint(CP_BOOT);
  
  // 🆕 JAMR_3 FIX-001: Inicializar Watchdog Timer
  // Configurar watchdog con timeout de 120 segundos para recuperación automática
  // ESP-IDF v5.3+ requiere estructura de configuración
  Serial.println("🛡️  Inicializando Watchdog Timer...");
  
  // Primero desinicializar si ya existe (Arduino lo inicializa automáticamente)
  esp_task_wdt_deinit();
  
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT_SEC * 1000,  // Convertir segundos a milisegundos
    .idle_core_mask = 0,                         // No monitorear tareas idle
    .trigger_panic = true                        // Ejecutar reset automático si timeout
  };
  
  esp_task_wdt_init(&wdt_config);              // Inicializar con configuración
  esp_task_wdt_add(NULL);                      // Agregar tarea actual al watchdog
  Serial.println("✅ Watchdog configurado: timeout=" + String(WATCHDOG_TIMEOUT_SEC) + "s");
  esp_task_wdt_reset();                        // Primera alimentación del watchdog
  
  // Imprimir mensaje de inicio
  Serial.println("🚀 SISTEMA DE MONITOREO AMBIENTAL ELATHIA");
  Serial.println("==========================================");
  Serial.println("Iniciando sistema...");
  Serial.println(String("🔖 Firmware activo: ") + FIRMWARE_VERSION_TAG);
  Serial.println("⏱️  Cronómetro iniciado - midiendo tiempo de funcionamiento");
  
  // Imprimir configuración de GPS
  Serial.println("🛰️  CONFIGURACIÓN DE GPS:");
  Serial.println("   GPS SIM (módem): " + String(gps_sim_enabled ? "✅ HABILITADO" : "❌ DESHABILITADO"));
  
  // =============================================================================
  // CONFIGURACIÓN DE GPIO Y PERIFÉRICOS
  // =============================================================================
  setupGPIO();
  updateCheckpoint(CP_GPIO_OK); // 🆕 FIX-004: GPIO configurados OK
  esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed watchdog después de GPIO
  delay(2000);
  // =============================================================================
  // INICIALIZACIÓN DE SISTEMAS
  // =============================================================================
  
  // Configurar sistema de tiempo
  setupTimeData(&sensordata);
  Serial.println("✅ Sistema de tiempo configurado");
  
  // =============================================================================
  // CONFIGURACIÓN DE GPS DEL MÓDEM SIM
  // =============================================================================
  
  if (gps_sim_enabled) {
    Serial.println("🛰️  Configurando GPS del módem SIM...");
    setupGpsSim(&sensordata);
    updateCheckpoint(CP_GPS_FIX); // 🆕 FIX-004: GPS fix obtenido
    Serial.println("✅ GPS del módem SIM configurado");
  } else {
    Serial.println("⚠️  GPS del módem SIM deshabilitado");
    Serial.println("   El sistema funcionará sin geolocalización");
  }
  
  // =============================================================================
  // LECTURA DE SENSORES
  // =============================================================================
  
  Serial.println("🌱 Iniciando lectura de sensores...");
  
  // Leer todos los sensores del sistema
  setupSensores(&sensordata);
  updateCheckpoint(CP_SENSORS_OK); // 🆕 FIX-004: Sensores leídos OK
  esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed watchdog después de sensores
  
  // Imprimir resumen de datos leídos
  imprimirSensorData(&sensordata);
  
  Serial.println("✅ Lectura de sensores completada");
  
  // =============================================================================
  // CONFIGURACIÓN Y COMUNICACIÓN DEL MÓDEM
  // =============================================================================
  
  Serial.println("📡 Iniciando configuración del módem LTE/GSM...");
  
  // 🆕 FIX-004: Preparar health data para incluir en transmisión
  if (g_had_crash) {
    Serial.println("📊 Incluyendo datos de diagnóstico en transmisión");
  }
  
  // Incluir health data en el payload (siempre, para tracking de boot_count)
  sensordata.health_checkpoint = rtc_last_checkpoint;
  sensordata.health_crash_reason = rtc_crash_reason;
  sensordata.H_health_boot_count = (byte)(rtc_boot_count >> 8);
  sensordata.L_health_boot_count = (byte)(rtc_boot_count & 0xFF);
  sensordata.H_health_crash_ts = (byte)(rtc_timestamp_ms >> 16);
  sensordata.L_health_crash_ts = (byte)((rtc_timestamp_ms >> 8) & 0xFF);
  
  // 🆕 REQ-004: Incluir versión de firmware en el payload
  sensordata.fw_major = FIRMWARE_VERSION_MAJOR;
  sensordata.fw_minor = FIRMWARE_VERSION_MINOR;
  sensordata.fw_patch = FIRMWARE_VERSION_PATCH;
  
  // Configurar e inicializar el módem
  setupModem(&sensordata);
  updateCheckpoint(CP_DATA_SENT); // 🆕 FIX-004: Datos enviados OK
  esp_task_wdt_reset(); // 🆕 FIX-005: Feed watchdog tras completar envío
  
  Serial.println("✅ Configuración del módem completada");
  
  // =============================================================================
  // FINALIZACIÓN Y PREPARACIÓN PARA SLEEP
  // =============================================================================
  
  Serial.println("💤 Preparando sistema para modo deep sleep...");
  
  // Detener cronómetro antes de entrar en sleep
  cronometroDispositivo.detener();
  
  // Obtener tiempo total de funcionamiento
  unsigned long tiempoTotalFuncionamiento = cronometroDispositivo.obtenerDuracion();
  
  // Convertir a segundos y minutos para mejor legibilidad
  unsigned long segundos = tiempoTotalFuncionamiento / 1000;
  unsigned long minutos = segundos / 60;
  segundos = segundos % 60;
  
  // Imprimir estadísticas de tiempo de funcionamiento
  Serial.println("⏱️  ESTADÍSTICAS DE TIEMPO DE FUNCIONAMIENTO:");
  Serial.println("=============================================");
  Serial.print("   Tiempo total: ");
  Serial.print(tiempoTotalFuncionamiento);
  Serial.println(" ms");
  Serial.print("   Duración: ");
  Serial.print(minutos);
  Serial.print(" minutos y ");
  Serial.print(segundos);
  Serial.println(" segundos");
  Serial.println("   Cronómetro estado: " + String(cronometroDispositivo.estaCorriendo() ? "CORRIENDO" : "DETENIDO"));
  
  // Imprimir estadísticas finales del sistema
  String stats = getSystemStats();
  Serial.println("📊 ESTADÍSTICAS FINALES DEL SISTEMA:");
  Serial.println(stats);
  markCycleSuccess(); // 🆕 FIX-006: Limpiar diagnóstico tras ciclo exitoso
  esp_task_wdt_reset(); // 🆕 FIX-005: Feed watchdog tras imprimir estadísticas
  
 
  
  // Configurar GPIO para modo sleep
  setupGPIO();
  esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed watchdog después de GPIO
  
  Serial.println("✅ Sistema preparado para deep sleep");
  Serial.println("🌙 Entrando en modo deep sleep...");
  Serial.println("⏰ Próximo despertar en " + String(TIME_TO_SLEEP) + " segundos");
  
  // Pequeña pausa para que se vean los mensajes
  delay(2000);
  
  // Entrar en modo deep sleep
  esp_task_wdt_reset(); // 🆕 JAMR_3 FIX-001: Feed watchdog antes de sleep

  sleepIOT();

  
}

// =============================================================================
// FUNCIÓN LOOP - NO UTILIZADA EN ESTE PROGRAMA
// =============================================================================

/**
 * Función loop (no utilizada)
 * El programa funciona en modo de evento único:
 * 1. Setup (lectura de sensores y envío)
 * 2. Deep sleep hasta próximo evento
 * 3. Repetir
 */
void loop() {
  // Esta función no se ejecuta en este programa
  // El ESP32 entra en deep sleep después del setup
}

// =============================================================================
// FUNCIONES AUXILIARES
// =============================================================================

/**
 * Imprime los datos de sensores en formato legible
 * Muestra todos los valores leídos en el monitor serial
 * 
 * @param data - Puntero a la estructura de datos de sensores
 */
void imprimirSensorData(sensordata_type* data) {
  Serial.println("📊 DATOS DE SENSORES LEÍDOS:");
  Serial.println("=============================");
  
  // =============================================================================
  // DATOS DE SUELO
  // =============================================================================
  
  Serial.println("🌱 DATOS DE SUELO:");
  
  // Temperatura del suelo (convertir de bytes a valor)
  int tempSuelo = (data->H_temperatura_suelo << 8) | data->L_temperatura_suelo;
  Serial.print("   Temperatura: ");
  Serial.print(tempSuelo / 100.0, 1);
  Serial.println("°C");
  
  // Humedad del suelo (convertir de bytes a valor)
  int humSuelo = (data->H_humedad_suelo << 8) | data->L_humedad_suelo;
  Serial.print("   Humedad: ");
  Serial.print(humSuelo / 100.0, 1);
  Serial.println("%");
  
  // Conductividad eléctrica (convertir de bytes a valor)
  int condSuelo = (data->H_conductividad_suelo << 8) | data->L_conductividad_suelo;
  Serial.print("   Conductividad: ");
  Serial.print(condSuelo);
  Serial.println(" μS/cm");
  
  // pH del suelo (convertir de bytes a valor)
  int phSuelo = (data->H_ph_suelo << 8) | data->L_ph_suelo;
  Serial.print("   pH: ");
  Serial.println(phSuelo / 100.0, 2);
  
  // =============================================================================
  // DATOS AMBIENTALES
  // =============================================================================
  
  Serial.println("🌡️  DATOS AMBIENTALES:");
  
  // Temperatura ambiente (convertir de bytes a valor)
  int tempAmb = (data->H_temperatura_ambiente << 8) | data->L_temperatura_ambiente;
  Serial.print("   Temperatura: ");
  Serial.print(tempAmb / 100.0, 1);
  Serial.println("°C");
  
  // Humedad relativa (convertir de bytes a valor)
  int humAmb = (data->H_humedad_relativa << 8) | data->L_humedad_relativa;
  Serial.print("   Humedad: ");
  Serial.print(humAmb / 100.0, 1);
  Serial.println("%");
  
  // =============================================================================
  // DATOS DEL SISTEMA
  // =============================================================================
  
  Serial.println("🔋 DATOS DEL SISTEMA:");
  
  // Voltaje de batería (convertir de bytes a valor)
  int batVolt = (data->H_bateria << 8) | data->L_bateria;
  float voltaje = (batVolt / 100.0) - 0.3; // Compensar offset
  Serial.print("   Voltaje batería: ");
  Serial.print(voltaje, 2);
  Serial.println("V");
  
  // Calidad de señal móvil (solo byte alto)
  int rsi = data->H_rsi;
  Serial.print("   Calidad señal: ");
  Serial.print(rsi);
  Serial.println(" dBm");
  
  // =============================================================================
  // DATOS GPS
  // =============================================================================
  
  Serial.println("🛰️  DATOS GPS:");
  
  // Convertir bytes de latitud a float
  union FloatToBytes {
    float f;
    byte b[4];
  } latConverter, lonConverter, altConverter;
  
  latConverter.b[0] = data->lat0;
  latConverter.b[1] = data->lat1;
  latConverter.b[2] = data->lat2;
  latConverter.b[3] = data->lat3;
  
  lonConverter.b[0] = data->lon0;
  lonConverter.b[1] = data->lon1;
  lonConverter.b[2] = data->lon2;
  lonConverter.b[3] = data->lon3;
  
  altConverter.b[0] = data->alt0;
  altConverter.b[1] = data->alt1;
  altConverter.b[2] = data->alt2;
  altConverter.b[3] = data->alt3;
  
  Serial.print("   Latitud: ");
  Serial.print(latConverter.f, 6);
  Serial.println("°");
  
  Serial.print("   Longitud: ");
  Serial.print(lonConverter.f, 6);
  Serial.println("°");
  
  Serial.print("   Altitud: ");
  Serial.print(altConverter.f, 1);
  Serial.println("m");
  
  Serial.println("=============================");
}

// =============================================================================
// NOTAS DE IMPLEMENTACIÓN
// =============================================================================

/*
 * FLUJO DEL PROGRAMA:
 * 
 * 1. INICIALIZACIÓN (setup):
 *    - Configuración de GPIO y periféricos
 *    - Inicialización de sistemas de tiempo y GPS
 *    - Lectura de todos los sensores
 *    - Configuración del módem LTE/GSM
 *    - Envío de datos al servidor
 *    - Preparación para modo sleep
 * 
 * 2. MODO SLEEP:
 *    - El ESP32 entra en deep sleep para ahorrar energía
 *    - Se programa un timer para despertar periódicamente
 *    - Al despertar, se ejecuta setup() nuevamente
 * 
 * 3. CICLO REPETITIVO:
 *    - Lectura → Envío → Sleep → Repetir
 * 
 * CARACTERÍSTICAS DEL SISTEMA:
 * 
 * - ENERGÍA: Modo deep sleep para máxima eficiencia
 * - COMUNICACIÓN: LTE/GSM con reintentos automáticos
 * - ALMACENAMIENTO: Buffer local con LittleFS
 * - SEGURIDAD: Encriptación AES de datos
 * - ROBUSTEZ: Manejo de errores y reintentos
 * - DIAGNÓSTICO: Logging estructurado y estadísticas
 * 
 * CONFIGURACIÓN:
 * 
 * - Ajustar TIME_SLEEP en sleepdev.h para cambiar frecuencia
 * - Modificar parámetros del módem en gsmlte.h
 * - Configurar tipos de sensores en sensores.h
 * - Ajustar pines según el hardware utilizado
 * 
 * MONITOREO:
 * 
 * - Usar monitor serial a 115200 baudios
 * - Los logs muestran estado del sistema
 * - Estadísticas disponibles con getSystemStats()
 * - Información de sensores con getSensorStats()
 */