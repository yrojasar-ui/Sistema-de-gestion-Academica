# Evidencias de preparación y verificación para la defensa

Generadas sobre una instancia completamente aislada (`build/demo-defensa/`, sin modificar cuentas existentes ni la API docente MongoDB) con [`scripts/preparar_demo_defensa.py`](../../../scripts/preparar_demo_defensa.py).

## Log verificado
- [verificacion-defensa.log](verificacion-defensa.log): preparación del dataset completo (17 avisos procesados por `biblioteca_worker` vía RabbitMQ 4.0.5, 4 comprobantes, 4 préstamos activos incluyendo fixture histórico vencido, 2 devoluciones, 1 anulación de cargo pagado conservando comprobante, reservas pendiente/apartada/atendida/cancelada, `E-DEF-5`/`E-DEF-6` disponibles y cargo pendiente `P-DEF-VIG` reservado para demo en vivo) + verificación de dos sesiones (`Ana` + `Admin`), reinicio real de `catalogo_api` y aislamiento de `Bruno`.

## Capturas auténticas en navegador Chrome (sin credenciales)
1. [01-admin-pagos-filtros.png](01-admin-pagos-filtros.png) — **Admin → Pagos simulados**: filtros por estado (`Pendientes: 3`, `Pago simulado realizado: 3`, `Anulados: 1`), lector, libro, importe, fecha UTC y comprobantes.
2. [02-admin-prestamos-ordenados.png](02-admin-prestamos-ordenados.png) — **Admin → Préstamos**: activos ordenados por vencimiento con estado UTC (`Vencido`, `Vence pronto`, `Vigente`) y libro/lector.
3. [03-ana-mi-biblioteca.png](03-ana-mi-biblioteca.png) — **Ana → Mi biblioteca**: préstamos ordenados por vencimiento UTC y solicitudes.
4. [04-ana-mis-pagos-confirmado.png](04-ana-mis-pagos-confirmado.png) — **Ana → Mis pagos simulados**: confirmación persistente *«Usted ha pagado. Pago simulado registrado.»*, comprobantes y cargo pendiente `P-DEF-VIG` listo para la demo en vivo.
5. [05-ana-mis-avisos-enlace-pagos.png](05-ana-mis-avisos-enlace-pagos.png) — **Ana → Mis avisos**: orden descendente por fecha y botón directo *Ir a Mis pagos* en el aviso de pago (`pago_simulado`).
6. [06-bruno-aislamiento-cola.png](06-bruno-aislamiento-cola.png) — **Bruno → Mi biblioteca**: aislamiento verificado (sin datos de Ana) y solicitud propia en cola FIFO.
