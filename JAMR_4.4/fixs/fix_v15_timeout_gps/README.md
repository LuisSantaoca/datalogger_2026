# FIX v15: Timeout Real de GPS (v4.4.14 → v4.4.15)

## 📋 Resumen

**Versión:** v4.4.14 validacion-pdp → v4.4.15 timeout-gps  
**Fecha identificación:** 14 Dic 2025 (fallos GPS 57s)  
**Problema resuelto:** Timeout de GPS declarado pero no aplicado en el loop de intentos (50×5s → hasta 250s)  
**Estado:** 📝 Documentado – pendiente implementación  
**Prioridad:** 🔴 CRÍTICA (riesgo de ciclos > watchdog y consumo elevado)

---

## 🎯 Problema Identificado

`getGpsIntegrated()` define `GPS_TOTAL_TIMEOUT_MS = 30000`, pero el bucle de 50 intentos no valida el tiempo transcurrido. El resultado real son búsquedas de 57s+ en fallos de fix, excediendo el límite esperado de 30s.

**Evidencia de log (14-Dic-2025 3:46 AM):**
```
[20847ms] GPS habilitado
[31927ms] Intento 1
[57577ms] Intento 6
[57577ms] ⚠️ Timeout total tras 6 intentos (real ~57s)
```

**Riesgos:**
- Ciclos largos (~57s GPS + LTE) que se acercan al watchdog de 90s
- Consumo adicional ~16 mAh por ciclo fallido (80mA × 57s)
- Menor autonomía y mayor probabilidad de reset en campo rural

---

## 🔍 Root Cause Analysis

- La constante `GPS_TOTAL_TIMEOUT_MS` no se usa dentro del loop.
- El loop ejecuta hasta 50 intentos con `delay(5000)`, pudiendo llegar a 250s.
- No hay corte temprano por tiempo real, sólo por número de intentos.

---

## ✅ Solución Propuesta (FIX-15)

### Aplicar timeout global en el bucle GPS
1) Registrar `startTime = millis()` al entrar a `getGpsIntegrated()`.
2) Antes de cada `delay(5000)` verificar si `millis() - startTime >= GPS_TOTAL_TIMEOUT_MS`.
3) Si se alcanza el timeout: log de warning `[FIX-15]` y salir del loop.
4) Conservar máximo 50 intentos, pero el tiempo manda.

### Código esperado (conceptual)
```cpp
bool getGpsIntegrated() {
  unsigned long startTime = millis();

  for (int i = 0; i < 50; i++) {
    if (gpsObtained) break;

    if (millis() - startTime >= GPS_TOTAL_TIMEOUT_MS) {
      logWarn("[FIX-15] Timeout GPS global %lums alcanzado", GPS_TOTAL_TIMEOUT_MS);
      break;
    }

    // Esperar 5s entre intentos
    delay(5000);
  }

  if (!gpsObtained) {
    logWarn("[FIX-15] GPS sin fix tras %lums", millis() - startTime);
  }
}
```

### Parámetros
- `GPS_TOTAL_TIMEOUT_MS = 30000` (ya definido en FIX-13)
- Máx. intentos: 50 (sin cambio), pero cortado por tiempo.

---

## 📊 Impacto Esperado

| Métrica | Antes | Después | Mejora |
|---------|-------|---------|--------|
| Tiempo máximo búsqueda | 57s (observado) | 30s | -27s |
| Riesgo watchdog 90s | Alto | Bajo | -
| Consumo en falla | ~16 mAh | ~8 mAh | -50% |
| Duración ciclo típica | 160-280s (en fallos) | <200s | -20-40% |

---

## 🧪 Plan de Validación

1) **Compilar y cargar v4.4.15.**
2) **Logs:** Confirmar líneas `[FIX-15] Timeout GPS global 30000ms` cuando no hay fix.
3) **Ciclos fallidos:** Medir duración total: debe bajar a <200s.
4) **Watchdog:** Verificar que no se acerque a 90s; margen >30s.
5) **Consumo:** Registrar caída de mAh por ciclo fallido (~-8 mAh).

---

## ⚠️ Riesgos y Mitigación

- **Timeout demasiado corto en cielos cerrados:** Si 30s resulta insuficiente en ciertas ubicaciones, ajustar a 40s tras prueba de campo.
- **Salidas tempranas con fix tardío:** Si se detecta que fixes reales requieren >30s, considerar lógica de dos fases (20s rápido + 20s extendido sólo si señal > umbral).

---

## 📦 Archivos a modificar

- `JAMR_4.4/gsmlte.cpp`: función `getGpsIntegrated()` (aplicar corte por tiempo en el loop de 50 intentos).

---

## 🚀 Siguientes pasos

1) Implementar FIX-15 en `gsmlte.cpp`.
2) Compilar y subir firmware v4.4.15.
3) Ejecutar 5 ciclos en campo rural y medir duración y logs.
4) Ajustar timeout a 30-40s según resultados.

---

**Documento creado:** 14 Dic 2025  
**Estado:** Pendiente implementación
