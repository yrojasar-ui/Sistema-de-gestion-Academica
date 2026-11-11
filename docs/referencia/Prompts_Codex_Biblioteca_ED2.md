# Biblioteca académica — Prompts de desarrollo para Codex

Yoel Alexander Rojas Arguedas · Estructuras de Datos II · Actualización: 9 de noviembre de 2026

## Repositorio confirmado por Yoel

- Repositorio único: https://github.com/yrojasar-ui/Sistema-de-gestion-Academica
- Remote HTTPS: https://github.com/yrojasar-ui/Sistema-de-gestion-Academica.git
- El nombre del repositorio no cambia el caso de uso: sigue siendo una biblioteca académica.
- Usarlo para segundo avance y final; no crear otro repositorio ni sustituir contenido existente sin revisarlo.
- Comprobación del 9 de noviembre de 2026: público, vacío y Pages desactivado. Revalidar antes de trabajar, porque puede cambiar.
- Cuando el segundo avance pase sus verificaciones, conservarlo con una etiqueta como entregable-2; continuar sobre el mismo historial hacia el 24 de noviembre. No crear esa etiqueta antes de tener el prototipo validado.
- El acceso conectado desde este chat permitió lectura y reportó push=false. Los commits/push desde WSL requieren autenticación con permisos en la cuenta correspondiente; no se subió código durante esta actualización.

Si todavía no tienes una copia local, en WSL:

```bash
mkdir -p ~/proyectos
cd ~/proyectos
git clone https://github.com/yrojasar-ui/Sistema-de-gestion-Academica.git
cd Sistema-de-gestion-Academica
code .
```

Si ya tienes código local, abre su carpeta y revisa git status y git remote -v antes de actuar. No lo descartes ni crees una copia divergente a ciegas. Copia esta guía dentro del repositorio y pide a Codex leerla y ejecutar el Prompt 0 y luego E2-A.

## Prioridades actualizadas: segundo avance y entrega final

| Hito | Fecha comunicada | Alcance |
|---|---|---|
| Entregable 2 — Prototipo funcional (50%) | La próxima semana; día y hora exactos por confirmar | C++ parcial: insertar, buscar y borrar en al menos una estructura avanzada; carga desde archivos **o** nube; integración modular; informe de 2–3 páginas con capturas y problemas/soluciones. |
| Proyecto final | 24 de noviembre de 2026 | Todas las estructuras y componentes del diseño, CRUD, servicio de colas, persistencia en nube, Git/CI, interfaz, informe técnico completo, presentación y defensa. |

La carga desde un archivo cumple la alternativa de datos iniciales del segundo avance. La nube sigue siendo obligatoria para el proyecto final. “50%” se conserva como parte del nombre/peso comunicado; no significa que sea suficiente implementar arbitrariamente la mitad de cada requisito.

**Cambio de infraestructura:** priorizar la API MongoDB proporcionada por el docente. Firestore, Firebase, Cloud Run y Pub/Sub dejan de ser dependencias obligatorias de esta guía. No migrar ni eliminar integraciones que ya funcionen sin evaluar su estado. Para el segundo avance no se requiere publicar la web ni contratar alojamiento de backend.

**Orden actual:** Prompt 0 para fijar contexto; luego E2-A, E2-B y E2-C. Después retomar los prompts 1–9 del proyecto final solo para lo pendiente, conservando el código y las pruebas del avance. No ejecutar las diez fases completas antes de entregar el prototipo.

La API del profesor se documenta aquí a partir del enunciado. No se ha comprobado su contrato ni su disponibilidad: el intento de consultar su URL base desde la herramienta web no permitió acceder. Esto no demuestra que esté caída. Debe probarse desde WSL, comenzando con una lectura acotada. No se realizaron escrituras remotas durante la preparación de esta guía.

## Prompt E2-A — Prototipo C++ modular con B+ y archivos

```text
Prioridad actual: Entregable 2 de Estructuras de Datos II, para la próxima semana; no inventes el día exacto. El proyecto final será el 24 de noviembre de 2026. Desarrolla el prototipo en el repositorio de nuestra biblioteca académica, con C++17 en WSL/VS Code. Conserva el trabajo existente; si alguna fase ya está terminada, verifícala y reutilízala.

Lee AGENTS.md, el estado del proyecto y el diseño inicial. Para este avance implementaremos el catálogo con B+, clave (tituloNormalizado, idLibro), manteniendo hojas enlazadas. No cambies a ISBN ni traslades la lógica al frontend. Si aún no hay AGENTS.md, créalo con el contexto del Prompt 0, pero implementa ahora solo este alcance.

Requisitos obligatorios de esta fase:
1. B+ propio con inserción, búsqueda y borrado reales. Incluye split de hojas/internos, redistribución, fusión y contracción de raíz al borrar. No simules borrar ocultando registros ni reconstruyas todo el árbol como sustituto del algoritmo de borrado.
2. Carga inicial desde data/libros_iniciales.json con datos sintéticos. Valida archivo, campos, IDs repetidos, títulos repetidos con IDs distintos y JSON mal formado. Informa origen y cantidad cargada; no dejes una carga parcialmente válida sin indicarlo.
3. Menú de consola: cargar, insertar libro, buscar por título, borrar una selección identificada inequívocamente, listar ordenado, visualizar B+, validar invariantes, guardar y salir. Modificación de título es deseable si no pone en riesgo los requisitos obligatorios.
4. Separación real: modelo Libro; estructura BPlusTree; interfaz IBookRepository; JsonBookRepository; CatalogService; ConsoleUI; main de composición. CMake debe construir módulos/bibliotecas enlazadas; no concentrar todo en main.cpp. Si el proyecto usa nombres equivalentes, conservarlos y documentar el mapeo.
5. La consulta debe recorrer B+. El repositorio carga/persiste registros; no reemplaza el índice. Explica cómo el servicio coordina ambos y qué ocurre si falla el guardado.
6. Guardar cambios de manera controlada en un archivo de trabajo separado del fixture inicial. Comprobar reinicio y recarga. No afirmar durabilidad ante cortes eléctricos sin haberla diseñado y verificado.

Pruebas: vacío, clave ausente, ID duplicado, mismo título con distinto ID, inserciones ascendentes/descendentes/mezcladas, splits, borrados que provoquen préstamo de una clave/fusión/reducción de raíz, borrar todo y reinsertar. Compara con un oráculo independiente, registra semilla y valida invariantes tras cada operación en las secuencias de prueba. Usa orden pequeño para la demostración y varios órdenes en pruebas.

Ejecuta build y CTest; conserva salida y códigos de retorno. Si sanitizers están disponibles, ejecútalos y declara exactamente su alcance. No reutilices cifras de File-X ni afirmes éxito sin ejecución.

Entrega README con comandos WSL, data inicial, código comentado y scripts/demo_entregable2.sh reproducible. El script debe usar un directorio temporal de datos, no borrar mis libros ni datos ajenos. Actualiza docs/ESTADO.md y docs/REQUISITOS.md separando lo exigido ahora de lo pendiente para el final.

No empieces todavía React, autenticación externa, el resto de estructuras o infraestructura cloud compleja. Si ya existen, consérvalos. El objetivo es cerrar correctamente B+ + archivos + modularidad.
```

