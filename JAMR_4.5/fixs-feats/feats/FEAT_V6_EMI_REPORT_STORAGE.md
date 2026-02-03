# FEAT-V6: Almacenamiento de Reportes EMI en LittleFS

---

## 📋 INFORMACIÓN GENERAL

| Campo | Valor |
|-------|-------|
| **ID** | FEAT-V6 |
| **Tipo** | Feature (Diagnóstico) |
| **Sistema** | LTEModule / LittleFS |
| **Archivos** | `src/data_lte/LTEModule.cpp`, `AppController.cpp` |
| **Estado** | 📝 Documentado (pendiente implementar) |
| **Fecha** | 2026-01-28 |
| **Prioridad** | Baja |
| **Depende de** | FEAT-V5 (Diagnóstico EMI) |

---

## 🔍 PROBLEMA

Con FEAT-V5, los reportes EMI solo se muestran por Serial.
- Si no hay monitor conectado, se pierden
- No hay histórico para análisis posterior
- Difícil correlacionar con eventos de campo

---

## 🎯 OBJETIVO

Guardar reportes EMI en LittleFS para:
1. Persistencia entre reinicios
2. Lectura automática al conectar Serial
3. Historial de diagnóstico

---

## 📐 DISEÑO SIMPLIFICADO

### Estructura de archivos

```
/emi/
  report_001.txt
  report_002.txt
  ...
  report_006.txt    ← Rotación circular (max 6 = 24h)
```

> **Sin index.txt** - usar contador simple en memoria que rota 1→6→1

### Contenido de cada reporte (~600 bytes)

```
=== EMI REPORT #3 ===
FW: v2.5.0
Reset: POWERON
VBAT: 4.12V
CSQ: 18
Timestamp: 1769612345
Ciclos: 20
AT_total: 312
AT_ok: 298 (95.5%)
AT_error: 8 (2.6%)
AT_timeout: 6 (1.9%)
Corrupted: 0 (0.0%)
Invalid_chars: 0
Bytes_total: 4523
Time_min_ms: 12
Time_max_ms: 1250
Time_avg_ms: 85
Diagnostic: OK
```

### Implementación

```cpp
// Rotación SIMPLE (sin index.txt)
static uint8_t s_reportNum = 1;  // 1-6, circular

void saveEMIReport() {
    LittleFS.mkdir("/emi");  // Crea si no existe
    
    char path[24];
    snprintf(path, sizeof(path), "/emi/report_%03d.txt", s_reportNum);
    
    File f = LittleFS.open(path, "w");
    if (!f) {
        Serial.println("[EMI] Warning: no pude guardar");
        return;  // No crítico - continuar
    }
    
    // Contexto de campo (valor alto, costo cero)
    f.printf("=== EMI #%d ===\n", s_reportNum);
    f.printf("FW: %s\n", FIRMWARE_VERSION);
    f.printf("Reset: %s\n", getResetReasonStr());
    f.printf("VBAT: %.2fV\n", getBatteryVoltage());
    f.printf("CSQ: %d\n", g_lastCSQ);
    f.printf("Timestamp: %lu\n", getEpochTime());
    // ... stats EMI de FEAT-V5 ...
    f.close();
    
    s_reportNum = (s_reportNum % 6) + 1;  // 1→2→3→4→5→6→1
}

// Al boot: solo resumen (no dump completo)
void printEMISummary() {
    int count = 0;
    for (int i = 1; i <= 6; i++) {
        char path[24];
        snprintf(path, sizeof(path), "/emi/report_%03d.txt", i);
        if (LittleFS.exists(path)) count++;
    }
    if (count > 0) {
        Serial.printf("[EMI] %d reportes guardados\n", count);
    }
}
```

### Comportamiento

1. **Cada 20 ciclos**: Guarda reporte en `/emi/report_NNN.txt`
2. **Save-on-anomaly**: Si `corrupted > 0`, guarda aunque no toque múltiplo de 20
3. **Al boot**: Solo resumen ("3 reportes EMI guardados")
4. **Rotación**: Sobrescribe el más antiguo (circular 1-6)

---

## 💾 RECURSOS

| Recurso | Uso estimado |
|---------|--------------|
| LittleFS | ~4 KB (6 reportes × 600 bytes) |
| RAM | Mínimo (f.printf directo) |
| Tiempo | ~50ms por escritura |
| Flash wear | 6 writes/día = 2190/año (< 100K límite) |

---

## ⚠️ CONSIDERACIONES

1. **No crítico**: Si falla escritura, solo log warning y continuar
2. **Sin sobreingeniería**: No index.txt, no escritura atómica, no comandos especiales
3. **Compatibilidad**: No afecta buffer de tramas existente

---

## 🧪 CRITERIOS DE ACEPTACIÓN

- [ ] Reportes se guardan cada 20 ciclos (o en anomalía)
- [ ] Al boot, muestra resumen (no dump)
- [ ] Rotación funciona (max 6 archivos)
- [ ] Incluye contexto: FW, Reset, VBAT, CSQ
- [ ] Fallo de escritura no bloquea el ciclo

---

## ❌ DECISIONES DE DISEÑO (NO implementar)

| Descartado | Razón |
|------------|-------|
| index.txt | Complejidad innecesaria - contador simple basta |
| Escritura atómica (.tmp+rename) | 500 bytes cada 4h, riesgo brownout ~0% |
| Reconstruir index escaneando | Solo 6 archivos, si falla → reiniciar en 001 |
| Suite de pruebas power-loss | Feature baja prioridad, no justifica |
| Comandos EMI:DUMP/CLEAR | No existe infraestructura, dump manual si se necesita |

---

## 📈 ALTERNATIVAS FUTURAS

1. **Enviar resumen por LTE**: Agregar campo EMI a trama
2. **Acceso BLE**: Descargar reportes via Bluetooth
3. **Compresión**: Si se necesita más histórico

---

## 🔗 REFERENCIAS

- FEAT-V5: Sistema de Diagnóstico (implementado)
- CrashDiagnostics: Usa patrón similar de almacenamiento
