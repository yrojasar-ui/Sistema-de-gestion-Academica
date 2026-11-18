# Matriz de requisitos

La matriz cubre el segundo avance, el catálogo web, su integración continua y el dominio integrado con modo Mongo explícito. Las pruebas de E2-A se conservan en
`evidencias/e2a/ejecucion-final/`. Los intercambios con la API docente y la
regresión de E2-B se recogen en `evidencias/e2b/`. Las capturas y la verificación de E2-C están en `evidencias/entregable2/`.

| ID | Fase | Criterio verificable | Implementación | Prueba/evidencia | Estado |
|---|---|---|---|---|---|
| P0 | 0 | Contexto, reglas, decisiones y matriz | AGENTS.md, docs | Inspección registrada en ESTADO.md | Documentado |
| A01 | E2-A | B+ propio, inserción/búsqueda/borrado y hojas enlazadas | `backend/src/BPlusTree.cpp` | `test_bplus.cpp` → `debug-ctest.log`, `asan-ctest.log` | Implementado y probado localmente |
| A02 | E2-A | Split, préstamo, fusión y contracción de raíz | `split`, `rebalance`, `validate` | 8 órdenes; contadores de ambos préstamos hoja/interno → logs CTest | Implementado y probado localmente |
| A03 | E2-A | JSON validado sin carga parcial; duplicados | `JsonBookRepository.cpp`, `CatalogService::load` | `test_catalog.cpp`: 18 entradas inválidas, ID global, homónimos, fallos de adaptador → logs CTest | Implementado y probado localmente |
| A04 | E2-A | Menú y consultas por índice; selección inequívoca | `ConsoleUI.cpp`, `CatalogService.cpp` | Integración selección/EOF + demo → `demo.log` | Implementado y probado localmente |
| A05 | E2-A | Bibliotecas CMake separadas | `CMakeLists.txt`: libro, bplus, json_repository, catalog_service, console_ui | `debug-build.log`, `asan-build.log`, `resultados.tsv` | Implementado y compilado localmente |
| A06 | E2-A | Archivo de trabajo separado; fallo y reinicio | `JsonBookRepository::save`, dirty en servicio | Fallo de escritura/rename, protección de fixture, reintento, dos procesos → logs CTest y demo | Implementado y probado localmente |
| A07 | E2-A | Español UTF-8 determinista | `Libro.cpp::normalizeTitle` | Acentos/mayúsculas/descompuestos/ñ/otros Unicode/UTF-8 inválido → `test_bplus.cpp` y logs CTest | Implementado y probado localmente |
| A08 | E2-A opcional | Modificar título conservando índice | CatalogService::update y API PUT de libro | biblioteca_api y dominio_repositorios | Implementado y verificado localmente |
| B01 | E2-B | GET, POST, PUT y DELETE de libro propio | `MongoApiBookRepository`, libcurl | [Contrato](API_MONGODB.md) y [sesiones reales](evidencias/e2b/nube.txt) | Verificado en nube con datos sintéticos |
| B02 | E2-B | Reconstrucción de B+ y consulta tras reinicio | `CatalogService` y consola | Segundo proceso en la demo de nube | Verificado |
| B03 | E2-B | Escritura incierta y recuperación | GET de verificación y recarga del servicio | `mongo_contrato_simulado` | Verificado localmente con fixtures |
| C01 | E2-C | Informe breve, capturas reales y guion de demostración | [PDF de 3 páginas](Informe_Entregable2_Biblioteca.pdf) y [fuente HTML](Informe_Entregable2_Biblioteca.html) | [Capturas y logs](evidencias/entregable2/) y [guion de 4 minutos](evidencias/entregable2/guion.md) | Preparado y verificado para revisión |
| CAT-WEB | Final, catálogo local | Alta, consulta por título/prefijo, borrado, reinicio y visualización del B+ | React/TypeScript/Vite → API C++ → CatalogService y JSON | [Recorrido en Chrome](evidencias/catalogo-web/navegador.txt), capturas y CTest | Verificado localmente |
| F01 | Final | Rojo-Negro, hash extensible, grafo y FIFO propios. Snapshots de las cinco estructuras | [Diseño y operaciones](DECISIONES.md#d11--índices-en-memoria-para-el-servicio-de-biblioteca) | [Resultados de CTest y snapshots](evidencias/estructuras/) | Integrado en el dominio local F02 |
| F02 | Final | Ejemplares, usuarios, préstamos, reservas y avisos con integridad | [LibraryService y contrato](DOMINIO_API.md) | biblioteca_api, dominio_repositorios y [demo](../scripts/demo_biblioteca.sh) | Implementado; JSON local completo y Mongo con recuperación automática |
| F03 | Final | API/worker/colas reales y persistencia cloud | API de dominio y MongoLibraryRepository; IEventPublisher preparado | [Contrato](PERSISTENCIA_MONGODB.md), [nube real y fixtures](evidencias/persistencia-mongodb/README.md) | Persistencia completa verificada con reinicios en nube. Worker y RabbitMQ pendientes |
| P4-01 | Final | Carga íntegra, índices, historia, secuencias y avisos tras reinicio | Agregado esquema 1; CatalogService + LibraryService candidatos | [Cuatro reinicios reales](evidencias/persistencia-mongodb/nube/nube.log), snapshots JSON | Verificado en nube sintética |
| P4-02 | Final | Confirmar antes de publicar; rechazo/incertidumbre; no POST ciego | Diario durable, GET antes/después, recuperación y bloqueo HTTP | mongo_biblioteca_recuperacion: timeout aplicado/tardío, HTML, lectura fallida, parcial, POST incierto | Fixtures deterministas, sin inducir fallos en nube |
| P4-03 | Final | Datos incompatibles no publican biblioteca vacía | Validación completa y /readyz 503; /healthz 200 | Esquema, referencias, duplicados, lectura incompleta, HTTP y conservación del estado | Verificado con fixtures |
| P4-04 | Final | Contrato real antes de elegir almacenamiento | Campo estado único con revisión; escritor único | [Sondeos](evidencias/persistencia-mongodb/capacidades.log), [filtro anidado](evidencias/persistencia-mongodb/filtro-anidado.log) | PUT condicionado observado; ID de negocio duplicable. Sin garantías de transacción/multiinstancia |
| P4-05 | Final | Modo local, consola, web y CI aislada | JSON agregado y Mongo explícitos; adaptador E2-B conservado | Regresión Debug/ASan, frontend y Actions del PR | Resultados detallados en evidencia de fase |
| F04 | Final | React, autenticación real, Git/CI y despliegue | Pantalla del catálogo y [flujo de CI](../.github/workflows/ci.yml) | CAT-WEB y comprobaciones del PR (CTest Debug, ASan/UBSan y build del frontend) | Parcial. Frontend preparado para Pages con CI previo; publicación y conexión HTTPS pendientes. Autenticación real implementada; ver A-PANEL |
| F05 | Final | Informe técnico, presentación y defensa | Sin iniciar | Sin artefactos | Pendiente |

Los archivos `.cpp` de pruebas están en `backend/tests/`. No se interpreta un
build local como CI remoto, JSON como nube, ni logs de terminal como capturas
del informe. Las cotas, límites y protocolo de guardado se explican en README.
El informe E2-C está integrado en main.

## Preparación de Pages

| Criterio | Implementación | Verificación | Pendiente |
|---|---|---|---|
| Subdirectorio del repositorio | base de Vite para build/preview | Build, archivos y GET 200 de JS/CSS; navegador integrado | Verificar URL pública tras merge |
| URL de API configurable sin duplicar /api | VITE_API_URL como origen HTTPS; validación antes de build | 5 pruebas Node; build válido/rechazos; aviso real sin backend | Proveer backend HTTPS y variable |
| WSL conservado | Raíz local y proxy /api a C++ | Navegador: 16 obras y búsqueda sin resultados | Ninguno en el recorrido comprobado |
| Despliegue posterior a controles | CI llama pages.yml solo en push main, needs backend/frontend | actionlint y CI del PR; PR no despliega | Habilitar Pages, fusionar y verificar deploy real |
| Secretos fuera del sitio | Artefacto exclusivo frontend/dist; variable pública; no secrets | Inspección de workflow y rechazo de credenciales en URL | Publicación HTTPS y configuración de proxy pendientes |


## Acceso y paneles (A-PANEL)

| Criterio | Implementación | Verificación real | Límite |
|---|---|---|---|
| Login, sesión, expiración, logout y roles | AuthService + Argon2id/libsodium + archivo privado | acceso_paneles_api, sesiones reales, errores genéricos, límite de intentos | Una sesión por cuenta; sin recuperación pública de contraseña |
| Usuario A no accede a B ni a administración | Denegación por defecto y consultas propias en HttpController | HTTP directo 401/403/404; prueba de navegador Ana/Bruno | No sustituye aislamiento del host o autenticación de API docente |
| ADMIN gestiona obras, copias, cuentas y solicitudes | Paneles → LibraryService/B+/hash/grafo/FIFO/RN | Edición, ejemplar, entrega y devolución en navegador; regresión API | No despliegue backend |
| Mi biblioteca y pagos propios | Filtrado backend; UI por rol y descarte de respuestas anteriores | Recorrido completo y 9 pruebas frontend | Sesión RAM: recargar exige login |
| FIFO, cancelación y anulación auditada | Auditoria, estados separados, confirmación física obligatoria | API y dominio, fallos sin parcial; dos copias y renovación | Confirmación física es responsabilidad administrativa |
| Cargo entero y recibo único SIMULADO | Cargo/Comprobante en agregado; idempotencia por préstamo | Doble pago, devolución independiente, anulación con recibo conservado | Sin dinero real ni pasarela |
| Reinicio y migración | JSON agregado v2, Mongo mismo protocolo, privado aparte | Dos reinicios API + navegador; migración local y Mongo fixture | No nueva prueba cloud del esquema v2 |
| Regresión y CI | CTest + npm test/build + Pages conservado | [Logs y capturas](evidencias/acceso-paneles/README.md) | RabbitMQ, HTTPS y entrega de avisos pendientes |
