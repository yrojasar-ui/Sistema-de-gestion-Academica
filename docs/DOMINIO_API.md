# Dominio y API de Biblioteca

La API combina el catálogo existente con usuarios, copias físicas, préstamos,
reservas y avisos. El contrato de solicitudes, respuestas y paginación está en
[OpenAPI 3.1](openapi.json). El proceso escucha en `127.0.0.1:8080`.
La pantalla del catálogo continúa usando `/api/libros` y la vista textual del B+.

El modo `--storage mongo` persiste la biblioteca completa y la reconstruye al
reiniciar. Ver [configuración y recuperación](PERSISTENCIA_MONGODB.md).
Las instrucciones de demo de esta sección corresponden al modo local JSON persistente.

## Ejecutar y demostrar

Desde la raíz del repositorio en WSL, con las dependencias C++ ya instaladas:

```bash
bash scripts/preparar_sodium.sh
cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build/debug -j2
bash scripts/demo_biblioteca.sh
```

El script crea un JSON temporal, arranca su propio backend en un puerto libre y
ejecuta Ana → Bruno → Carla. Comprueba estados, referencias y snapshots tras cada
mutación. Verifica recuperación automática tras reiniciar y luego importa reservas en
orden inverso para comprobar que la llegada original determina el turno.
Los archivos y procesos de demostración se limpian al terminar.

