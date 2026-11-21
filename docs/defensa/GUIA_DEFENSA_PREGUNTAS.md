# Guía de defensa — 24 preguntas y respuestas basadas en el código real

Todas las respuestas corresponden estrictamente a la implementación actual en este repositorio e indican los archivos exactos para verificarlas en vivo ante el jurado.

---

### A. Estructuras de datos propias en C++17 (30 % de la rúbrica)

**1. ¿Por qué eligieron un Árbol B+ para el catálogo en lugar de un BST o un `std::map`?**
- Un B+ agrupa múltiples claves por nodo (orden configurable `m ∈ [3, 128]`), mantiene altura reducida y enlaza todas las hojas en ambos sentidos (`anterior`/`siguiente`), lo que permite listados completos en `O(n)` y búsquedas exactas, por prefijo y por rango en `O(h log m + k)` sin recorrer el árbol completo. Además, el curso exige una estructura propia sin delegar el catálogo a `std::map`.
- **Archivos:** `backend/include/biblioteca/BPlusTree.hpp`, `backend/src/BPlusTree.cpp`, `backend/src/CatalogService.cpp`.

**2. ¿Cómo funciona la clave del B+ y cómo manejan títulos repetidos y acentos en español?**
- La clave es el par `(normalizeTitle(titulo), idLibro)`. Dos obras homónimas (como `L011` y `L012`, *"Programación modular"*) coexisten porque `idLibro` desempata de forma determinista. `normalizeTitle` valida UTF-8 estricto, pasa `A-Z` a minúsculas, quita tildes españolas (`áéíóú`), mapea `ü→u` y `Ñ→ñ` manteniendo `ñ ≠ n`, y compacta espacios. El límite explícito es que ordena lexicográficamente por bytes UTF-8 e ID ASCII, no por colación lingüística Unicode completa.
- **Archivos:** `backend/src/Libro.cpp`, `backend/src/BPlusTree.cpp`.

**3. ¿Cómo realizan el split al insertar y el borrado físico en el B+? ¿Cuál es su complejidad real?**
- Al superar `m-1` entradas en una hoja, se divide en dos, se actualizan los punteros dobles de hoja y se promueve el mínimo de la mitad derecha al padre; si un nodo interno supera `m` hijos, se divide promoviendo su separador medio y pudiendo crear una nueva raíz. Al borrar, se elimina físicamente la clave (no hay borrado lógico ni reconstrucción del árbol): si un nodo queda subocupado (`< ⌊m/2⌋` en hoja o `< ⌈m/2⌉` hijos en interno), intenta pedir prestado a su hermano izquierdo o derecho; si ambos están en el mínimo, fusiona hermanos y propaga la baja al padre, contrayendo la raíz si queda con un solo hijo. Como los separadores se recalculan consultando el mínimo de cada subárbol derecho, la cota conservadora de **esta implementación** es `O(m·h²)` en inserción/borrado.
- **Archivos:** `backend/src/BPlusTree.cpp`, `backend/tests/test_bplus.cpp` (23 416 operaciones, 2 036 201 comprobaciones contra `std::map`).

**4. ¿Para qué usan un `unordered_map` en `CatalogService`? ¿No viola la regla de no usar mapas estándar?**
- No la viola: `unordered_map<idLibro, Clave>` únicamente garantiza unicidad global de `idLibro` y localiza la clave compuesta al borrar o editar por ID. Ninguna consulta de catálogo (listado ordenado, título exacto, prefijo ni rango) usa ese mapa; todas consultan `BPlusTree`.
- **Archivos:** `backend/include/biblioteca/CatalogService.hpp`, `backend/src/CatalogService.cpp`.

**5. ¿Por qué usan un Árbol Rojo-Negro para préstamos y por qué solo almacena los préstamos activos?**
- Las consultas operativas de vencimientos exigen recorrer préstamos activos ordenados por `(fechaVencimiento, idPrestamo)` e insertar/retirar en `O(log n)` garantizado en peor caso. Al devolver o anular un préstamo, se elimina del Rojo-Negro (`PrestamosActivos::eliminar`) para mantener acotado el índice activo, mientras el registro histórico completo se conserva en `EstadoBiblioteca::prestamos`. Al renovar (`cambiarVencimiento`), se retira la clave anterior y se reinserta con la nueva fecha.
- **Archivos:** `backend/include/biblioteca/PrestamosActivos.hpp`, `backend/src/PrestamosActivos.cpp`, `backend/src/LibraryService.cpp`.