## Prompt E2-B — Adaptador para la API MongoDB del profesor

```text
Añade un adaptador intercambiable MongoApiBookRepository al prototipo, sin romper JsonBookRepository. El enunciado permite archivos O nube en el segundo avance: la API no debe impedir entregar el modo de archivo verificado si existe un bloqueo externo. Para el final sí debemos demostrar persistencia real en nube.

Contrato suministrado por el docente:
- GET: https://paginas-web-cr.com/Api/index.php?coleccion=clientes
- GET con filtro: la misma URL añadiendo &nombre=Mario.
- POST: {"coleccion":"clientes","datos":{"nombre":"Mario","edad":35}}
- PUT: {"coleccion":"clientes","filtro":{"nombre":"Mario"},"datos":{"edad":40,"ciudad":"San José"}}
- DELETE: {"coleccion":"clientes","filtro":{"_id":"68ffba15ad2f8f5cb40f98d1"}}
Los encabezados del enunciado mencionan apis/mongodb.php, pero el GET usa /Api/index.php. Verifica la ruta pública para cada método: no adivines que ambas son equivalentes. La forma de las respuestas, autenticación, paginación y garantías no fueron proporcionadas.

Usa C++ con libcurl y una librería JSON. Mantén verificación TLS, timeouts y errores comprensibles. Define configuración de endpoint/colección sin fijar clientes ni Mario como datos propios. Propón una colección exclusiva como biblioteca_yoel_ed2_libros y un campo proyecto con el mismo ámbito; verifica que el servicio permite ese nombre.

Empieza con GET de la colección propia, sin descargar colecciones ajenas. Inspecciona HTTP, Content-Type y cuerpo real: un 200 puede contener un error de aplicación. Documenta el sobre JSON y formato real de _id. Prueba el adaptador con fixtures de respuestas sanitizadas, sin inventar que equivalen a una prueba cloud.

Modelo sugerido: idLibro estable propio, titulo, autor, temas y proyecto; _id pertenece a MongoDB y no sustituye silenciosamente idLibro. Al cargar registros, construye nuestro B+ local y haz las consultas académicas mediante ese índice. No presentes los índices internos de MongoDB como nuestra implementación.

Para la prueba CRUD real, usa únicamente registros sintéticos creados por este proyecto. Guarda los _id devueltos y solo actualiza/borra esos registros con filtros exactos. Rechaza filtros vacíos, operaciones masivas y limpieza de la colección compartida. No ejecutes los ejemplos de borrado de clientes/Mario del enunciado.

POST/PUT/DELETE deben manejar respuestas ambiguas y timeouts: no reintentes ciegamente un POST que quizá ya se ejecutó. Verifica/reconcilia por ID propio cuando el contrato lo permita. Si el backend no permite determinar el resultado, muestra estado incierto y no afirmes éxito. Actualiza el índice solo después del resultado confirmado o reconstruye desde una lectura verificada; evita divergencia silenciosa.

Implementa selección explícita --storage=json o --storage=mongo. Nunca cambies a JSON silenciosamente si falla la nube. Si falta Internet o el contrato no está claro, conserva el prototipo de archivo y documenta el bloqueo exacto.

Crea docs/API_MONGODB.md con métodos/rutas comprobados, ejemplos de nuestros libros, respuestas sanitizadas, límites y pruebas realizadas. No supongas autenticación, transacciones, unicidad o actualizaciones condicionales solo porque el servicio usa MongoDB. Usa datos sintéticos: un nombre de colección o campo proyecto organiza datos pero no es control de acceso.

Demuestra, si el servicio está disponible, insertar libro propio -> volver a consultar -> cargar B+ -> buscar -> borrar ese libro -> verificar ausencia. Captura resultados y distingue claramente evidencia real, pruebas simuladas y pendientes.
```

## Prompt E2-C — Informe de 2–3 páginas y cierre del avance

