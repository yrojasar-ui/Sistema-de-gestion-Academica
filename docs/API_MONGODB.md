# API MongoDB del catálogo

El 10 de noviembre de 2026 se comprobó el CRUD en la colección `biblioteca_yoel_ed2_libros` desde Ubuntu en WSL. Las dos ejecuciones utilizaron libros sintéticos nuevos, cada uno con un `idLibro` generado para esa ejecución. La evidencia conserva los intercambios HTTP y la búsqueda desde otro proceso de consola. [Transcripción sanitizada](evidencias/e2b/nube.txt)

## Ruta y operaciones observadas

Host HTTPS `paginas-web-cr.com`. El primer GET solicitó exclusivamente la colección propia. La comparación de rutas dio estos resultados.

| Método y ruta | Respuesta real | Comprobación |
|---|---|---|
| GET `/Api/index.php?coleccion=biblioteca_yoel_ed2_libros` | 200, `text/html; charset=UTF-8`, página titulada «API» | El adaptador rechaza ese tipo de contenido. |
| GET `/Api/apis/mongodb.php?coleccion=biblioteca_yoel_ed2_libros` | 200, `application/json; charset=UTF-8` | Objeto con `coleccion`, `total` y `documentos`. La primera lectura devolvió cero documentos. |
| GET en la misma ruta con `&idLibro=…` | 200, JSON | Un documento para el ID presente y cero para otro ID mientras el primero existía. |
| POST `/Api/apis/mongodb.php` | 200, JSON con `mensaje` | El alta devolvió un mensaje. El GET posterior devolvió los campos enviados y el `_id` asignado. |
| PUT `/Api/apis/mongodb.php` | 200, JSON con `modificados: 1` | Un GET recuperó el título nuevo con el mismo `_id`. |
| DELETE `/Api/apis/mongodb.php` | 200, JSON con `eliminados: 1` | Un GET por el mismo `idLibro` devolvió `total: 0` y `documentos: []`. |

## Ejemplo del intercambio

Este libro reproduce la primera alta con identificadores sustituidos. En la solicitud POST se envió el siguiente cuerpo JSON.

```json
{
  "coleccion": "biblioteca_yoel_ed2_libros",
  "datos": {
    "idLibro": "E2B-FIXTURE", "titulo": "Bitácora sintética E2B",
    "autor": "Autor sintético", "temas": ["prueba"],
    "proyecto": "biblioteca_yoel_ed2_libros"
  }
}
```

El acuse contenía `{"mensaje":"Documento insertado correctamente en 'biblioteca_yoel_ed2_libros'"}`. El GET filtrado devolvió el documento en este sobre.

```json
{
  "coleccion": "biblioteca_yoel_ed2_libros",
  "total": 1,
  "documentos": [{
    "_id": {"$oid": "0123456789abcdef01234567"},
    "idLibro": "E2B-FIXTURE", "titulo": "Bitácora sintética E2B",
    "autor": "Autor sintético", "temas": ["prueba"],
    "proyecto": "biblioteca_yoel_ed2_libros"
  }]
}
```

`idLibro` es la identidad estable del catálogo. El adaptador conserva el ObjectId leído para cada libro. Antes de PUT o DELETE vuelve a leer el ID y comprueba que coinciden el ObjectId y los campos del registro seleccionado. En el filtro de escritura, la API aceptó el ObjectId como cadena hexadecimal. Ambos métodos usaron este cuerpo base. El PUT añadió `datos` con los campos a modificar.

```json
{
  "coleccion": "biblioteca_yoel_ed2_libros",
  "filtro": {
    "_id": "0123456789abcdef01234567",
    "idLibro": "E2B-FIXTURE",
    "proyecto": "biblioteca_yoel_ed2_libros"
  }
}
```

## Ejecutar en WSL