**6. ¿Qué variante de Rojo-Negro implementaron y qué invariantes validan?**
- Un árbol Rojo-Negro inclinado a la izquierda (LLRB) con rotaciones izquierda/derecha, `invertirColores`, y `moverRojoIzquierda`/`moverRojoDerecha` para el borrado. `validar()` comprueba: raíz negra, orden estricto BST, ningún enlace rojo a la derecha, ningún par de enlaces rojos consecutivos y misma altura negra en todos los caminos hacia hojas nulas.
- **Archivos:** `backend/src/PrestamosActivos.cpp`, `backend/tests/test_rojo_negro.cpp`.

**7. ¿Cómo funciona el Hash Extensible de usuarios y qué ocurre si hay muchas colisiones?**
- Aplica FNV-1a de 64 bits sobre `idUsuario` y usa los `D` bits menos significativos para indexar un directorio de tamaño `2^D`. Cada cubeta tiene capacidad 4 y profundidad local `d ≤ D`. Si una cubeta desborda con `d == D` y `D < 12`, el directorio se duplica (`2^D → 2^(D+1)`) y la cubeta se divide redistribuyendo por el bit adicional. Si se alcanza la profundidad máxima (12 por defecto, configurable hasta 16), el directorio deja de crecer para acotar memoria y las colisiones adicionales se almacenan y buscan linealmente dentro de la cubeta (`O(k)`).
- **Archivos:** `backend/include/biblioteca/IndiceUsuarios.hpp`, `backend/src/IndiceUsuarios.cpp`, `backend/tests/test_hash_extensible.cpp`.

**8. ¿Cómo se construye y mantiene el Grafo de Temas? ¿Usan IA para recomendaciones?**
- No se usa IA. Es un grafo no dirigido por listas de adyacencia ordenadas donde cada vértice es un `idLibro` y existe una única arista entre dos obras si comparten al menos un tema normalizado (`normalizeTitle`), sin bucles ni aristas duplicadas. Al editar o borrar un libro (`LibraryService::editarLibro` / `borrarLibro`), se recalculan simétricamente las aristas afectadas. Soporta `vecinos`, `bfs` y `dfs` deterministas desempatados por ID.
- **Archivos:** `backend/include/biblioteca/GrafoTemas.hpp`, `backend/src/GrafoTemas.cpp`, `backend/tests/test_grafo_temas.cpp`.

**9. ¿Cómo opera la Cola FIFO de reservas y por qué usa cancelación lógica?**
- Cada libro tiene su propia lista enlazada `ColaReservas` con punteros `frente` y `final`, ordenada por `(llegada, idReserva)` donde `llegada` es una secuencia global monótona (`siguienteLlegada`). Al cancelar una reserva, su estado pasa a `cancelada` en el dominio con auditoría (`Auditoria`), sin recorrer la cola en ese instante; cuando una copia queda disponible, `ColaReservas::atender(predicadoCancelada)` descarta los nodos cancelados del frente hasta entregar la primera reserva válida en `O(1 + c)`.
- **Archivos:** `backend/include/biblioteca/ColaReservas.hpp`, `backend/src/ColaReservas.cpp`, `backend/src/LibraryService.cpp`.

**10. ¿Cuál es la diferencia entre la Cola FIFO académica y RabbitMQ? ¿Por qué existen ambos?**
- `ColaReservas` (FIFO C++) pertenece al **dominio síncrono**: decide qué lector recibe el próximo ejemplar disponible de un libro y se reconstruye desde las reservas persistidas al arrancar. **RabbitMQ** pertenece a la **integración asincrónica entre procesos**: desacopla la generación de avisos de negocio de su procesamiento por `biblioteca_worker` con durabilidad en disco, publisher confirms, ACK manual, reintentos y DLQ. Ninguno reemplaza al otro.
- **Archivos:** `backend/src/ColaReservas.cpp`, `backend/src/AmqpBroker.cpp`, `backend/apps/biblioteca_worker.cpp`.