```text
Prepara exclusivamente el informe del Entregable 2 de nuestra biblioteca, con 2–3 páginas totales; usa 3 páginas para incluir evidencia legible. No escribas ahora el informe final largo. Revisa primero el código y ejecuta el prototipo: no inventes funciones, capturas, resultados ni problemas.

Autor: Yoel Alexander Rojas Arguedas; Universidad CENFOTEC; Estructuras de Datos II. No inventes nombre del docente ni fecha exacta del avance. El final está previsto para el 24 de noviembre de 2026.

Página 1: objetivo del avance, alcance implementado, módulos reales y diagrama compacto ConsoleUI -> CatalogService -> BPlusTree / IBookRepository -> JSON o API MongoDB. Explica la clave del B+, la carga inicial y qué queda para el final.

Página 2: capturas REALES de ejecución que demuestren carga de datos, inserción/búsqueda y borrado con consulta posterior. Incluye una vista legible de la estructura y/o salida de validación. Usa varias capturas pequeñas solo si siguen siendo legibles; el código mostrado sin ejecutar no sustituye la evidencia. Si no puedes capturar la terminal, deja el documento como borrador y proporciona los pasos exactos para que yo aporte las capturas; no fabriques una terminal que parezca ejecutada.

Página 3: resumen de pruebas realmente ejecutadas y tabla problema observado -> causa/evidencia -> solución aplicada o propuesta -> estado. No presentes riesgos hipotéticos como errores ya encontrados. Incluye instrucciones mínimas de reproducción y enlace real al repositorio si ya existe; de lo contrario indica pendiente, sin inventarlo.

Entrega docs/Informe_Entregable2_Biblioteca.pdf, fuente editable y docs/evidencias/entregable2 con capturas originales y logs. Verifica que el PDF tenga 2–3 páginas sin cortes y que todas las afirmaciones correspondan al commit evaluado. Añade un guion breve de 3–5 minutos para mostrar carga -> insertar -> buscar -> borrar -> buscar ausente -> estructura -> módulos.

Revisa el cierre con esta lista: C++ compila; B+ inserta/busca/borra; datos cargados de archivo o nube; módulos integrados; capturas reales; problemas/soluciones documentados; informe 2–3 páginas. No declares este avance incompleto solo por no tener todavía web, broker o las cinco estructuras; esos requisitos permanecen pendientes para el final.

Si GitHub ya está configurado, sube los cambios siguiendo las reglas del repositorio y crea una etiqueta de entrega después de pasar pruebas; no fuerces historial ni sobrescribas etiquetas existentes. Si falta acceso, entrega el código y los comandos pendientes. No publiques datos de otros estudiantes ni credenciales.
```

## Cómo usar esta guía

Esta guía contiene instrucciones para desarrollar el proyecto, no certifica que la aplicación esté implementada. Se basa en `Diseno_Biblioteca_Academica.pdf`, documento inicial de cinco páginas. File-X corresponde a otro caso de estudio y no sustituye este proyecto.

En VS Code, abre una carpeta desde WSL, preferiblemente dentro de tu directorio Linux, por ejemplo `~/proyectos/Sistema-de-gestion-Academica`. Comprueba que VS Code esté conectado a WSL antes de abrir Codex. Sigue el orden actualizado: 0, E2-A, E2-B, E2-C; después las fases finales que falten. Cada fase debe terminar con cambios ejecutados y evidencia; una respuesta que solo enumere intenciones no cierra una fase. Si algo falla, usa el prompt de reparación al final.

Si tienes el PDF, colócalo en `docs/referencia/Diseno_Biblioteca_Academica.pdf`. Los prompts ya incluyen las decisiones principales, por lo que su ausencia no debe detener la preparación inicial. Antes de documentar fidelidad completa al PDF, sí debe leerse el archivo real.

## Arquitectura actualizada

| Componente | Segundo avance | Proyecto final |
|---|---|---|
| Interfaz | Consola C++ | Web React/TypeScript en Pages, conservando consola para diagnóstico. |
| Estructuras | B+ completo para catálogo | B+, Rojo-Negro, hash extensible, grafo y FIFO según diseño. |
| Persistencia | Archivo JSON verificado; adaptador de API del profesor | Persistencia real mediante API MongoDB del profesor, sujeta a capacidades comprobadas. |
| Modularidad | UI, servicio, estructura y repositorio separados | API C++, componentes de dominio, persistencia y worker. |
| Mensajería | No exigida en este avance | Broker real, inicialmente RabbitMQ local para integración; alojamiento posterior según entorno disponible. |
| Calidad | CMake, pruebas y Git | CI/CD, integración, despliegue y evidencia del sistema completo. |
| Documentación | Informe de 2–3 páginas | Informe técnico completo, presentación y defensa. |

La API MongoDB provee acceso a almacenamiento, no ejecuta nuestro C++. GitHub Pages sirve el frontend estático: para una web operativa desde cualquier equipo, la API C++ debe alojarse aparte y tener una URL HTTPS accesible. No elegir un proveedor pagado antes de revisar el entorno y costos; ese despliegue no bloquea el segundo avance.

Un RabbitMQ ejecutándose localmente es un broker real, pero no prueba que esté alojado en nube. La consigna final exige base de datos en nube y servicio de colas; no indica que el broker deba estar necesariamente en nube. Si se quiere que toda la web funcione sin WSL encendido, también debe mantenerse disponible el procesamiento de mensajes fuera de ese equipo.

La propuesta anterior con Firestore/Firebase/Cloud Run/Pub/Sub queda sustituida como opción predeterminada. Si ya hay código funcional con esos servicios, evalúa su reutilización sin desmontarlo automáticamente. Registrar toda decisión antes de cambiar una integración que ya funciona.

## Prompt 0 — Contexto permanente y reglas del proyecto

