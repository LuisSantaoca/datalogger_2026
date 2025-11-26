/**
 * @file sensores.cpp
 * @brief Implementación del sistema de lectura de sensores
 * @author Elathia
 * @date 2025
 */

#include "sensores.h"
#include <SoftwareSerial.h>
#include <AHT10.h>
#include <AHT20.h>

EspSoftwareSerial::UART sonda;
AHT10 myAHT10(0x38);
AHT20 myAHT20;

// =============================================================================
// ESTRUCTURA DE ESTADO DE SENSORES
// =============================================================================

struct SensorStatus {
  bool sondaOK = false;
  bool ahtOK = false;
  bool bateriaOK = false;
  int erroresSonda = 0;
  int erroresAHT = 0;
  int erroresBateria = 0;
} sensorStatus;

byte H_temperatura_suelo = 0X00;
byte L_temperatura_suelo = 0X00;
byte H_humedad_suelo = 0X00;
byte L_humedad_suelo = 0X00;
byte H_conductividad_suelo = 0X00;
byte L_conductividad_suelo = 0X00;
byte H_ph_suelo = 0X00;
byte L_ph_suelo = 0X00;
byte H_humedad_relativa = 0X00;
byte L_humedad_relativa = 0X00;
byte H_temperatura_ambiente = 0X00;
byte L_temperatura_ambiente = 0X00;
byte H_bateria = 0X00;
byte L_bateria = 0X00;

// =============================================================================
// FUNCIÓN COMÚN PARA COMUNICACIÓN RS485
// =============================================================================

/**
 * Función común para comunicación RS485 con sondas
 * @param request Array de bytes del comando Modbus
 * @param requestLength Longitud del comando
 * @param response Array para almacenar la respuesta
 * @param responseLength Longitud esperada de la respuesta
 * @param sondaName Nombre de la sonda para logging
 * @return true si la comunicación fue exitosa
 */
bool readSondaRS485(byte* request, int requestLength, byte* response, int responseLength, const char* sondaName) {
  // Limpiar buffer de respuesta
  memset(response, 0, responseLength);

  // Limpiar buffer de entrada antes de enviar
  while (sonda.available()) {
    sonda.read();  // Descartar datos antiguos
  }

  // Enviar comando
  sonda.write(request, requestLength);
  sonda.flush();

  delay(10);
  unsigned long timeout = millis() + 200;
  int bytesRead = 0;

  // Leer respuesta
  while (millis() < timeout && bytesRead < responseLength) {
    if (sonda.available()) {
      response[bytesRead++] = sonda.read();
    }
  }

  // Validar respuesta
  if (bytesRead == 0) {
    Serial.print("❌ ERROR: ");
    Serial.print(sondaName);
    Serial.println(" - No se recibió respuesta");
    return false;
  }

  if (bytesRead < 5) {  // Respuesta mínima válida Modbus
    Serial.print("❌ ERROR: ");
    Serial.print(sondaName);
    Serial.print(" - Respuesta muy corta: ");
    Serial.println(bytesRead);
    return false;
  }

  // Validar estructura Modbus básica
  if (response[0] != request[0]) {  // Verificar dirección del esclavo
    Serial.print("❌ ERROR: ");
    Serial.print(sondaName);
    Serial.println(" - Dirección de esclavo incorrecta");
    return false;
  }

  // Log del comando enviado
  Serial.print("----REQUEST 485 ");
  Serial.println(sondaName);

  for (int j = 0; j < requestLength; j++) {
    if (request[j] < 0x10) Serial.print("0");
    Serial.print(request[j], HEX);
    Serial.print(",");
  }
  Serial.println(" ");

  // Log de la respuesta recibida
  Serial.print("----RESPONSE 485 ");
  Serial.println(sondaName);

  for (int k = 0; k < responseLength; k++) {
    if (response[k] < 0x10) Serial.print("0");
    Serial.print(response[k], HEX);
    Serial.print(",");
  }
  Serial.println(" ");

  Serial.print("✅ ");
  Serial.print(sondaName);
  Serial.print(" - Comunicación exitosa (");
  Serial.print(bytesRead);
  Serial.println(" bytes)");

  return true;
}

