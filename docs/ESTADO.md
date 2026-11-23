# Estado del proyecto

Estado vigente: **cierre de entrega final (20 de noviembre de 2026)** al final de este documento. Los apartados
anteriores conservan los resultados y límites históricos de cada fase.

## Prompt 0 — contexto registrado

Inspección del 9 de noviembre de 2026 (America/Guatemala): copia existente en
`/home/yoerojas/proyectos/Sistema-de-gestion-Academica`, rama main sin commits,
solo `.git`, remoto `https://github.com/yrojasar-ui/Sistema-de-gestion-Academica.git`.
No había AGENTS.md, código, integraciones ni PDF de diseño. Se conserva `.git`.
La guía adjunta es la base; no se afirma fidelidad completa a un PDF no disponible.

## E2-A — implementado y verificado localmente

Ejecución final iniciada el **9 de noviembre de 2026 a las 21:49:22 -06:00**.
Entorno: Ubuntu 26.04.1 LTS en WSL2 x86_64; G++ 15.2.0, CMake 4.2.3,
GNU Make 4.4.1, Git 2.53.0. Compilador, build y pruebas ejecutados dentro de Linux.

Se incorporaron modelo Libro, BPlusTree propio, IBookRepository,
JsonBookRepository, CatalogService, ConsoleUI y main de composición. CMake
construye cinco bibliotecas y enlaza la consola y las pruebas. Hay 16 libros
sintéticos iniciales, con dos obras de igual título y diferente ID.
README documenta uso, algoritmos, límites y comandos; AGENTS, matriz y decisiones
distinguen este avance de lo pendiente. Se conserva íntegra la guía recibida.

### Resultado real de comandos

Directorio canónico: [`evidencias/e2a/ejecucion-final/`](evidencias/e2a/ejecucion-final/).
Cada log incluye comando y código de retorno. El TSV contiene los siete pasos.

| Paso | Resultado | Retorno | Evidencia |
|---|---|---|---|
| Configurar Debug con CMake | Correcto | 0 | `debug-configure.log` |
| Compilar Debug (`-j2`) | Correcto, sin advertencias observadas | 0 | `debug-build.log` |
| CTest Debug (`-V --output-on-failure`) | **3/3**, 25,65 s | 0 | `debug-ctest.log` |
| Configurar ASan/UBSan | Correcto | 0 | `asan-configure.log` |
| Compilar ASan/UBSan (`-j2`) | Correcto, sin advertencias observadas | 0 | `asan-build.log` |
| CTest ASan/UBSan | **3/3**, 179,22 s | 0 | `asan-ctest.log` |
| Demo independiente | Correcta; guardado y recarga en segundo proceso | 0 | `demo.log` |

Comando que produjo estos archivos:

```bash
cd /home/yoerojas/proyectos/Sistema-de-gestion-Academica
bash scripts/verificar_e2a.sh docs/evidencias/e2a/ejecucion-final
```

Para una nueva ejecución, usar `bash scripts/verificar_e2a.sh` sin argumento:
creará un directorio fechado y conservará esta evidencia.

### Cobertura comprobada

En cada configuración se ejecutó `bplus_oraculo` con semilla **20261109**, órdenes
**3, 4, 5, 6, 7, 8, 16 y 32**, secuencias ascendentes, descendentes y mezcladas,
operaciones aleatorias y las 120 permutaciones de cinco claves.
Se compararon **23 416 operaciones** y **2 036 201 comprobaciones** con un
oráculo `std::map`, validando invariantes tras cada operación de las secuencias.
Incluye vacío, ausentes, duplicados, borrar todo y reinsertar.

Contadores agregados observados, iguales en Debug y ASan:

| Rama ejecutada | Cantidad |
|---|---:|
| Splits de hoja / interno | 1796 / 645 |
| Préstamos de hoja desde izquierda / derecha | 553 / 448 |
| Préstamos de interno desde izquierda / derecha | 161 / 117 |
| Fusiones de hoja / interno | 1796 / 645 |
| Contracciones de raíz | 262 |

`catalogo_json_consola` realizó **121 comprobaciones**, incluyendo 18 entradas
JSON inválidas, campos/tipos, UTF-8, archivo ausente/demasiado grande, IDs repetidos,
títulos homónimos, fallos del adaptador, selección de borrado y EOF con cambios.
Comprobó protección del fixture (incluidos enlaces), conservación del snapshot
anterior ante guardado inválido, error de escritura/rename, limpieza del temporal,
reintento de guardado, catálogo intacto tras carga rechazada y reconstrucción al
recargar. Un adaptador de prueba verifica que las consultas no llaman al repositorio.

`demo_reinicio` ejecutó dos procesos reales de consola, con archivo de trabajo
temporal: cargó 16 libros, insertó/buscó/borró DEMO-01, verificó ausencia, insertó
PERSIST-01 y guardó. El segundo proceso cargó 17 libros, encontró PERSIST-01 y
confirmó ausencia de DEMO-01. La suma del fixture permaneció intacta.