---

### B. Mensajería asincrónica con RabbitMQ y Worker C++17 (20 %)

**11. ¿Qué significa «publisher confirm» y por qué no equivale a que el usuario haya recibido el aviso?**
- `confirm.select` indica únicamente que el broker RabbitMQ recibió el mensaje en el exchange y (al ser persistente en cola durable) lo aceptó para encolar; además `mandatory=1` permite detectar `AMQP_BASIC_RETURN_METHOD` si el mensaje no tenía ninguna cola enlazada. Que el broker confirme la publicación solo pasa el aviso de `pendiente` a `publicado`. El usuario únicamente ve el aviso en *Mis avisos* cuando `biblioteca_worker` lo consume y el escritor único confirma `estado="procesado"` + `Notificacion`.
- **Archivos:** `backend/src/AmqpBroker.cpp` (`publishWithConfirm`), `backend/src/LibraryService.cpp`, `frontend/src/Panels.tsx`.

**12. ¿Cuándo envía el worker el ACK manual y qué pasa si el proceso muere justo antes del ACK?**
- `biblioteca_worker` consume con `basic.qos(1)` y `no_ack=false`, y llama `consumer.ack(deliveryTag)` **solo después** de recibir `HTTP 200` de `catalogo_api` en `POST /api/interno/avisos/{id}/procesar` (confirmando persistencia durable en JSON o MongoDB). Si el worker cae después de ese `HTTP 200` y antes del `basic.ack`, RabbitMQ reentrega el mensaje (`redelivered=true`). Al procesarlo nuevamente, `LibraryService::procesarAviso` detecta que `idAviso` ya está `procesado` con `NT-AV-n` y responde `200 {"duplicado": true}` sin crear una segunda notificación visible, tras lo cual el worker envía el ACK. Este caso está probado contra RabbitMQ real con `--test-fail-before-ack`.
- **Archivos:** `backend/apps/biblioteca_worker.cpp`, `backend/src/LibraryService.cpp`, `backend/tests/test_rabbitmq_worker.py` (escenario 6).

**13. ¿Garantizan entrega «exactamente una vez» (exactly-once)?**
- No. En redes distribuidas con procesos independientes no prometemos «exactamente una vez» en el transporte. Garantizamos entrega **al menos una vez (`at-least-once`)** combinada con **deduplicación persistente 1:1** por `idAviso`/`idEvento` en el único escritor (`NT-AV-n`), logrando un efecto visible idempotente para el lector.
- **Archivos:** `backend/src/LibraryService.cpp`, `backend/src/LibraryValidation.cpp`.

**14. ¿Qué ocurre cuando RabbitMQ está caído? ¿Se bloquean las peticiones web o se pierden los avisos?**
- Ni se bloquean ni se pierden. `Aviso` se persiste junto con el cambio de negocio (`estado="pendiente"`). El hilo `AvisoOutboxRelay` ejecuta la conexión y publicación AMQP **fuera** de `stateMutex`, por lo que catálogo, préstamos, reservas y pagos continúan respondiendo normalmente. El relay aplica backoff progresivo (`1 s → 5 s`) y, cuando RabbitMQ vuelve a estar disponible, publica automáticamente todos los pendientes acumulados sin simular éxito mientras estuvo caído.
- **Archivos:** `backend/src/AmqpBroker.cpp` (`AvisoOutboxRelay::loop`), `backend/tests/test_rabbitmq_worker.py` (escenarios 1 y 3).

