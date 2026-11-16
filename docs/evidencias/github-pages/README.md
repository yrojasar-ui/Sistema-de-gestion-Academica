# Preparación de GitHub Pages

Base: main `6692b41`, PR #8 integrado. Rama `feat/github-pages`.
Fecha: 15 de noviembre de 2026, America/Guatemala.
Árbol limpio antes de crear la rama. `entregable-2` se conserva en
`0cb6d3156b802fd27b66e5e7052bd7ab20986d87`.

## Verificaciones ejecutadas en WSL

| Verificación | Resultado | Evidencia |
|---|---|---|
| Node 24.22.3, npm ci, pruebas frontend | 5/5 pruebas de configuración de API | [frontend.log](frontend.log) |
| TypeScript y Vite, sin URL de API | Build correcto | [frontend.log](frontend.log) |
| Recursos de producción | JS/CSS existentes bajo la subruta y GET 200 con MIME correcto | [recursos.log](recursos.log) |
| URL HTTPS de prueba | Build correcto; es un dominio .invalid, no un backend activo | [build-configurada.log](build-configurada.log) |
| URL terminada en /api o con credenciales sintéticas | Build rechazado antes de generar artefactos | [build-configurada.log](build-configurada.log) |
| Navegador integrado, build servido con preview | Mensaje exacto sin backend, controles deshabilitados | [navegador.txt](navegador.txt) |
| Desarrollo con proxy y C++ real | 16 obras y búsqueda sin resultados; JSON temporal | [navegador.txt](navegador.txt) y [recursos.log](recursos.log) |
| CTest Debug existente | 13/13, 27,30 s | [debug-ctest.log](debug-ctest.log) |
| CTest ASan/UBSan existente | 13/13, 166,46 s | [asan-ctest.log](asan-ctest.log) |
| Workflow | actionlint 1.7.12 sin errores; condiciones, permisos y artefacto revisados | [workflows.log](workflows.log) |

Las cinco pruebas cubren desarrollo con proxy, producción sin variable,
origen HTTPS con barra opcional, rechazo de /api/rutas/query/credenciales/HTTP
y rechazo de localhost/loopback. Se usan Node test y dependencias existentes.
No se incorporó una biblioteca de pruebas ni se modificó package-lock.json.

La descarga local de actionlint 1.7.12 se verificó con el SHA-256 publicado en
su release oficial. No se añadió al runtime ni se actualizó ninguna dependencia
del proyecto. La validación estática comprobó: `needs: [backend, frontend]`,
guardas push/main en caller/build/deploy, workflow_call exclusivo, artefacto
frontend/dist, entorno github-pages, permisos de deploy Pages/OIDC y ausencia
de uso de secrets.

El build configurado se hizo en un directorio de prueba separado; el build
principal quedó sin VITE_API_URL. No se contactó el dominio .invalid ni se probó
un backend remoto. La advertencia de outDir fuera de frontend corresponde
únicamente a esa compilación aislada.

## Estado de GitHub y pendientes

Lectura de configuración del repositorio: Pages ya tiene `build_type: workflow`
(Source = GitHub Actions); la variable `VITE_API_URL` devolvió 404 (ausente).
No se cambió la configuración de Pages ni se crearon variables o secretos.
La URL esperada es `https://yrojasar-ui.github.io/Sistema-de-gestion-Academica/`;
no se verificó una publicación efectiva de esta rama.

El PR ejecuta CI sin desplegar. Sus resultados y enlace a Actions se registran
en la descripción del PR, separados de los resultados WSL anteriores.
Solo después del merge y de pasar todos los controles de main se ejecutará
el workflow reutilizable de Pages sobre ese commit.

Pendiente: revisar/fusionar, comprobar la publicación en Pages, disponer de una
API C++ pública HTTPS, configurar la variable y verificar CORS desde el origen
`https://yrojasar-ui.github.io`. Autenticación/autorización y alojamiento del
backend deben resolverse antes de exponer el perfil administrativo actual.
Pages no ejecuta C++, MongoDB ni RabbitMQ. No se contrataron servicios.
