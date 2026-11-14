# Biblioteca académica — reglas permanentes

Autor: Yoel Alexander Rojas Arguedas. Universidad CENFOTEC, Estructuras de Datos II.
Responder, comentar y documentar en español. Este proyecto no es File-X.

## Alcance autorizado

El segundo avance y el catálogo web están integrados en main. La integración continua está activa.
La fase actual cubre las estructuras del Prompt 2 y sus pruebas.
Segundo avance el 16 de noviembre de 2026 y entrega final el 24 de noviembre de 2026.
Trabajar en este repositorio e historial; inspeccionar y conservar archivos existentes.

## Diseño y reglas

- Núcleo C++17, CMake, consola y módulos separados; RAII y errores explícitos.
- Catálogo: B+ propio, clave `(tituloNormalizado, idLibro)`, hojas enlazadas,
  inserción, búsqueda y borrado algorítmico con redistribución/fusión/contracción.
  Documentar normalización española determinista y sus límites de ordenación UTF-8.
- No sustituir estructuras académicas por mapas estándar, frontend o índices cloud.
  Contenedores auxiliares deben justificarse; `std::map` se admite como oráculo de pruebas.
- Libro es una obra; Ejemplar es copia física. Entidades finales: Libro, Ejemplar,
  Usuario, Prestamo y Reserva; preservar historial de préstamos.
- Una biblioteca, lectores y bibliotecarios, ejemplares físicos y avisos internos.
  Sin pagos, multas monetarias, sedes ni recomendaciones con IA.
- Final: Rojo-Negro para préstamos activos `(fechaVencimiento,idPrestamo)`;
  quitar al devolver conservando historia, retirar/reinsertar al cambiar vencimiento.
- Final: hash extensible propio por idUsuario (directorio, profundidades y cubetas);
  grafo no dirigido por temas compartidos, listas de adyacencia, sin bucles/duplicados;
  FIFO propia por libro, llegada y desempate por ID, cancelación lógica.
- Final: solo usuarios activos reciben préstamos; máximo uno activo por ejemplar;
  una reserva pendiente por lector/libro, primera reserva válida; una copia prestada,
  apartada o retirada no está disponible. Reserva por libro, no por ejemplar.
- Persistencia tras IBookRepository: JSON ahora; API MongoDB docente después,
  con contrato real comprobado. Distinguir local, mock y nube; sin fallback silencioso.
- Final: React/TypeScript/Vite en Pages; API y worker C++; RabbitMQ como base local.
  Proveedor remoto por decidir. No imponer Firebase/Firestore/Cloud Run/Pub/Sub.
  No inventar autenticación, transacciones ni garantías de la API externa.

## Calidad y conservación

- Dependencias fijadas y licencias identificadas; bibliotecas externas permitidas
  para JSON, HTTP, pruebas y visualización, no para las estructuras académicas.
- Pruebas deterministas, semillas, oráculo independiente e invariantes tras operaciones.
  Conservar comandos, salida y códigos reales de build, CTest y sanitizers.
- No inventar resultados, capturas, métricas, enlaces, calificación o despliegues.
- No borrar trabajo/datos del usuario ni forzar historial. No publicar secretos,
  credenciales ni información personal en servicios compartidos.
- Registrar decisiones y actualizar ESTADO/REQUISITOS con implementación → prueba → evidencia.
  No marcar capacidades externas verificadas con mocks.
- Al cerrar fase: cambios, decisiones, comandos, resultados, límites y commit local
  descriptivo si Git está listo; nunca reconfigurar identidad del usuario.
- Rúbrica final: estructuras 30%, integración/comunicación 20%, nube/CI 15%,
  informe 20%, defensa 15%. Requiere CRUD, visualización interna, colas reales,
  base nativa en nube, Git/CI, informe técnico y defensa; no implementar ahora.
