# Acceso, paneles y pagos simulados

Fase autorizada el 16 de noviembre de 2026. Implementación en curso; los
resultados se registran en evidencias/acceso-paneles, sin atribuir a la nube
las pruebas con fixtures.

## Transiciones antes de implementar

| Entidad | Origen → destino | Condición y efecto |
|---|---|---|
| Reserva (también solicitud) | pendiente → apartada | FIFO asigna una copia disponible a la primera reserva válida. |
| Reserva | apartada → atendida | El administrador entrega esa copia al mismo lector y abre el préstamo. |
| Reserva | pendiente/apartada → cancelada | Motivo, fecha y responsable conservados; una copia apartada pasa al siguiente turno. |
| Préstamo | activo → devuelto | Devolución física; conserva historial, elimina clave del Rojo-Negro y atiende FIFO. |
| Préstamo | activo → cancelado | Solo administrador, motivo y confirmación explícita de copia no entregada o recuperada. No registra devolución ficticia. |
| Préstamo | activo → activo | Renovación: elimina/reinserta la clave de vencimiento. |
| Cargo | pendiente → pagado | Pago SIMULADO, un comprobante por préstamo; reintentar devuelve el mismo comprobante. |
| Cargo | pendiente/pagado → anulado | Anulación del préstamo; el comprobante anterior se conserva y su cargo queda anulado. No se cobra ni se reembolsa dinero real. |

Un préstamo devuelto no puede anularse. Un préstamo cancelado no puede
devolverse ni renovarse. Pagar no altera el ejemplar. Las reservas canceladas
siguen siendo reservas, nunca préstamos devueltos. No hay nueva entidad
“solicitud”: es una reserva con estado pendiente/apartada/atendida/cancelada.

Cargo demo: 10000 unidades menores CRC (₡100,00), configurable en el servidor.
Los préstamos previos a esta fase se migran sin cargo retroactivo.
Los importes son enteros; el navegador no determina propietario ni importe.

## Autenticación elegida

Contraseñas Argon2id con libsodium y sesiones aleatorias de 256 bits con
randombytes_buf. Biblioteca fijada por versión y SHA-256; referencias oficiales:
[hash de contraseñas](https://doc.libsodium.org/password_hashing/default_phf)
y [aleatoriedad](https://doc.libsodium.org/generating_random_data).

Bearer guardado solo en memoria del navegador, nunca localStorage ni cookies.
Cada petición lleva Authorization explícito: no hay credencial ambiental que
un formulario de otro origen pueda usar (CSRF). CORS admite solo orígenes
configurados y Authorization; requiere HTTPS fuera de loopback. Recargar la
página o reiniciar el servidor exige iniciar sesión de nuevo. Logout invalida
la sesión en el servidor. Cambiar de cuenta aborta solicitudes anteriores.

Las cuentas persistentes se guardan aparte del negocio, en un directorio
privado del servidor, jamás en Mongo docente. Aprovisionar un usuario y crear
su cuenta son operaciones explícitas separadas: un fallo al crear una cuenta
deja al usuario sin acceso, no anuncia una cuenta creada. El primer admin se
aprovisiona por comando local con contraseña interactiva, sin servidor abierto.
