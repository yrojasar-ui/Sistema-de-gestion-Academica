# Biblioteca Académica

Catálogo de obras de Yoel Alexander Rojas Arguedas para Estructuras de Datos II,
Universidad CENFOTEC. La pantalla React consulta una API C++17 local. El mismo
`CatalogService` y `BPlusTree` que usa la consola listan, buscan, insertan y
borran las obras. El panel del índice muestra los nodos y las hojas enlazadas
emitidos por C++.

## Frontend en GitHub Pages

Esta fase prepara el despliegue estático; la publicación efectiva se comprueba
después de revisar y fusionar el PR. La URL esperada es
`https://yrojasar-ui.github.io/Sistema-de-gestion-Academica/`; no se presenta
como un sitio ya publicado.

1. En el repositorio, abrir **Settings > Pages > Build and deployment >
   Source > GitHub Actions**.
2. En **Settings > Secrets and variables > Actions > Variables**, crear la
   variable de repositorio **`VITE_API_URL`** cuando exista una API C++ pública.
3. Revisar y fusionar el PR. El workflow `Biblioteca Académica` ejecuta
   CTest Debug, ASan/UBSan y validación/build del frontend. Solo si todos pasan
   en un push a `main`, invoca `pages.yml` sobre el mismo commit.
   Los PR validan sin publicar ni crear un despliegue.
4. El workflow recompila con Node **24.22.3**, `npm ci`, `npm test`,
   `npm run build` y `npm run verify:dist`; sube **únicamente `frontend/dist`**
   con acciones oficiales fijadas por SHA. El trabajo de despliegue usa el
   entorno `github-pages` y permisos `pages:write` / `id-token:write`.
   Se pueden configurar revisores requeridos y restringir ese entorno a main.
   Ningún secreto de API se pasa al cliente.

**Formato exacto de VITE_API_URL:** origen HTTPS de la API C++,
por ejemplo `https://biblioteca-api.example.org` (ejemplo, no servicio activo).
Admite puerto explícito y una barra final opcional. **No incluir `/api` ni
otras rutas, usuario/contraseña, query o fragmento**. El cliente añade `/api/`
una sola vez: `https://biblioteca-api.example.org/api/libros`.
Un origen inválido, HTTP o localhost hace fallar el build antes de incorporar
el valor al JavaScript. La variable es pública y queda incluida en el bundle:
no introducir tokens ni credenciales. No es la URL de la API MongoDB docente.

Si se omite o queda vacía, el sitio compila y muestra exactamente
**“Servidor de biblioteca pendiente de configuración”**, sin solicitar datos
a localhost/Pages, sin inventar libros y con los controles de catálogo
deshabilitados. Una API configurada pero inaccesible muestra “API sin conexión”;
un resultado válido vacío conserva “Sin resultados” o “El catálogo está vacío”.
Cambiar la variable exige recompilar: ejecutar de nuevo todos los jobs del
workflow de `main` o publicar otro commit revisado en main.

**Verificar el despliegue:** en Actions, abrir la ejecución de `main` y comprobar
que los controles, `Pages después de CI / build` y `deploy` terminan en success.
Seguir `page_url` del entorno `github-pages` o `Visit site` en Settings > Pages.
Abrir la subruta del repositorio, recargarla y revisar en Network que JS/CSS
responden 200 bajo `/Sistema-de-gestion-Academica/assets/`. Si no hay URL de API,
comprobar el mensaje anterior y ausencia de solicitudes `/api`. Si hay backend,
comprobar que las solicitudes van a su origen HTTPS y no contienen `/api/api`.

**Conexión pendiente del backend:** Pages sirve HTML/JS/CSS; **no ejecuta C++
ni RabbitMQ**. Todavía hace falta alojar la API con HTTPS válido, proceso
persistente y almacenamiento configurado, y preparar el worker/broker en su fase.
La API actual escucha en loopback: un proxy HTTPS del alojamiento deberá
reenviar hacia ese puerto y permitir GET, POST, PUT, DELETE y OPTIONS.
Arrancar C++ con `--origin https://yrojasar-ui.github.io` (origen sin la subruta)
para autorizar CORS; verificar el preflight y `Content-Type`.
CORS no sustituye autenticación ni autorización. El perfil administrativo actual
no tiene autenticación: la protección de un backend público queda pendiente.
No se contrataron servicios ni se configuró un proveedor remoto.

