# Demostración del catálogo en cuatro minutos

Desde la raíz del repositorio en WSL, con CMake, un compilador C++17 y libcurl de desarrollo instalados, prepará una compilación Debug y un archivo temporal.

~~~bash
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j2
ctest --test-dir build/debug --output-on-failure
temporal=$(mktemp -d)
sha256sum data/libros_iniciales.json > "$temporal/inicial.sha256"
build/debug/biblioteca --storage=json --initial data/libros_iniciales.json --working "$temporal/trabajo.json"
~~~

| Tiempo | Qué hacer | Qué mostrar |
|---|---|---|
| 0:00–0:30 | Elegí **1** y luego **1** para cargar el catálogo inicial. | Aparecen el nombre del JSON y 16 libros. |
| 0:30–1:05 | Elegí **2**. Ingresá, en líneas separadas, «E2C-01», «Atlas del campus», «Autor de prueba» y «mapas», sin comillas. | La consola confirma la inserción con el ID y la marca «sin guardar». |
| 1:05–1:30 | Elegí **3** y escribí «Atlas del campus». | Aparece un resultado con el ID E2C-01. |
| 1:30–2:15 | Elegí **4**, buscá el mismo título, escribí **E2C-01** y confirmá con **SI**. | Aparecen el ID seleccionado y la confirmación del borrado. |
| 2:15–2:45 | Elegí **3** y repetí «Atlas del campus». Luego elegí **7**. | La búsqueda devuelve cero resultados y el B+ informa «Invariantes OK». |
| 2:45–3:10 | Elegí **8** para guardar en el archivo temporal y **0** para salir. | El destino es el archivo de trabajo. Verificá la huella del catálogo inicial con el comando siguiente. |
| 3:10–4:00 | Abrí el diagrama de la página 1. Seguí una búsqueda desde ConsoleUI y CatalogService hasta las hojas del B+. Indicá dónde intervienen los dos repositorios al cargar o guardar. | Usá las obras L011 y L012 del catálogo para explicar el desempate por ID. |

~~~bash
sha256sum -c "$temporal/inicial.sha256"
~~~

Las seis imágenes PNG son capturas originales de la ventana de xterm en WSL, tomadas mientras la misma consola C++ atendía las operaciones. El informe presenta recortes de esas imágenes. La [salida de la sesión](sesion-json.txt) conserva entradas, respuestas, código de retorno y huella del JSON. La [verificación local](verificacion-local.txt) registra la compilación y CTest.
