# Persistencia MongoDB y recuperación (Prompt 4)

La API C++ tiene modos **local** y **mongo** explícitos. El modo Mongo carga
automáticamente toda la biblioteca al arrancar; no importa exportaciones manuales.
El modo local también conserva libros y dominio en un agregado JSON durable.
No se cambia de modo después de un error. No hay RabbitMQ ni worker en esta fase.

## Contrato observado y decisión

Se verificó primero GET en la colección propia `biblioteca_yoel_final_v1`.
La ruta válida es `https://paginas-web-cr.com/Api/apis/mongodb.php`;
GET devuelve `coleccion`, `total` y `documentos`, con `_id: {"$oid": "..."}`.
La ruta `/Api/index.php` del segundo avance devolvía HTTP 200 con HTML;
se corrigió la ruta y se exige JSON válido. Un HTTP 200 por sí solo no confirma nada.

Los [sondeos reales](evidencias/persistencia-mongodb/capacidades.log) usaron
registros sintéticos con IDs únicos y eliminaron exclusivamente sus ObjectId.
Se observaron objetos anidados, filtros exactos de ID y revisión, cero modificaciones
con revisión obsoleta y una sola modificación en una competencia acotada de dos PUT.
El [sondeo de filtro anidado](evidencias/persistencia-mongodb/filtro-anidado.log)
comprobó específicamente `estado.revision`. **Dos POST con el mismo ID de negocio
crearon dos documentos**: no existe unicidad observada para ese campo.

No se probaron autenticación, aislamiento entre usuarios, transacciones,
paginación ni límites remotos de tamaño. Una lectura debe tener
`total == documentos.size()`; no hay soporte inventado de páginas. La colección
dedicada contiene exactamente un documento. Se rechazan una respuesta truncada
detectable, duplicados o documentos ajenos. El GET completo no acredita ausencia
de truncamiento silencioso del servidor si también falseara el total.

Se elige **una instantánea de biblioteca en un documento**, con libros y dominio
dentro de **un solo campo `estado`**. No se encadenan escrituras entre colecciones.
Esto elimina la ventana de escritura parcial entre libros y dominio en nuestro
protocolo. No se afirma una transacción distribuida ni una garantía de atomicidad
del servicio docente. La API no dispone de documentación contractual que garantice
el comportamiento observado bajo todos los fallos o clientes concurrentes.

Se reutiliza el transporte `curlRequest` y el protocolo extraído de
`MongoApiBookRepository` (validación de URL/colección, MIME, errores HTTP/JSON y
ObjectId). Ese adaptador continúa operando los libros individuales de la consola
E2-B sin cambiar su colección. `MongoLibraryRepository` implementa las interfaces
de instantáneas de libros y dominio para el agregado completo; no representa el
dominio como libros ficticios ni vuelve a implementar el CRUD de la consola.

## Esquema remoto 1

Una colección exclusiva por biblioteca, de 1–100 caracteres ASCII seguros.
`proyecto` debe coincidir exactamente con la colección configurada.
`idBiblioteca` es `principal`; el ObjectId remoto es independiente de los IDs
de negocio. No hay migración automática desde las colecciones de E2-B.

```json
{
  "_id": {"$oid": "0123456789abcdef01234567"},
  "proyecto": "biblioteca_yoel_final_v1",
  "idBiblioteca": "principal",
  "versionEsquema": 1,
  "estado": {
    "revision": 1,
    "libros": [],
    "dominio": {
      "version": 2,
      "usuarios": [],
      "ejemplares": [],
      "prestamos": [],
      "reservas": [],
      "avisos": [],
      "cargos": [],
      "comprobantes": [],
      "siguienteLlegada": 1
    }
  }
}
```

Los tipos, IDs, estados, fechas y referencias de las entidades siguen
[DOMINIO_API](DOMINIO_API.md) y [OpenAPI](openapi.json). Se conserva todo el
historial, incluso reservas canceladas/atendidas y avisos pendientes.
`siguienteLlegada` y los IDs `AV-n` no se reinician. Los índices se derivan:
B+ por título normalizado/ID, hash por usuario, Rojo-Negro por vencimiento/ID,
grafo por temas y FIFO por libro ordenada por llegada. Las consultas continúan
recorriendo esas estructuras C++; el remoto es almacenamiento.

Cota propia: **1 MiB para el documento completo** y hasta 10 000 registros
por conjunto; no se presenta como un límite medido del servidor. El transporte
admite hasta 16 MiB de respuesta y profundidad JSON 32. Un exceso o versión
desconocida se rechaza. No hay partición ni archivado automático.

## Protocolo de escritura y resultados

