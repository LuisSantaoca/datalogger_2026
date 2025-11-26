# Flujo del Programa - Sistema GSM/LTE Elathia

## 📋 Descripción General

Sistema robusto de comunicación LTE/GSM para dispositivos IoT que gestiona automáticamente la conexión con operadores móviles, encripta y transmite datos de sensores con almacenamiento local como respaldo.

---

## 🔄 Flujo Principal de Ejecución

### 1️⃣ **Inicialización del Sistema** (`setupModem()`)

```
setupModem() → Punto de entrada principal
    ↓
    ├─ initModemConfig()          // Configura parámetros del módem
    ├─ SerialMon.begin(115200)    // Inicia comunicación serial
    ├─ SerialAT.begin(UART_BAUD)  // Inicia comunicación con módem
    └─ startGsm()                 // Inicia GSM básico
```

**Acciones:**
- 🔧 Inicializa configuración (servidor, APN, timeouts)
- 📡 Configura comunicación serial con el módem SIM7080G
- 📱 Verifica tarjeta SIM y funcionalidad de radio

---

### 2️⃣ **Obtención de Información SIM**

```
getIccid()
    ↓
    ├─ Lee ICCID de la tarjeta SIM
    ├─ getSignalQualityFromCESQ() → Obtiene métricas LTE
    │   ├─ Envía: AT+CESQ
    │   ├─ Parsea respuesta: +CESQ: rxlev,ber,rscp,ecno,rsrq,rsrp
    │   ├─ Extrae RSRQ (índice 4): 0-34
    │   ├─ Extrae RSRP (índice 5): 0-97 (fallback)
    │   ├─ Convierte a escala 0-31 (compatible con CSQ)
    │   └─ Fallback a AT+CSQ si falla
    └─ Actualiza variables globales (iccidsim0, signalsim0)
```

**Variables actualizadas:**
- `iccidsim0`: Identificador único de la SIM
- `signalsim0`: Calidad de señal normalizada (0-31, mayor es mejor)
  - **Fuente exclusiva:** AT+CESQ (NO se usa AT+CSQ)
  - Calculada desde RSRQ (Reference Signal Received Quality)
  - Fallback a RSRP (Reference Signal Received Power) si RSRQ no disponible
  - Valor 0 si CESQ falla completamente (sin métricas LTE disponibles)

---

### 3️⃣ **Preparación del Sistema de Archivos**

```
iniciarLittleFS()
    ↓
    ├─ Monta sistema de archivos LittleFS
    ├─ Verifica espacio disponible
    └─ Maneja reintentos (hasta 3 intentos)
```

**⚠️ Crítico:** Si falla, reinicia el ESP32

---

### 4️⃣ **Preparación y Encriptación de Datos**

```
dataSend(data)
    ↓
    ├─ Construye payload con:
    │   ├─ ICCID
    │   ├─ Datos de sensores (temperatura, humedad, etc.)
    │   ├─ Coordenadas GPS (si disponibles)
    │   └─ CRC16 para integridad
    ├─ Encripta con AES-128
    └─ Retorna cadena encriptada
```

**Estructura del payload:**
```
[ICCID][SENSOR_DATA][GPS_DATA][CRC16] → AES → [ENCRYPTED_DATA]
```

---

### 5️⃣ **Almacenamiento Local (Buffer)**

```
guardarDato(cadenaEncriptada)
    ↓
    ├─ Guarda en /buffer.txt (LittleFS)
    ├─ Gestión inteligente de espacio:
    │   ├─ Límite: MAX_LINEAS (10 líneas)
    │   └─ Si se excede: elimina datos más antiguos
    └─ Cada línea: timestamp + datos encriptados
```

**Propósito:** Respaldo local para reenvío en caso de fallo de conexión

---

### 6️⃣ **Estrategia de Conexión LTE** (`startLTE()`)

