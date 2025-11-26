/**
 * @file sensoresElathia.ino
 * @brief Programa principal del sistema de monitoreo ambiental Elathia
 * @author Elathia
 * @date 2025
 * @version 2.0
 * 
 * @details Sistema IoT para agricultura de precisión con ESP32-S3.
 * Características:
 * - Sensores ambientales y de suelo
 * - Comunicación LTE/GSM (SIM7080G)
 * - GPS integrado
 * - Deep sleep (10 min)
 * - Encriptación AES
 * - Buffer local (LittleFS)
 */

#include "gsmlte.h"
#include "sleepdev.h"
#include "sensores.h"
#include "crono.h"
#include "timedata.h"
#include "type_def.h"

sensordata_type sensordata;
Cronometro cronometroDispositivo;
bool gps_sim_enabled = true;

/**
 * @brief Inicialización del sistema
 * @details Se ejecuta una vez al encender o despertar del ESP32.
 * Secuencia: GPIO → Tiempo → GPS → Sensores → Módem → Sleep
 */
void setup() {
  cronometroDispositivo.iniciar();
  Serial.begin(115200);
  delay(500);
  
  Serial.println("🚀 SISTEMA DE MONITOREO AMBIENTAL ELATHIA");
  Serial.println("==========================================");
  Serial.println("Iniciando sistema...");
  Serial.println("⏱️  Cronómetro iniciado - midiendo tiempo de funcionamiento");
  Serial.println("🛰️  CONFIGURACIÓN DE GPS:");
  Serial.println("   GPS SIM (módem): " + String(gps_sim_enabled ? "✅ HABILITADO" : "❌ DESHABILITADO"));
  
  setupGPIO();
  delay(1000);
  
  setupTimeData(&sensordata);
  Serial.println("✅ Sistema de tiempo configurado");
  
  if (gps_sim_enabled) {
    Serial.println("🛰️  Configurando GPS del módem SIM...");
    setupGpsSim(&sensordata);
    Serial.println("✅ GPS del módem SIM configurado");
  } else {
    Serial.println("⚠️  GPS del módem SIM deshabilitado");
    Serial.println("   El sistema funcionará sin geolocalización");
  }
  
  Serial.println("🌱 Iniciando lectura de sensores...");
  setupSensores(&sensordata);
  imprimirSensorData(&sensordata);
  Serial.println("✅ Lectura de sensores completada");
  
  Serial.println("📡 Iniciando configuración del módem LTE/GSM...");
  setupModem(&sensordata);
  Serial.println("✅ Configuración del módem completada");
  
  Serial.println("💤 Preparando sistema para modo deep sleep...");
  cronometroDispositivo.detener();
  
  unsigned long tiempoTotalFuncionamiento = cronometroDispositivo.obtenerDuracion();
  unsigned long segundos = tiempoTotalFuncionamiento / 1000;
  unsigned long minutos = segundos / 60;
  segundos = segundos % 60;
  
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
  
  String stats = getSystemStats();
  Serial.println("📊 ESTADÍSTICAS FINALES DEL SISTEMA:");
  Serial.println(stats);
  
  setupGPIO();
  
  Serial.println("✅ Sistema preparado para deep sleep");
  Serial.println("🌙 Entrando en modo deep sleep...");
  Serial.println("⏰ Próximo despertar en " + String(TIME_TO_SLEEP) + " segundos");
  
  delay(1000);
  sleepIOT();
}

/**
 * @brief Función loop no utilizada
 * @details El sistema opera en ciclos: Setup → Deep Sleep → Despertar → Setup
 */
void loop() {
}

/**
 * @brief Imprime los datos de sensores en formato legible
 * @param data Puntero a la estructura de datos de sensores
 */
void imprimirSensorData(sensordata_type* data) {
  Serial.println("📊 DATOS DE SENSORES LEÍDOS:");
  Serial.println("=============================");
  
  Serial.println("🌱 DATOS DE SUELO:");
  int tempSuelo = (data->H_temperatura_suelo << 8) | data->L_temperatura_suelo;
  Serial.print("   Temperatura: ");
  Serial.print(tempSuelo / 100.0, 1);
  Serial.println("°C");
  
  int humSuelo = (data->H_humedad_suelo << 8) | data->L_humedad_suelo;
  Serial.print("   Humedad: ");
  Serial.print(humSuelo / 100.0, 1);
  Serial.println("%");
  
  int condSuelo = (data->H_conductividad_suelo << 8) | data->L_conductividad_suelo;
  Serial.print("   Conductividad: ");
  Serial.print(condSuelo);
  Serial.println(" μS/cm");
  
  int phSuelo = (data->H_ph_suelo << 8) | data->L_ph_suelo;
  Serial.print("   pH: ");
  Serial.println(phSuelo / 100.0, 2);
  
  Serial.println("🌡️  DATOS AMBIENTALES:");
  int tempAmb = (data->H_temperatura_ambiente << 8) | data->L_temperatura_ambiente;
  Serial.print("   Temperatura: ");
  Serial.print(tempAmb / 100.0, 1);
  Serial.println("°C");
  
  int humAmb = (data->H_humedad_relativa << 8) | data->L_humedad_relativa;
  Serial.print("   Humedad: ");
  Serial.print(humAmb / 100.0, 1);
  Serial.println("%");
  
  Serial.println("🔋 DATOS DEL SISTEMA:");
  int batVolt = (data->H_bateria << 8) | data->L_bateria;
  float voltaje = (batVolt / 100.0) - 0.3;
  Serial.print("   Voltaje batería: ");
  Serial.print(voltaje, 2);
  Serial.println("V");
  
  int rsi = data->H_rsi;
  Serial.print("   Calidad señal: ");
  Serial.print(rsi);
  Serial.println(" dBm");
  
  Serial.println("🛰️  DATOS GPS:");
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