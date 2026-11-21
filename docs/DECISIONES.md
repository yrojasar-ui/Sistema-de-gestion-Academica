# Decisiones

## D01 — alcance y repositorio

9 de noviembre de 2026: conservar la copia WSL y su remoto; no crear otra copia.
Primero Prompt 0, luego solo E2-A. No publicar, etiquetar ni preparar informe E2-C.
No hay código previo ni PDF local; se aplican las decisiones expresas de la guía.

## D02 — arquitectura del prototipo

Libro → BPlusTree; IBookRepository → JsonBookRepository; CatalogService coordina
índice y persistencia; ConsoleUI se limita a interacción; main compone objetos.
La carga completa se valida antes de sustituir el catálogo. Guardado explícito
en archivo de trabajo distinto al fixture. Los errores no se ocultan.

## D03 — B+ y contenedores auxiliares

Orden m máximo de hijos; hojas máximo m-1, mínimo floor(m/2); internos mínimo
ceil(m/2). Nodos con unique_ptr a hijos y punteros observadores a padre/hermanos.
Separadores calculados como mínimos derechos; borrado local por préstamo o fusión.
No se usa una biblioteca de árboles. Vectores guardan claves/hijos dentro del nodo.
Un unordered_map auxiliar id→clave impone unicidad global y permite localizar el
borrado por ID; todas las consultas académicas usan B+. std::map solo es oráculo
en pruebas o tabla de etiquetas en visualización. Efecto: datos reales en hojas,
selección segura de títulos homónimos y organización visible verificable.

## D04 — normalización limitada y explícita

UTF-8 estricto; minúsculas ASCII y equivalencias españolas para acentos, diéresis
y ñ precompuestos/descompuestos. Ñ se conserva como ñ, diferente de n. Espacios
normalizados; otros Unicode se conservan. Comparación lexicográfica por bytes,
no orden lingüístico. Cubre acentos del diseño sin añadir una biblioteca Unicode
completa ni prometer equivalencias fuera de la política probada.

## D05 — JSON local y consistencia

nlohmann/json 3.12.0 (MIT), encabezado oficial incluido y hash comprobable para
build offline. Esquema estricto y límites documentados. Carga íntegra validada
en un B+ temporal antes de intercambiar estado. Guardado manual de snapshot con
temporal exclusivo y rename local. Un fallo mantiene dirty y datos en memoria.
Sin fsync/locking: no garantía de corte eléctrico ni múltiples escritores.
La API MongoDB no se consulta: E2-B está fuera del alcance solicitado.

## D06 — ejecución y alcance de pruebas

CTest ejecuta un runner C++ de comprobaciones explícitas (no assert desactivable),
oráculo std::map, pruebas de integración y demo de dos procesos. Se registra semilla
20261109 y se valida cada operación en secuencias de árbol. Órdenes 3,4,5,6,7,8,16,32.
Contadores de ramas exigen préstamos de ambos lados, en hojas e internos, además
de splits, fusiones y contracción. ASan/UBSan instrumentan todos los módulos C++.
No son benchmarks ni una prueba de corrección universal. El informe E2-C y sus
capturas no se producen en esta fase; los logs pertenecen a la verificación E2-A.

## D07 — entorno real y Git

Se utiliza Ubuntu WSL2 del usuario, no una copia Windows. Faltaban CMake y Make;
la instalación detectó ausencia del binario tar pese a estar registrado por dpkg.
Se recuperó tar desde el paquete oficial de Ubuntu y se reinstaló antes de instalar
CMake/Make. No se sustituyó ningún archivo de proyecto preexistente.
La copia inicial no tenía commits ni identidad Git configurada en WSL: no se
inventa nombre/correo. El commit local queda pendiente mientras falte esa identidad.

## D08 — revisión posterior a la primera ejecución

La revisión del servicio identificó que la consulta de ruta del adaptador ocurría
después de intercambiar el catálogo. Se adelantó esa operación y se añadió un
fallo inyectado para verificar que estado y dirty permanecen intactos. También
se impuso el límite de 10000 registros en el servicio, además del adaptador JSON,
y se comprobó que un snapshot inválido no sobrescribe el archivo anterior.
No se observó pérdida real de datos del usuario; las pruebas usan datos sintéticos.

La primera ejecución ASan de B+ tardó 177,09 s con timeout de 180 s. Se aumentó
el margen a 600 s para reproducibilidad en WSL sin quitar casos ni relajar
invariantes. Se volvió a ejecutar build y CTest en ambas configuraciones.
`ejecucion-01` conserva el estado previo y `ejecucion-final` identifica las fuentes
finales con SHA-256. No hay SHA de commit porque no hay identidad Git configurada.