```
startLTE()
    ↓
    ├─ Configura modo de red (CAT-M/NB-IoT)
    ├─ Configura bandas LTE permitidas
    │
    ├─ showAvailableOperators() → Obtiene operadores disponibles (AT+COPS=?)
    │   └─ Llena array dinámico operators[]
    │
    └─ Prueba operadores SECUENCIALMENTE (orden detectado por COPS):
        
        Para cada operador:
        ↓
        ├─ Configura APN (AT+CGDCONT=1,"IP","apn") ← ⚠️ CRÍTICO ANTES DE CADA INTENTO
        │
        └─ connectAndSendWithOperator(i)
            ├─ cleanPDPContext() → Limpia sesiones PDP residuales
            │   ├─ Verifica AT+CNACT? primero
            │   ├─ Solo desactiva si hay PDP activo (evita error 500)
            │   └─ logCpsiInfo() si falla
            │
            ├─ AT+COPS=1,2,"código" → Conecta al operador
            │   └─ logCpsiInfo() si falla
            │
            ├─ waitForNetworkRegistration() → Espera +CEREG: 1 o 5
            │   ├─ Verifica AT+CEREG? periódicamente
            │   ├─ Timeout: 30 segundos
            │   ├─ Confirma registro antes de continuar
            │   └─ logCpsiInfo() si falla
            │
            ├─ AT+CNACT=0,1 → Activa PDP context
            │   └─ logCpsiInfo() si falla
            │
            ├─ verifyPDPActive() → Verifica IP asignada
            │   ├─ Ejecuta AT+CNACT?
            │   ├─ Busca: +CNACT: 0,1,"<IP>"
            │   ├─ Confirma PDP activo con IP válida
            │   └─ logCpsiInfo() si falla
            │
            ├─ ⚠️ CRÍTICO: Valida IP no sea 0.0.0.0 ni vacía
            │   ├─ modem.getIPAddress() != "0.0.0.0"
            │   ├─ modem.getIPAddress() != ""
            │   └─ logCpsiInfo() si IP inválida
            │
            ├─ enviarDatos() → ⚠️ GESTIONA TCP COMPLETO INTERNAMENTE
            │   ├─ tcpClose() + delay(300) → Limpia TCP residual
            │   ├─ tcpOpen() → Abre socket TCP limpio
            │   ├─ Envía TODOS los datos del buffer
            │   ├─ tcpClose() → Cierra socket TCP
            │   └─ Retorna true/false según éxito
            │
            ├─ ⚠️ Valida resultado de enviarDatos()
            │   └─ logCpsiInfo() si falla envío TCP
            │
            ├─ Verifica que buffer esté vacío
            │
            └─ Si éxito completo: RETORNA true
        
        Si falla un operador:
        ↓
        ├─ deregisterFromNetwork() → Desregistro apropiado
        │   ├─ AT+COPS=2
        │   ├─ Espera +CEREG: 0 (hasta 5s)
        │   └─ Si no llega: AT+CFUN=1,1 (reset rápido)
        │
        ├─ cleanPDPContext() → Limpia PDP
        └─ Prueba siguiente operador
            
        Si TODOS fallan → Intenta modo automático (AT+COPS=0)
        ↓
        ├─ cleanPDPContext()
        ├─ Configura APN
        ├─ AT+COPS=0
        ├─ waitForNetworkRegistration()
        ├─ AT+CNACT=0,1
        ├─ verifyPDPActive()
        └─ enviarDatos() → Gestiona TCP internamente
```

**Orden de Prioridad de Operadores:**
1. 🥇 **Operadores en orden COPS** (disponibles en la zona)
2. 🥈 **Sin evaluación previa** (la señal medida no representa el operador destino)
3. 🥉 **Prueba directa** con cada operador
4. 🏃 **Modo automático** como último recurso

**⚠️ IMPORTANTE - Por qué NO se evalúa señal por adelantado:**
- ❌ La señal medida con AT+CESQ pertenece a la RED ACTUAL (celda donde está registrado)
- ❌ NO refleja la señal que tendrá con el operador que vas a probar
- ❌ Genera falsos positivos (puede mostrar "Telcel excelente" pero en realidad es señal de AT&T)
- ✅ Mejor estrategia: probar directamente cada operador sin pre-evaluación

**Criterio de Éxito:** Se requiere:
1. ✅ Limpieza PDP exitosa (sin contextos residuales)
2. ✅ Conexión al operador (AT+COPS exitoso)
3. ✅ Registro en red confirmado (+CEREG: 1 o 5)
4. ✅ PDP activo con IP asignada (verificado con AT+CNACT?)
5. ✅ **IP válida (NO "0.0.0.0" ni vacía)**
6. ✅ Limpieza de TCP residual exitosa
7. ✅ Socket TCP abierto exitosamente (dentro de enviarDatos)
8. ✅ TODOS los datos del buffer enviados
9. ✅ Socket TCP cerrado correctamente (dentro de enviarDatos)
10. ✅ Buffer vacío tras envío

**⚠️ Gestión del Ciclo de Vida TCP:**
- **Responsabilidad única:** La función `enviarDatos()` gestiona TODO el ciclo TCP
- **Limpieza preventiva:** tcpClose() + delay(300ms) ANTES de tcpOpen()
- **Apertura:** tcpOpen() se ejecuta SOLO dentro de enviarDatos()
- **Cierre:** tcpClose() se ejecuta SOLO dentro de enviarDatos()
- **Validación:** enviarDatos() retorna bool (true=éxito, false=fallo)
- **Evita duplicados:** NO llamar tcpOpen/tcpClose fuera de enviarDatos()
- **Ventajas:** Previene conexiones corruptas, cierres duplicados, simplifica lógica, mayor robustez

**⏱️ Tiempo Estimado por Operador:** 
- Detección COPS: 30-120 segundos (una sola vez)
- Configuración APN: 1-2 segundos
- Limpieza PDP: 1-2 segundos
- Desregistro previo (CEREG): 5-8 segundos
- Conexión al operador: 5-20 segundos
- Registro en red (CEREG): 5-30 segundos
- Activación PDP: 2-5 segundos
- Verificación IP: 1-2 segundos
- Envío de datos (con TCP): 5-30 segundos
- **Total por operador: 25-100 segundos**