void setupSensores(sensordata_type* data) {
  sonda.begin(RS485_BAUD, RS485_CFG, RS485_RX, RS485_TX);

  digitalWrite(GPIO_NUM_3, HIGH);
  digitalWrite(GPIO_NUM_7, LOW);
  delay(2000);

  switch (TYPE_SONDA) {
    case SONDA_DFROBOT_EC:
      read_dfrobot_sonda_ec();
      break;
    case SONDA_DFROBOT_EC_PH:
      read_dfrobot_sonda_ecph();
      break;
    case SONDA_SEED_EC:
      read_seed_sonda_ec();
      break;
    default:
      Serial.printf("SIN SONDA");
      break;
  }

  switch (TYPE_AHT) {
    case AHT10_SEN:
      readAht10();
      break;
    case AHT20_SEN:
      readAht20();
      break;
    default:
      Serial.printf("sin sensor");
      break;
  }

  readBateria();

  data->H_temperatura_suelo = H_temperatura_suelo;
  data->L_temperatura_suelo = L_temperatura_suelo;
  data->H_humedad_suelo = H_humedad_suelo;
  data->L_humedad_suelo = L_humedad_suelo;
  data->H_conductividad_suelo = H_conductividad_suelo;
  data->L_conductividad_suelo = L_conductividad_suelo;
  data->H_ph_suelo = H_ph_suelo;
  data->L_ph_suelo = L_ph_suelo;
  data->H_humedad_relativa = H_humedad_relativa;
  data->L_humedad_relativa = L_humedad_relativa;
  data->H_temperatura_ambiente = H_temperatura_ambiente;
  data->L_temperatura_ambiente = L_temperatura_ambiente;
  data->H_bateria = H_bateria;
  data->L_bateria = L_bateria;

  digitalWrite(GPIO_NUM_3, LOW);
  digitalWrite(GPIO_NUM_7, HIGH);

  // Mostrar resumen de estado
  Serial.println("=== RESUMEN DE ESTADO ===");
  Serial.print("Sonda: ");
  Serial.println(sensorStatus.sondaOK ? "✅ OK" : "❌ ERROR");
  Serial.print("AHT: ");
  Serial.println(sensorStatus.ahtOK ? "✅ OK" : "❌ ERROR");
  Serial.print("Batería: ");
  Serial.println(sensorStatus.bateriaOK ? "✅ OK" : "❌ ERROR");

  if (sensorStatus.erroresSonda > 0) {
    Serial.print("Errores de sonda: ");
    Serial.println(sensorStatus.erroresSonda);
  }
  if (sensorStatus.erroresAHT > 0) {
    Serial.print("Errores de AHT: ");
    Serial.println(sensorStatus.erroresAHT);
  }
  if (sensorStatus.erroresBateria > 0) {
    Serial.print("Errores de batería: ");
    Serial.println(sensorStatus.erroresBateria);
  }
  Serial.println("========================");
}

void read_dfrobot_sonda_ec() {

  byte request[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x03, 0x05, 0xCB };
  byte response[12];

  if (readSondaRS485(request, 8, response, 12, "Sonda DFROBOT EC")) {
    // Validar que los datos no sean todo ceros
    if (response[3] == 0x00 && response[4] == 0x00 && response[5] == 0x00 && response[6] == 0x00) {
      Serial.println("⚠️ ADVERTENCIA: Sonda DFROBOT EC - Datos en cero");
    }

    // Validar rangos razonables
    if (response[5] > 100 || response[6] > 100) {  // Temperatura > 100°C
      Serial.println("⚠️ ADVERTENCIA: Sonda DFROBOT EC - Temperatura sospechosa");
    }

    // Si todo está bien, procesar datos
    H_temperatura_suelo = response[5];
    L_temperatura_suelo = response[6];
    H_humedad_suelo = response[3];
    L_humedad_suelo = response[4];
    H_conductividad_suelo = response[7];
    L_conductividad_suelo = response[8];
    H_ph_suelo = 0X00;
    L_ph_suelo = 0x00;

    sensorStatus.sondaOK = true;
    sensorStatus.erroresSonda = 0;
    Serial.println("✅ Sonda DFROBOT EC - Datos procesados correctamente");
  } else {
    // En caso de error, limpiar variables y actualizar estado
    Serial.println("❌ Sonda DFROBOT EC - Error en comunicación, limpiando datos");
    H_temperatura_suelo = 0x00;
    L_temperatura_suelo = 0x00;
    H_humedad_suelo = 0x00;
    L_humedad_suelo = 0x00;
    H_conductividad_suelo = 0x00;
    L_conductividad_suelo = 0x00;
    H_ph_suelo = 0x00;
    L_ph_suelo = 0x00;

    sensorStatus.sondaOK = false;
    sensorStatus.erroresSonda++;
  }
}