El binario completo requiere libcurl 7.85 o posterior con HTTPS y certificados de confianza. En Ubuntu el paquete de desarrollo es `libcurl4-openssl-dev`. CMake usa la biblioteca del sistema y el JSON 3.12.0 incluido en el repositorio. La prueba utilizó libcurl 8.18.0, distribuida bajo la licencia curl.

```bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DENABLE_MONGO=ON
cmake --build build/debug -j2
./build/debug/biblioteca --storage=mongo \
  --endpoint https://paginas-web-cr.com/Api/apis/mongodb.php \
  --collection biblioteca_yoel_ed2_libros
```

Mongo carga la colección al arrancar. La opción `1` la vuelve a leer, `2` inserta, `3` busca por título, `8` actualiza por ID y `4` permite seleccionar y confirmar el borrado. Las opciones `6` y `7` muestran el B+ y validan sus invariantes. Para repetir la demostración, use un ID nuevo, anótelo y borre únicamente el libro de esa ejecución. En las pruebas se inició un segundo proceso antes de actualizar para comprobar la persistencia y la reconstrucción del B+.

Con `--storage=json`, o sin indicar almacenamiento, sigue vigente el guardado explícito de E2-A. Para compilar solo ese modo puede usar `-DENABLE_MONGO=OFF`. Un fallo al iniciar Mongo termina con error. Cada escritura Mongo afecta a un documento. El repositorio rechaza el guardado de snapshots y genera internamente el filtro exacto de cada mutación.

## Confirmación y recuperación

- Antes del POST, el adaptador comprueba con GET que el ID esté libre. Envía la escritura una vez. Después contrasta los campos mediante otro GET filtrado. Para PUT verifica también el ObjectId original. Para DELETE exige ausencia.
- Cuando el acuse se pierde o es ilegible, el GET permite reconciliar un estado reconocible. Una respuesta de rechazo, un contador inesperado, lecturas contradictorias o un fallo al releer pueden dejar el resultado incierto. La consola pide recargar. Para decidir si corresponde otra operación se comprueba el ID propio.
- Tras confirmar el cambio, el servicio reconstruye y valida el B+ con una lectura completa. Si esta falla, informa que el cambio ya fue confirmado y bloquea el uso del índice hasta una recarga correcta. Las búsquedas, listados y prefijos usan las hojas del B+.
- El transporte verifica el certificado y el host, admite HTTPS, limita la conexión a 6 s, la solicitud a 20 s y la respuesta a 16 MiB. El lector valida proyecto, identidad, tipos, duplicados y que `total` coincida con los documentos recibidos. El límite local es de 10 000 libros.

## Alcance de las pruebas

**Nube.** Se verificaron la lectura de la colección y por ID, el alta, la actualización, el borrado y la ausencia posterior. La segunda ejecución utilizó el binario C++ en los cuatro métodos, comprobó la lectura tras reiniciar la consola y validó el B+ después de las escrituras. [Evidencia de nube](evidencias/e2b/nube.txt)

**Pruebas locales.** CTest usa respuestas sanitizadas del intercambio observado y un transporte en memoria. Comprueba los cuerpos y filtros enviados, el número y orden de solicitudes, cambios de ObjectId, HTTP 200 con HTML o error de aplicación, contadores incorrectos, timeouts y fallos durante la recarga. La suite conserva las tres pruebas originales de E2-A. [Fixture del contrato](../backend/tests/fixtures/mongo/intercambio.json) · [CTest Debug](evidencias/e2b/debug-ctest.log) · [CTest con ASan/UBSan](evidencias/e2b/asan-ctest.log)

**Pendiente de verificación externa.** No se provocaron timeouts, caídas del servidor ni escrituras concurrentes en la nube. Tampoco se verificaron paginación, autenticación, unicidad de `idLibro`, transacciones o actualizaciones condicionales. La colección y el campo `proyecto` delimitan el conjunto de datos en el cliente. El control de acceso depende del servidor. Otro cliente puede escribir entre la comprobación y la mutación. La reconciliación acredita el estado observado en el GET. La ruta HTML solo se consultó mediante GET.