---

### 7️⃣ **Envío de Datos por TCP** (`enviarDatos()`)

```
enviarDatos()
    ↓
    ├─ 🧹 tcpClose() → CRÍTICO: Cierra TCP residual primero
    │   └─ delay(300ms) → Limpieza completa del socket
    │
    ├─ 🔌 tcpOpen() → Abre socket TCP limpio al servidor
    │   ├─ AT+CAOPEN=0,0,TCP,servidor,puerto
    │   ├─ Espera: +CAOPEN: 0,0 (éxito)
    │   └─ Timeout adaptativo según señal
    │
    ├─ Lee buffer local (/buffer.txt)
    │
    ├─ Para cada línea pendiente:
    │   ├─ Ignora líneas ya enviadas (#ENVIADO)
    │   ├─ tcpSendData(datos, timeout)
    │   │   ├─ Envía: AT+CASEND=0,<len>
    │   │   ├─ Espera: ">"
    │   │   ├─ Transmite datos
    │   │   └─ Espera confirmación: +CASEND: 0,0
    │   ├─ Si éxito: marca línea como #ENVIADO
    │   └─ Si fallo: conserva para reintento
    │
    ├─ 🔌 tcpClose() → Cierra socket TCP
    │   ├─ AT+CACLOSE=0
    │   ├─ Espera: +CACLOSE: 0,0 (éxito)
    │   └─ Libera recursos del módem
    │
    ├─ Reemplaza buffer con datos actualizados
    │
    └─ Retorna:
        ├─ true → Todos los datos enviados sin fallos
        └─ false → Hubo fallos de TCP/socket
```

**🔑 Características Clave:**
- **Gestión TCP Completa:** enviarDatos() es RESPONSABLE ÚNICO del ciclo TCP
- **Limpieza Preventiva:** Cierra TCP residual ANTES de abrir nuevo (previene corrupción)
- **Socket Único:** Un solo open/close por sesión de envío
- **Sin Duplicados:** Ninguna otra función debe llamar tcpOpen/tcpClose
- **Validación de Resultado:** Retorna bool para detectar fallos
- **Robustez:** Socket siempre se cierra al finalizar (éxito o fallo)

**Gestión de Errores:**
- ✅ Datos enviados → Se marcan con `#ENVIADO`
- ❌ Fallo de envío → Se conservan para próximo intento
- 🔄 Reintentos adaptativos según calidad de señal
- 🛡️ Socket siempre se cierra al finalizar (éxito o fallo)
- 📊 Retorna estado preciso para decisión de operador

---

### 8️⃣ **Limpieza del Buffer** (`limpiarEnviados()`)

```
limpiarEnviados()
    ↓
    ├─ Lee buffer.txt
    ├─ Filtra líneas que NO contengan #ENVIADO
    ├─ Reescribe buffer con solo datos pendientes
    └─ Libera espacio en LittleFS
## 🔄 Subsistema GPS (Opcional)

```
setupGpsSim(data)
    ↓
    ├─ startGps()
    │   ├─ Activa GPS (AT+CGNSPWR=1)
    │   └─ Configura modo GNSS
    │
    ├─ getGpsSim() → Obtiene coordenadas (hasta 100 reintentos)
    │   ├─ Lee posición (AT+CGNSINF)
    │   ├─ Valida fix GPS
    │   └─ Almacena: lat, lon, alt, velocidad, precisión
    │
    ├─ Convierte coordenadas a bytes (FloatToBytes)
    │
    └─ stopGps() → Desactiva GPS para ahorrar energía
```

---

## 🛡️ Funciones Auxiliares Críticas

### `deregisterFromNetwork()`
```
Desregistro seguro de red antes de cambiar operador
    ↓
    ├─ AT+COPS=2 → Solicita desregistro
    ├─ Espera URC: +CEREG: 0 (5 segundos)
    │   └─ Confirma desregistro exitoso
    │
    └─ Si timeout → AT+CFUN=1,1 (reset rápido)
        └─ Fuerza reinicio del módem para limpieza completa
```

**Propósito:** Evita problemas de "PDP already activated" y conflictos de operador

### `cleanPDPContext()`
```
Limpieza inteligente de contexto PDP
    ↓
    ├─ AT+CNACT? → Consulta estado actual
    │
    ├─ Si responde: +CNACT: 0,1,"<IP>"
    │   └─ AT+CNACT=0,0 → Desactiva PDP
    │
    └─ Si responde: +CNACT: 0,0 (ya desactivado)
        └─ No hace nada (evita error 500)
```

**Propósito:** Previene error 500 "PDP already activated" en próximos intentos

### `waitForNetworkRegistration()`
```
Verifica registro en red antes de activar datos
    ↓
    ├─ Polling: AT+CEREG? cada 2 segundos
    ├─ Busca: +CEREG: 0,1 (home) o +CEREG: 0,5 (roaming)
    ├─ Timeout: 30 segundos
    │
    └─ Retorna:
        ├─ true → Registrado exitosamente
        └─ false → No se pudo registrar