void read_dfrobot_sonda_ecph() {
  delay(15000);
  byte request[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x04, 0x44, 0x09 };
  byte response[12];
  bool lecturaExitosa = false;

  Serial.println("🔄 Iniciando 10 lecturas de sonda DFROBOT EC PH (descartando primeras 9)");

  // Realizar 10 lecturas, descartar las primeras 9
  for (int i = 0; i < 15; i++) {
    Serial.print("📊 Lectura ");
    Serial.print(i + 1);
    Serial.print("/10");

    if (i < 9) {
      Serial.println(" (descartando)");
    } else {
      Serial.println(" (procesando)");
    }

    bool lecturaActual = readSondaRS485(request, 8, response, 12, "Sonda DFROBOT EC PH");

    // Solo procesar la última lectura (la #10)
    if (i == 9) {
      lecturaExitosa = lecturaActual;
    }

    // Pequeña pausa entre lecturas para estabilizar la comunicación
    if (i < 9) {
      delay(2000);
    }
  }

  if (lecturaExitosa) {
    // Validar que los datos no sean todo ceros
    if (response[3] == 0x00 && response[4] == 0x00 && response[5] == 0x00 && response[6] == 0x00) {
      Serial.println("⚠️ ADVERTENCIA: Sonda DFROBOT EC PH - Datos en cero");
    }

    // Validar rangos razonables
    if (response[5] > 100 || response[6] > 100) {  // Temperatura > 100°C
      Serial.println("⚠️ ADVERTENCIA: Sonda DFROBOT EC PH - Temperatura sospechosa");
    }

    // Si todo está bien, procesar datos
    H_temperatura_suelo = response[5];
    L_temperatura_suelo = response[6];
    H_humedad_suelo = response[3];
    L_humedad_suelo = response[4];
    H_conductividad_suelo = response[7];
    L_conductividad_suelo = response[8];
    H_ph_suelo = response[9];
    L_ph_suelo = response[10];

    sensorStatus.sondaOK = true;
    sensorStatus.erroresSonda = 0;
    Serial.println("✅ Sonda DFROBOT EC PH - Datos procesados correctamente (lectura #10)");
  } else {
    // En caso de error, limpiar variables y actualizar estado
    Serial.println("❌ Sonda DFROBOT EC PH - Error en comunicación final, limpiando datos");
    H_temperatura_suelo = 0x00;
    L_temperatura_suelo = 0x00;
    H_humedad_suelo = 0x00;
    L_humedad_suelo = 0x00;
    H_conductividad_suelo = 0x00;
    L_conductividad_suelo = 0x00;
    H_ph_suelo = 0x00;
    L_ph_suelo = 0x00;

    sensorStatus.sondaOK = false;
    sensorStatus.erroresSonda++;
  }
}

void read_seed_sonda_ec() {
  byte request[] = { 0x12, 0x04, 0x00, 0x00, 0x00, 0x03, 0xB2, 0xA8 };
  byte response[12];

  if (readSondaRS485(request, 8, response, 12, "Sonda Seed")) {
    // Validar que los datos no sean todo ceros
    if (response[3] == 0x00 && response[4] == 0x00 && response[5] == 0x00 && response[6] == 0x00) {
      Serial.println("⚠️ ADVERTENCIA: Sonda Seed - Datos en cero");
    }

    // Validar rangos razonables
    if (response[3] > 100 || response[4] > 100) {  // Temperatura > 100°C
      Serial.println("⚠️ ADVERTENCIA: Sonda Seed - Temperatura sospechosa");
    }

    // Si todo está bien, procesar datos
    H_temperatura_suelo = response[3];
    L_temperatura_suelo = response[4];
    H_humedad_suelo = response[5];
    L_humedad_suelo = response[6];
    H_conductividad_suelo = response[7];
    L_conductividad_suelo = response[8];
    H_ph_suelo = 0X00;
    L_ph_suelo = 0x00;

    sensorStatus.sondaOK = true;
    sensorStatus.erroresSonda = 0;
    Serial.println("✅ Sonda Seed - Datos procesados correctamente");
  } else {
    // En caso de error, limpiar variables y actualizar estado
    Serial.println("❌ Sonda Seed - Error en comunicación, limpiando datos");
    H_temperatura_suelo = 0x00;
    L_temperatura_suelo = 0x00;
    H_humedad_suelo = 0x00;
    L_humedad_suelo = 0x00;
    H_conductividad_suelo = 0x00;
    L_conductividad_suelo = 0x00;
    H_ph_suelo = 0x00;
    L_ph_suelo = 0x00;

    sensorStatus.sondaOK = false;
    sensorStatus.erroresSonda++;
  }
}

