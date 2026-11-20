# GitHub Actions

[Controles del PR #11](https://github.com/yrojasar-ui/Sistema-de-gestion-Academica/pull/11/checks)
muestra la ejecución del HEAD actual. La rama es feat/acceso-paneles-biblioteca
y el destino main; el PR se mantiene sin fusionar.

El workflow Biblioteca Académica compila Debug y ASan/UBSan, ejecuta todos los
CTest y valida frontend con npm ci, npm test, npm run build y verify:dist.
libsodium se descarga por versión/SHA y ejecuta sus propias pruebas.
Todos los datos de CI son temporales o fixtures: no se escribe en API docente.

Pages queda omitido porque es un pull request. El despliegue estático solo se
habilita para push a main después de controles exitosos. No se despliega C++.

Los logs de esta carpeta son de WSL. El enlace de ejecución y resultado
observado en GitHub se entrega en el PR y al finalizar la tarea, separado de
estos resultados locales.