```

**Propósito:** Confirma que el módem está registrado en red antes de operaciones de datos

### `verifyPDPActive()`
```
Verifica que el contexto PDP esté activo con IP asignada
    ↓
    ├─ AT+CNACT? → Consulta estado PDP
    ├─ Busca: +CNACT: 0,1,"<dirección IP>"
    │
    └─ Retorna:
        ├─ true → PDP activo con IP válida
        └─ false → PDP inactivo o sin IP
```

**Propósito:** Confirma conectividad de datos antes de intentar abrir socket TCP
```
setupGpsSim(data)
    ↓
    ├─ startGps()
    │   ├─ Activa GPS (AT+CGNSPWR=1)
    │   └─ Configura modo GNSS
    │
    ├─ getGpsSim() → Obtiene coordenadas (hasta 100 reintentos)
    │   ├─ Lee posición (AT+CGNSINF)
    │   ├─ Valida fix GPS
    │   └─ Almacena: lat, lon, alt, velocidad, precisión
    │
    ├─ Convierte coordenadas a bytes (FloatToBytes)
    │
    └─ stopGps() → Desactiva GPS para ahorrar energía
```

---

## 📊 Sistema de Timeouts Adaptativos

### Función `getAdaptiveTimeout()`

El sistema implementa un algoritmo inteligente de cálculo de timeouts que se ajusta dinámicamente a las condiciones de red y al historial de comunicación. Esta optimización mejora tanto la velocidad de respuesta como la confiabilidad del sistema.

#### 🔧 Algoritmo Detallado

```
getAdaptiveTimeout()
    ↓
    1. TIMEOUT BASE
       └─ Valor inicial: 10,000ms (10 segundos)
          └─ Configurado en modemConfig.baseTimeout
    
    2. AJUSTE POR CALIDAD DE SEÑAL (signalsim0)
       │
       ├─ 📶 Señal EXCELENTE (signalsim0 > 20)
       │   ├─ Cálculo: baseTimeout * 0.6
       │   ├─ Reducción: 40% del tiempo
       │   ├─ Ejemplo: 10,000ms → 6,000ms
       │   └─ Razón: Red estable permite respuestas más rápidas
       │
       └─ 📶 Señal DÉBIL (signalsim0 < 10)
           ├─ Cálculo: baseTimeout * 1.2
           ├─ Incremento: 20% más tiempo
           ├─ Ejemplo: 10,000ms → 12,000ms
           └─ Razón: Red inestable requiere más tiempo de espera
    
    3. AJUSTE POR HISTORIAL DE FALLOS (consecutiveFailures)
       │
       ├─ Cálculo: baseTimeout += (consecutiveFailures * 1000)
       ├─ +1,000ms por cada fallo consecutivo
       │
       ├─ Ejemplos:
       │   ├─ 1 fallo: +1,000ms
       │   ├─ 2 fallos: +2,000ms
       │   ├─ 3 fallos: +3,000ms
       │   └─ 5 fallos: +5,000ms
       │
       └─ Razón: Problemas recurrentes necesitan estrategia más paciente
    
    4. APLICACIÓN DE LÍMITES
       │
       ├─ LÍMITE INFERIOR: 3,000ms (3 segundos)
       │   └─ Evita timeouts demasiado cortos que causen fallos
       │
       └─ LÍMITE SUPERIOR: 20,000ms (20 segundos)
           └─ Evita esperas excesivas que bloqueen el sistema