**15. ¿Cómo manejan mensajes malformados, fallos del escritor y la cola de fallidos (DLQ)?**
- Si el mensaje recibido por el worker tiene JSON inválido o campos incompatibles, el worker intenta reportar fallo definitivo y ejecuta `basic.reject(requeue=false)`, haciendo que RabbitMQ lo enrute a la DLQ durable `biblioteca.avisos.fallidos` vía `biblioteca.dlx`. Si falla transitoriamente el escritor (ej. `503` o desconexión), espera con backoff exponencial y republica con cabecera `x-intentos` confirmada antes de hacer ACK del intento previo. Al agotar `--max-retries`, marca el aviso como `fallido` con error sanitizado y lo envía a la DLQ. El administrador puede revisar el diagnóstico en el panel *Avisos* y ejecutar `POST /api/avisos/{id}/reintento` (no existe borrado silencioso).
- **Archivos:** `backend/apps/biblioteca_worker.cpp`, `backend/src/HttpController.cpp`, `backend/tests/test_rabbitmq_worker.py` (escenarios 8 y 9).

**16. ¿Por qué el worker no escribe directamente en MongoDB ni comparte la contraseña del administrador?**
- Porque la API MongoDB docente no ofrece transacciones ni bloqueo multiinstancia: si el worker sobrescribiera `estado.avisos` en paralelo a `catalogo_api`, se perderían mutaciones. Mantener un **único escritor** serializa todas las revisiones. Además, por principio de mínimo privilegio, `biblioteca_worker` usa `ServiceIdentity` (`service.token` `0600` independiente, verificado por hash BLAKE2b en tiempo constante) que solo autoriza `/api/interno/avisos/*` y recibe `403` en toda ruta humana o administrativa.
- **Archivos:** `backend/include/biblioteca/ServiceIdentity.hpp`, `backend/src/ServiceIdentity.cpp`, `backend/src/HttpController.cpp`.

---

### C. Persistencia local, MongoDB cloud, seguridad y operación (15 % + 20 %)

**17. ¿Por qué eligieron un solo documento agregado en MongoDB en lugar de una colección por entidad?**
- Al sondear la API real `paginas-web-cr.com/Api/apis/mongodb.php` observamos que soporta objetos anidados y filtros en `PUT` por `estado.revision`, pero no ofrece transacciones entre colecciones y dos `POST` con el mismo ID de negocio duplican documentos. Un documento agregado único (`idBiblioteca="principal"`, `estado={revision, libros, dominio}`) elimina ventanas de escritura parcial entre libros y dominio bajo un único escritor.
- **Archivos:** `backend/src/MongoLibraryRepository.cpp`, `docs/PERSISTENCIA_MONGODB.md`.

**18. ¿Para qué sirve el diario `.pending.json` y qué pasa si ocurre un timeout durante un `PUT` a MongoDB?**
- Antes de contactar la red, `MongoLibraryRepository::write` guarda en `<mongo-state>.pending.json` (con `fsync` de archivo y directorio) el estado anterior y el esperado. Tras enviar un único `POST/PUT`, siempre ejecuta un `GET` completo de confirmación. Si hubo timeout pero el `GET` muestra exactamente `esperado`, confirma y limpia el diario. Si el `GET` falla o muestra un estado antiguo/parcial, el resultado es **incierto**: conserva el diario, bloquea la biblioteca (`/readyz 503`, `/healthz 200`) y **jamás reenvía el `POST/PUT` automáticamente** al reiniciar; exige recuperación por `GET` (`POST /api/recuperacion`) o intervención del operador si hubo escritura parcial externa.
- **Archivos:** `backend/src/MongoLibraryRepository.cpp`, `backend/tests/test_mongo_library.cpp`.

**19. ¿Cómo protege la aplicación la privacidad entre usuarios y contra CSRF o ataques de fuerza bruta detrás de un proxy?**
- Todas las rutas privadas requieren `Authorization: Bearer` validado en el servidor (`AuthService`, Argon2id, token aleatorio 256 bits en RAM del cliente, sin cookies ni `localStorage`, eliminando CSRF ambiental). Las rutas `/api/mi-biblioteca`, `/api/mis-avisos` y `/api/mis-*` filtran por propietario en C++ y devuelven `404` ante IDs ajenos. El rate-limit (5 intentos / 60 s por IP y usuario) usa `--trusted-proxy`: ignora `X-Forwarded-For` de clientes arbitrarios y solo lee la IP real cuando la conexión proviene del proxy confiable (Nginx/Caddy).
- **Archivos:** `backend/src/AuthService.cpp`, `backend/src/HttpController.cpp`, `backend/tests/test_acceso_api.py`.