ASan y UBSan instrumentaron todos los módulos C++ del proyecto, tests y consola,
con `-fsanitize=address,undefined -fno-omit-frame-pointer`, detección de fugas y
detención ante el primer error. **No emitieron diagnósticos en estas ejecuciones**.
Esto cubre los caminos ejercitados, no demuestra corrección universal. Los tiempos
son de la suite con validación intensiva, no benchmarks del B+.

### Identificación y conservación

`fuentes.sha256` identifica 22 archivos de código, pruebas, datos, scripts,
dependencia y CMake. Se volvió a ejecutar `sha256sum -c` al finalizar: **22/22 OK**.
`entorno.log` registra versiones, rama y remoto. `ejecucion-01` conserva la primera
ejecución; no reemplaza la evidencia final después de la revisión del servicio.

El remoto se revalidó con `git ls-remote origin`: código 0, sin referencias
(`evidencias/e2a/remoto.log`). No se modificó el remoto ni el historial existente.
Al cerrar las pruebas, `git var GIT_AUTHOR_IDENT` en WSL devolvió **128**, identidad
desconocida (`evidencias/e2a/identidad-git.log`). El commit quedó pendiente en ese momento.
El usuario autorizó después subir e integrar E2-A en `main`. Para los commits se
reutilizó la identidad ya configurada en Git de Windows, sin cambiar la configuración
de WSL. El historial de GitHub identifica la revisión publicada; estos logs conservan
el estado anterior a la publicación. No se creó una etiqueta de entrega.

### Problemas y límites

- Al empezar faltaban CMake y Make; apt/dpkg falló porque faltaba el ejecutable tar
  aunque el paquete figuraba instalado. Se recuperó desde su paquete oficial,
  se reinstaló tar y se instalaron CMake/Make; el build posterior se ejecutó realmente.
- La revisión detectó una posible publicación de carga antes de resolver su
  procedencia. Se adelantó esa resolución y se probó el fallo inyectado; la suite
  final confirma conservación del estado. No fue una pérdida observada de datos reales.
- La prueba ASan inicial duró 177,09 s frente a timeout de 180 s. Se amplió a 600 s
  sin reducir casos; la ejecución final completó la suite instrumentada en 179,22 s.
- Guardado local de un escritor, sin fsync ni coordinación entre procesos.
  Reinicio normal probado; cortes eléctricos y multiinstancia no garantizados.
- Modificar título es opcional en E2-A y no se implementó. Rango está en BPlusTree
  y pruebas; la consola expone título y prefijo. Normalización española limitada,
  documentada; no orden lingüístico ni normalización Unicode completa.
- PDF inicial no disponible; no se afirma fidelidad completa a ese documento.
- Al cierre de E2-A, E2-B, E2-C y las fases finales quedaban pendientes.

## E2-B — adaptador MongoDB

El 10 de noviembre de 2026 se creó `feat/e2b-mongodb` desde `main` en
`d2454ca`. En la API del profesor se completaron dos ciclos con libros
sintéticos de esta ejecución. El primero permitió fijar el contrato. El segundo
ejercitó la consola C++, con búsqueda por B+ desde otro proceso después del alta.
Ambos terminaron con un GET filtrado que confirmó ausencia.

| Verificación de esta rama | Resultado | Evidencia |
|---|---|---|
| Rutas, cuerpos GET/POST/PUT/DELETE, identidad y reinicio con nube | CRUD observado | [Contrato](API_MONGODB.md) y [transcripción](evidencias/e2b/nube.txt) |
| CTest Debug | 4/4 en 25,93 s | [Salida](evidencias/e2b/debug-ctest.log) |
| CTest ASan/UBSan, con detección de fugas | 4/4 en 159,75 s | [Salida](evidencias/e2b/asan-ctest.log) |
| Demo independiente E2-A con archivo temporal | Código 0, fixture inicial intacto | [Salida](evidencias/e2b/demo.log) |
| Configuraciones, compilaciones, CTest y demo | Siete pasos con retorno 0 | [Resultados](evidencias/e2b/resultados.tsv) |

Se ejecutó `bash scripts/verificar_e2a.sh build/e2b-pruebas`. El oráculo B+
mantuvo sus 23 416 operaciones y 2 036 201 comprobaciones. El catálogo JSON y
la consola conservaron las 121 comprobaciones y la demo entre procesos.
La cuarta prueba usa un transporte simulado y fixtures sanitizados.
Los logs archivados conservan los comandos y códigos de retorno; se recortaron
los espacios finales de línea. El manifiesto identifica 28 archivos.

