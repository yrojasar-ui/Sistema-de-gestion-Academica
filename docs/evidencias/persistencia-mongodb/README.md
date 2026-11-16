# Evidencias del Prompt 4

Ejecución en Ubuntu/WSL el **15 de noviembre de 2026, America/Guatemala**.
Los timestamps de nube usan UTC (16 de noviembre). Base:
`a0cec44e61df72033e8de1b11e9bba52cedabfa3`, PR #7 integrado.
Su [CI de main](https://github.com/yrojasar-ui/Sistema-de-gestion-Academica/actions/runs/43798520621)
estaba en success antes de comenzar. No se movió `entregable-2`.

## Nube real

El GET previo de la colección propia está en [get-inicial.json](get-inicial.json).
[capacidades.log](capacidades.log) y [filtro-anidado.log](filtro-anidado.log)
conservan los sondeos sintéticos de contrato, filtros, PUT concurrentes acotados
y duplicación de ID. Sus ObjectId se eliminaron individualmente y el GET final
quedó vacío. La observación de filtros no acredita transacciones ni multiinstancia.

La [demo final](nube/nube.log) se ejecutó con el servidor C++ real y una colección
nueva generada por el script. [backend.log](nube/backend.log) muestra los procesos.
No se exportó/importó dominio para restaurarlo: cada arranque leyó Mongo.
Los JSON siguientes se guardaron **solo como evidencia** de lo recuperado:

| Reinicio | Resultado observado | Estado e índices |
|---|---|---|
| 1, préstamo | Ana conserva la única copia prestada y el préstamo renovado. Bruno turno 1 y Carla turno 2; siguiente 3. Título nuevo en B+ y temas/aristas actualizados. | [reinicio-1-prestamo.json](nube/reinicio-1-prestamo.json) |
| 2, devolución | Historial de Ana conservado, RN sin activos, copia apartada para Bruno, Carla pendiente. Avisos AV-1 y AV-2 pendientes. | [reinicio-2-apartado.json](nube/reinicio-2-apartado.json) |
| 3, después de Carla | Cancelar Bruno cedió la copia a Carla; recibió y devolvió. Dos préstamos históricos, cero activos, copia disponible, AV-1…AV-4 sin duplicados. | [reinicio-3-historial.json](nube/reinicio-3-historial.json) |
| 4, nueva reserva | Nueva reserva de Ana turno 3, siguiente 4; apartado y nuevo AV-5 persistidos. | [reinicio-4-secuencia.json](nube/reinicio-4-secuencia.json) |

Cada mutación comprueba invariantes; cada reinicio compara todos los libros y
entidades contra el estado anterior. Se comprueban las claves del Rojo-Negro,
usuarios en hash y orden de FIFO exportados. El préstamo incompatible devuelve
409 sin cambiar el dominio. La limpieza elimina solo el ObjectId propio,
filtrado también por proyecto e idBiblioteca, y confirma `total:0`.

Comando reproducible desde la raíz:

```bash
python3 scripts/demo_mongodb.py --binary build/debug/catalogo_api \
  --evidence "build/demo-mongodb-$(date +%Y%m%d-%H%M%S)"
```

## Fixtures y comprobaciones en WSL

La nube compartida **no sufrió fallos inducidos**. El CTest
`mongo_biblioteca_recuperacion` inyecta un transporte local determinista:
timeout aplicado, timeout con aplicación tardía, HTTP 200 HTML, rechazo,
fallo de lectura antes/después de escritura, falta de directorio para diario,
POST incierto a través de reinicio, estado parcial, datos incompatibles,
duplicados, respuesta incompleta y edición externa. Comprueba que no hay cambios
visibles parciales ni reenvío ciego, y prueba HTTP real sobre loopback para
healthz/readyz/bloqueo/recuperación. La reparación del parcial la realiza el
fixture; en producción requiere intervención del operador y control del servidor.

| Verificación WSL | Resultado | Registro |
|---|---|---|
| Regresión Debug completa | 13/13, retorno 0 | [debug-ctest.log](wsl/debug-ctest.log) |
| Regresión ASan/UBSan completa, fugas habilitadas | 13/13, retorno 0 | [asan-ctest.log](wsl/asan-ctest.log) |
| Composición final de API y Mongo, Debug | 4/4 | [api-final-debug.log](api-final-debug.log) |
| Composición final de API y Mongo, ASan/UBSan | 4/4 | [api-final-asan.log](api-final-asan.log) |
| Consola JSON y archivo inicial | Demo de reinicio correcta; fixture intacto | [demo.log](wsl/demo.log) |
| Compilación sin Mongo y recorrido HTTP local | Correctos | [local-sin-mongo.log](local-sin-mongo.log) |
| Node 24.22.3, npm ci y npm run build | TypeScript y Vite correctos | [frontend.log](frontend.log) |
| OpenAPI / JSON Schema en WSL | 36 esquemas, 411 respuestas locales, 75 respuestas del modo Mongo y 4 snapshots conformes | [contrato.log](contrato.log) |

[Comandos y retornos](wsl/resultados.tsv), [entorno](wsl/entorno.log) y
[hashes al iniciar regresión](wsl/fuentes.sha256). La comprobación final de
API se repitió después de aislar la construcción del repositorio JSON al modo
local; [hashes finales](fuentes-final.sha256) identifican las fuentes publicadas.
Los logs de compilación finales están en [Debug](build-final-debug.log) y
[ASan](build-final-asan.log).

GitHub Actions es una ejecución distinta de WSL: consultar el enlace y resultado
del nuevo HEAD en el PR de esta rama. El workflow conserva Debug, ASan/UBSan,
frontend y artefactos CTest; no incluye `demo_mongodb.py` ni credenciales cloud.
Este directorio registra las verificaciones WSL y nube real, sin presentarlas
como resultados ejecutados por GitHub.

## Límites

Escritor único, agregado máximo 1 MiB (cota local), sin autenticación,
transacciones o paginación verificadas en nube. ID de negocio duplicable en el
servidor. La competencia acotada de dos PUT no prueba aislamiento general.
La incertidumbre irresoluble mantiene bloqueo y requiere intervención.
El diario debe conservarse. El futuro worker no puede escribir este agregado
directamente; RabbitMQ, publicación y entrega de avisos siguen pendientes.
Ver [protocolo y recuperación](../../PERSISTENCIA_MONGODB.md).
