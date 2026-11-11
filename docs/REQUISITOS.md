# Matriz de requisitos

Alcance: Prompt 0 y E2-A. Estados locales respaldados por ejecución real; no hay
ninguna capacidad verificada en nube. Evidencia canónica en
`evidencias/e2a/ejecucion-final/`; la ejecución preliminar se conserva separada.

| ID | Fase | Criterio verificable | Implementación | Prueba/evidencia | Estado |
|---|---|---|---|---|---|
| P0 | 0 | Contexto, reglas, decisiones y matriz | AGENTS.md, docs | Inspección registrada en ESTADO.md | Documentado |
| A01 | E2-A | B+ propio, inserción/búsqueda/borrado y hojas enlazadas | `backend/src/BPlusTree.cpp` | `test_bplus.cpp` → `debug-ctest.log`, `asan-ctest.log` | Implementado y probado localmente |
| A02 | E2-A | Split, préstamo, fusión y contracción de raíz | `split`, `rebalance`, `validate` | 8 órdenes; contadores de ambos préstamos hoja/interno → logs CTest | Implementado y probado localmente |
| A03 | E2-A | JSON validado sin carga parcial; duplicados | `JsonBookRepository.cpp`, `CatalogService::load` | `test_catalog.cpp`: 18 entradas inválidas, ID global, homónimos, fallos de adaptador → logs CTest | Implementado y probado localmente |
| A04 | E2-A | Menú y consultas por índice; selección inequívoca | `ConsoleUI.cpp`, `CatalogService.cpp` | Integración selección/EOF + demo → `demo.log` | Implementado y probado localmente |
| A05 | E2-A | Bibliotecas CMake separadas | `CMakeLists.txt`: libro, bplus, json_repository, catalog_service, console_ui | `debug-build.log`, `asan-build.log`, `resultados.tsv` | Implementado y compilado localmente |
| A06 | E2-A | Archivo de trabajo separado; fallo y reinicio | `JsonBookRepository::save`, dirty en servicio | Fallo de escritura/rename, protección de fixture, reintento, dos procesos → logs CTest y demo | Implementado y probado localmente |
| A07 | E2-A | Español UTF-8 determinista | `Libro.cpp::normalizeTitle` | Acentos/mayúsculas/descompuestos/ñ/otros Unicode/UTF-8 inválido → `test_bplus.cpp` y logs CTest | Implementado y probado localmente |
| A08 | E2-A opcional | Modificar título conservando índice | Sin implementar | Sin pruebas | Pendiente opcional |
| B01 | E2-B | API MongoDB verificada | Sin iniciar | Sin evidencia cloud | Pendiente |
| C01 | E2-C | Informe 2–3 páginas y capturas | Sin iniciar | Sin informe | Pendiente |
| F01 | Final | Rojo-Negro, hash extensible, grafo, FIFO propios | Sin iniciar | Sin pruebas | Pendiente |
| F02 | Final | Ejemplares/usuarios/préstamos/reservas e integridad | Sin iniciar | Sin pruebas | Pendiente |
| F03 | Final | API/worker/colas reales y persistencia cloud | Sin iniciar | Sin evidencia cloud | Pendiente |
| F04 | Final | React, autenticación real, Git/CI y despliegue | Sin iniciar | Sin evidencia remota | Pendiente |
| F05 | Final | Informe técnico, presentación y defensa | Sin iniciar | Sin artefactos | Pendiente |

Los archivos `.cpp` de pruebas están en `backend/tests/`. No se interpreta un
build local como CI remoto, JSON como nube, ni logs de terminal como capturas
del informe. Las cotas, límites y protocolo de guardado se explican en README.
El cierre completo del Entregable 2 (informe E2-C) no se declara realizado.