En este WSL estaba instalada la biblioteca compartida libcurl 8.18.0. Se
extrajeron los encabezados oficiales de Ubuntu en `build-tools/e2b-curl/` para
la compilación. Se pasaron a CMake `CURL_INCLUDE_DIR` y `CURL_LIBRARY_RELEASE`
apuntando respectivamente a esos encabezados y a la biblioteca compartida del
sistema. También se compiló en otro directorio con `-DENABLE_MONGO=OFF` y se
comprobó el arranque JSON y el rechazo explícito de la selección Mongo.

La verificación externa cubre las operaciones de catálogo de E2-B. Las garantías
del servidor y los fallos probados únicamente mediante simulación quedan
enumerados en [API MongoDB](API_MONGODB.md). Al cierre de E2-B quedaban pendientes E2-C y las fases finales.

## E2-C — informe del segundo avance

La rama del informe parte de `f745b22`, que integra E2-A y E2-B. La compilación
en WSL y CTest Debug terminaron con código 0. Pasaron las cuatro pruebas en
22,38 s. El informe cita la ejecución ASan/UBSan versionada en E2-B, así como
el CRUD y el reinicio en la nube observados en esa fase.

En la sesión local se cargaron 16 obras, se insertó y encontró «Atlas del
campus» (E2C-01), se confirmó su borrado y se repitió la búsqueda con cero
resultados. El B+ validó sus invariantes y el JSON inicial conservó su SHA-256.
Se guardaron seis capturas originales de xterm y la salida de la misma sesión.

El [PDF](Informe_Entregable2_Biblioteca.pdf) tiene tres páginas A4. Se
revisaron las tres páginas renderizadas, los seis pies y la legibilidad de los
recortes. La [fuente editable](Informe_Entregable2_Biblioteca.html) usa rutas
relativas a los PNG. La revisión, fusión y etiqueta de entrega quedan pendientes.

[Guion y reproducción](evidencias/entregable2/guion.md) ·
[Sesión JSON](evidencias/entregable2/sesion-json.txt) ·
[Compilación y CTest](evidencias/entregable2/verificacion-local.txt)

## Primer recorrido web del proyecto final

La rama `feat/catalogo-web` parte de `0cb6d31`, que integra E2-C. La pantalla
del catálogo consume el servicio C++17 mediante HTTP local. La demo usa el
archivo de trabajo `data/catalogo_web.json`. El arranque importa las 16 obras del
fixture solo cuando falta el archivo de trabajo.

Se probó con Playwright en Chrome 157.0.8103.51 contra Vite y el proceso C++
de WSL. Se insertó E2C-01, se comprobó por título exacto y prefijo, y se comparó
el texto del panel con el snapshot HTTP. Con el backend detenido, la interfaz
indicó desconexión. Al reiniciar el ejecutable con el mismo JSON reapareció el
libro. Cancelar el borrado lo conservó. Confirmarlo dejó cero coincidencias y
el B+ validó sus invariantes con 16 obras. El SHA-256 del catálogo inicial
coincidió con el registrado en E2-C. El recorrido solicitado se verificó completo.

[Registro del navegador](evidencias/catalogo-web/navegador.txt) ·
[Catálogo inicial](evidencias/catalogo-web/01-catalogo.png) ·
[Árbol con el libro insertado](evidencias/catalogo-web/02-arbol.png) ·
[Búsqueda vacía y validación](evidencias/catalogo-web/03-validacion.png).

En la regresión pasaron las cuatro pruebas anteriores y la nueva prueba HTTP
en ambas configuraciones: 5/5 en Debug (25,92 s) y 5/5 con ASan/UBSan
(176,86 s). La demo independiente de la consola terminó con retorno 0.
El frontend compiló después de instalar las dependencias desde el lockfile.
[Regresión local y compilación del frontend](evidencias/catalogo-web/pruebas.txt).

## Integración continua del catálogo web

El [flujo de GitHub Actions](../.github/workflows/ci.yml) parte del catálogo integrado en el PR #4.
En los PR y en cada push a `main`, dos jobs compilan la consola, la API y las pruebas con CMake.
Ambos ejecutan la suite registrada en CTest. El job instrumentado activa ASan, detección de fugas
y UBSan, y falla ante el primer diagnóstico. El oráculo del B+ dispone de 600 segundos.
Los artefactos `ctest-Debug` y `ctest-ASan-UBSan` contienen el XML de resultados y el log.
El tercer job verifica TypeScript y genera el build de Vite a partir de `frontend/package-lock.json`.

La prueba HTTP usa un servidor local en loopback y un JSON temporal. El contrato MongoDB se
ejercita con el transporte simulado de `backend/tests/test_mongo.cpp` y su fixture versionado.
Las operaciones reales de E2-B siguen documentadas en [el registro de nube](evidencias/e2b/nube.txt).

## Prompt 2 — estructuras de biblioteca