```text
Actúa como desarrollador de este repositorio y tutor técnico. Implementaremos una biblioteca académica para el proyecto final de Estructuras de Datos II de Universidad CENFOTEC. Trabajo en WSL dentro de VS Code. Necesito entender y defender el código que construyamos. Responde y documenta en español.

Primero inspecciona el directorio y cualquier AGENTS.md existente. Conserva el trabajo previo. Si existe docs/referencia/Diseno_Biblioteca_Academica.pdf, léelo y úsalo como base. No confundas este proyecto con File-X. No cambies decisiones del diseño sin registrar su motivo y efecto.

REQUISITOS DEL CURSO:
- Implementaciones propias de B/B+, árbol balanceado, hash dinámico y grafo.
- Insertar, buscar, modificar y eliminar datos con reglas de integridad.
- Visualización de organización interna real.
- Componentes modulares y comunicación asincrónica por servicio de colas.
- Persistencia en una base de datos nativa en nube.
- Núcleo C++, repositorio Git, CI con compilación/pruebas/despliegue.
- Informe técnico con UML, arquitectura, justificaciones y resultados reales.
- Presentación y defensa con demostración práctica.
- Pesos: estructuras 30%; integración/comunicación 20%; nube/CI 15%; informe 20%; defensa 15%. No inventes una calificación.

DISEÑO DE BIBLIOTECA QUE DEBES RESPETAR:
1. Actores: lector y bibliotecario. Una sola biblioteca, ejemplares físicos y avisos internos. Sin pagos, multas monetarias, sedes ni recomendaciones con IA.
2. Entidades: Libro, Ejemplar, Usuario, Prestamo y Reserva. Libro es una obra; Ejemplar es una copia física. Conserva el historial de préstamos.
3. B+: índice de Libro con clave (tituloNormalizado, idLibro). Hojas enlazadas; búsqueda ordenada, por prefijo y rangos. Define una normalización determinista para español y prueba acentos, mayúsculas y UTF-8; no prometas un orden lingüístico que no implementes.
4. Rojo-Negro: solo préstamos activos con clave (fechaVencimiento, idPrestamo). Se elimina la entrada al devolver, conservando el registro histórico. Permite actualizar vencimientos retirando e insertando la clave.
5. Hash extensible: acceso por idUsuario, con directorio, profundidad global/local y división de cubetas. No sustituirlo por unordered_map.
6. Grafo no dirigido: libros como vértices, arista si comparten al menos un tema; listas de adyacencia, sin bucles ni aristas duplicadas.
7. FIFO propia por libro: IDs de reservas por llegada; empates por ID. Cancelación lógica, descartando canceladas al atender.
8. Solo usuarios activos pueden recibir préstamos. Un ejemplar admite como máximo un préstamo activo. Un lector no puede tener dos reservas pendientes del mismo libro. Se respeta la primera reserva válida. La reserva es por libro, no por ejemplar. Una copia prestada, apartada o retirada no está disponible.

HITOS Y ARQUITECTURA OBJETIVO:
- Segundo avance la próxima semana (fecha exacta pendiente): B+ con insertar/buscar/borrar, carga por archivo O nube, modularidad e informe de 2–3 páginas. Ejecutar E2-A, E2-B, E2-C antes de implementar todo el final.
- Proyecto final: 24 de noviembre de 2026, con todos los requisitos del curso.
- C++17, CMake y consola para el prototipo. Frontend React/TypeScript/Vite en GitHub Pages para el final.
- Persistencia detrás de IBookRepository: JSON para demostrar carga inicial; API MongoDB proporcionada por el profesor para nube. Usar REST mediante libcurl y documentar contrato observado.
- API y worker C++ separados para el final, con Docker si aporta reproducibilidad. Broker real: RabbitMQ como base local; proveedor de alojamiento por decidir antes de desplegar.
- No configurar Firestore/Firebase/Cloud Run/Pub/Sub por obligación. Evaluar primero lo existente y la API del docente.
- Identidad y roles según las capacidades realmente implementadas. No afirmar que un selector de rol da seguridad; no publicar datos personales en una API compartida sin aislamiento comprobado.
- Diferenciar modo local, mock y nube. No cambiar de almacenamiento silenciosamente ante errores.
- Bibliotecas externas permitidas para HTTP, JSON, autenticación, criptografía, pruebas y visualización. Las cinco estructuras académicas deben ser propias.

REGLAS DE TRABAJO:
- Desarrolla solo la fase solicitada. Implementa y prueba; no respondas únicamente con un plan.
- No reemplaces las estructuras por mapas estándar, arrays del frontend ni índices de MongoDB. Puedes usar contenedores auxiliares documentando su función y std::map como oráculo de pruebas.
- No inventes pruebas aprobadas, métricas, capturas, enlaces, despliegues ni resultados.
- No declares finalizado un requisito externo que solo tenga un adaptador simulado.
- No introduzcas autenticación ficticia de producción ni roles elegidos por el navegador.
- No borres trabajo del usuario, no hagas force push y no subas tokens, .env, credenciales o datos personales reales.
- Usa dependencias fijadas, licencias identificadas, RAII y errores manejados. Prioriza una arquitectura pequeña que pueda explicar un estudiante.
- Al terminar cada fase: archivos modificados, decisiones, comandos ejecutados, resultado observado, limitaciones y un commit local descriptivo si el repositorio está listo. No reconfigures mi identidad Git sin consultarla.

Ahora crea o actualiza, sin destruir instrucciones existentes, AGENTS.md con estas reglas; docs/REQUISITOS.md con IDs y criterios verificables; docs/ESTADO.md; y docs/DECISIONES.md. La matriz debe enlazar requisito -> implementación -> prueba -> evidencia y distinguir pendiente, implementado localmente y verificado en nube. No marques nada como aprobado por anticipado. Todavía no implementes toda la aplicación.
```

## Prompt 1 — Proyecto compilable en WSL

```text
Lee AGENTS.md y ejecuta la fase de preparación del proyecto.

Inspecciona WSL, distribución, g++, CMake, Git, Node/npm y Docker. Reporta versiones reales y revisa requisitos vigentes de las herramientas que elijas. No mezcles ejecutables de Windows con herramientas Linux sin justificarlo. Instala solo lo autorizado; si falta un paso interactivo, deja el comando exacto y continúa con lo no bloqueado.

Prepara un monorepo con separación entre estructuras, dominio, aplicación y adaptadores:
- backend/include y backend/src: estructuras, entidades, servicios, interfaces de repositorio/mensajería y adaptadores.
- backend/apps: API y worker.
- backend/tests: unitarias e integración.
- frontend, infra, scripts, docs y .github/workflows.
No crees archivos vacíos como evidencia de una funcionalidad.

Configura C++17, CMakePresets.json, CTest, formato y advertencias -Wall -Wextra -Wpedantic. Añade presets para debug, release y ASan/UBSan cuando el compilador lo permita. Fija las dependencias y documenta cómo recuperarlas. Utiliza librerías mantenidas para HTTP, JSON y pruebas; explica la selección. No programes criptografía propia.

Crea README con comandos reales para compilar, probar y arrancar desde WSL. Añade .gitignore, .env.example sin secretos y tareas opcionales de VS Code. Inicializa Git si no existe, sin inventar usuario o remote. Prepara frontend mínimo que compile y /healthz en API con versión de la aplicación; la salud del proceso no debe afirmar conectividad de nube.

Añade CI inicial de compilación y pruebas en pull_request y push. No publiques todavía. Ejecuta el build, CTest y el build del frontend. Actualiza ESTADO.md con resultados reales y una lista breve de bloqueos. Esta fase termina con un proyecto mínimo reproducible, no con una biblioteca simulada completa.
```

## Prompt 2 — Las cinco estructuras propias en C++