Desarrollo WSL conserva `http://127.0.0.1:5173/` y el proxy
`/api → http://127.0.0.1:8080`; `VITE_API_URL` se ignora en desarrollo.
Para comprobar localmente el artefacto de producción sin configurar backend:

~~~bash
cd frontend
npm ci
npm test
VITE_API_URL= npm run build
npm run verify:dist
npm run preview -- --port 4173
# Abrir http://127.0.0.1:4173/Sistema-de-gestion-Academica/
~~~

[Evidencias de esta preparación](docs/evidencias/github-pages/README.md).
Referencias oficiales:
[base de Vite para Pages](https://vite.dev/guide/static-deploy#github-pages),
[variables públicas de Vite](https://vite.dev/guide/env-and-mode),
[workflow y permisos de Pages](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages).

## Persistencia completa en MongoDB (Prompt 4)

La API puede recuperar automáticamente libros, usuarios, ejemplares, préstamos,
reservas, secuencias y avisos pendientes desde una instantánea remota confirmada.
Conserva el modo local JSON/memoria. No hay cambio silencioso de almacenamiento.
La consola y el informe del segundo avance se conservan.

Desde la raíz en WSL, con libcurl de desarrollo instalada:

~~~bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DENABLE_MONGO=ON
cmake --build build/debug -j2
mkdir -p data/mongo-runtime
./build/debug/catalogo_api --storage mongo \
  --endpoint https://paginas-web-cr.com/Api/apis/mongodb.php \
  --collection biblioteca_yoel_final_v1 \
  --mongo-state data/mongo-runtime/biblioteca_yoel_final_v1 --port 8080 --initialize
~~~

La inicialización explícita crea una biblioteca vacía únicamente si no existe.
Para reiniciar, repetí el comando **sin `--initialize`**, conservando colección
y ruta `--mongo-state`. No borrar su diario pendiente ni utilizar una colección
con libros individuales de E2-B. La segunda terminal y URL del frontend son las
mismas que en la sección siguiente.

~~~bash
# Demo real: colección sintética nueva, cuatro reinicios y limpieza propia.
python3 scripts/demo_mongodb.py --binary build/debug/catalogo_api \
  --evidence "build/demo-mongodb-$(date +%Y%m%d-%H%M%S)"
# Salud del proceso y disponibilidad de la biblioteca:
curl -sS http://127.0.0.1:8080/healthz
curl -sS http://127.0.0.1:8080/readyz
# Solo relectura/reconstrucción; nunca reenvía la escritura incierta:
curl -sS -X POST http://127.0.0.1:8080/api/recuperacion \
  -H 'Content-Type: application/json' --data '{}'
~~~

[Esquema, confirmaciones, recuperación y límites](docs/PERSISTENCIA_MONGODB.md).
[Evidencias reales y fixtures separados](docs/evidencias/persistencia-mongodb/README.md).
Solo un escritor y datos sintéticos. El futuro worker deberá coordinar sus cambios
de avisos mediante el único escritor; no debe sobrescribir el agregado directamente.
RabbitMQ, autenticación y despliegue siguen pendientes. CI no ejecuta la demo cloud.

## Arrancar el catálogo web en dos terminales WSL

**Terminal 1 · API C++**. Requiere G++, CMake y Make. Desde la raíz se puede
compilar únicamente el servidor de la demo, con el repositorio JSON.

~~~bash
cd /home/yoerojas/proyectos/Sistema-de-gestion-Academica
cmake -S . -B build/catalogo-web -DCMAKE_BUILD_TYPE=Debug -DENABLE_MONGO=OFF -DBUILD_TESTING=OFF
cmake --build build/catalogo-web --target catalogo_api -j2
./build/catalogo-web/catalogo_api --storage local
~~~

El servidor escucha en `127.0.0.1:8080`. Al primer arranque crea
`data/catalogo_web.json` a partir de las 16 obras de `data/libros_iniciales.json`.
En los siguientes arranques carga el archivo de trabajo, incluso si contiene
una lista vacía. Si el archivo existente es inválido, el arranque informa el
error. Para reanudar una demo después de Ctrl+C, repetí el último comando. Para
usar otro archivo de trabajo, agregá `--working ruta/al/catalogo.json` al
comando del servidor y conservá esa ruta al reiniciarlo.

**Terminal 2 · React, TypeScript y Vite**. Usá Node.js de Linux 22.12 o posterior
con npm. El bloque usa el Node preparado en esta copia de WSL si está presente.
En un clon nuevo podés usar tu instalación de Linux o el bloque de instalación
local de Node que figura a continuación.

~~~bash
cd /home/yoerojas/proyectos/Sistema-de-gestion-Academica
if [ -x build/catalogo-prep/node-v24.22.3-linux-x64/bin/node ]; then
  export PATH="$PWD/build/catalogo-prep/node-v24.22.3-linux-x64/bin:$PATH"
fi
cd frontend
node --version
npm ci
npm run dev
~~~

Si tu WSL x86_64 aún no tiene Node, ejecutá esto una vez antes de la terminal 2.
Descarga la distribución oficial en el directorio de compilación del proyecto
y verifica su SHA-256.

~~~bash
cd /home/yoerojas/proyectos/Sistema-de-gestion-Academica
mkdir -p build/catalogo-prep
cd build/catalogo-prep
version=v24.22.3
archivo="node-$version-linux-x64.tar.xz"
curl -fSLO "https://nodejs.org/dist/$version/$archivo"
curl -fsSL "https://nodejs.org/dist/$version/SHASUMS256.txt" -o SHASUMS256.txt
grep -F "  $archivo" SHASUMS256.txt | sha256sum -c - && tar -xf "$archivo"
~~~

Abrí **[http://127.0.0.1:5173/](http://127.0.0.1:5173/)** desde el navegador.
Vite reenvía `/api` al backend en loopback. Las lecturas y escrituras se
procesan en C++. La página muestra ocho obras por vez, en el orden devuelto
por el B+. El buscador admite prefijo y título exacto. El botón **Ver árbol B+**
incluye la salida textual y la validación de invariantes.

### Contrato HTTP de la demo

| Método | Ruta | Resultado |
|---|---|---|
| GET | `/api/libros` | `{"libros":[…]}` en orden de título e ID. |
| GET | `/api/libros?titulo=Atlas%20del%20campus` | Búsqueda exacta normalizada. |
| GET | `/api/libros?prefijo=Atl` | Búsqueda por prefijo en el B+. |
| POST | `/api/libros` | Recibe JSON con `idLibro`, `titulo`, `autor` y `temas`. Devuelve 201 con el libro guardado. |
| DELETE | `/api/libros/E2C-01` | Borra el ID indicado. Devuelve 204 al guardar o 404 si está ausente. |
| GET | `/api/arbol` | `{"estructura":"…"}` con nodos, hijos y hojas enlazadas. |
| GET | `/api/arbol/validacion` | `{"valido":true,"detalle":"…"}` o el diagnóstico del validador. |

Ejemplo de cuerpo para el alta:

~~~json
{"idLibro":"E2C-01","titulo":"Atlas del campus","autor":"Autor de prueba","temas":["mapas"]}
~~~

Los errores de entrada devuelven 400, un ID duplicado devuelve 409 y un fallo de
guardado devuelve 503. Los cuerpos se limitan a 32 KiB. La API confirma las
mutaciones después del guardado atómico del JSON. Si falla el guardado, relee
el último archivo confirmado. Si tampoco puede recuperarlo, devuelve 503 hasta
el reinicio. Un único trabajador coordina lecturas y escrituras. Dos instancias
de esta API respetan un bloqueo sobre el mismo archivo de trabajo. El archivo
`.lock` puede permanecer al salir y el sistema libera el bloqueo al terminar
el proceso.

[Capturas y resultados de la prueba de navegador](docs/evidencias/catalogo-web/).

## Compilar y ejecutar la consola en WSL

Trabajar desde Ubuntu, usando compilador y herramientas Linux:

```bash
cd /home/yoerojas/proyectos/Sistema-de-gestion-Academica
# Solo si faltan herramientas:
sudo apt-get update
sudo apt-get install g++ cmake make libcurl4-openssl-dev

cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j2
ctest --test-dir build/debug --output-on-failure
./build/debug/biblioteca
```

La dependencia JSON 3.12.0 está incluida, con su licencia y SHA-256 en
`third_party/`. No se descarga nada durante la configuración de CMake.

Para usar la nube, ejecute `./build/debug/biblioteca --storage=mongo`. El contrato, los ejemplos de la colección propia y la evidencia se encuentran en [API MongoDB](docs/API_MONGODB.md).

En el menú JSON: `1` y después `1` carga los 16 libros sintéticos iniciales;
`2` inserta; `3` busca título exacto normalizado; `4` muestra coincidencias,
pide el ID exacto y confirma; `5` lista; `6` muestra B+; `7` valida;
`8` guarda; `9` busca prefijo; `0` sale. Los IDs distinguen obras homónimas.
Al salir con cambios, `g` guarda, `d` descarta y cualquier otra respuesta cancela.
EOF con cambios pendientes produce código 2 y avisa que no se guardaron.

```bash
./build/debug/biblioteca --storage=json --order 3 \
  --initial data/libros_iniciales.json \
  --working data/libros_trabajo.json
```

Se comienza con catálogo vacío; la carga es explícita para no sustituir datos
de trabajo por el fixture sin elección del usuario. Para retomar un guardado,
seleccionar `1` y después `2`. Una ruta personalizada puede cargarse desde el
menú. El archivo destino solo se configura mediante `--working`.

## Demostración y evidencia reproducible

```bash
bash scripts/demo_entregable2.sh
bash scripts/verificar_e2a.sh
```

La demo usa `mktemp -d`, guarda en su propio directorio temporal y lo limpia al
terminar. No escribe en el archivo inicial ni en los libros de trabajo del usuario.
Ejecuta dos procesos: carga → inserta → busca → visualiza/valida → borra →
comprueba ausencia → agrega otro libro → guarda → reinicia y recarga.
Sus comprobaciones fallan con código distinto de cero si el resultado cambia.

El verificador conserva comandos, salida, código por paso, versiones, estado Git
y hashes SHA-256 de las fuentes en un directorio nuevo bajo `docs/evidencias/e2a/`.
Ejecuta compilación y CTest Debug, compilación y CTest con AddressSanitizer y
UndefinedBehaviorSanitizer, detección de fugas y demo independiente.
No usa escrituras cloud. Resultados de esta ejecución: `docs/ESTADO.md`.

Para ejecutar únicamente la variante instrumentada:

```bash
cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build/asan -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/asan -V --output-on-failure
```

## Módulos y recorrido de una consulta

| Módulo CMake | Código | Responsabilidad |
|---|---|---|
| `libro` | `backend/include/biblioteca/Libro.hpp`, `backend/src/Libro.cpp` | Modelo, validación y normalización |
| `bplus` | `BPlusTree.hpp/.cpp` | Nodos, algoritmo, hojas enlazadas, consultas e invariantes |
| `json_repository` | `IBookRepository.hpp`, `JsonBookRepository.hpp/.cpp` | Carga y guardado de snapshots JSON |
| `mongo_repository` | `MongoApiBookRepository.hpp/.cpp`, `CurlTransport.cpp` | CRUD con verificación por ID y libcurl |
| `catalog_service` | `CatalogService.hpp/.cpp` | Coordina índice, unicidad por ID y cambios pendientes |
| `console_ui` | `ConsoleUI.hpp/.cpp` | Entrada/salida y selección inequívoca |
| `biblioteca` | `backend/apps/main.cpp` | Composición y opciones de ejecución |

`ConsoleUI` llama a `CatalogService`, que consulta `BPlusTree`. En JSON el repositorio se usa al cargar o guardar. En Mongo también interviene en cada escritura y en la recarga posterior. Los registros están en las hojas; el auxiliar
`unordered_map<id, clave>` comprueba unicidad y localiza la clave al borrar por ID.
No responde búsquedas por título ni sustituye B+. Un `std::map` en las pruebas
es el oráculo independiente, y otro en la visualización solo asigna nombres N0,
N1… a nodos existentes. Esas etiquetas son locales a cada visualización.

## B+ y normalización

Orden `m` = máximo `m` hijos internos y `m-1` registros en hojas, entre 3 y 128.
Hojas no raíz: mínimo `floor(m/2)` registros. Internos no raíz: mínimo
`ceil(m/2)` hijos. Raíz interna: al menos dos hijos. Todas las hojas están a la
misma profundidad, enlazadas en ambos sentidos. El separador i es exactamente
el mínimo del hijo i+1. La raíz vacía es una sola hoja.

Una inserción divide nodos desbordados. Un borrado retira físicamente la entrada;
si hay subocupación toma una entrada/hijo de un hermano con excedente o fusiona
hermanos y propaga la pérdida del hijo. Una raíz con un solo hijo se contrae.
No hay borrado lógico ni reconstrucción del índice al borrar. Solo la carga
inicial/reinicio construye un índice nuevo a partir de registros validados.

Clave: `(normalizeTitle(titulo), idLibro)`. La normalización valida UTF-8,
convierte A–Z a minúsculas, ÁÉÍÓÚ/áéíóú a vocal sin acento, Ü/ü a u, Ñ a ñ,
y conserva ñ distinta de n. Reconoce también las formas españolas descompuestas
vocal+agudo, u+diéresis y n+tilde. Recorta y agrupa espacios ASCII, tabulador,
CR/LF y espacio no separable. Conserva los demás caracteres Unicode.
No es case folding Unicode general, NFC completo ni colación lingüística:
la ordenación es lexicográfica por bytes UTF-8 y luego ID ASCII.

Con altura h y orden m, la búsqueda desciende h nodos con búsqueda binaria de
separadores; el listado recorre hojas en O(n), y rangos/prefijos añaden el costo
de los resultados. Actualizar separadores aquí vuelve a obtener mínimos de
subárboles: cota conservadora O(m·h²) para inserción/borrado, aparte de comparar
y copiar cadenas. Se prioriza un algoritmo explicable; no se afirma la cota de
una versión optimizada con mínimos almacenados. La validación visita todo el árbol.

## Contrato JSON y guardado

Array de objetos con exactamente `idLibro`, `titulo`, `autor`, `temas`:

```json
[{"idLibro":"DEMO-1","titulo":"Árboles","autor":"Autor Sintético","temas":["computación"]}]
```

ID: 1–64 caracteres ASCII alfanuméricos, guion o guion bajo; único globalmente.
Título y autor no vacíos, máximo 512 y 256 bytes UTF-8. Hasta 32 temas de máximo
128 bytes, no vacíos. Máximo 10000 libros y archivo de 16 MiB. Temas vacíos (`[]`)
son válidos. Títulos normalizados repetidos con distintos IDs son válidos.
Se rechazan campos ausentes/desconocidos/repetidos, tipos incorrectos, IDs
duplicados, JSON truncado, contenido sobrante, UTF-8 inválido y anidación excesiva.
La carga es completa o no cambia el catálogo; informa origen y cantidad.

Las modificaciones quedan en memoria y marcan cambios pendientes. Guardar
serializa todo a un temporal exclusivo en el directorio destino, comprueba
escritura/cierre y realiza rename sobre el archivo de trabajo. Se bloquea usar el
fixture como destino, incluso mediante alias de ruta/enlace duro, y se rechazan
enlaces simbólicos de salida. Si falla, no se anuncia éxito, se limpia el temporal
y se conservan los cambios en memoria para reintentar. El directorio de destino
debe existir. El guardado exitoso solo entonces limpia la bandera de cambios.

Alcance de la consola: un proceso escritor, archivos locales Linux/WSL. No hay bloqueo entre
procesos, detección de ediciones externas, transacciones cloud ni `fsync`.
Se verificó reinicio normal, no durabilidad frente a cortes eléctricos.

## Informe del segundo avance

E2-A y E2-B están integrados en `main`. E2-C documenta el prototipo evaluado en
`f745b22`. El [informe en PDF](docs/Informe_Entregable2_Biblioteca.pdf) y la
[fuente HTML editable](docs/Informe_Entregable2_Biblioteca.html) comparten las
[capturas, el guion de demostración y los registros](docs/evidencias/entregable2/).
Para editar o volver a imprimir el informe, abrí el HTML desde su ubicación en
el repositorio, conservando la carpeta de capturas. Al imprimir, elegí A4,
escala 100 %, gráficos de fondo y desactivá los encabezados y pies del navegador.

## Integración continua

[GitHub Actions](.github/workflows/ci.yml) ejecuta tres comprobaciones en cada PR y en cada push a `main`.
Dos compilan el backend C++17 y ejecutan CTest en Debug, una de ellas con ASan/UBSan y detección de fugas.
Cada una conserva el informe JUnit y el log de CTest durante 14 días. Las pruebas usan archivos temporales,
una API de catálogo en loopback y un transporte simulado para MongoDB.
La tercera usa Node 24.22.3, instala desde el lockfile con `npm ci` y ejecuta `npm run build` en `frontend/`.

## Estructuras de biblioteca

El backend incluye un Rojo-Negro para vencimientos, un hash extensible de usuarios,
un grafo de temas y una FIFO por libro. Cada estructura y el B+ exportan su estado
interno a JSON. Las [decisiones de diseño](docs/DECISIONES.md#d11--índices-en-memoria-para-el-servicio-de-biblioteca)
describen las claves, los límites y cómo los usará el servicio.

Desde la raíz, con el build Debug compilado, se pueden ver los ejemplos y repetir sus pruebas:

```bash
ctest --test-dir build/debug -V -R '^(rojo_negro|hash_extensible|grafo_temas|fifo_reservas|snapshot_bplus)$'
```

## Dominio y API de biblioteca

La API permite administrar usuarios y ejemplares, prestar, devolver, renovar y
gestionar reservas. La edición de libros actualiza título en B+ y temas en el
grafo. El [contrato OpenAPI](docs/openapi.json) y la [guía de dominio con diagramas](docs/DOMINIO_API.md)
detallan los endpoints, estados, persistencia y límites.

Los libros se guardan en el JSON de trabajo. Las entidades nuevas viven en
memoria durante el proceso. Para recuperarlas tras reiniciar, exportá
`GET /api/dominio` y restaurá ese documento mediante `PUT /api/dominio`
sobre el catálogo correspondiente. Los avisos quedan pendientes para la futura
publicación por eventos. La administración local funciona sin autenticación.

Para repetir Ana → Bruno → Carla desde la raíz en WSL, una vez compilado el backend:

```bash
bash scripts/demo_biblioteca.sh
```

El script arranca una API propia con datos temporales, verifica el recorrido,
exporta y reconstruye los turnos tras reiniciar y limpia sus archivos.

## Qué queda pendiente

El informe E2-C se integró en `main`. Para el proyecto final quedan la persistencia MongoDB de las entidades nuevas, sus vistas, el broker, la autenticación y el despliegue. El segundo avance se entrega el 16 de noviembre de 2026 y el proyecto final el 24 de noviembre de 2026.

Documentos de seguimiento: `AGENTS.md`, `docs/REQUISITOS.md`,
`docs/DECISIONES.md`, `docs/ESTADO.md`. El PDF de diseño no estaba en el repositorio;
las decisiones se tomaron de la guía adjunta, sin afirmar haber leído ese PDF.

## Guion de demostración de tres minutos

| Tiempo | Acción | Resultado visible |
|---|---|---|
| 0:00–0:30 | Abrí el catálogo con un archivo de trabajo nuevo y recorré sus dos páginas. | Aparecen las 16 obras iniciales, con título, autor e ID. |
| 0:30–1:00 | En **Agregar libro**, ingresá E2C-01, «Atlas del campus», «Autor de prueba» y el tema «mapas». Guardá y buscá «Atl» por prefijo. | El total sube a 17. La búsqueda muestra el ID E2C-01. |
| 1:00–1:30 | Elegí **Título exacto**, escribí «Atlas del campus» y abrí **Ver árbol B+**. | Aparece una obra. En la salida C++ se ve su clave en una hoja y al final la cadena de hojas. |
| 1:30–2:10 | En la terminal 1, usá Ctrl+C. Recargá el navegador. Reiniciá el mismo backend con el mismo archivo, pulsá **Reintentar** y buscá otra vez «Atlas del campus». | Primero aparece **API sin conexión**. Tras reiniciar, vuelve a aparecer E2C-01. |
| 2:10–2:40 | En la fila E2C-01, pulsá **Borrar** y **Confirmar borrado**. Repetí la búsqueda exacta. | El catálogo confirma el borrado y la consulta muestra **Sin resultados**. |
| 2:40–3:00 | Abrí **Ver árbol B+** y pulsá **Validar invariantes**. | El árbol contiene 16 libros y el validador C++ confirma sus invariantes. |
