# Módulo: data_time

## Descripción
El módulo data_time gestiona el reloj de tiempo real (RTC) para mantener timestamp preciso de las lecturas.

## Componentes

### RTCModule
- **Propósito**: Gestión de tiempo real del sistema
- **Funcionalidad Principal**:
  - Inicialización del RTC
  - Sincronización con servidor NTP
  - Mantenimiento de timestamp
  - Generación de marcas de tiempo para datos
  - Alarmas programables

## Archivos
- `RTCModule.cpp/h`: Implementación del control RTC
- `config_data_time.h`: Configuración de RTC y timezone

## Funciones Principales

### initRTC()
Inicializa el RTC interno o externo del microcontrolador.

### syncWithNTP()
Sincroniza el RTC con servidor de tiempo por internet.

### setDateTime()
Configura fecha y hora manualmente.

### getDateTime()
Obtiene la fecha/hora actual.

### getTimestamp()
Retorna timestamp Unix (segundos desde 1970).

### getFormattedTime()
Retorna tiempo en formato legible (ISO 8601, etc.).

### setAlarm()
Configura alarma para despertar o ejecutar acción.

## Formatos de Tiempo Soportados

### Unix Timestamp
```
1702838400  // Segundos desde 01/01/1970
```

### ISO 8601
```
2025-12-17T15:30:45Z
```

### Formato Personalizado
```
17/12/2025 15:30:45
```

## Flujo de Operación

### Inicialización
1. Configurar RTC interno/externo
2. Verificar batería RTC (si aplica)
3. Sincronizar con NTP si hay conexión
4. Configurar timezone

### Durante Operación
1. Mantener tiempo actualizado
2. Generar timestamps para lecturas
3. Verificar alarmas programadas
4. Re-sincronizar periódicamente con NTP

### Con Datos de Sensor
1. Leer sensor
2. Obtener timestamp actual
3. Asociar timestamp con lectura
4. Almacenar/transmitir con marca temporal

## Consideraciones

### Sin Conexión Internet
- Usar RTC interno para mantener tiempo
- Precisión: ±20 ppm (error acumulativo)
- Re-sincronizar cuando haya conexión

### Con Batería RTC
- Mantiene tiempo durante power-off
- Duración: varios años con batería CR2032
- Verificar voltaje de batería periódicamente

### Timezone
- Configurar offset UTC según ubicación
- Considerar horario de verano (DST)
- Almacenar datos en UTC, mostrar en local

## Integración con Otros Módulos

### Con GPS
- Obtener tiempo preciso de satélites GPS
- Usar como fuente de sincronización alternativa

### Con Sleep
- RTC mantiene tiempo durante deep sleep
- Usar alarmas RTC para wake-up programado

### Con Data Format
- Incluir timestamp en formato de datos
- Estandarizar formato temporal para transmisión
