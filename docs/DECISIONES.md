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