```

#### 📊 Ejemplos de Cálculo Real

**Escenario 1: Condiciones Ideales**
```
Señal: 25 (excelente)
Fallos consecutivos: 0
─────────────────────────────
Base: 10,000ms
Ajuste por señal: 10,000 * 0.6 = 6,000ms
Ajuste por fallos: 6,000 + 0 = 6,000ms
Límites aplicados: max(3000, min(6000, 20000)) = 6,000ms
─────────────────────────────
✅ Resultado: 6,000ms (6 segundos)
```

**Escenario 2: Señal Débil sin Fallos**
```
Señal: 8 (débil)
Fallos consecutivos: 0
─────────────────────────────
Base: 10,000ms
Ajuste por señal: 10,000 * 1.2 = 12,000ms
Ajuste por fallos: 12,000 + 0 = 12,000ms
Límites aplicados: max(3000, min(12000, 20000)) = 12,000ms
─────────────────────────────
✅ Resultado: 12,000ms (12 segundos)
```

**Escenario 3: Señal Buena con Múltiples Fallos**
```
Señal: 15 (normal - sin ajuste)
Fallos consecutivos: 3
─────────────────────────────
Base: 10,000ms
Ajuste por señal: 10,000ms (sin cambio)
Ajuste por fallos: 10,000 + (3 * 1000) = 13,000ms
Límites aplicados: max(3000, min(13000, 20000)) = 13,000ms
─────────────────────────────
✅ Resultado: 13,000ms (13 segundos)
```

**Escenario 4: Condiciones Extremas (Peor Caso)**
```
Señal: 5 (muy débil)
Fallos consecutivos: 8
─────────────────────────────
Base: 10,000ms
Ajuste por señal: 10,000 * 1.2 = 12,000ms
Ajuste por fallos: 12,000 + (8 * 1000) = 20,000ms
Límites aplicados: max(3000, min(20000, 20000)) = 20,000ms
─────────────────────────────
✅ Resultado: 20,000ms (20 segundos - límite máximo alcanzado)
```

**Escenario 5: Señal Excelente con 1 Fallo**
```
Señal: 28 (excelente)
Fallos consecutivos: 1
─────────────────────────────
Base: 10,000ms
Ajuste por señal: 10,000 * 0.6 = 6,000ms
Ajuste por fallos: 6,000 + (1 * 1000) = 7,000ms
Límites aplicados: max(3000, min(7000, 20000)) = 7,000ms
─────────────────────────────
✅ Resultado: 7,000ms (7 segundos)
```

#### 📈 Tabla de Referencia Rápida

| Calidad de Señal | Fallos | Timeout Base | Ajuste Señal | Ajuste Fallos | **Timeout Final** |
|------------------|--------|--------------|--------------|---------------|-------------------|
| Excelente (25)   | 0      | 10,000ms     | -40%         | +0ms          | **6,000ms** ⚡     |
| Excelente (25)   | 2      | 10,000ms     | -40%         | +2,000ms      | **8,000ms** ⚡     |
| Normal (15)      | 0      | 10,000ms     | 0%           | +0ms          | **10,000ms** 📊   |
| Normal (15)      | 3      | 10,000ms     | 0%           | +3,000ms      | **13,000ms** 📊   |
| Débil (8)        | 0      | 10,000ms     | +20%         | +0ms          | **12,000ms** 🐢   |
| Débil (8)        | 2      | 10,000ms     | +20%         | +2,000ms      | **14,000ms** 🐢   |
| Muy Débil (5)    | 5      | 10,000ms     | +20%         | +5,000ms      | **17,000ms** 🐌   |
| Muy Débil (5)    | 10+    | 10,000ms     | +20%         | +10,000ms     | **20,000ms** ⏸️   |

#### 🎯 Beneficios del Sistema Adaptativo

1. **⚡ Optimización de Velocidad**
   - Con señal excelente, reduce tiempos de espera innecesarios
   - Acelera el flujo de comunicación en condiciones ideales
   - Mejora la experiencia de usuario en zonas de buena cobertura

2. **🛡️ Robustez ante Problemas**
   - Aumenta paciencia cuando hay problemas de red
   - Evita fallos prematuros en condiciones adversas
   - Se adapta automáticamente a la calidad del entorno

3. **🔄 Aprendizaje del Historial**
   - Aprende de fallos anteriores
   - Ajusta estrategia basándose en experiencia reciente
   - Previene ciclos de fallos repetitivos

4. **⚖️ Balance Automático**
   - No requiere configuración manual
   - Encuentra el punto óptimo entre velocidad y confiabilidad
   - Se autoajusta continuamente según condiciones cambiantes

#### 🔍 Variables Involucradas

```c
// Calidad de señal actual (0-31) - calculada desde AT+CESQ
// Se obtiene de RSRQ (0-34) convertido a escala CSQ (0-31)
// Valores típicos:
// - Excelente: 20-31  (RSRQ > 23)
// - Buena: 15-19      (RSRQ 17-23)
// - Regular: 10-14    (RSRQ 12-16)
// - Débil: 5-9        (RSRQ 6-11)
// - Muy débil: 0-4    (RSRQ 0-5)
extern int signalsim0;

// Respuesta AT+CESQ: +CESQ: rxlev,ber,rscp,ecno,rsrq,rsrp
// - rxlev: Nivel de señal recibida (0-63, 99=desconocido)
// - ber: Tasa de error de bits (0-7, 99=desconocido)
## 🎯 Criterios de Éxito/Fallo

### ✅ Conexión Exitosa Cuando:
1. ✅ Se desregistra correctamente de red anterior (+CEREG: 0)
2. ✅ Se limpia contexto PDP sin errores
3. ✅ Se establece conexión con operador objetivo
4. ✅ Se confirma registro en red (+CEREG: 1 o 5)
5. ✅ Se activa contexto PDP exitosamente
6. ✅ Se verifica IP asignada (AT+CNACT?)
7. ✅ Se abre socket TCP correctamente (dentro de enviarDatos)
8. ✅ **Se envían TODOS los datos del buffer**
9. ✅ Se cierra socket TCP correctamente (dentro de enviarDatos)
10. ✅ Buffer queda vacío tras envío