El commit `dab9b86609b5aea5cbc1a4ddd3d55223e677f066` implementa los cuatro índices
en C++17 y la exportación JSON de las cinco estructuras. Las [decisiones de diseño](DECISIONES.md#d11--índices-en-memoria-para-el-servicio-de-biblioteca)
documentan el alcance en memoria y los límites. Los [snapshots de ejemplo](evidencias/estructuras/snapshots.json)
proceden de las pruebas de Debug. La integración con préstamos, reservas y vistas queda para la fase siguiente.

| Caso CTest | Semilla | Operaciones contrastadas | Comprobaciones |
|---|---:|---:|---:|
| rojo_negro | 20261114 | 3085 | 9261 |
| hash_extensible | 20261115 | 659 | 26795 |
| grafo_temas | 20261116 | 518 | 6310 |
| fifo_reservas | 20261117 | 582 | 5043 |
| snapshot_bplus | 20261118 | 7 | 17 |
| bplus_oraculo | 20261109 | 23416 | 2036201 |

Los contadores de operaciones corresponden a las secuencias de cada prueba. Las
comprobaciones incluyen consultas al oráculo, recorridos e invariantes.

En Ubuntu 26.04.1 bajo WSL2, GCC 15.2.0 y CMake 4.2.3, el comando
`bash scripts/verificar_e2a.sh build/prompt2/dab9b86` completó las dos compilaciones
y la demo de reinicio. CTest pasó 10/10 en [Debug](evidencias/estructuras/debug-ctest.log)
y 10/10 con [ASan/UBSan y detección de fugas](evidencias/estructuras/asan-ctest.log).
Los [códigos de retorno](evidencias/estructuras/resultados.tsv) y los
[hashes de las fuentes](evidencias/estructuras/fuentes.sha256) corresponden a esa ejecución.
Los resultados de GitHub Actions se consultan por separado en las comprobaciones del PR.

## Prompt 3 — dominio y API local

El 14 de noviembre de 2026 se partió de main con el PR #6 integrado en
`594f221d3f1dd9d966d1a0700eba9c2869b58db3`. Su [CI en main](https://github.com/yrojasar-ui/Sistema-de-gestion-Academica/actions/runs/43607624786)
tenía los tres jobs aprobados antes de comenzar.

El código comprobado corresponde a `2e326855e09b2764ce11ee00576f421885d590aa`.
El [contrato OpenAPI](openapi.json), los [diagramas y reglas de dominio](DOMINIO_API.md)
y las [decisiones de confirmación local](DECISIONES.md#d12--dominio-con-catálogo-único-y-confirmación-local)
describen esta implementación. Los libros mantienen su repositorio JSON y las
entidades nuevas usan MemoryDomainRepository con exportación y reconstrucción explícitas.

| Comprobación WSL | Resultado | Evidencia |
|---|---|---|
| Configuración y compilación C++17 Debug | Código 0 | [Build](evidencias/dominio/debug-build.log) |
| CTest Debug | 12/12, 31,66 s | [CTest](evidencias/dominio/debug-ctest.log) |
| Configuración y compilación ASan/UBSan | Código 0 | [Build instrumentado](evidencias/dominio/asan-build.log) |
| CTest ASan/UBSan con fugas | 12/12, 228,97 s | [CTest instrumentado](evidencias/dominio/asan-ctest.log) |
| Demo anterior de consola y reinicio | Código 0 | [Demo E2](evidencias/dominio/demo.log) |
| Ana → Bruno → Carla por HTTP | Completado, índices y estados comprobados | [Demo de dominio](evidencias/dominio/demo-dominio.log) |
| Escenarios adicionales por HTTP | Completados | [Aceptación](evidencias/dominio/contrato-http.log) |
| Frontend Node 24.22.3, npm ci y npm run build | Código 0 | [Build frontend](evidencias/dominio/frontend.log) |
| Contrato OpenAPI en WSL | 34 schemas y 411 respuestas HTTP reales validados | [Contrato](evidencias/dominio/openapi.log) |

El recorrido rechaza el segundo préstamo sin alterar datos, JSON ni snapshots.
La devolución de Ana conserva su historial, vacía el índice activo y aparta E1
para Bruno. Su cancelación reasigna E1 a Carla. Carla recibe la copia y devuelve,
dejando dos préstamos históricos, cero activos y E1 disponible.

Se probaron usuarios inactivos, reservas duplicadas, dos copias prestadas a la
vez, desactivación con reservas, cambio de vencimiento, título y temas, borrados
con referencias, eliminación física sin historial, importaciones incompatibles,
calendario, límite de exportación y fallos de confirmación. La prueba C++ inyecta fallos de repositorio
en devolución, cancelación, renovación y escritura de libros y contrasta datos
e índices con el estado anterior.

Los comandos de CMake/CTest y sus códigos están en los logs y en
[resultados.tsv](evidencias/dominio/resultados.tsv). Se ejecutó
`bash scripts/verificar_e2a.sh build/prompt3/2e32685` y
`bash scripts/demo_biblioteca.sh`. El [entorno](evidencias/dominio/entorno.log)
y los [hashes de las fuentes](evidencias/dominio/fuentes.sha256) identifican el código probado.
Los resultados del runner de GitHub se registran por separado en el PR.

Quedan pendientes los adaptadores MongoDB de entidades nuevas y sus garantías
de recuperación, el publicador/worker RabbitMQ, autenticación y vistas del
dominio. Los avisos observados son registros pendientes en memoria.

## Prompt 4 — persistencia Mongo completa

El 15 de noviembre de 2026 (America/Guatemala) se partió de main con PR #7
integrado, `a0cec44`, y se verificó su
[CI correcto](https://github.com/yrojasar-ui/Sistema-de-gestion-Academica/actions/runs/43798520621).
El árbol estaba limpio; rama de trabajo `feat/persistencia-mongodb`.
Se conserva `entregable-2` en `0cb6d3156b802fd27b66e5e7052bd7ab20986d87`.

`MongoLibraryRepository` almacena libros y dominio en un agregado versionado.
Reutiliza el transporte y protocolo del adaptador E2-B. El modo Mongo carga y
valida completamente antes de habilitar operaciones, reconstruye los cinco
índices y conserva historia, secuencias y avisos sin importación manual.
El modo local JSON/memoria, la consola y la pantalla web se mantienen.

Cada escritura se registra en un diario durable, se envía una sola vez y se
confirma por GET antes de publicar en memoria. Se distinguen confirmación,
rechazo e incertidumbre. Incertidumbre o carga inválida bloquean la biblioteca;
healthz sigue informando el proceso y readyz informa almacenamiento y recuperación.
El endpoint de recuperación solo relee/reconstruye. Un diario pendiente ilegible
también impide inicializar y no se presenta como rechazo sin cambios.

### Verificación real y alcance

[Índice y logs de la fase](evidencias/persistencia-mongodb/README.md).

- **Nube real:** GET inicial, objetos anidados, filtros de ID/revisión, un PUT
  ganador en competencia acotada y aceptación de POST duplicados. Se limpiaron
  solo ObjectId sintéticos creados. No acredita multiinstancia ni transacciones.
- **Historia real:** primer reinicio conserva préstamo renovado de Ana, copia
  prestada y turnos Bruno 1 → Carla 2; segundo conserva apartado Bruno y aviso
  pendiente; tercero conserva historial de Ana/Carla, copia disponible y cuatro
  avisos sin duplicados; cuarto conserva nueva reserva turno 3, siguiente 4 y
  aviso AV-5. Edición de título/temas comprobada por B+ y grafo tras reinicio.
  La demo final limpió su documento y confirmó GET vacío.
- **WSL:** regresión Debug 13/13 (27,25 s), ASan/UBSan 13/13 (195,45 s),
  consola y frontend correctos. Tras la revisión final de composición/diario
  se repitieron las cuatro pruebas API/Mongo en ambos modos instrumentales.
  También se compiló sin Mongo y se comprobó el recorrido HTTP local.
- **Contrato:** 411 respuestas locales, 75 respuestas del modo Mongo y cuatro
  snapshots de reinicio conformes con OpenAPI / JSON Schema.
- **Fixtures:** lecturas fallidas, timeouts aplicados y tardíos, HTML, rechazo,
  parcial, datos incompatibles, duplicados, truncamiento detectable y diario
  incierto a través de reinicio. Se bloquea sin publicar estado parcial y se
  recupera por GET cuando el remoto coincide exactamente. No se indujeron
  fallos en el servidor compartido.

GitHub Actions ejecuta otra verificación del HEAD publicado: el enlace y estado
remoto se consignan en el PR, separados de estos resultados WSL.
CI continúa aislado de la API docente.

Límites pendientes: autenticación y aislamiento externos, unicidad servidor,
paginación, transacciones, tamaño remoto y soporte multiinstancia no garantizados.
La cota propia es 1 MiB por agregado. Un parcial/resultado irresoluble exige
intervención del operador; no se descarta el diario para asumir rechazo.
El worker futuro debe coordinar sus cambios por el escritor único o por un
registro de entregas separado con contrato verificado; no puede sobrescribir
avisos en el agregado directamente. RabbitMQ, entrega de avisos, autenticación,
despliegue e informe final no se implementan en esta fase.

[Diseño y recuperación](PERSISTENCIA_MONGODB.md), [API](openapi.json) y
[diagramas](DOMINIO_API.md#diagramas-editables) actualizados.

## Preparación del frontend para GitHub Pages

15 de noviembre de 2026, America/Guatemala. Se partió de main actualizado
`6692b41` (PR #8 integrado), sin cambios locales; rama `feat/github-pages`.

Vite genera recursos bajo `/Sistema-de-gestion-Academica/` en build/preview,
manteniendo raíz y proxy en desarrollo. `VITE_API_URL` es un origen HTTPS
público sin /api; el cliente añade ese segmento una sola vez. Sin variable,
muestra “Servidor de biblioteca pendiente de configuración”, no consulta
localhost ni inventa datos. Los controles quedan deshabilitados.
Una URL inválida no vacía detiene el build antes de incluirla en el bundle.

`pages.yml` usa acciones oficiales fijadas por SHA, Node 24.22.3, npm ci/build,
artefacto exclusivo frontend/dist y entorno github-pages con permisos mínimos.
CI lo invoca únicamente para push main después de pasar los jobs backend
(Debug y ASan/UBSan) y frontend. Los PR validan sin desplegar.

[Resultados WSL y navegador](evidencias/github-pages/README.md): frontend 5/5,
build con/sin origen configurado, rechazo de /api y credenciales, rutas de
recursos con GET 200, mensaje sin backend en navegador, proxy local con 16 obras
y búsqueda sin resultados. CTest existente: Debug 13/13 y ASan/UBSan 13/13.
actionlint 1.7.12 sin errores. No se modificaron backend, fixture inicial,
informe E2-C, dependencias fijadas ni etiqueta entregable-2.

Se comprobó por lectura que Pages ya usa Source = GitHub Actions y que la
variable VITE_API_URL está ausente. Publicación real y conexión HTTPS/CORS
quedan pendientes de revisión/merge y configuración. El enlace de Actions del
PR distingue los resultados GitHub de los de WSL. No se contrató alojamiento
ni se desplegaron C++ o RabbitMQ.


## Acceso, paneles y pagos simulados — 16 de noviembre de 2026

Base: main 8b860935 con PR #9 integrado. Árbol limpio al comenzar, rama
feat/acceso-paneles-biblioteca. Se conservan entregable-2, informes, catálogo
inicial, consola, Pages y proxy WSL. No se fusionan PR ni se publica el backend.

Implementados AuthService (libsodium/Argon2id, sesiones opacas, expiración,
logout, roles y limitación de intentos), cuentas privadas persistentes y
autorización por propietario. Todos los endpoints sensibles requieren sesión;
salud pública mínima. Paneles ADMIN/USUARIO en español conectados a API real.
Mi biblioteca separa solicitudes/préstamos y pagos propios; cambio de sesión
aborta solicitudes y descarta respuestas anteriores.

Dominio v2: auditoría de cancelaciones, anulación administrativa con confirmación
física, cargos enteros y comprobantes SIMULADO idempotentes. Repositorio local
agregado persistente; Mongo conserva confirmación, diario y bloqueo. Migración
explícita, sin cargos retroactivos ni credenciales en el agregado.
README contiene bootstrap, dos terminales, respaldo y guion de 5–7 minutos.
OpenAPI y diagramas actualizados.

WSL: 14/14 CTest Debug y 14/14 ASan/UBSan, demo de consola preservada.
Pruebas adicionales posteriores amplían pago Mongo, migración y fallo del
almacén privado; sus logs están separados. Frontend: npm ci, 9/9 tests,
build y verificación de recursos bajo subruta Pages.
Navegador integrado real: edición administrativa, copia, solicitud de Ana,
entrega, comprobante simulado, cambio a Bruno sin datos de Ana, devolución y
reinicio con historial conservado. Capturas auténticas de viewport estrecho.

[Evidencia y resultados definitivos](evidencias/acceso-paneles/README.md).
No se realizaron escrituras cloud en esta fase: migración y pagos Mongo están
comprobados con fixtures. Pendientes: backend HTTPS público, despliegue remoto
del backend, RabbitMQ/worker y entrega real de avisos; no se implementa cobro real.
Segundo avance: 16 de noviembre; entrega final: 24 de noviembre de 2026.

Al continuar el 17 de noviembre, main ya contenía PR #10 (integración realizada
por el usuario de las etapas de dominio/autenticación) y 4c9d21b, que retiró el
guion antiguo de tres minutos. Se incorporó ese main a la rama, conservando esa
retirada y el nuevo guion de 5–7 minutos solicitado para esta fase. PR #11
completa paneles, documentación y verificación; permanece sin fusionar.

## Correcciones del PR #11 — 18 de noviembre de 2026

Rama feat/acceso-paneles-biblioteca, PR abierto sin fusionar. Reconciliación
de mutaciones inciertas integrada en catálogo y paneles: resultado explícito,
consulta de estado/listas y envío bloqueado para ese intento. Lecturas y
respuestas tardías quedan ligadas a la sesión original. Mi biblioteca incluye
título/autor desde un DTO autorizado; conserva historial, permisos e índices.

WSL: CTest Debug **14/14** y ASan/UBSan **14/14**; frontend **18/18**,
npm ci, TypeScript/Vite y verify:dist correctos. Recorrido API: **195 solicitudes**;
**119 respuestas JSON** contrastadas con OpenAPI. Navegador real: solicitud
aplicada con respuesta perdida por proxy local; reconsulta automática y manual
sin repetir POST; títulos en activos/solicitud/historial; Bruno no ve datos de Ana.
La carrera de cambio de cuenta durante la lectura y backend bloqueado se
verificaron con fixtures deterministas, no forzando fallos en nube.

[Evidencias y comandos](evidencias/acceso-paneles/correcciones-pr11/README.md).
GitHub Actions del nuevo HEAD se verifica al publicar y se enlaza en el PR;
estos registros son locales. Se conservan entregable-2, catálogo inicial,
persistencia, Pages y las evidencias históricas. Sin despliegue ni escrituras cloud.

## Cierre final — 20 de noviembre de 2026

Base: `main` en `225e151` (PR #11 ya fusionado el 19 de noviembre de 2026), árbol limpio, rama revisable `feat/cierre-final-biblioteca` (PR abierto sin fusionar). Se conservan intactos `entregable-2`, `data/libros_iniciales.json` (`SHA-256: 7852aadb1c01f71840aa34e8cf828495caf3ca195c88d656019d6ff702cffce5`), las cinco estructuras C++17, la consola, los informes anteriores y Pages.

### Implementado y comprobado en esta fase

- **RabbitMQ 4.0.5 real y worker C++17 independiente:** cliente AMQP 0-9-1 con `rabbitmq-c 0.15.0` fijado por SHA-256 (`scripts/preparar_rabbitmq_c.sh`), colas y exchanges durables (`biblioteca.eventos`, `biblioteca.avisos`, `biblioteca.dlx`, `biblioteca.avisos.fallidos`), `confirm.select` + `mandatory=1` + detección de `basic.return`, hilo desacoplado `AvisoOutboxRelay` sin bloquear HTTP, binario `biblioteca_worker` con `prefetch=1`, ACK manual post-persistencia, deduplicación 1:1 (`NT-AV-n`), reintentos acotados con backoff y DLQ.
- **Identidad de servicio mínima (`ServiceIdentity`):** token `0600` independiente de cuentas humanas (`--bootstrap-service-token`), hash BLAKE2b en tiempo constante, autorizado exclusivamente en `/api/interno/avisos/{id}/(procesar|fallo)`.
- **Bandeja *Mis avisos* y panel administrativo *Avisos*:** notificaciones procesadas visibles solo a su dueño (`/api/mis-avisos`), estado del broker sin credenciales, diferenciación de los cuatro estados (`pendiente de publicación`, `publicado`, `procesado`, `fallido`) y reintento manual sin borrado silencioso (`/api/avisos/{id}/reintento`). Catálogo y préstamos siguen operativos si RabbitMQ cae.
- **Proxy de confianza, Docker, HTTPS y CI:** `--trusted-proxy` en `catalogo_api` para rate-limit por IP real sin confiar en cabeceras arbitrarias; `deploy/Dockerfile.backend`, `deploy/Dockerfile.frontend`, `docker-compose.yml`, `.env.example`, `deploy/Caddyfile.https.example`, `deploy/nginx-https.conf.example` y `docs/DESPLIEGUE_HTTPS_DOCKER.md`; CI actualizado en `.github/workflows/ci.yml` con `rabbitmq-server` real en Debug y ASan/UBSan.
- **Informe, presentación y defensa:** [Informe Técnico Final](Informe_Final_Biblioteca.pdf) (14 páginas A4 + [HTML editable](Informe_Final_Biblioteca.html) + 5 diagramas editables en `docs/diagramas/`), [Presentación editable PPTX](Presentacion_Final_Biblioteca.pptx) y [PDF](Presentacion_Final_Biblioteca.pdf) (14 diapositivas revisadas), [guion de exposición](defensa/GUION_EXPOSICION.md), [guion de demo y Plan B](defensa/GUION_DEMO_Y_PLAN_B.md) y [guía de defensa (24 preguntas)](defensa/GUIA_DEFENSA_PREGUNTAS.md).

### Resultados reales verificados (20 de noviembre de 2026)

| Comprobación | Entorno real | Resultado | Evidencia |
|---|---|---|---|
| CTest Debug | WSL2 Ubuntu 26.04 · GCC 15.2 · RabbitMQ 4.0.5 | **16/16** en 48,87 s (código 0) | [debug-ctest.log](evidencias/final/pruebas/debug-ctest.log) |
| CTest ASan/UBSan + fugas | Instrumentado completo + RabbitMQ 4.0.5 | **16/16** en 181,52 s, **0 diagnósticos** | [asan-ctest.log](evidencias/final/pruebas/asan-ctest.log) |
| Frontend Node 24.22.3 | `npm test`, `tsc`, `vite build`, `verify:dist` | **19/19** pruebas OK, bundle 265,79 kB | [frontend-y-entorno.log](evidencias/final/pruebas/frontend-y-entorno.log) |
| MongoDB cloud + RabbitMQ + Worker | Colección sintética `biblioteca_yoel_final_3be394f489e7431c` | Rev. 40, **10/10 avisos** procesados, reinicio íntegro, **0 secretos**, limpieza `total=0` | [nube-final.log](evidencias/final/mongodb-nube/nube-final.log), [checkpoint-final.json](evidencias/final/mongodb-nube/checkpoint-final.json) |
| Navegador Chrome 157.0.8103.80 | Escritorio (1280×800), móvil (390×844) y Pages | Admin/Ana/Bruno, aislamiento, 8 capturas auténticas | [navegador.log](evidencias/final/navegador/navegador.log) |
| GitHub Pages remoto | `https://yrojasar-ui.github.io/Sistema-de-gestion-Academica/` | `HTTP/2 200`, frontend estático publicado con mensaje servidor pendiente | [08-github-pages-publicado.png](evidencias/final/navegador/08-github-pages-publicado.png) |

### Pendientes externos documentados

1. **Alojamiento HTTPS remoto del backend:** pendiente de seleccionar y aprovisionar un proveedor Linux con TLS y volumen persistente (no se contrataron servicios de pago ni se inventó URL pública). Pages está publicado como frontend estático, pero la aplicación remota conectada requiere definir `VITE_API_URL` tras desplegar el backend.
2. **Ejecución de contenedores Docker:** los manifiestos se prepararon y validaron sintácticamente, pero el daemon de Docker Desktop no estaba activo en WSL (`dockerDesktopLinuxEngine` inaccesible); no se reporta ejecución de contenedores.

Índice completo de evidencias: [docs/evidencias/final/README.md](evidencias/final/README.md) · Lista final de entrega: [docs/LISTA_ENTREGA_FINAL.md](LISTA_ENTREGA_FINAL.md).


## Refinamiento pre-defensa: pagos, estados UTC, auditoría de excepciones y dataset aislado — 22 de noviembre de 2026

Base: `main` en `f3de312` (PR #12 fusionado), rama `feat/defensa-ux-validaciones`, conservando `entregable-2` (`0cb6d31`) y los entregables anteriores.

- **Pagos simulados**: filtros por estado en *Admin → Pagos* con lector, libro, importe, fecha UTC, comprobante e insignia *«Pago simulado realizado»*; confirmación persistente *«Usted ha pagado. Pago simulado registrado.»* en *Usuario → Mis pagos* (conservada tras recargar y reiniciar); cargos pagados y luego anulados conservan el comprobante histórico; idempotencia en `LibraryService::pagar()` ante doble clic o reintento sin duplicar comprobante ni aviso.
- **Préstamos, reservas, historiales y avisos**: préstamos activos ordenados por vencimiento con estado UTC (*Vencido*, *Vence hoy*, *Vence pronto*, *Vigente*); posición en cola FIFO (`posicionCola`) en reservas pendientes y fecha de cierre + motivo en atendidas, canceladas, devoluciones y anulaciones; *Mis avisos* ordenado por fecha descendente con enlace *Ir a Mis pagos* desde el aviso `pago_simulado`.
- **Auditoría de excepciones y validaciones**: documentada en [AUDITORIA_EXCEPCIONES_Y_VALIDACIONES.md](AUDITORIA_EXCEPCIONES_Y_VALIDACIONES.md) (capturas vacías eliminadas en `HttpController.cpp` y `biblioteca_worker.cpp`, manejo de JSON corrupto en `LocalLibraryRepository.cpp` como `503 PersistenceError`, validación temprana de IDs y `AMQP_PORT`, registro sanitizado en logs y pruebas de regresión `regresion_validaciones` + `domain-helpers.test.ts`).
- **Datos y verificación para defensa**: script repetible [scripts/preparar_demo_defensa.py](../scripts/preparar_demo_defensa.py) sobre instancia aislada (`build/demo-defensa/`), 17 avisos procesados por el worker, dos sesiones (`Ana`/`Admin`), reinicio real de API, aislamiento de `Bruno`, cargo pendiente `P-DEF-VIG` y ejemplares `E-DEF-5`/`E-DEF-6` listos para demostración en vivo. Evidencias y 6 capturas sin credenciales en [evidencias/defensa/README.md](evidencias/defensa/README.md).
- **Resultados verificados en WSL2**: CTest Debug **17/17** (56,61 s), ASan/UBSan + LeakSanitizer **7/7** suites de integración y regresión (39,91 s, 0 diagnósticos), Frontend **25/25** pruebas + `build:docker` + `verify:dist:docker` + `build` + `verify:dist`.