1. Preparar y validar una copia de los registros y estructuras, sin publicar.
2. GET completo: debe coincidir con la última instantánea confirmada.
   Si difiere o falla, no se envía la mutación y se bloquea para recargar.
3. Guardar intención en `<mongo-state>.pending.json`: endpoint, colección,
   documento anterior y documento esperado. Archivo temporal, rename y fsync
   de archivo/directorio antes de contactar al servidor.
4. Enviar **una sola vez** POST para inicializar o PUT para cambiar `estado`.
   PUT filtra ObjectId, proyecto, idBiblioteca y la revisión anterior
   `estado.revision`. La revisión nueva viaja dentro de `estado`.
5. GET completo después de cualquier acuse, timeout o respuesta ambigua.
   Debe coincidir exactamente con el documento esperado (ObjectId asignado
   por el servidor únicamente al crear). Solo entonces se retira el diario,
   se publica el estado preparado y se devuelve éxito.

| Resultado | Evidencia y comportamiento |
|---|---|
| Confirmado | GET coincide con todo el estado esperado. Puede confirmar incluso tras timeout o HTML. Se publica la copia preparada. |
| Rechazado | No se envió la mutación, o rechazo explícito/contador cero más GET idéntico al anterior. No se publica la copia. |
| Incierto | No hay lectura concluyente, se observa el estado anterior tras timeout sin rechazo, o hay un estado diferente/parcial. No se afirma que no cambió el remoto. Se conserva la intención y se bloquea la biblioteca. |

`IDomainRepository::replace` retorna solo con confirmación; `PersistenceError`
declara rechazo o incertidumbre. `CatalogService::commit` admite repositorios
de instantáneas, incluido Mongo, y publica el B+ después de guardar.
El adaptador de registros individuales sigue reservado al recorrido de consola.
Una excepción conserva los índices visibles anteriores, pero **no implica**
que el servidor remoto conserve el estado anterior.

Los rechazos de reglas de negocio mantienen los HTTP existentes (400/404/409).
Los problemas de persistencia usan 503 con `error` y `resultado`.
Un fallo previo a enviar puede bloquear por falta de lectura actual aunque la
mutación se sepa rechazada. Un rechazo remoto confirmado no exige bloquear.

## Arranque y recuperación

Se mantiene el mismo endpoint, colección y ruta `--mongo-state` entre reinicios.
El diario es un registro de incertidumbre, no la fuente de las entidades.
No borrarlo para desbloquear una escritura; no arrancar otra instancia con otra
ruta para eludirlo. El bloqueo del archivo estable solo coordina procesos locales
que usan esa misma ruta. No es un bloqueo remoto.

Al arrancar se carga toda la instantánea, se reconcilia cualquier intención,
se construyen y validan los cinco índices en candidatos y recién entonces se
intercambian los estados visibles. Una biblioteca vacía solo es válida si existe
un documento explícitamente inicializado con sus listas vacías. Un GET ausente,
inválido, incompatible o fallido deja la API **no lista**, sin publicar un catálogo
vacío ni recurrir a JSON/memoria. El proceso HTTP permanece disponible.

- `GET /healthz`: 200 si el proceso HTTP responde.
- `GET /readyz`: público mínimo, 200 con `listo:true`; 503 con `listo:false`.
  `GET /api/almacenamiento` exige ADMIN y entrega almacenamiento, recuperación
  y resultado reales. El detalle de datos bloqueados no es público.
- Mientras está bloqueada, las rutas de biblioteca devuelven 503, incluidas
  lecturas. Solo salud, OPTIONS, sesión y recuperación/diagnóstico ADMIN siguen habilitados.
- `POST /api/recuperacion`, cuerpo `{}`: GET y reconstrucción íntegra,
  sin reenvío de POST/PUT. En local valida el estado actual.

```bash
curl -sS http://127.0.0.1:8080/healthz
curl -sS http://127.0.0.1:8080/readyz
# POST /api/recuperacion requiere Authorization: Bearer de una sesión ADMIN.
# No coloque credenciales o tokens literales en comandos guardados/versionados.
```

Si existe intención pendiente, la recuperación automática exige observar
**exactamente** lo esperado. Si la solicitud tardía se aplica, una nueva
recuperación GET la confirma sin duplicados. Si continúa ausente/antigua o
parcial, se mantiene el bloqueo. No hay un botón para asumir rechazo ni reenviar.