### ❌ Se Considera Fallo Cuando:
1. ❌ No se puede desregistrar de red anterior (timeout +CEREG)
2. ❌ Error al limpiar PDP (error 500 indica problema residual)
3. ❌ No se puede registrar con operador objetivo
4. ❌ Falla verificación de registro (+CEREG timeout)
5. ❌ Falla activación de datos (PDP)
6. ❌ No se obtiene IP asignada
7. ❌ No se puede abrir socket TCP
8. ❌ Fallo en envío de datos por TCP
9. ❌ **Quedan datos pendientes en buffer**
10. ❌ Error al cerrar socket TCP
```

#### 🔧 Uso en el Código

El timeout adaptativo se utiliza en múltiples puntos críticos:

```cpp
// 1. Comandos AT generales
sendATCommand("+COPS=1,2,\"33403\"", "OK", getAdaptiveTimeout());

// 2. Apertura de conexiones TCP
tcpOpen();  // Usa getAdaptiveTimeout() internamente

// 3. Envío de datos
tcpSendData(datos, getAdaptiveTimeout());

// 4. Lectura de respuestas del módem
readResponse(getAdaptiveTimeout());
```

#### ⚠️ Consideraciones Importantes

- **Reset de Fallos:** El contador `consecutiveFailures` se resetea a 0 solo después de un envío completamente exitoso (buffer vacío)
- **Persistencia:** Los timeouts se recalculan en cada operación, reflejando siempre el estado actual
- **Límites Fijos:** Los límites de 3s-20s son absolutos y no pueden sobrepasarse sin importar las condiciones
- **Prioridad:** La seguridad y confiabilidad se priorizan sobre la velocidad en condiciones adversas

**Optimización automática:** El sistema encuentra el equilibrio perfecto entre velocidad y confiabilidad según las condiciones actuales de red y el historial de comunicación.

---

## 🔐 Sistema de Encriptación

```
Flujo de Encriptación:
    ↓
    ├─ Construye payload binario
    ├─ Agrega CRC16 al final
    ├─ Encripta con AES-128-ECB
    ├─ Codifica en hexadecimal
    └─ Prepara para envío TCP
```

**Clave AES:** Definida en `cryptoaes.h`

---

## 📝 Sistema de Logging

```
logMessage(level, message)
    ↓
    Niveles:
    ├─ 0: ❌ ERROR   (siempre visible)
    ├─ 1: ⚠️  WARN    (siempre visible)
    ├─ 2: ℹ️  INFO    (siempre visible)
    └─ 3: 🔍 DEBUG   (solo si enableDebug=true)