**20. ¿Cómo maneja el frontend una respuesta perdida tras enviar un formulario (`POST/PUT/DELETE`)?**
- `mutation-attempt.ts` y `FormDialog` (`ui.tsx`) distinguen rechazo explícito de negocio (permite corregir) de resultado incierto (corte de red, JSON truncado, HTML inesperado o 5xx sin resultado). Ante incertidumbre, bloquean el botón de envío de ese intento para evitar duplicar la operación, ejecutan una reconsulta automática `GET` vinculada a la generación de sesión actual y habilitan únicamente *Consultar estado*.
- **Archivos:** `frontend/src/mutation-attempt.ts`, `frontend/src/ui.tsx`, `frontend/src/session-client.ts`.

**21. ¿Qué diferencia existe entre devolver un préstamo, anular un préstamo y cancelar una reserva?**
- **Devolver** (`devolver`): transición física normal (`activo → devuelto`), retira del Rojo-Negro, libera copia, atiende FIFO y emite aviso `devolucion`. **Anular** (`anular`): solo `ADMIN` por error operativo, exige motivo y `copiaRecuperada=true`, marca `cancelado` (nunca `devuelto`), anula el cargo y conserva cualquier comprobante previo en historial. **Cancelar reserva** (`cancelar`): solo sobre reservas `pendiente`/`apartada`, registra `Auditoria` y reasigna la copia apartada al siguiente turno FIFO; jamás se registra como préstamo devuelto.
- **Archivos:** `backend/src/LibraryService.cpp`, `docs/ACCESO_PANELES.md`.

**22. ¿Cómo funcionan los pagos simulados y cómo garantizan que no haya cobros dobles ni dinero real?**
- Al crear un préstamo se genera un `Cargo` con importe entero (centésimos, ej. `10000` CRC = ₡100,00). `LibraryService::pagar` verifica propiedad en el servidor, rechaza cargos anulados y, si ya existe un `Comprobante` para ese `idPrestamo`, retorna el mismo comprobante (idempotencia 1:1) con `etiqueta="SIMULADO"`. No existe pasarela de pago, no se solicitan datos bancarios y pagar no altera la disponibilidad del ejemplar.
- **Archivos:** `backend/src/LibraryService.cpp`, `backend/src/LibraryValidation.cpp`, `frontend/src/Panels.tsx`.

**23. ¿Por qué GitHub Pages muestra «Servidor de biblioteca pendiente de configuración»?**
- Porque GitHub Pages aloja **únicamente** el bundle estático HTML/JS/CSS compilado por Vite bajo `/Sistema-de-gestion-Academica/`; no ejecuta C++ ni RabbitMQ. Cuando la variable pública `VITE_API_URL` está vacía (pendiente de contratar/aprovisionar un servidor HTTPS para el backend), el cliente muestra deliberadamente ese mensaje y deshabilita controles en lugar de intentar consultar `localhost` o inventar datos falsos.
- **Archivos:** `frontend/src/api-config.ts`, `.github/workflows/pages.yml`, `docs/DESPLIEGUE_HTTPS_DOCKER.md`.

**24. En resumen, ¿qué garantiza exactamente el sistema y qué NO garantiza?**
- **Sí garantiza:** invariantes de las 5 estructuras C++17 (0 fugas bajo ASan/UBSan), reglas de negocio con escritor único, aislamiento de roles/propietarios y credenciales `0600` fuera de Mongo, entrega `at-least-once` durable en RabbitMQ con ACK manual y deduplicación 1:1, y confirmación `GET` sin fallback silencioso.
- **No garantiza:** entrega «exactamente una vez» en red, transacciones o multiinstancia en la API MongoDB docente, agregados mayores a 1 MiB, pagos reales, correos externos ni disponibilidad de un servidor HTTPS remoto aún no contratado.
- **Archivos:** `docs/Informe_Final_Biblioteca.pdf`, `docs/ESTADO.md`, `docs/PERSISTENCIA_MONGODB.md`.