## D09 — persistencia por documento en E2-B

La interfaz de escritura por registro permite conservar el guardado de snapshots
de E2-A. En Mongo el servicio relee y valida el catálogo después de confirmar
cada mutación. Para un prototipo de hasta 10 000 obras, aceptamos el coste de
un GET completo y una reconstrucción del B+ por cambio. Esto permite incorporar
otros cambios visibles en el momento de la lectura. El adaptador mantiene una
tabla auxiliar de las identidades y los campos leídos para validar la selección.
El contrato observado y los límites de concurrencia figuran en [API MongoDB](API_MONGODB.md).

## D10 — catálogo web con el B+ del servicio de consola

La interfaz React solicita el listado o la búsqueda a `catalogo_api`. El
controlador llama a `CatalogService::all`, `byTitle` o `byPrefix` y serializa
el resultado. React divide esa respuesta en páginas. El panel muestra la
cadena de `CatalogService::visualize` y pide la validación al servicio. El
navegador no reconstruye las claves ni los nodos.

Para la demo usamos un JSON de trabajo dedicado y el guardado atómico de
`JsonBookRepository`. Cada mutación se guarda antes de responder. Un solo
trabajador HTTP evita lecturas de cambios pendientes de guardar. Si la
escritura falla, se intenta recargar el último snapshot. El bloqueo de archivo
coordina las instancias de la API que usan el mismo destino. La consola conserva
su propio archivo y su guardado explícito. Vite y la API escuchan en loopback.

## D11 — índices en memoria para el servicio de biblioteca

| Estructura | Operación que usará el servicio | Estado que exporta C++ |
|---|---|---|
| `PrestamosActivos` (Rojo-Negro con enlaces inclinados a la izquierda) | Insertar la fecha ISO de ancho fijo y el ID al prestar, retirar la clave al devolver y reindexarla con `cambiarVencimiento` al renovar. El dominio conservará el historial y validará la fecha. | Clave compuesta como ID de nodo, padre, hijos, color y altura negra (la hoja nula cuenta como uno). `contiene` admite una traza de claves visitadas. |
| `IndiceUsuarios` (hash extensible) | Consultar, reemplazar los datos y eliminar por ID. El hash FNV-1a de 64 bits selecciona una entrada con sus bits bajos. | Referencias del directorio, profundidades, ocupación e IDs. El nombre y el estado del usuario se excluyen. `traza` identifica la posición y cubeta consultadas. |
| `GrafoTemas` (listas de adyacencia) | Insertar, modificar o retirar una obra. Una intersección de temas normalizados crea una sola arista. Cambiar temas modifica los enlaces en ambos sentidos. El borrado limpia las listas de todos los vecinos. | IDs, temas normalizados, listas de vecinos y número de aristas. BFS y DFS recorren el componente desde el libro elegido, con desempate por ID. |
| `ColaReservas` (una lista enlazada por libro) | Encolar por secuencia de llegada y desempatar por ID. Al atender, el servicio proporciona un predicado de cancelación. La cola consume las canceladas del frente y devuelve la primera válida. | Libro, extremos, tamaño y cada ID con su llegada y enlace siguiente. El estado de cancelación se consulta en el dominio. |

En el hash, la capacidad inicial por cubeta es cuatro y la profundidad máxima
predeterminada es doce. Cada división distribuye por un bit adicional. Al llegar
al límite, los registros que colisionan comparten la misma cubeta y la búsqueda
es lineal en su ocupación. El límite configurable (0–16) acota el directorio.
El borrado conserva las cubetas para reutilizarlas y mantiene el tamaño del directorio.

En el grafo, el mapa auxiliar localiza el vértice por ID. Las listas ordenadas
representan las relaciones y los conjuntos de visitados evitan ciclos en los
recorridos. La única escritura de aristas es la edición de los temas que las
justifican. En la FIFO, el conjunto auxiliar detecta IDs repetidos. La lista
determina el turno. Las llegadas crecientes se insertan por el puntero final
en tiempo esperado constante. Una llegada fuera de orden puede recorrer la lista.

El B+ ofrece `snapshot(claveOpcional)`. Enumera los mismos nodos que la vista
textual y exporta claves, hijos y ambos enlaces entre hojas. La búsqueda opcional
registra el descenso real hasta la hoja. Los IDs numéricos del B+ valen dentro
de una captura. Todos los métodos `snapshot()` devuelven texto JSON.

## D12 — dominio con catálogo único y confirmación local

LibraryService comparte el CatalogService que sirve al catálogo web. Los libros
persisten con JsonBookRepository. Los registros del dominio se almacenan mediante
IDomainRepository en MemoryDomainRepository. Las consultas de usuarios por ID,
activos por vencimiento, catálogo y relaciones utilizan sus estructuras propias.
Los vectores contienen registros e historial y las tablas auxiliares ubican
colas por libro. Los recorridos académicos se mantienen en los índices.