```

**Formato:** `[timestamp] [NIVEL]: mensaje`

---

## 🎯 Criterios de Éxito/Fallo

## 📌 Notas Importantes

1. **Estrategia Secuencial:** El sistema prueba operadores UNO POR UNO hasta lograr envío completo
2. **Buffer Persistente:** Los datos se guardan localmente antes de intentar envío
3. **Verificación de Envío:** Solo se considera éxito si el buffer queda vacío
4. **Timeouts Inteligentes:** Se adaptan dinámicamente según condiciones de red
5. **Ahorro de Energía:** El módem se apaga automáticamente al finalizar
6. **Desregistro Obligatorio:** Antes de cambiar operador, se desregistra completamente (+CEREG: 0)
7. **Limpieza PDP Inteligente:** Verifica estado antes de desactivar (evita error 500)
8. **Verificación de Registro:** Confirma +CEREG: 1 o 5 antes de activar datos
9. **Verificación de IP:** Confirma IP asignada antes de abrir socket TCP
10. **Gestión TCP Única:** enviarDatos() es el ÚNICO responsable de open/close TCP
11. **Sin Duplicados:** No llamar tcpOpen/tcpClose fuera de enviarDatos()
12. **APN por Intento:** Se configura APN antes de CADA intento de operador
### ❌ Se Considera Fallo Cuando:
1. No se puede registrar con operador
2. Falla activación de datos (PDP)
3. No se puede abrir socket TCP
4. Fallo en envío de datos por TCP
| Comando | Propósito | Criticidad |
|---------|-----------|------------|
| `AT+CNMP` | Configura modo de red | 🔧 Config |
| `AT+CMNB` | Configura banda (CAT-M/NB-IoT) | 🔧 Config |
| `AT+CESQ` | **Obtiene métricas LTE detalladas (RSRQ, RSRP) - ÚNICA FUENTE DE SEÑAL** | 📊 Monitoreo |
| `AT+COPS=?` | Lista operadores disponibles | 🔍 Detección |
| `AT+COPS=1,2,"código"` | Conecta a operador específico | ⚡ Crítico |
| `AT+COPS=2` | Desregistra de red | ⚡ Crítico |
| `AT+CEREG?` | **Verifica registro en red (EPS)** | ⚡ Crítico |
| `AT+CGDCONT=1,"IP","apn"` | **Configura APN antes de cada operador** | ⚡ Crítico |
| `AT+CNACT?` | **Consulta estado PDP y verifica IP** | ⚡ Crítico |
| `AT+CNACT=0,1` | Activa contexto PDP | ⚡ Crítico |
| `AT+CNACT=0,0` | Desactiva contexto PDP | 🧹 Limpieza |
| `AT+CAOPEN=0,0,TCP,IP,PORT` | Abre socket TCP | 🔌 Conexión |
| `AT+CASEND=0,<len>` | Envía datos por TCP | 📤 Transmisión |
| `AT+CACLOSE=0` | Cierra socket TCP | 🔌 Conexión |
| `AT+CFUN=1,1` | Reset rápido del módem | 🔄 Fallback |
| `AT+CPSI?` | Obtiene métricas de señal completas (info adicional) | 📊 Monitoreo |

**⚠️ Notas Importantes:** 
- El sistema NO utiliza `AT+CSQ` (comando legacy 2G/3G). Solo usa `AT+CESQ` para métricas LTE.
- `AT+CEREG?` es CRÍTICO para confirmar registro en red antes de activar datos.
- `AT+CNACT?` se usa para VERIFICAR estado PDP antes de desactivar (evita error 500).
- `AT+CGDCONT` debe ejecutarse ANTES de cada intento de conexión con operador.
## 📦 Estructura de Datos Principal

```c
sensordata_type {
    float temperatura
    float humedad
    float presion
    float co2
    float gps_latitude
    float gps_longitude
    float gps_altitude
---

## 🐛 Historial de Correcciones Críticas

### Versión 2.1 - Correcciones Implementadas

#### 1️⃣ **Problema: COPS=2 no desregistraba inmediatamente**
- **Síntoma:** Módem quedaba registrado tras AT+COPS=2
- **Causa:** Comando no espera confirmación de desregistro
- **Solución:** 
  - Implementada función `deregisterFromNetwork()`
  - Espera URC `+CEREG: 0` tras AT+COPS=2 (5s timeout)
  - Fallback a `AT+CFUN=1,1` si no llega confirmación

#### 2️⃣ **Problema: Error 500 "PDP already activated"**
- **Síntoma:** Fallos al activar PDP con error 500
- **Causa:** Intentaba desactivar PDP cuando ya estaba desactivado
- **Solución:**
  - Implementada función `cleanPDPContext()`
  - Verifica estado con `AT+CNACT?` antes de desactivar
  - Solo ejecuta `AT+CNACT=0,0` si detecta `+CNACT: 0,1`

#### 3️⃣ **Problema: APN no configurada correctamente**
- **Síntoma:** Fallos de PDP por APN incorrecta o ausente
- **Causa:** APN se configuraba una sola vez al inicio
- **Solución:**
  - Configura APN con `AT+CGDCONT` ANTES de cada operador
  - Se ejecuta en bucle principal de `startLTE()`

#### 4️⃣ **Problema: No verificaba registro en red (CEREG)**
- **Síntoma:** Intentaba activar PDP sin estar registrado
- **Causa:** No esperaba confirmación de registro EPS
- **Solución:**
  - Implementada función `waitForNetworkRegistration()`
  - Polling de `AT+CEREG?` hasta obtener `+CEREG: 0,1` o `+CEREG: 0,5`
  - Timeout de 30 segundos

#### 5️⃣ **Problema: No verificaba PDP activo con IP**
- **Síntoma:** Abría TCP sin confirmar conexión de datos
- **Causa:** Asumía PDP activo tras AT+CNACT=0,1
- **Solución:**
  - Implementada función `verifyPDPActive()`
  - Verifica `AT+CNACT?` retorne `+CNACT: 0,1,"<IP>"`
  - Confirma IP asignada antes de continuar

#### 6️⃣ **Problema: Cierre duplicado de TCP**
- **Síntoma:** `AT+CACLOSE=0` se ejecutaba dos veces
- **Causa:** Llamadas a `tcpClose()` en múltiples lugares
- **Solución:**
  - Removidas llamadas a `tcpClose()` de `connectAndSendWithOperator()`
  - `enviarDatos()` gestiona TODO el ciclo TCP internamente
  - Socket se abre y cierra UNA SOLA VEZ por sesión

#### 7️⃣ **Problema: Error de compilación "expected unqualified-id before 'else'"**
- **Síntoma:** Error de sintaxis en línea 1715
- **Causa:** Bloque `else` huérfano tras remover wrapper `if (tcpOpen())`
- **Solución:**
  - Reestructurado código en `connectAndSendWithOperator()`
  - Removido bloque `else` huérfano
  - Simplificada lógica de retorno

---

### Versión 2.2 - Correcciones Profesionales Adicionales

#### 8️⃣ **Problema: TCP residual no se cerraba antes de envío**
- **Síntoma:** Conexiones corruptas, SEND FAIL, envíos duplicados
- **Causa:** Socket TCP previo quedaba abierto por error (+TCPSTATE: CONNECTED)
- **Solución:**
  - Agregado `tcpClose()` forzado antes de `tcpOpen()` en `enviarDatos()`
  - Delay de 300ms para limpieza del socket
  - Previene conexiones corruptas silenciosas

#### 9️⃣ **Problema: IP 0.0.0.0 considerada válida**
- **Síntoma:** Socket TCP abierto pero sin envío real de datos
- **Causa:** `verifyPDPActive()` retornaba true con IP inválida
- **Solución:**
  - Verificación explícita: `modem.getIPAddress() == "0.0.0.0"`
  - Validación de IP vacía (`""`)
  - Rechazo de operador si IP inválida
  - Log de IP en conexión exitosa

#### 🔟 **Problema: No se validaba resultado de enviarDatos()**
- **Síntoma:** Fallos de TCP interpretados como "operador malo"
- **Causa:** `enviarDatos()` retornaba void, no había validación
- **Solución:**
  - Cambiada firma: `void enviarDatos()` → `bool enviarDatos()`
  - Retorna `true` si envío exitoso (sin fallos)
  - Retorna `false` si falla TCP o socket
  - Validación explícita en `connectAndSendWithOperator()`
  - Distinción entre fallo de red vs fallo de socket

#### 1️⃣1️⃣ **Problema: No se registraba causa exacta de fallo**
- **Síntoma:** Logs genéricos sin métricas detalladas
- **Causa:** No se llamaba `logCpsiInfo()` en cada punto de fallo
- **Solución:**
  - `logCpsiInfo()` agregado en TODOS los puntos de fallo:
    * Fallo limpieza PDP
    * Fallo COPS
    * Fallo CEREG
    * Fallo activación PDP
    * Fallo verificación PDP
    * Fallo IP inválida
    * Fallo envío TCP
  - Facilita diagnóstico preciso por operador

---

**Versión:** 2.2  
**Fecha:** 25 de Noviembre, 2025  
**Autor:** Elathia  
**Última actualización:** Correcciones profesionales TCP e IP, validación de envío

## 🚀 Secuencia Completa Resumida

```
1. setupModem(data)
   └─ Configura todo el sistema

2. Preparación
   ├─ Encripta datos
   └─ Guarda en buffer local

3. Conexión
   ├─ Obtiene operadores disponibles
   └─ Prueba cada uno secuencialmente

4. Envío
   ├─ Abre TCP
   ├─ Envía TODOS los datos
   └─ Verifica buffer vacío

5. Limpieza
   ├─ Elimina datos enviados
   └─ Apaga módem

6. Fin
   └─ Sistema listo para hibernación
```

---

## ⚙️ Configuración Clave

| Parámetro | Valor | Descripción |
|-----------|-------|-------------|
| `DB_SERVER_IP` | d04.elathia.ai | Servidor destino |
| `TCP_PORT` | 12607 | Puerto TCP |
| `APN` | "em" | APN del operador |
| `MODEM_NETWORK_MODE` | 38 | Modo LTE/GSM |
| `CAT_M` | 1 | Banda CAT-M |
| `SEND_RETRIES` | 5 | Reintentos máximos |
| `MAX_LINEAS` | 10 | Límite buffer local |

---

## 📌 Notas Importantes

1. **Estrategia Secuencial:** El sistema prueba operadores UNO POR UNO hasta lograr envío completo
2. **Buffer Persistente:** Los datos se guardan localmente antes de intentar envío
3. **Verificación de Envío:** Solo se considera éxito si el buffer queda vacío
4. **Timeouts Inteligentes:** Se adaptan dinámicamente según condiciones de red
5. **Ahorro de Energía:** El módem se apaga automáticamente al finalizar

---

## 🔧 Comandos AT Principales Usados

| Comando | Propósito |
|---------|-----------|
| `AT+CNMP` | Configura modo de red |
| `AT+CMNB` | Configura banda (CAT-M/NB-IoT) |
| `AT+CESQ` | **Obtiene métricas LTE detalladas (RSRQ, RSRP) - ÚNICA FUENTE DE SEÑAL** |
| `AT+COPS=?` | Lista operadores disponibles |
| `AT+COPS=1,2,"código"` | Conecta a operador específico |
| `AT+COPS=2` | Desregistra de red |
| `AT+CREG?` | Verifica registro en red |
| `AT+CNACT=1` | Activa contexto PDP |
| `AT+CAOPEN=0,0` | Abre socket TCP |
| `AT+CASEND=0,<len>` | Envía datos por TCP |
| `AT+CACLOSE=0` | Cierra socket TCP |
| `AT+CPSI?` | Obtiene métricas de señal completas (info adicional) |

**Nota Importante:** El sistema NO utiliza `AT+CSQ` (comando legacy 2G/3G). Solo usa `AT+CESQ` para obtener métricas LTE precisas y reales.

---

## 📈 Métricas de Señal Monitoreadas

- **RSRP** (Reference Signal Received Power): -140 a -44 dBm
- **RSRQ** (Reference Signal Received Quality): -20 a -3 dB
- **RSSI** (Received Signal Strength Indicator): Intensidad de señal
- **SNR** (Signal to Noise Ratio): -20 a 30 dB

---

**Versión:** 2.0  
**Fecha:** 30 de Octubre, 2025  
**Autor:** Elathia
