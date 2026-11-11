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
