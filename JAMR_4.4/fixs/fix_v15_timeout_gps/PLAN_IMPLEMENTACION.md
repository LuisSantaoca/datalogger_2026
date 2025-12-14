# Plan de Implementación FIX-15: Timeout Real de GPS (30s)

## 🎯 Objetivo
Aplicar el timeout global `GPS_TOTAL_TIMEOUT_MS` (30s) en el loop de `getGpsIntegrated()` para evitar búsquedas >30s.

## ✅ Checklist
- [ ] Localizar `getGpsIntegrated()` en `JAMR_4.4/gsmlte.cpp` (sección GPS integrado SIM7080).
- [ ] Confirmar constante `GPS_TOTAL_TIMEOUT_MS = 30000` está presente.
- [ ] Añadir `startTime = millis()` al inicio de la función (si no existe).
- [ ] Dentro del `for (int i = 0; i < 50; i++)` verificar timeout **antes** del `delay(5000)`.
- [ ] Loggear `[FIX-15] Timeout GPS global ...` al alcanzar el límite.
- [ ] Loggear duración real cuando no hay fix.
- [ ] Compilar y verificar 0 errores.

## ✏️ Código sugerido (parche)
```cpp
bool getGpsIntegrated() {
    unsigned long startTime = millis();
    gpsObtained = false;

    for (int i = 0; i < 50; i++) {
        if (gpsObtained) break;

        // ✅ FIX-15: aplicar timeout global de 30s
        if (millis() - startTime >= GPS_TOTAL_TIMEOUT_MS) {
            logWarn("[FIX-15] Timeout GPS global %lums alcanzado", GPS_TOTAL_TIMEOUT_MS);
            break;
        }

        // ... lógica de intento de fix ...

        delay(5000);  // espera entre intentos
    }

    if (!gpsObtained) {
        logWarn("[FIX-15] GPS sin fix tras %lums", millis() - startTime);
    }

    return gpsObtained;
}
```

## 📍 Ubicación en archivo
- Buscar sección GPS alrededor de `getGpsIntegrated()` en `gsmlte.cpp` (cerca de los intentos 1..50).
- El loop actual tiene `for (int i = 0; i < 50; i++)` con `delay(5000)`.

## 🧪 Validación
- Logs deben mostrar `[FIX-15] Timeout GPS global 30000ms alcanzado` cuando no haya fix.
- Duración de fase GPS debe ser <30s (en fallos) + overhead mínimo.
- Ciclo total debe quedar <200s incluso en fallos GPS.
- Sin watchdog triggers (90s) en fase GPS.

## ⏱️ Tiempo estimado
- Edición: 5 minutos
- Compilación: 2 minutos
- Prueba rápida: 2 ciclos (40 min con sleep 1200s) o 1 ciclo en entorno de prueba acelerado.

## 🚨 Rollback
- Revertir el bloque FIX-15 o comentar la verificación de timeout si se requiere extender a 40s; recompilar.

**Plan creado:** 14 Dic 2025  
**Estado:** Listo para implementar