void readAht10() {
  // Validar inicialización
  if (!myAHT10.begin()) {
    Serial.println("❌ ERROR: AHT10 no se pudo inicializar");
    // Limpiar variables en caso de error
    H_humedad_relativa = 0x00;
    L_humedad_relativa = 0x00;
    H_temperatura_ambiente = 0x00;
    L_temperatura_ambiente = 0x00;
    sensorStatus.ahtOK = false;
    sensorStatus.erroresAHT++;
    return;  // Salir de la función
  }

  // Validar lecturas
  int temp = myAHT10.readTemperature() * 100;
  int hum = myAHT10.readHumidity() * 100;

  // Verificar si las lecturas son válidas
  if (temp == -999 || hum == -999) {  // AHT10 retorna -999 en caso de error
    Serial.println("❌ ERROR: AHT10 - Lectura fallida");
    sensorStatus.ahtOK = false;
    sensorStatus.erroresAHT++;
    return;
  }

  // Verificar rangos válidos
  if (temp < -4000 || temp > 8000 || hum < 0 || hum > 10000) {
    Serial.println("⚠️ ADVERTENCIA: AHT10 - Valores fuera de rango");
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.println(" (debería estar entre -4000 y 8000)");
    Serial.print("Hum: ");
    Serial.print(hum);
    Serial.println(" (debería estar entre 0 y 10000)");
  }

  // Si todo está bien, procesar datos
  H_humedad_relativa = (hum >> 8) & 0xFF;
  L_humedad_relativa = hum & 0xFF;
  H_temperatura_ambiente = (temp >> 8) & 0xFF;
  L_temperatura_ambiente = temp & 0xFF;

  sensorStatus.ahtOK = true;
  sensorStatus.erroresAHT = 0;

  Serial.println("✅ AHT10 - Lectura exitosa");
  Serial.println("---------Sensor AHT10----------");
  Serial.println(temp);
  Serial.println(hum);
  Serial.println("-----------");
}

void readAht20() {
  // Validar inicialización
  if (!myAHT20.begin()) {
    Serial.println("❌ ERROR: AHT20 no se pudo inicializar");
    // Limpiar variables en caso de error
    H_humedad_relativa = 0x00;
    L_humedad_relativa = 0x00;
    H_temperatura_ambiente = 0x00;
    L_temperatura_ambiente = 0x00;
    sensorStatus.ahtOK = false;
    sensorStatus.erroresAHT++;
    return;  // Salir de la función
  }

  // Validar lecturas
  int temp = myAHT20.getTemperature() * 100;
  int hum = myAHT20.getHumidity() * 100;

  // Verificar si las lecturas son válidas
  if (temp == -999 || hum == -999) {  // AHT20 retorna -999 en caso de error
    Serial.println("❌ ERROR: AHT20 - Lectura fallida");
    sensorStatus.ahtOK = false;
    sensorStatus.erroresAHT++;
    return;
  }

  // Verificar rangos válidos
  if (temp < -4000 || temp > 8000 || hum < 0 || hum > 10000) {
    Serial.println("⚠️ ADVERTENCIA: AHT20 - Valores fuera de rango");
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.println(" (debería estar entre -4000 y 8000)");
    Serial.print("Hum: ");
    Serial.print(hum);
    Serial.println(" (debería estar entre 0 y 10000)");
  }

  // Si todo está bien, procesar datos
  H_humedad_relativa = (hum >> 8) & 0xFF;
  L_humedad_relativa = hum & 0xFF;
  H_temperatura_ambiente = (temp >> 8) & 0xFF;
  L_temperatura_ambiente = temp & 0xFF;

  sensorStatus.ahtOK = true;
  sensorStatus.erroresAHT = 0;

  Serial.println("✅ AHT20 - Lectura exitosa");
  Serial.println("Sensor AHT20");
  Serial.println(temp);
  Serial.println(hum);
  Serial.println("-----------");
}

void readBateria() {
  const int numLecturas = 10;
  long suma = 0;
  int lecturasValidas = 0;

  for (int i = 0; i < numLecturas; i++) {
    int lectura = analogRead(ADC_VOLT_BAT);

    // Validar que la lectura esté en rango razonable
    if (lectura > 0 && lectura < 4095) {
      suma += lectura;
      lecturasValidas++;
    }
    delay(5);
  }

  // Verificar que tengamos suficientes lecturas válidas
  if (lecturasValidas < 5) {
    Serial.println("❌ ERROR: Batería - Demasiadas lecturas inválidas");
    H_bateria = 0x00;
    L_bateria = 0x00;
    sensorStatus.bateriaOK = false;
    sensorStatus.erroresBateria++;
    return;
  }

  int promedio = suma / lecturasValidas;
  float voltaje = (promedio * 2 * 3.3) / 4095;

  // Validar rango de voltaje
  if (voltaje < 2.0 || voltaje > 4.5) {
    Serial.println("⚠️ ADVERTENCIA: Batería - Voltaje fuera de rango normal");
    Serial.print("Voltaje: ");
    Serial.print(voltaje, 2);
    Serial.println("V");
  }

  int ajustes = (voltaje + 0.3) * 100;
  H_bateria = highByte(ajustes);
  L_bateria = lowByte(ajustes);

  sensorStatus.bateriaOK = true;
  sensorStatus.erroresBateria = 0;

  Serial.println("✅ Batería - Lectura exitosa");
  Serial.println("---------Bateria----------");
  Serial.println(ajustes);
  Serial.println("-------------------");
}