Ante una escritura parcial del servidor, se conserva el diario como diagnóstico.
Un operador debe detener escritores externos y resolver el documento con
control del servidor, completando exactamente la instantánea esperada; después
se usa recuperación GET. El fixture de parcial demuestra ese caso, sin causar
fallos en la nube compartida. No se automatiza una compensación que pueda
pisar cambios ajenos. Si no se puede determinar el resultado (o fue reemplazado
por cambios externos distintos), la limitación es operativa: requiere intervención
y respaldo del administrador, no se desbloquea descartando el diario.

Un reinicio con diario intacto jamás repite POST. Deben conservarse volumen y
diario en cualquier futuro despliegue. Pérdida del volumen, manipulación del diario,
restauración remota anterior o cambios directos del proveedor quedan fuera de
la garantía. La durabilidad del fsync depende del sistema de archivos/WSL.
No se promete exactamente-una-vez entre clientes HTTP que pierdan la respuesta:
el cliente debe consultar el ID o recuperar, sin regenerar IDs automáticamente.

## Un escritor y futuro worker

La API tiene un trabajador HTTP y publica por intercambio de candidatos.
El sondeo de dos PUT demuestra una observación acotada de filtro de revisión,
**no** una arquitectura multiinstancia. Antes de escribir se detectan cambios
externos por comparación completa; después de una confirmación, un cliente
directo todavía puede alterar el documento. Las consultas locales representan
la última carga/escritura confirmada; no hacen polling remoto.

No se han implementado roles ni autenticación, ni se ha verificado aislamiento
de la API docente. Usar solo datos sintéticos. La unicidad de negocio se valida
en C++ y se rechazan documentos duplicados al leer, pero no se promete unicidad
servidor. No se presupone transacción, bloqueo remoto o autorización por el campo
`proyecto`.

Los avisos se guardan con la misma mutación de préstamo/reserva.
**El futuro worker no debe modificar esta instantánea directamente**: debe
solicitar cambios al único escritor mediante una interfaz de comandos que aún
falta implementar. Como alternativa futura, un registro de entrega separado,
append-only e idempotente por `idAviso`, requerirá su propio contrato verificado
y coordinación antes de habilitarlo. No se debe añadir un worker que escriba
`estado.avisos` en paralelo: podría perder sus confirmaciones al reemplazar
el agregado. RabbitMQ, publicación, ack, deduplicación de entregas y estados
de aviso enviado siguen pendientes.

## Comandos reproducibles (WSL)

Seguir el [README vigente](../README.md#persistencia-completa-en-mongodb-prompt-4):
preparar libsodium, compilar con libcurl, crear el primer ADMIN con bootstrap
local y conservar --auth-store y --mongo-state al reiniciar.
No usar una colección E2-B de libros individuales. --initialize solo crea si está
vacía. Un dominio v1 exige respaldo y --migrate explícito: se valida, escribe y
confirma v2 antes de servir. El sobre Mongo mantiene versionEsquema=1; el
contenido estado.dominio.version=2 contiene auditoría, cargos y comprobantes.
Las cuentas permanecen en archivo privado, fuera de ese documento.

Demostración real aislada (Python estándar; no ejecutar desde CI):

```bash
python3 scripts/demo_mongodb.py --binary build/debug/catalogo_api \
  --evidence "build/demo-mongodb-$(date +%Y%m%d-%H%M%S)"
```

Genera una colección propia con UUID, hace GET previo, inicializa una biblioteca
vacía y ejecuta Ana → Bruno → Carla con cuatro reinicios de procesos.
Comprueba renovación, edición de título/temas, índices, avisos e historial, y
demuestra que el siguiente turno es 3. Verifica cada lectura contra el estado
anterior al reinicio. Al finalizar elimina únicamente el ObjectId propio y
verifica GET vacío. Conserva logs y snapshots sintéticos en el directorio de
evidencias. Si no puede confirmar limpieza, conserva el diario temporal y avisa.
No lee ni modifica `data/libros_iniciales.json`.

Pruebas deterministas, sin nube:

```bash
ctest --test-dir build/debug -R 'mongo_|biblioteca_api|catalogo_http' --output-on-failure
bash scripts/verificar_e2a.sh build/verificacion-persistencia
cd frontend && npm ci && npm run build
```

Ver [resultados y evidencia](evidencias/persistencia-mongodb/README.md).


## Evidencia de la fase de acceso

La evidencia cloud anterior corresponde a dominio v1, antes de autenticación y
pagos. Esta fase no realizó escrituras cloud. Migración v1/v2, cargos,
comprobantes, fallo/reintento y recuperación se verifican con transporte
determinista en mongo_biblioteca_recuperacion. Los reinicios con cuentas y paneles
se verifican en JSON local. Ver [evidencias](evidencias/acceso-paneles/README.md).
