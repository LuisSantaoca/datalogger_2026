# Módulo: data_sleepwakeup

## Descripción
El módulo data_sleepwakeup gestiona los modos de bajo consumo del sistema para optimizar el uso de energía.

## Componentes

### SLEEPModule
- **Propósito**: Control de modos de ahorro de energía
- **Funcionalidad Principal**:
  - Configuración de deep sleep
  - Configuración de wake-up sources
  - Gestión de timers de despertar
  - Reducción de consumo energético

## Archivos
- `SLEEPModule.cpp/h`: Implementación de control de sleep
- `config_data_sleepwakeup.h`: Configuración de modos y timers

## Modos de Operación

### Deep Sleep
- Modo de menor consumo
- CPU y periféricos apagados
- Solo RTC y wake-up activos
- Consumo: < 10 μA

### Light Sleep
- Modo de bajo consumo moderado
- CPU en pausa, periféricos pueden estar activos
- Wake-up más rápido que deep sleep
- Consumo: < 1 mA

### Modem Sleep (WiFi/LTE off)
- CPU activa pero módems apagados
- Para operación local sin comunicación
- Consumo: < 50 mA

## Wake-Up Sources

### Timer Wake-Up
Despertar periódico programado (ej: cada hora para lectura de sensores).

### External Interrupt Wake-Up
Despertar por señal externa (botón, sensor de movimiento).

### RTC Alarm Wake-Up
Despertar en hora/fecha específica.

## Configuración Típica

```cpp
// Ejemplo de configuración
#define SLEEP_TIME_SECONDS 3600  // 1 hora
#define WAKE_UP_PIN GPIO_NUM_33
#define USE_DEEP_SLEEP true
```

## Flujo de Operación

### Antes de Sleep
1. Guardar datos en buffer/memoria persistente
2. Apagar periféricos no necesarios (GPS, LTE)
3. Configurar wake-up source
4. Entrar en modo sleep

### Durante Sleep
- Sistema en bajo consumo
- Solo wake-up sources activos
- Memoria RTC preservada

### Al Despertar
1. Sistema se reinicia (deep sleep) o continúa (light sleep)
2. Recuperar estado de memoria RTC
3. Re-inicializar periféricos necesarios
4. Continuar operación normal

## Estrategias de Ahorro

### Lectura Periódica
1. Deep sleep por X minutos
2. Wake-up por timer
3. Leer sensores
4. Transmitir datos
5. Volver a sleep

### Evento Externo
1. Deep sleep indefinido
2. Wake-up por interrupción externa
3. Procesar evento
4. Volver a sleep
