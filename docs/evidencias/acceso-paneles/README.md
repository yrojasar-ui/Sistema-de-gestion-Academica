# Evidencia de acceso y paneles

Pruebas locales del 16 de noviembre de 2026 (UTC registra 17 de noviembre).
Cierre y revisión: 17 de noviembre. Datos sintéticos aislados; ninguna escritura
en API docente durante esta fase. No se fusiona ni despliega el backend.

| Comprobación | Resultado observado | Registro |
|---|---|---|
| CTest Debug, regresión completa | 14/14, retorno 0 | [Log completo](regresion/debug-ctest.log) |
| CTest ASan/UBSan, fugas y errores detenidos | 14/14, retorno 0 | [Log completo](regresion/asan-ctest.log) |
| Consola E2-A y catálogo inicial | Demo correcta; inicial intacto | [Demo](regresion/demo.log), [retornos](regresion/resultados.tsv) |
| Ampliación posterior de pruebas de acceso/Mongo | 2/2 Debug y 2/2 ASan/UBSan | [Debug](debug-adicional.log), [ASan](asan-adicional.log) |
| API final y demo reproducible | 177 solicitudes; dos reinicios, propiedad, roles y fallos sin parcial | [API](api-final.log), [demo](demo.log) |
| Frontend Node 24.22.3 | npm ci; 9/9 pruebas; TypeScript/Vite y recursos Pages correctos | [Log](frontend.log) |
| Contrato OpenAPI | 442 respuestas de regresión y 146 de acceso validadas contra esquemas; login/token excluido del registro | [Log](openapi.log) |
| Workflows | actionlint retornó 0, sin diagnósticos | [Salida vacía](actionlint.log) |
| Navegador real integrado | Recorrido completo y reinicio; viewport estrecho | [Operaciones](navegador.md), capturas abajo |
| GitHub Actions | Controles del HEAD publicado en PR #11 | [Seguimiento](actions.md) |

La regresión completa se ejecutó antes de ampliar los casos de pago Mongo,
migración local y fallo del almacén privado. Las ampliaciones están verificadas
en los logs adicionales. La compilación final y API final incluyen la comprobación
de error al ocultar la contraseña del bootstrap. CI ejecuta toda la suite sobre
el HEAD publicado, sin usar la API docente.

## Capturas auténticas

- [Solicitud de Ana apartada, sin préstamo activo](01-solicitud-ana.png).
- [Comprobante PG-1 de Ana, ₡100,00 SIMULADO](02-pago-simulado.png).
- [Préstamo P-DEMO devuelto y solicitud atendida tras reiniciar](03-historial-tras-reinicio.png).

Se capturaron directamente del navegador, sin composición ni datos inventados.
Los usuarios, libro editado, ejemplar y préstamo pertenecen a una biblioteca
temporal en build. Las contraseñas de prueba son aleatorias, privadas y no
están versionadas. Las capturas no contienen contraseñas ni tokens.

## Qué se comprobó y qué no

**Local real:** Argon2id y sesiones, login inválido/genérico, expiración,
logout, límites de intentos, bloqueo de rutas administrativas, A sin recursos
de B, flujo solicitud/entrega/pago/devolución, cancelación FIFO, anulación
auditada, reintento de pago, dos copias, renovación, edición B+/grafo, referencias,
reinicios de cuentas/negocio y fallos de guardado conservando estado.

**Fixtures Mongo:** lectura fallida, timeout aplicado/tardío, HTTP 200 con HTML,
rechazo y escritura parcial; diario durable/bloqueo/recuperación; migración
explícita v1/v2; cargos y comprobantes reconstruidos e idempotencia.
No se presenta esto como una prueba cloud nueva.

**Pendiente externo:** API HTTPS pública, proxy de confianza y sus límites,
integración real Pages/API remota, RabbitMQ/worker y entrega de avisos.
La API docente sigue sin garantías de autenticación, aislamiento, transacciones
o soporte multiinstancia. Un solo escritor; credenciales fuera de ella.
No se cobran pagos reales. No se hizo auditoría externa ni prueba de carga.

La advertencia de indentación de HttpController observada en el build ASan
inicial se corrigió separando la respuesta HTTP del if; no cambia el comportamiento.