Para confirmar una mutación se preparan copias de los registros y de los nodos
reales, se aplican los algoritmos y se validan las referencias antes de guardar.
El repositorio local garantiza reemplazo completo o excepción conservando el
estado previo. El nuevo estado visible se publica con swap. La edición de libros
prepara B+ y grafo y confirma el JSON antes de sustituir el estado visible.
Este mecanismo añade coste de copia y validación a cada operación.

La [guía del dominio](DOMINIO_API.md) define transiciones, política de varias
copias, retirada, desactivación, renovación, recuperación y límites de esta fase.
La secuencia de llegada la asigna el servicio y viaja en la exportación del
dominio. El endpoint administrativo de reconstrucción valida referencias y
restaura ese orden. Los avisos se preparan pendientes; IEventPublisher define
la futura frontera de publicación. La autorización por roles y el transporte
de mensajes requieren sus fases de implementación.

## D13 — Instantánea Mongo confirmada y recuperación conservadora

Prompt 4: una colección propia contiene un documento `principal` de esquema 1,
con libros y dominio dentro del campo `estado` versionado. La elección evita
encadenar cambios entre colecciones sin transacciones. Se observó filtro exacto
y de revisión anidada, incluso una competencia acotada de dos PUT; se observaron
POST duplicados. No se extrapola a soporte multiinstancia o unicidad remota.

Se extrae el protocolo común de MongoApiBookRepository y se reutiliza libcurl.
Una intención durable precede al único envío; GET completo confirma antes de
publicar. Timeout con lectura antigua no es rechazo: bloquea, conserva diario
y exige recuperación GET. Arranque inválido mantiene healthz vivo y readyz
negativo. Ambos servicios se reconstruyen en candidatos antes del intercambio.

La cota propia de 1 MiB, el escritor único y la intervención ante un resultado
permanentemente indeterminado son límites explícitos. El worker futuro no puede
escribir este agregado por su cuenta. [Contrato, comandos y límites](PERSISTENCIA_MONGODB.md).

## D14 — Pages estático y origen de API explícito

El frontend de producción usa la subruta del repositorio; desarrollo mantiene
la raíz y proxy local. VITE_API_URL contiene únicamente un origen HTTPS público:
el cliente agrega /api/. Sin variable hay un aviso de configuración pendiente
y no se hacen solicitudes de catálogo. Una URL no vacía inválida detiene el
build antes de incluirla en el bundle.