Para usar la API manualmente, seguir los comandos de dos terminales y bootstrap
privado del [README](../README.md#arrancar-el-catálogo-web-en-dos-terminales-wsl).
No hay acceso administrativo anónimo. Todas las rutas sensibles requieren Bearer;
ADMIN administra/exporta/recupera/visualiza índices. USUARIO usa catálogo y rutas
propias; identidad y rol se calculan en el servidor. Health/ready son públicos
mínimos. El 401 indica sesión inválida, 403 permisos/origen, 429 intentos de
login limitados. Ver [acceso y transiciones](ACCESO_PANELES.md).

El límite de cuerpo es 1 MiB y cada colección admite hasta 10 000 registros.
El dominio serializado también tiene un límite total de 1 MiB para permitir
restaurar toda exportación aceptada. Se rechaza la operación que lo exceda.
Los límites de texto se aplican en bytes UTF-8.

## Estado, identidad y fechas

| Entidad | Identidad y reglas |
|---|---|
| Libro | `idLibro` inmutable. Se reutiliza `Libro`. La edición retira la clave anterior e inserta la nueva en B+, y recalcula las aristas afectadas al cambiar temas. |
| Usuario | `idUsuario`, nombre y `activo`. Se reutiliza el registro del hash. Consultar por ID utiliza `IndiceUsuarios::buscar`. |
| Ejemplar | `idEjemplar` y `idLibro`. Disponible, prestado, apartado o retirado. Retirar o reincorporar solo se permite sobre copias libres o ya retiradas. |
| Prestamo | `idPrestamo`, ejemplar, usuario, inicio UTC, vencimiento y devolución UTC. Activo requiere devolución y cancelación vacías; estado devuelto/cancelado es explícito. Se conserva el registro después de devolver. |
| Reserva | `idReserva`, libro, usuario, llegada, estado, copia asignada y creación UTC. Pendiente → apartada → atendida. Pendiente y apartada pueden pasar a cancelada. |
| Aviso | `idAviso` generado por el servicio, tipo, destinatario, referencia y creación UTC. Los eventos de devolución y apartado quedan pendientes. |

Los IDs admiten entre 1 y 64 letras ASCII, números, guiones y guiones bajos.
Los IDs administrativos se validan por entidad. POST /api/solicitudes genera ID y llegada desde el servicio y obtiene el lector de la sesión.
El servicio asigna instantes `AAAA-MM-DDTHH:MM:SSZ` y secuencias de llegada
enteras positivas representables exactamente en JSON/JavaScript.
Las fechas `AAAA-MM-DD` se validan según calendario gregoriano y años bisiestos.
El vencimiento inicial debe ser posterior al día actual UTC. Renovar exige una
fecha posterior a la vigente y sustituye la clave del Rojo-Negro.
En esta fase se permite renovar aunque existan reservas pendientes.

## Reglas de atención y conservación

Una copia admite un préstamo activo. Solo puede prestarse a un usuario activo.
Una copia apartada puede prestarse al lector de la reserva que la recibió.
Las reservas pendientes o apartadas son únicas por lector y libro.
La llegada la asigna el servicio al aceptar la reserva, con una secuencia global.
Las colas se organizan por libro y las copias libres se recorren en orden de alta.
Cada copia atiende el siguiente turno válido. Crear o reincorporar una copia
también permite atender reservas.

Devolver cierra el préstamo, elimina su clave del Rojo-Negro, libera la copia y
atiende la FIFO. Cancelar un apartado libera la copia para la reserva siguiente.
Desactivar un usuario con un préstamo activo se rechaza. Para usuarios sin
préstamos activos, la desactivación cancela sus reservas y reasigna sus apartados.
Las canceladas pueden quedar en la cola hasta que el recorrido las descarte.

El borrado de usuario rechaza operaciones pendientes. Con historial, lo
desactiva. El borrado de copia rechaza préstamos y apartados activos. Con
historial, la retira. Los registros sin referencias se eliminan físicamente.
Borrar un libro referenciado por cualquier ejemplar o reserva se rechaza.
Las operaciones de préstamo y reserva históricas se conservan.

## Fuentes de datos y confirmación

`CatalogService` es la única fuente de libros en el proceso. Tanto el catálogo
web como `LibraryService` lo consultan. `JsonBookRepository` conserva el
archivo de trabajo y la protección del catálogo inicial.

**En modo local**, LocalLibraryRepository conserva libros y dominio en una
instantánea JSON. Carga y valida referencias antes de servir. El reinicio no
vacía ninguna entidad. La importación ADMIN sigue disponible para recuperación
explícita, pero no es necesaria para reiniciar. Se reconstruyen hash, Rojo-Negro,
grafo, B+ y FIFO por llegada, sin reiniciar avisos ni comprobantes.
MemoryDomainRepository queda para pruebas y candidatos de validación.

Cada mutación de dominio copia sus registros y nodos reales, aplica los
algoritmos de las estructuras y verifica invariantes antes de llamar al
repositorio. `replace` retorna solo con confirmación. En Mongo un error puede ser
rechazado o incierto; el resultado incierto bloquea y conserva el diario para
relectura explícita, sin afirmar que el remoto no cambió. Después de confirmar
se intercambia el estado visible con una operación sin asignación.
Un rechazo conserva también la topología de los índices.
La edición de libros prepara B+ y grafo y confirma la instantánea JSON o Mongo antes de publicarlos.
El grafo se deriva del catálogo y se reconstruye al arrancar.

El servidor usa un trabajador para serializar solicitudes. El bloqueo existente
coordina las API que comparten el JSON de libros. La consola debe utilizar su
propio archivo de trabajo, como en la demostración existente.
La copia y validación completa tienen coste adicional al recorrido del índice.
La comprobación del grafo compara los temas de cada par de libros.
Esta implementación prioriza consistencia y comprobación en la demo local.

## Persistencia Mongo y avisos

`MongoLibraryRepository` implementa las interfaces de libros y dominio con un
único documento versionado que contiene ambos. Carga, valida y reconstruye todos
los índices al arrancar; conserva secuencias, historia e IDs de avisos. No se
requiere exportación/importación. Reutiliza el transporte HTTP y protocolo del
adaptador E2-B. [Diseño, capacidades reales y recuperación](PERSISTENCIA_MONGODB.md).

`IEventPublisher::publish(const Aviso&)` sigue siendo la frontera preparada.
Los avisos se persisten pendientes en el mismo agregado. RabbitMQ, publicación,
ack y worker siguen pendientes. El futuro worker deberá canalizar cambios por
el único escritor o usar un registro separado con contrato verificado; no puede
escribir avisos directamente en paralelo a reemplazos de esta instantánea.

## Diagramas editables

El caso de uso distingue las acciones del lector y de administración. Los
actores son roles comprobados por AuthService en cada solicitud.

```mermaid
flowchart LR
    L[Lector] --> Consultar[Consultar catálogo y relaciones]
    L --> Reservar[Reservar o cancelar]
    B[Bibliotecario] --> Administrar[Administrar libros, usuarios y ejemplares]
    B --> Prestar[Prestar, renovar y devolver]
    B --> Revisar[Consultar historial, vencimientos y estructuras]
```

```mermaid
classDiagram
    Libro "1" --> "0..*" Ejemplar
    Usuario "1" --> "0..*" Prestamo
    Ejemplar "1" --> "0..*" Prestamo
    Usuario "1" --> "0..*" Reserva
    Libro "1" --> "0..*" Reserva
    Reserva "0..*" --> "0..1" Ejemplar : copia asignada
    Usuario "1" --> "0..*" Aviso
    Usuario "1" --> "0..1" CuentaPrivada
    Prestamo "1" --> "0..1" CargoSimulado
    CargoSimulado "1" --> "0..1" Comprobante
    CuentaPrivada "1" --> "0..1" SesionEnRAM
    class Libro {
        string idLibro
        string titulo
        string autor
        vector temas
    }
    class Ejemplar {
        string idEjemplar
        string idLibro
        string estado
    }
    class Usuario {
        string idUsuario
        string nombre
        bool activo
    }
    class Prestamo {
        string idPrestamo
        string inicioUTC
        string vencimiento
        string devolucionUTC
    }
    class Reserva {
        string idReserva
        uint64 llegada
        string estado
        string creadaUTC
    }
    class Aviso {
        string idAviso
        string tipo
        string estado
    }
```

```mermaid
flowchart TD
    UI[Paneles React por rol] --> HTTP[HttpController]
    HTTP --> AUTH[AuthService: identidad y permisos]
    AUTH --> PRIV[Archivo privado 0600: cuentas Argon2id]
    AUTH --> SES[Sesiones efímeras en RAM]
    HTTP --> D[LibraryService]
    HTTP --> C[CatalogService]
    D --> C
    C --> BP[BPlusTree]
    C --> J[LocalLibraryRepository: agregado JSON]
    D --> H[IndiceUsuarios]
    D --> R[PrestamosActivos]
    D --> G[GrafoTemas]
    D --> F[ColaReservas por libro]
    D --> I[IDomainRepository]
    I --> J
    I --> MR[MongoLibraryRepository]
    C --> MR
    MR --> P[Protocolo compartido con MongoApiBookRepository]
    P --> T[curlRequest existente]
    T --> N[API docente: un agregado libros y dominio]
    MR --> JRN[Diario local de intención]
    D --> A[Avisos pendientes]
    A -. fase posterior .-> E[IEventPublisher]
```

```mermaid
sequenceDiagram
    participant Cliente
    participant HTTP as HttpController
    participant Servicio as LibraryService
    participant RN as Rojo-Negro
    participant FIFO
    participant Repo as Repositorio local o Mongo confirmado
    Cliente->>HTTP: devolver préstamo de Ana
    HTTP->>Servicio: devolver(P-Ana)
    Servicio->>Servicio: preparar copia del estado y cerrar préstamo
    Servicio->>RN: eliminar(vencimiento, P-Ana)
    Servicio->>FIFO: atender primera reserva válida
    FIFO-->>Servicio: R-Bruno
    Servicio->>Servicio: apartar E1 y preparar aviso
    Servicio->>Repo: replace(estado validado)
    Repo-->>Servicio: confirmado
    Servicio->>Servicio: publicar estado preparado
    Servicio-->>HTTP: operación completada
    HTTP-->>Cliente: 200
    Cliente->>HTTP: cancelar R-Bruno
    HTTP->>Servicio: cancelar(R-Bruno)
    Servicio->>FIFO: atender siguiente turno
    FIFO-->>Servicio: R-Carla
    Servicio->>Repo: replace(estado con E1 para Carla)
    Repo-->>Servicio: confirmado
    Servicio-->>Cliente: 200
```

El retorno confirmado del diagrama incluye GET de verificación en modo Mongo.
Si queda incierto, no se ejecuta el intercambio ni se responde 200: se devuelve
503, se bloquea la biblioteca y se ofrece recuperación por lectura.

```mermaid
classDiagram
    IBookRepository <|.. JsonBookRepository
    IBookRepository <|.. MongoLibraryRepository
    IDomainRepository <|.. LocalLibraryRepository
    IBookRepository <|.. LocalLibraryRepository
    IDomainRepository <|.. MongoLibraryRepository
    CatalogService --> IBookRepository
    LibraryService --> CatalogService
    LibraryService --> IDomainRepository
    MongoLibraryRepository --> MongoApiProtocol
    MongoApiBookRepository --> MongoApiProtocol
    MongoApiProtocol --> HttpTransport
    MongoLibraryRepository --> PersistenceError
    class MongoLibraryRepository {
        refresh(initialize)
        accept()
        load()
        save(libros)
        replace(dominio)
        ready()
    }
    class PersistenceError {
        WriteOutcome outcome
    }
```


## Cancelaciones, anulación y simulación

[Transiciones completas](ACCESO_PANELES.md#transiciones-antes-de-implementar).
Reserva cancelada conserva auditoría; anular préstamo requiere ADMIN, motivo y
copiaRecuperada=true. Se retira RN y se atiende FIFO; no inventa devolución.
Cargo demo de 10000 centésimos CRC, servidor configurable, solo SIMULADO.
Un comprobante por préstamo, reintento idempotente. Anulación invalida el cargo,
conserva comprobante y no ejecuta cobro/reembolso real. El esquema de dominio
v2 se migra explícitamente; credenciales y sesiones nunca viajan en exportaciones.
