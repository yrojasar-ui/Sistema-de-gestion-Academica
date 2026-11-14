# Biblioteca Académica

Catálogo de obras de Yoel Alexander Rojas Arguedas para Estructuras de Datos II,
Universidad CENFOTEC. La pantalla React consulta una API C++17 local. El mismo
`CatalogService` y `BPlusTree` que usa la consola listan, buscan, insertan y
borran las obras. El panel del índice muestra los nodos y las hojas enlazadas
emitidos por C++.

## Arrancar el catálogo web en dos terminales WSL

**Terminal 1 · API C++**. Requiere G++, CMake y Make. Desde la raíz se puede
compilar únicamente el servidor de la demo, con el repositorio JSON.

~~~bash
cd /home/yoerojas/proyectos/Sistema-de-gestion-Academica
cmake -S . -B build/catalogo-web -DCMAKE_BUILD_TYPE=Debug -DENABLE_MONGO=OFF -DBUILD_TESTING=OFF
cmake --build build/catalogo-web --target catalogo_api -j2
./build/catalogo-web/catalogo_api
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

## Qué queda pendiente

El informe E2-C se integró en `main`. La edición de títulos en modo JSON es opcional. Para el proyecto final quedan la integración con préstamos y reservas, las demás vistas, el broker, la autenticación y el despliegue. El segundo avance se entrega el 16 de noviembre de 2026 y el proyecto final el 24 de noviembre de 2026.

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