pages.yml es reutilizable y solo lo invoca CI para push main después de pasar
backend Debug/ASan y frontend. Ambos checkout/build usan el commit del evento.
El artefacto contiene únicamente frontend/dist; deploy usa github-pages con
Pages/OIDC y sin secretos del backend. Los PR no despliegan.
La publicación efectiva, HTTPS/CORS remoto, autenticación y alojamiento C++/
RabbitMQ quedan separados de esta preparación. [Manual](../README.md#frontend-en-github-pages).

## D15 — Sesiones y almacenamiento privado

AuthService usa libsodium 1.0.22 fijado, Argon2id interactivo y tokens opacos
aleatorios de 256 bits. Bearer en RAM del cliente; sin cookies/localStorage,
sin credenciales ambientales para CSRF. CORS limita orígenes y Authorization.
Expiración absoluta, revocación y limitación de intentos. El backend deniega
por defecto; USUARIO accede a recursos propios y ADMIN a administración.
[Detalles, respaldo y límites](ACCESO_PANELES.md). La cuenta privada no se
duplica en Mongo. Aprovisionamiento de entidad y cuenta son operaciones separadas.

## D16 — Dominio v2 y pagos simulados

Sustituye la limitación local en RAM descrita en D12. LocalLibraryRepository
guarda un agregado JSON durable. El protocolo Mongo de D13 se conserva: mismo
sobre, dominio versión 2, migración explícita confirmada. Las cinco estructuras
se reconstruyen sin reiniciar turnos ni avisos. Cancelaciones y anulaciones
tienen auditoría; devolución sigue siendo una transición distinta.
Cargo entero por préstamo y comprobante único SIMULADO; anular el préstamo
anula su cargo y conserva el comprobante previo. No se integra un proveedor de pagos.

## D17 — Paneles y aislamiento en el navegador

Conserva D14 (Pages, subruta, proxy y VITE_API_URL). La sesión solo vive en RAM;
al cambiar cuenta se abortan peticiones y se descartan respuestas de generaciones
anteriores. Los paneles se desmontan, sin cachés compartidas de biblioteca personal.
Todas las acciones llaman la API real; los menús no son el control de autorización.
Los snapshots administrativos se muestran como JSON/texto del C++, sin reconstruir
estructuras académicas en JavaScript.

## D18 — Reconciliar por lectura y enriquecer solo recursos propios

Una escritura con resultado incierto bloquea el reenvío de ese formulario.
Se releen recurso y listas mediante GET; consultar no cambia el resultado
original ni demuestra por sí solo el resultado causal de la escritura.
Errores de negocio, transporte y protocolo se distinguen; el resultado explícito
del servidor se conserva. La recuperación del repositorio sigue siendo una
operación administrativa explícita. La generación de sesión abarca toda la
cadena mutación → reconsulta, no solo cada petición individual.

El resumen de libro se incorpora en el DTO de mi-biblioteca después de filtrar
por propietario. Se evita abrir consultas administrativas o realizar un GET por
tarjeta. Se muestran metadatos actuales; no se altera el historial ni el esquema
persistido. No se introduce un índice sustituto de las estructuras C++.

Pruebas: [correcciones del PR #11](evidencias/acceso-paneles/correcciones-pr11/README.md).

## D19 — Mensajería real RabbitMQ, patrón Outbox, Worker C++17 e Identidad de Servicio

Para la entrega final se adopta `rabbitmq-c 0.15.0` (MIT), compilado estáticamente y verificado por SHA-256 mediante `scripts/preparar_rabbitmq_c.sh`, como cliente AMQP 0-9-1 contra RabbitMQ 4.0.5 real. La arquitectura preserva un **único escritor** del agregado de biblioteca (`catalogo_api`): el worker independiente (`biblioteca_worker`) jamás escribe directamente en JSON local ni en MongoDB.

- **Separación de fases:** el aviso se persiste en `EstadoBiblioteca::avisos` (`estado="pendiente"`, `idEvento="EV-AV-n"`) en la misma operación confirmada que el cambio de negocio. `AvisoOutboxRelay` corre en un hilo desacoplado sincronizado con `stateMutex`, haciendo todo el I/O de red AMQP **fuera** del cerrojo para no bloquear HTTP.
- **Confiabilidad sin prometer «exactamente una vez»:** exchanges y colas durables (`biblioteca.eventos`, `biblioteca.avisos`, `biblioteca.dlx`, `biblioteca.avisos.fallidos`), mensajes persistentes (`delivery_mode=2`), `confirm.select` + `mandatory=1` detectando `basic.return`, `prefetch=1` y ACK manual solo después de confirmar `HTTP 200` durable en `/api/interno/avisos/{id}/procesar`. La deduplicación persistente 1:1 en `Notificacion` (`NT-AV-n`) garantiza que un redelivery tras caída pre-ACK no duplique notificaciones visibles.
- **Identidad mínima (`ServiceIdentity`):** secreto aleatorio de 256 bits (`0600`) independiente de `cuentas.json`, verificado por hash BLAKE2b en tiempo constante (`sodium_memcmp`), restringido únicamente a `/api/interno/avisos/*` y rechazado (`403`) en cualquier ruta humana o administrativa.

## D20 — Bandeja Mis avisos, Proxy de confianza y Empaquetado Docker/HTTPS

- **Bandeja interna como destino real:** `GET /api/mis-avisos` entrega únicamente notificaciones procesadas de la sesión autenticada (aislamiento estricto A/B) y un resumen honesto de avisos propios pendientes/publicados sin simular éxito cuando el broker está caído. `GET /api/avisos` (ADMIN) diferencia `pendiente`, `publicado`, `procesado` y `fallido`, muestra errores sanitizados y habilita reintento explícito (`POST /api/avisos/{id}/reintento`) sin borrado silencioso.
- **Rate-limit detrás de proxy (`--trusted-proxy`):** `HttpController` ignora `X-Forwarded-For`/`X-Real-IP` por defecto y solo extrae la IP cliente real cuando la conexión TCP proviene de una IP explícitamente confiable (Nginx/Caddy), evitando que todos los usuarios compartan el mismo bloqueo por IP y evitando suplantación desde clientes directos.
- **Empaquetado y despliegue honesto:** `deploy/Dockerfile.backend`, `deploy/Dockerfile.frontend`, `docker-compose.yml` y las plantillas HTTPS (`Caddyfile.https.example`, `nginx-https.conf.example`) cierran `/api/interno/*` en el borde y mantienen RabbitMQ en red interna. GitHub Pages está verificado en producción (`HTTP/2 200`) como frontend estático; el alojamiento HTTPS remoto del backend se mantiene explícitamente como pendiente de selección de proveedor.