```text
Implementa las estructuras del diseño y sus pruebas. No avances a la interfaz visual todavía.

B+: orden parametrizable bien definido; búsqueda exacta, rangos y prefijos; inserción con splits de hojas e internos; eliminación con redistribución, fusión y contracción de raíz; hojas enlazadas. Modificar una clave requiere mantener la ordenación. Valida separadores, ocupación, profundidad común y cadena de hojas.

Rojo-Negro: inserción, búsqueda, eliminación y corrección de colores/rotaciones. Valida raíz negra, ausencia de padre e hijo rojos, orden de claves, consistencia de padres y misma altura negra. Prueba particularmente la eliminación y el árbol vacío.

Hash extensible: directorio compartido de referencias a cubetas; hash estable; profundidades local/global, splits y duplicación del directorio. Busca, modifica y elimina. Define explícitamente si fusionas cubetas y contraes directorio o si esa optimización no existe. Resuelve colisiones extremas y límite de profundidad sin crecimiento infinito ni pérdida de registros; documenta la política.

Grafo: CRUD de vértices y aristas, simetría de adyacencia, ausencia de duplicados y bucles; vecinos y BFS/DFS; al borrar un vértice elimina enlaces entrantes y salientes. No uses una matriz de adyacencia si el diseño pide listas.

FIFO propia: encolar, consultar frente, desencolar y recorrer; inicio/final consistentes y tamaño correcto. La cancelación de Reserva pertenece al dominio; la cola conserva IDs y permite descartar cancelados en orden.

Todas deben exportar snapshots JSON reales para visualización: IDs estables dentro del snapshot, claves, hijos, enlaces, colores, cubetas, profundidades y contenidos según corresponda. Incluye estadísticas y trazas de búsqueda donde aporten valor. No serialices direcciones de memoria ni datos privados de usuarios.

Pruebas deterministas y aleatorias con semillas registradas, oráculos independientes, casos vacíos, límites, duplicados, modificaciones, borrado completo, reinserción y secuencias mixtas. Usa órdenes pequeños de B+ para forzar splits/fusiones y prueba varios órdenes pares e impares. Cuenta las operaciones realmente comprobadas.

Ejecuta CTest y sanitizers; conserva comandos, versión de compilador, salida, códigos de retorno y commit/hash del código. Si alguna herramienta no se ejecuta, indícalo. No desactives controles silenciosamente ni generalices “sin errores” más allá de las pruebas realizadas. Explica cómo cada estructura será utilizada por el servicio de biblioteca.
```

## Prompt 3 — Dominio, casos de uso y API real

```text
Integra las estructuras en un servicio de biblioteca C++ con API REST documentada. Lee el diseño y las pruebas existentes antes de modificarlo.

Implementa Libro, Ejemplar, Usuario, Prestamo, Reserva y Aviso. Define estados, IDs, fechas, normalización de títulos y validaciones. Usa UTC para instantes y presenta fechas al usuario de manera explícita. Implementa catálogo, usuarios, ejemplares, préstamos, devoluciones, reservas, cancelaciones, vencimientos y recomendaciones por temas.

Expón operaciones reales de crear, consultar, modificar y eliminar cuando corresponda. Protege referencias: no borrar una entidad con operaciones activas incompatibles; preservar historial mediante desactivación/retirada cuando sea necesario. Prueba eliminación real de registros sin referencias, no solo ocultarlos en pantalla. No elimines un préstamo histórico para simular CRUD.

Fija estas reglas: máximo un préstamo activo por ejemplar; usuarios activos; reserva única pendiente por lector/libro; respeto de turno; disponibilidad correcta; devolver cierra el préstamo y lo retira del Rojo-Negro; si hay reserva válida aparta el ejemplar y prepara el aviso. Define qué ocurre con varias copias, reservas avisadas, usuarios desactivados y cancelación de una reserva que ya tiene un ejemplar apartado. El turno se decide de manera coherente con las operaciones de negocio, no por orden de entrega del broker. Usa transacciones/operaciones condicionales únicamente si el repositorio realmente las soporta; documenta límites y recuperación si no.

La búsqueda de catálogo debe utilizar B+, vencimientos el Rojo-Negro, usuarios el hash y relaciones el grafo. Incluye trazas o pruebas que demuestren estas rutas. Usar la API MongoDB para persistencia no deberá reemplazar estos recorridos.

Diseña interfaces de repositorio y publicador de eventos; expón capacidades transaccionales solo cuando el adaptador las pueda cumplir. Para esta fase admite un adaptador local explícito en memoria; no lo presentes como persistencia definitiva. Separa controladores HTTP de reglas de negocio y estructuras.

Publica OpenAPI con solicitudes, respuestas, errores y paginación. Incluye endpoints de snapshots para bibliotecario, /healthz y /readyz con semántica distinta. Usa errores 400/401/403/404/409/503 según el caso. Añade límites razonables de entradas y cuerpos. CORS debe aceptar orígenes configurados.

Prueba flujos completos de préstamo, devolución, reservas canceladas, modificación de título/tema/vencimiento y rechazo de operaciones inválidas. Usa la API desde un script de demostración reproducible. Actualiza diagramas de casos de uso, clases, componentes y secuencia en Mermaid o PlantUML; conserva las fuentes editables y documenta cambios frente al diseño inicial.
```

## Prompt 4 — Persistencia MongoDB, recuperación y límites

