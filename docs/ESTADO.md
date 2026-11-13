# Estado del proyecto

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
