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

## Almacenamiento y migración

El dominio serializado es versión 2: auditoría de cancelación, estado explícito
de préstamo, cargos y comprobantes. El sobre Mongo conserva versionEsquema=1
porque conserva exactamente el protocolo del agregado; estado.dominio.version=2
identifica el cambio del contenido. Arrancar con dominio v1 falla cerrado.
Con respaldo y escritor detenido, --migrate convierte v1, valida referencias y
cinco estructuras, escribe una vez y confirma por GET. Conserva IDs, turnos,
avisos e historial; no cobra préstamos anteriores. Las cancelaciones antiguas
identifican responsable "migracion" y dejan claro que su fecha/responsable original
no fueron registrados (fecha de creación como marcador histórico).

LocalLibraryRepository guarda libros y dominio en un único JSON versión 2.
Un catálogo de trabajo anterior (array) requiere --migrate; conserva sus libros
y crea dominio vacío porque esa versión local no lo persistía. Si se conserva
una exportación antigua, migrela explícitamente con el procedimiento indicado
en el README antes de importarla. No se reemplazan datos incompatibles por vacío.
La consola sigue utilizando JsonBookRepository y su formato anterior.

El guardado local prepara archivo 0600, fsync, rename y fsync del directorio.
Fallo anterior a rename: rechazado, memoria anterior; fallo posterior:
incierto, mutaciones bloqueadas hasta reiniciar/releer. Mongo conserva diario,
confirmación y bloqueo anteriores; las entidades nuevas viajan en el mismo
agregado. Ni credenciales ni sesiones forman parte de ese agregado.
Un único escritor y archivos sin modificaciones externas durante la ejecución.

## Cuentas y sesiones

Almacén privado esquema 1: idUsuario, rol, hash Argon2id y activa.
Directorio 0700 y archivo 0600, del usuario del proceso, sin enlaces simbólicos.
Bloqueo exclusivo independiente del archivo de negocio. Sesiones solo en RAM:
se conserva únicamente el hash del identificador opaco y su vencimiento;
reiniciar exige login nuevo y no crea sesiones duplicadas.
Duración absoluta predeterminada: 1800 segundos (--session-seconds, 1–86400).
Una sesión por cuenta; un nuevo login invalida la anterior. Límite 1024 sesiones.
Cinco fallos por cuenta o IP en 60 segundos; tablas acotadas. No se confía en
X-Forwarded-For. Una futura API detrás de proxy debe definir sus proxies de
confianza; actualmente todos sus clientes compartirían la IP del proxy.

Errores genéricos de credenciales; cuentas desconocidas verifican hash ficticio
del mismo coste. Cambiar rol/contraseña o deshabilitar cuenta invalida sesiones.
Un usuario inactivo no puede iniciar ni usar sesión. Antes de desactivar una
entidad con cuenta ADMIN se exige deshabilitar esa cuenta; nunca el último ADMIN.
Importar dominio debe conservar las entidades de administradores activos.

Authorization: Bearer en toda ruta privada. Solo /healthz, /readyz y POST
/api/sesion/login son públicos, además de preflight OPTIONS sin datos.
El administrador puede iniciar sesión durante bloqueo Mongo para recuperar;
ninguna mutación de negocio se habilita antes de validar el estado.
/readyz informa solo listo; /api/almacenamiento entrega detalle al ADMIN.
La recuperación no contiene un bypass de autenticación.

Respaldo: detener API, copiar conjuntamente el archivo de negocio o respaldo
Mongo confirmado, directorio privado y diario Mongo si existe, a almacenamiento
privado cifrado con permisos restringidos. No copiar al repositorio, Pages ni
API docente. Restaurar con el servidor detenido, mismo propietario y permisos;
arrancar normalmente, revisar /readyz y autenticarse de nuevo. No restaurar un
diario sobre otro ámbito. La pérdida del almacén privado requiere recuperación
del respaldo; no hay contraseña predeterminada ni endpoint público de reset.