```text
Continúa a partir del adaptador MongoApiBookRepository del segundo avance. No configures Firestore por defecto. Revisa docs/API_MONGODB.md y prueba solo registros propios de este proyecto. No vuelvas a desarrollar un adaptador que ya funciona.

Persiste entidades, estados, historial y reservas mediante el contrato comprobado. Separa IDs del dominio de _id de MongoDB. Maneja carga paginada si el servicio pagina y define un comportamiento explícito si impone límites. Al reiniciar reconstruye los índices C++ desde los registros persistidos y valida sus invariantes. Las consultas académicas deben recorrer las estructuras propias.

Prueba las capacidades que realmente necesitemos: autenticación, aislamiento, filtros exactos, operaciones condicionales, conflictos, unicidad, paginación y límites. Los ejemplos del profesor solo demuestran formas de solicitudes; no prueban ninguna garantía de atomicidad o autorización. No agregues campos como version esperando que el servidor los aplique como precondiciones sin verificarlo.

Diseña consistencia entre almacenamiento e índices: éxito confirmado antes de actualizar el estado visible, recuperación ante fallo y manejo de resultados inciertos. Repite lectura para verificar una escritura cuando sea necesario. No reintentes POST a ciegas ni ignores respuestas de error dentro de HTTP 200.

Para préstamo/devolución, determina si existe soporte de cambios atómicos en el contrato. Si no existe, no inventes transacciones distribuidas: documenta el modelo académico de escritura coordinada, sus límites frente a clientes externos/concurrencia y la recuperación de cambios parciales. Un mutex local no protege contra otros procesos o clientes de la API. Antes de prometer garantías mayores, propone una extensión autorizada del backend o un repositorio alternativo y registra la decisión; no cambies de proveedor silenciosamente.

Demuestra persistencia después de reiniciar, reconstrucción de índices, actualización de título/tema/vencimiento y tratamiento de fallos de red. Prueba concurrencia y registra los resultados, incluidos límites no resueltos. No declares soportada una arquitectura multiinstancia si solo se ha probado un escritor.

Usa datos sintéticos de biblioteca. La API docente puede estar compartida; no publiques información personal ni credenciales. Si la web tiene roles, aplica autorizaciones en la API C++ y documenta que estas no protegen el acceso directo a una API externa abierta. No presentes un selector de rol como autenticación.

Actualiza manual de configuración, contrato, diagramas, pruebas y matriz. Diferencia “adaptador implementado”, “probado con servidor local simulado” y “probado contra nube real”. Si una capacidad no está disponible, explica el impacto preciso en el proyecto.
```

## Prompt 5 — Servicio de colas y worker C++

```text
Implementa comunicación asincrónica entre el servicio de biblioteca y un worker C++ mediante un broker real. Usa RabbitMQ local como base reproducible, salvo que ya exista otro broker funcional y documentado. Verifica clientes compatibles con C++ y fija versiones. No implementes el productor/consumidor principal en Python para sustituir C++.

Conserva la FIFO propia de reservas: ordena turnos de negocio. El broker transporta mensajes entre componentes; son responsabilidades diferentes.

Flujo: la devolución y atención de reserva generan un evento para crear un aviso interno. Define eventId propio, tipo, versión, entidad, fecha y payload mínimo. El worker registra el aviso y la API permite consultarlo. Una operación de biblioteca confirmada y un aviso pendiente deben tener estados distintos.

Implementa confirmación de publicación y ACK del consumidor después de persistir el resultado, reintentos acotados, cola de errores y tratamiento de mensajes inválidos. Procesamiento idempotente con deduplicación durable por eventId y pruebas de caída antes/después del ACK. No prometas exactly-once ni atomicidad entre deduplicación y escritura si la API de persistencia no las garantiza.

Resuelve el fallo entre cambio de negocio y publicación: usa outbox cuando pueda almacenarse atómicamente con el cambio, o documenta y prueba un mecanismo de recuperación/reconciliación apropiado a las capacidades de la API docente. Escribir dos documentos sucesivos no es una transacción. No marques esta garantía como completada si sigue habiendo una ventana de pérdida sin recuperación.

Crea un arranque reproducible de broker/API/worker, por ejemplo Docker Compose. No expongas la consola administrativa ni uses credenciales de ejemplo en un despliegue público. Un broker local cumple una integración real por colas, pero no prueba alojamiento cloud. Planifica su disponibilidad fuera de WSL si la aplicación publicada debe funcionar de forma independiente.

Demuestra evento publicado -> consumido -> aviso, mensaje repetido sin efecto duplicado dentro de las garantías implementadas, caída/reinicio del worker y recuperación de mensajes pendientes. Conserva logs correlacionados y un diagrama de secuencia. Declara todos los límites derivados del almacenamiento compartido.
```

## Prompt 6 — Interfaz web y visualización didáctica

```text
Construye el frontend React + TypeScript + Vite conectado a la API real. La lógica de estructuras y reglas de préstamo debe permanecer en C++.

Diseño: biblioteca académica clara, moderna y usable, en español. Fondo claro, azul para acciones, verde para disponibilidad, tipografía legible, contraste adecuado, navegación con teclado y diseño adaptable. No uses una plantilla de landing page: necesito una aplicación de gestión funcional.

Vistas:
1. Catálogo: búsqueda por título/prefijo y rango, fichas de libros, disponibilidad y libros relacionados.
2. Mis préstamos, reservas y avisos del lector.
3. Administración de libros, ejemplares y usuarios para bibliotecario.
4. Préstamos, devoluciones y vencimientos con filtros.
5. Laboratorio de estructuras: B+ con hojas enlazadas; Rojo-Negro con colores y altura negra; hash con directorio/cubetas/profundidades; grafo de libros; FIFO por libro con estados.
6. Panel de integración para bibliotecario: outbox, eventos/avisos y errores pertinentes, sin exponer secretos.

Cada formulario debe guardar y consultar datos reales. Incluye carga, vacío, validación, error de red, permisos y confirmación de borrado. No sustituyas errores del backend por datos inventados ni presentes como guardado algo que solo está en localStorage.

El laboratorio usa snapshots de los nodos y enlaces exportados por C++. No reconstruyas un árbol ilustrativo desde una lista ordenada ni implementes otro B+ en JavaScript. La traza resaltada debe venir del backend. Mostrar tamaño/orden/color/ocupación según la estructura y un texto breve de su propósito. Limita nodos visibles en árboles grandes e indica truncamiento.

Aplica la política de acceso documentada en la fase previa. Ocultar botones no sustituye permisos en backend. La URL de API es configuración pública; credenciales privilegiadas nunca van en el frontend. Si hay roles de demostración sin identidad verificada, indícalo claramente y limita los datos a sintéticos.

Configura base de Vite para un sitio de proyecto /Sistema-de-gestion-Academica/ y navegación compatible con Pages, preferiblemente HashRouter. Usa URL HTTPS del backend en producción; no localhost. El origen CORS es https://yrojasar-ui.github.io sin ruta de repositorio. Si la API está caída, informa del problema y permite reintentar.

Ejecuta pruebas E2E con Playwright contra el stack local: acceso según el mecanismo realmente implementado, CRUD permitido, préstamo, bloqueo de doble préstamo, reserva/devolución/aviso y snapshots. Añade comprobación de rutas y assets bajo subruta. Usa datos sintéticos identificados. Guarda capturas reales para el informe, sin información privada.
```

