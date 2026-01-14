# Hallazgos Pendientes - Futuros FIXs y FEATs

**Proyecto:** sensores_rv03  
**Última Actualización:** 2026-01-13  
**Origen:** Auditoría de Trazabilidad Requisitos-Código (ATRC)

---

## 🔴 Prioridad Crítica

### FIX-V2: Fallback a Escaneo de Operadoras
- **Estado:** 📋 Documentado, pendiente implementación
- **Requisito:** RF-12
- **Archivo Doc:** `fixs-feats/fixs/FIX_V2_FALLBACK_OPERADORA.md`
- **Descripción:** Si `configureOperator()` falla con operadora guardada, debe escanear alternativas
- **Síntoma:** Dispositivo queda offline tras cambio de zona geográfica

---

### FIX-V3: Modo Solo-Adquisición por Baja Batería
- **Estado:** 📋 Por documentar
- **Requisito:** RF-06, RF-09
- **Descripción:** Implementar UTS (Umbral Transmisión Segura) y desactivar modem cuando batería baja
- **Componentes necesarios:**
  - Definir UTS (ej: 3.3V)
  - Lógica de comparación voltaje batería vs UTS
  - Histéresis para evitar ciclos ON/OFF rápidos
  - Persistencia del modo en NVS
- **Síntoma potencial:** Dispositivo muere completamente sin enviar datos acumulados

---

### FIX-V4: Limitación de Escaneos de Operador
- **Estado:** 📋 Por documentar
- **Requisito:** RF-14
- **Descripción:** Máximo 3 escaneos completos por día para ahorrar batería
- **Componentes necesarios:**
  - Contador de escaneos en NVS
  - Reset diario (usando RTC)
  - Lógica de bypass para eventos excepcionales
- **Síntoma potencial:** Batería agotada en zonas sin cobertura por escaneos repetidos

---

## 🟠 Prioridad Alta

### FIX-V5: Protección Brown-out
- **Estado:** 📋 Por documentar
- **Requisito:** RNF-02
- **Descripción:** Entrar a modo seguro si voltaje cae bajo UMO (Umbral Mínimo Operativo)
- **Componentes necesarios:**
  - Definir UMO (ej: 3.0V)
  - Detección de caída de voltaje
  - Modo seguro: solo mantener RTC y memoria
  - Garantizar escrituras completas antes de entrar

---

### FIX-V6: Protocolo de Recuperación Escalonado
- **Estado:** 📋 Por documentar
- **Requisito:** RF-15
- **Descripción:** Recuperación en <120s con escalamiento: UART → modem → reinicio parcial
- **Componentes necesarios:**
  - Detector de estado zombie (timeouts consecutivos)
  - Nivel 1: Reset UART
  - Nivel 2: Reset modem (CFUN=1,1)
  - Nivel 3: Reinicio completo ESP32
  - Preservar datos en todos los niveles

---

### FEAT-V3: Sistema de Logs Críticos
- **Estado:** 📋 Por documentar
- **Requisito:** RF-05
- **Descripción:** Almacenar últimos 10 eventos críticos en memoria persistente
- **Eventos a registrar:**
  - Watchdog triggers
  - Brown-out detectados
  - Fallos de modem
  - Fallos RS-485
  - Reinicios inesperados
- **Formato sugerido:** timestamp + código_evento + contexto (32 bytes/evento)

---

### FEAT-V4: CLI de Mantenimiento Serial
- **Estado:** 📋 Por documentar
- **Requisito:** RF-16, RF-17
- **Descripción:** Interfaz de comandos por puerto serial para diagnóstico
- **Comandos mínimos:**
  - `STATUS` - Estado general del sistema
  - `BATTERY` - Voltaje actual de batería
  - `RTC` - Fecha/hora actual
  - `MEMORY` - Uso de LittleFS
  - `MODEM` - Estado del modem
  - `BUFFER` - Líneas en cola de transmisión
  - `EXPORT` - Exportar todo el buffer
  - `LOGS` - Ver logs críticos
- **Criterio:** Respuesta <2s por comando

---

## 🟡 Prioridad Media

### FEAT-V5: Conexión TLS/SSL
- **Estado:** 📋 Por documentar
- **Requisito:** RNF-03
- **Descripción:** Migrar de TCP plano a TLS 1.2+
- **Cambios necesarios:**
  - Usar `AT+CASSLCFG` para configurar SSL
  - Cambiar `AT+CAOPEN` a modo SSL
  - Gestionar certificados (o usar SNI sin verificación)
  - Criterio: handshake <8s en 80% de conexiones
- **Complejidad:** Alta (certificados, memoria, timeouts)

---

### FEAT-V6: Validación de Éxito RS-485
- **Estado:** 📋 Por documentar
- **Requisito:** RF-01
- **Descripción:** Contador de tasa de éxito (≥98% en 3 intentos)
- **Componentes:**
  - Contador de intentos
  - Contador de éxitos
  - Alarma si tasa <98%

---

## 📊 Resumen de Backlog

| ID | Tipo | Prioridad | Requisito | Estado |
|----|------|-----------|-----------|--------|
| FIX-V2 | Fix | 🔴 Crítica | RF-12 | Documentado |
| FIX-V3 | Fix | 🔴 Crítica | RF-06, RF-09 | Por documentar |
| FIX-V4 | Fix | 🔴 Crítica | RF-14 | Por documentar |
| FIX-V5 | Fix | 🟠 Alta | RNF-02 | Por documentar |
| FIX-V6 | Fix | 🟠 Alta | RF-15 | Por documentar |
| FEAT-V3 | Feat | 🟠 Alta | RF-05 | Por documentar |
| FEAT-V4 | Feat | 🟠 Alta | RF-16, RF-17 | Por documentar |
| FEAT-V5 | Feat | 🟡 Media | RNF-03 | Por documentar |
| FEAT-V6 | Feat | 🟡 Media | RF-01 | Por documentar |

---

## 📝 Proceso para Nuevos Hallazgos

1. Identificar requisito incumplido
2. Agregar a este archivo con estado "Por documentar"
3. Crear archivo completo en `fixs-feats/fixs/` o `fixs-feats/feats/`
4. Actualizar `AUDITORIA_REQUISITOS.md` con referencia
5. Implementar según metodología de FIXs
6. Marcar como completado en ambos documentos
