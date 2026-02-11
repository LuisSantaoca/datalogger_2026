# PREMISAS PARA IMPLEMENTACIÓN DE FIXES
## Guía de Desarrollo Seguro para sensores_4.5_luz

**Creado:** 2026-02-10  
**Heredado de:** JAMR_4.5 `fixs-feats/PREMISAS_DE_FIXS.md`  
**Proyecto:** sensores_4.5_luz (ESP32-S3 + SIM7080G)  
**Aplicable a:** Todos los fixes desde FIX-V1 en adelante

---

## Filosofía Central

> **"Si no lo toco, no lo rompo. Si lo toco, lo valido. Si falla, lo deshabilito."**

Cada fix debe ser:
- **Aditivo** — Agregar funcionalidad, no cambiar existente
- **Defensivo** — Defaults seguros si algo falla
- **Reversible** — Rollback en < 5 minutos vía feature flag
- **Validado** — Testing gradual antes de despliegue
- **Documentado** — Trazabilidad completa en `fixs-feats/fixs/`

## Reglas de Implementación

1. **Un branch por fix** — `fix-v1/nombre`, merge solo después de validación
2. **Cambios mínimos** — Solo archivos directamente relacionados
3. **Feature flag obligatorio** — `ENABLE_FIX_Vn_NOMBRE` con `#else` preservando original
4. **Logging exhaustivo** — Formato `[NIVEL][MODULO] Mensaje`
5. **No cambiar lógica existente** — Agregar condicionales, nunca borrar
6. **Marcadores de código** — `// FIX-Vn` línea, `// [FIX-Vn START/END]` bloque