## Prompt 7 — GitHub, CI/CD y despliegue verificable

```text
Usa el repositorio existente yrojasar-ui/Sistema-de-gestion-Academica: https://github.com/yrojasar-ui/Sistema-de-gestion-Academica.git . Publica allí el código revisado y prepara GitHub Pages y el backend. No crees otro repositorio, no renombres este, no fuerces historial ni modifiques mi portafolio. Conserva las versiones del segundo avance y del final con etiquetas después de verificar cada entrega.

Primero revisa git status, remotes, contenido a subir y gh auth status. Usa la cuenta realmente autenticada; no deduzcas mi usuario de conversaciones previas. Si falta autenticación, indica el paso interactivo sin pedir que pegue credenciales. El repositorio ya existe: inspecciona su rama e historial actuales antes de asociar el remote y subir cambios; no asumas que sigue vacío. Para GitHub Free usa repositorio público si es necesario para Pages, revisando antes que solo contenga código y datos sintéticos publicables.

CI para PR y main:
- Configuración y compilación C++ con dependencias fijadas.
- CTest, pruebas relevantes e integración con datos de prueba aislados. No ejecutar escrituras en la API compartida desde cada PR; hacer smoke tests cloud acotados y controlados.
- ASan/UBSan en un job compatible, con opciones y límites documentados.
- Typecheck, pruebas/build frontend y E2E pertinentes.
- Informes/logs como artefactos; nunca continuar al despliegue ignorando fallos.

Pages: workflow con acciones oficiales vigentes y permisos mínimos. Publica solo frontend/dist. No publiques .env, backend, credenciales ni todo el repositorio como sitio. Configura Pages con GitHub Actions. Respeta la subruta del repositorio. Distingue build exitoso de sitio publicado y verificado.

Backend y worker: prepara imágenes Linux, configuración por entorno, puertos y healthchecks. Elige alojamiento HTTPS compatible con procesos C++ y con el ciclo de vida del worker/broker; revisa documentación vigente, límites y costos antes de crear recursos. La API MongoDB docente solo almacena datos, no aloja nuestros ejecutables. GitHub Pages tampoco ejecuta C++.

Configura despliegue controlado al proveedor finalmente elegido, preferiblemente sin credenciales de larga duración cuando ofrezca federación OIDC. Permisos mínimos, secretos fuera de repositorio/frontend, y pruebas/deploy sobre el mismo commit. No despliegues código de PR no confiables ni fuerces migración a Google Cloud.

Antes de habilitar facturación o crear recursos facturables sin autorización, presenta la opción concreta y costos actuales. Completa mientras tanto Dockerfiles, workflows, scripts y documentación. No prometas gratuidad, independencia de WSL ni nube verificada sin evidencia.

Con acceso autorizado, realiza el despliegue y verifica desde Pages: acceso según diseño, crear libro, reservar/prestar/devolver, aviso asincrónico y persistencia tras reiniciar backend. Registra URLs reales, commit SHA, ejecución de Actions, fecha y resultados. Añade cómo detener/eliminar solo los recursos de este proyecto y cómo volver a una imagen anterior. No finalices con URLs de ejemplo como si estuvieran activas.
```

## Prompt 8 — Informe técnico minucioso y reproducible

```text
Redacta el informe técnico final a partir de este repositorio y su evidencia actual. Primero audita REQUISITOS.md, código, workflows, pruebas, logs, despliegues y el PDF inicial. No completes huecos con suposiciones. Si la nube no está verificada, escribe esa limitación y no presentes el proyecto como terminado.

Autor: Yoel Alexander Rojas Arguedas. Universidad CENFOTEC. Curso: Estructuras de Datos II. No inventes profesor, integrantes, fecha de entrega o resultados.

Entrega docs/Informe_Tecnico_Biblioteca.md y PDF final legible. Si es viable, añade DOCX editable. Mantén fuentes y un proceso de generación reproducible. No copies el informe File-X ni uses sus métricas como resultados de biblioteca.

Contenido:
- Problema, justificación, objetivos, alcance, actores y reglas.
- Matriz trazable de requisitos funcionales/no funcionales y rúbrica.
- Arquitectura por componentes, responsabilidades y flujo Pages -> API C++ -> API MongoDB / broker seleccionado -> worker.
- UML de casos de uso y clases con atributos, operaciones, asociaciones y multiplicidades; componentes y despliegue; secuencias de préstamo/devolución/aviso, duplicados y fallo de publicación. Diagrama real de cada estructura.
- Para B+, Rojo-Negro, hash, grafo y FIFO: propósito concreto, claves, diseño, algoritmos, invariantes, complejidad temporal/espacial y límites. Distingue costo de estructura, normalización, persistencia, red y reconstrucción de índices.
- CRUD, integridad, atomicidad, reconstrucción tras reinicio y tratamiento de historia.
- Mensajería: FIFO de reservas frente a broker, outbox, idempotencia, reintentos y dead-letter.
- Persistencia nativa en nube, autenticación, roles, manejo de secretos y separación local/nube.
- Git y CI/CD con evidencia de ejecuciones reales; despliegue controlado y recuperación.
- Pruebas unitarias, integración, E2E y fallos con datos, pasos, resultado esperado/observado y archivos de respaldo.
- Métricas reproducibles de 100, 1000 y 10000 registros donde sean apropiadas: inserción/búsqueda/eliminación, altura, ocupación, splits/fusiones/rotaciones, cubetas y crecimiento. Define semillas, consultas, repeticiones, compilador y hardware. Si mides tiempos, reporta método y dispersión. Separa benchmarks locales de latencia cloud. No infieras consumo total de RAM a partir de sizeof ni pruebes eficiencia con cifras inventadas.
- Resultados, límites, deuda técnica, conclusiones y fuentes oficiales con fecha de consulta.
- Manual de instalación WSL, variables, compilación, pruebas, emuladores, nube y uso.

Cada tabla numérica debe derivarse de CSV/JSON reales. Genera un contraste automático entre las tablas finales y esos archivos. Capturas solo de la aplicación funcionando; diagramas consistentes con código. Identifica commit evaluado y guarda manifest de evidencias. Diferencia ausencia de defectos detectados de garantía de corrección absoluta.

Renderiza e inspecciona todas las páginas del PDF: sin cortes, tablas ilegibles, diagramas fuera de margen ni páginas vacías. Mantén una extensión suficiente para explicar lo implementado, sin relleno ni un límite de páginas inventado. Entrega el documento y los pendientes concretos que impiden cerrar algún requisito.
```

