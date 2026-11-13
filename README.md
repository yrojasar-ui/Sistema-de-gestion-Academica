# Biblioteca académica — prototipo E2-A y adaptador E2-B

Proyecto de Yoel Alexander Rojas Arguedas, Universidad CENFOTEC,
Estructuras de Datos II. Se ejecutaron **Prompt 0, E2-A y E2-B** de la guía
conservada en `docs/referencia/Prompts_Codex_Biblioteca_ED2.md`.

Catálogo C++17 con B+ propio, consola, guardado JSON explícito y persistencia por documento en la API MongoDB del profesor.
Incluye inserción, consulta por título/prefijo/rango, borrado real, listado,
visualización de los nodos reales y validación de invariantes. El rango está
disponible en la API C++ y sus pruebas; título y prefijo también en consola.
El modo Mongo permite actualizar un libro por ID. El modo JSON conserva el menú de E2-A.

## Compilar y ejecutar en WSL

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

Alcance: un proceso escritor, archivos locales Linux/WSL. No hay bloqueo entre
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

## Qué queda pendiente

La revisión y fusión del informe preceden a la etiqueta `entregable-2`. La
edición de títulos en modo JSON es opcional. Para el proyecto final quedan las
otras estructuras, préstamos y reservas, API HTTP propia, frontend, broker,
autenticación, CI remoto y despliegue. La fecha exacta del segundo avance sigue
pendiente y el proyecto final está previsto para el 24 de noviembre de 2026.

Documentos de seguimiento: `AGENTS.md`, `docs/REQUISITOS.md`,
`docs/DECISIONES.md`, `docs/ESTADO.md`. El PDF de diseño no estaba en el repositorio;
las decisiones se tomaron de la guía adjunta, sin afirmar haber leído ese PDF.
