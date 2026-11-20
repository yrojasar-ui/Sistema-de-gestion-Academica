# Correcciones del PR #11

Ejecución local en WSL, 18 de noviembre de 2026 (UTC en respuestas del servidor:
19 de noviembre). Rama feat/acceso-paneles-biblioteca. Registros sintéticos
aislados, sin API docente. Se mantienen las evidencias anteriores.

## Resultados WSL

| Comprobación | Resultado | Registro |
|---|---|---|
| CTest Debug, regresión completa | 14/14; salida 0 | [ctest-debug.txt](ctest-debug.txt) |
| CTest ASan/UBSan, regresión completa | 14/14; salida 0 | [ctest-asan.txt](ctest-asan.txt) |
| npm ci, pruebas, build y rutas Pages | 18/18; todos salida 0 | [frontend.txt](frontend.txt) |
| API con cuentas/dominio temporales | 195 solicitudes; salida 0 | [api.txt](api.txt) |
| Respuestas reales frente a OpenAPI | 119 respuestas JSON; salida 0 | [openapi.txt](openapi.txt) |

Comandos desde la raíz WSL (Node 24.22.3 en PATH, dependencias fijadas del proyecto):

~~~bash
cmake --build build/debug -j2
ctest --test-dir build/debug --output-on-failure
cmake --build build/asan -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build/asan --output-on-failure
(cd frontend && npm ci && npm test && npm run build && npm run verify:dist)
python3 backend/tests/test_acceso_api.py \
  build/debug/catalogo_api data/libros_iniciales.json
~~~

Las pruebas frontend se incluyen automáticamente en el npm test ya ejecutado
por CI; la regresión de API está registrada como acceso_paneles_api en CTest.
No se añadieron dependencias ni accesos a nube.

## Qué cubren las pruebas deterministas

- El mismo controlador de intento que usa FormDialog: POST/PUT/DELETE aplicados
  pero respuesta perdida, GET automático y manual; ningún reenvío de escritura.
- Resultado confirmado/rechazado/incierto conservado, 409 de negocio,
  bloqueo 503, pérdida de conexión, HTML y JSON truncado.
- Cambio de cuenta durante una reconsulta que ignora abort y antes de iniciar
  el GET: no publica la respuesta de A ni usa las credenciales nuevas de B.
- Renderizado real de LoanCard/ReserveCard: título y autor en activo,
  devuelto, anulado y solicitudes pendientes/apartadas/atendidas/canceladas.
- API C++ con sesiones: título/autor correctos tras préstamo, devolución y
  reinicios; edición posterior actualiza metadatos sin cambiar historia.
  Filas propias, endpoints ajenos 404, rutas administrativas 403.
- La regresión conserva pagos idempotentes, FIFO, fallos sin cambios parciales
  y reconstrucción de estructuras. No se alteraron las reglas de dominio.

## Navegador real y fallo inducido localmente

Se ejecutó la API C++ Debug con JSON y cuentas privadas sintéticas, puerto
18080; un proxy de prueba local en 8080 reenvió las peticiones del Vite en
5173. El proxy recibió 201 al crear la solicitud y cerró esa conexión sin
entregar la respuesta. Vite presentó una respuesta no interpretable; el cliente
la clasificó como incierta. No se provocó un fallo en el servidor compartido.

Ana tenía PA activo para “Atlas del campus” y PH devuelto para “Historia del
campus”, ambos de Autora sintética. Solicitó Historia del campus: la API creó
SOL-1 apartada. El formulario mostró que podía haberse guardado y consultó
libro, biblioteca propia, búsqueda y disponibilidad. Se pulsó Consultar estado.
El [registro del proxy](navegador-peticiones.txt) contiene **un solo
POST /api/solicitudes**, seguido por GET, sin reenvío. Los POST de login/logout
son cambios explícitos de sesión, no reintentos de la mutación.

- [Formulario tras reconsulta](01-reconsulta.png): advertencia incierta,
  reserva apartada y botón de solo consulta. Captura durante la verificación;
  posteriormente se simplificó el resumen de estado a texto legible.
- [Mi biblioteca de Ana](02-biblioteca-con-titulos.png): título/autor en activo,
  solicitud e historial, junto con IDs, estado y fechas.
- [Mi biblioteca de Bruno](03-bruno-sin-datos-de-ana.png): sin préstamos,
  solicitudes ni historial de Ana después de cerrar e iniciar sesión.

No se guardaron contraseñas, tokens, encabezados ni cuerpos de login en estas
evidencias. La captura se tomó del navegador; no se generó una imagen ficticia.

## GitHub y límites

Los registros de esta carpeta son de WSL. Los [checks del PR #11](https://github.com/yrojasar-ui/Sistema-de-gestion-Academica/pull/11/checks)
identifican el HEAD publicado; su ejecución y resultado se enlazan en la
descripción del PR, separados de los resultados locales.

El navegador comprobó catálogo → solicitud incierta → reconsulta y biblioteca
personal/cambio de cuenta. La carrera durante el GET, pago incierto y los demás
formularios usan el controlador común probado con fixtures; no se afirma
haber inducido manualmente el fallo en cada formulario. No se verificó esta
interrupción contra Mongo docente, Pages+API HTTPS ni múltiples escritores.
No se garantiza ejecución exactamente una vez para mutaciones genéricas:
se evita el reenvío automático y se muestra el estado leído sin reinterpretar
el resultado original. El pago sí conserva su idempotencia de servidor.
La recuperación de una biblioteca bloqueada requiere intervención explícita.