## Prompt 9 — Presentación, demostración y defensa

```text
Prepara presentación y defensa basadas únicamente en el código, informe y evidencia final. No agregues funciones que la aplicación no tenga.

Si no hay duración oficial, prepara una versión base de 12 diapositivas para aproximadamente 12-15 minutos, indicando que es una propuesta adaptable. Crea PPTX y PDF cuando las herramientas lo permitan; conserva una fuente editable reproducible. Añade notas del presentador.

Guion de diapositivas:
1. Problema, objetivo y alcance de la biblioteca.
2. Requisitos y qué se implementó realmente.
3. Arquitectura y razón de separar Pages del backend C++.
4. Modelo Libro/Ejemplar/Usuario/Prestamo/Reserva.
5. B+ y una operación sobre nodos reales.
6. Rojo-Negro y vencimientos; hash extensible y usuarios.
7. Grafo de libros y FIFO de reservas.
8. Comunicación asincrónica: publicación/recuperación -> broker -> worker -> aviso.
9. MongoDB mediante API docente, consistencia, acceso y recuperación.
10. Pruebas, métricas y CI/CD con evidencia real.
11. Demostración guiada y resultados observados.
12. Límites, conclusiones y siguiente paso justificado.

Crea docs/Guion_Defensa.md y PDF con qué decir, qué abrir, qué ejecutar y qué debería verse. Respeta un orden continuo: panorama -> carpetas -> estructuras -> dominio -> repositorios/mensajería -> API/worker -> frontend -> tests -> workflows. Cita archivos y símbolos reales y explica cómo se conectan; evita generalidades que no pueda señalar en el repositorio.

Demostración repetible con datos sintéticos y preparación idempotente:
- Crear/consultar/modificar un libro y ver B+.
- Forzar una división con un índice didáctico pequeño del mismo código, separado de datos operativos; mostrar también cómo lo utiliza el catálogo real.
- Registrar usuario y observar hash; mostrar un split si la demo lo requiere.
- Crear préstamo y mostrar vencimiento en Rojo-Negro.
- Intentar prestar otra vez el mismo ejemplar y ver rechazo.
- Reservar con otro lector; devolver; comprobar turno/apartado y aviso producido por worker.
- Ver grafo tras modificar temas y cancelación de reserva.
- Eliminar un registro sin referencias y verificar invariantes.
- Reiniciar backend y demostrar persistencia/reconstrucción.
- Abrir ejecución CI y sitio Pages reales.
No reinicies ni resetees una nube compartida sin acotar los datos de la demo.

Incluye un plan alternativo local claramente identificado si no hay Internet, sin presentarlo como evidencia de nube. Prepara 30 preguntas probables del docente con respuesta corta, explicación técnica y archivo/símbolo que la respalda. Incluye: B+ frente a ABB/AVL; delete y merges; altura negra; colisiones/splits; FIFO frente a broker; ACK; duplicados; outbox; transacciones; fallo tras commit; reconstrucción; qué ejecuta Pages; costos; límites de métricas; autenticación; pruebas y CI.

Termina con auditoría de los cinco rubros de evaluación: evidencia disponible, pendientes y riesgos, sin porcentaje de aprobación inventado. Revisa presentación y guion visualmente y verifica que no contradigan el informe ni los resultados. Crea una lista corta de preparación previa a la clase.
```

## Prompt de reparación — Cuando una fase falla o queda incompleta

```text
Revisa la fase actual, AGENTS.md y docs/ESTADO.md. Reproduce el fallo con los comandos del proyecto y encuentra su causa. Corrige la implementación manteniendo el diseño y las reglas de negocio. No elimines pruebas válidas, no relajes invariantes para ocultar errores y no sustituyas integración real por mocks en producción. Ejecuta las pruebas pertinentes y conserva la evidencia. Si falta acceso externo, completa el trabajo local y marca el bloqueo exacto. Actualiza código, documentación y estado de manera consistente. Explica causa, cambio y resultado antes de pasar a la fase siguiente.
```

## Comprobación final para Yoel

- Los árboles, hash, grafo y FIFO están implementados en C++ y se usan realmente.
- El CRUD respeta integridad y las visualizaciones muestran datos internos reales.
- La persistencia conserva datos al reiniciar; la API de nube se probó realmente con nuestros datos.
- El broker y el worker intercambian eventos; hay pruebas de duplicados, fallos y límites de las garantías implementadas.
- La web Pages funciona con el backend remoto y no depende de tu WSL encendido.
- CI compila y prueba; el despliegue tiene controles y enlaces verificables.
- El informe, presentación y defensa coinciden con el mismo código y resultados.
- Puedes explicar cada decisión y recorrer el código; generar archivos no sustituye estudiar y practicar.

## Fuentes y alcance de la verificación

- Enunciados del proyecto final y Entregable 2 proporcionados por el usuario; constituyen la fuente de requisitos y fechas comunicadas.
- Diseño inicial Diseno_Biblioteca_Academica.pdf.
- API suministrada por el profesor: https://paginas-web-cr.com/Api/index.php . Contrato y disponibilidad pendientes de comprobar desde WSL; no se verificó CRUD durante la edición de esta guía.
- GitHub Pages: https://docs.github.com/en/pages/getting-started-with-github-pages/what-is-github-pages
- Publicación con workflows: https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages
- RabbitMQ, conceptos de productor/broker/consumidor: https://www.rabbitmq.com/tutorials/tutorial-one-python . Referencia conceptual; el proyecto debe implementar los componentes requeridos en C++.

Las versiones, precios, requisitos de servicios y acciones de CI deben volver a verificarse al ejecutar los prompts. No se fijan cifras de costos ni cuotas supuestamente gratuitas en esta guía.
