# Módulo: data_format

## Descripción
El módulo data_format gestiona el formateo y estructuración de datos de sensores para transmisión y almacenamiento.

## Componentes

### FORMATModule
- **Propósito**: Formateo y serialización de datos
- **Funcionalidad Principal**:
  - Conversión de datos de sensores a formatos estándar
  - Serialización para transmisión (JSON, CSV, binario)
  - Validación de formato de datos
  - Empaquetado de múltiples lecturas de sensores

## Archivos
- `FORMATModule.cpp/h`: Implementación del formateo de datos
- `config_data_format.h`: Configuración de formatos y estructuras de datos

## Casos de Uso
- Formateo de datos GPS (latitud, longitud, altitud)
- Estructuración de lecturas de sensores analógicos
- Preparación de datos para envío por LTE
- Formateo de timestamps y metadatos

## Flujo de Operación
1. Recibe datos crudos de diferentes sensores
2. Aplica formato según configuración
3. Valida integridad de datos
4. Genera estructura lista para transmisión/almacenamiento
