# Evidencias de la entrega final — Biblioteca Académica

Fecha de ejecución: **20 de noviembre de 2026** (America/Costa_Rica / America/Guatemala).
Rama: `feat/cierre-final-biblioteca` · Entorno: **Ubuntu 26.04.1 LTS en WSL2 x86_64** (`g++ 15.2.0`, `CMake 4.2.3`, `Node 24.22.3`, `Python 3.14.4`, `RabbitMQ 4.0.5` sobre Erlang/OTP 27, `Chrome 157.0.8103.80`).
Catálogo inicial `data/libros_iniciales.json` conservado intacto (`SHA-256: 7852aadb1c01f71840aa34e8cf828495caf3ca195c88d656019d6ff702cffce5`).

## 1. Resumen de comprobaciones ejecutadas

| Verificación | Tipo | Resultado real | Evidencia |
|---|---|---|---|
| CTest Debug (16/16 pruebas) | Local + RabbitMQ real + fixtures Mongo | **16/16 aprobadas** en 48,87 s (código 0) | [debug-ctest.log](pruebas/debug-ctest.log) |
| CTest ASan/UBSan + detección de fugas (16/16) | Local + RabbitMQ real + sanitizers | **16/16 aprobadas** en 181,52 s, **0 diagnósticos** (código 0) | [asan-ctest.log](pruebas/asan-ctest.log), [asan-build.log](pruebas/asan-build.log) |
| Frontend (`npm test`, `tsc`, `vite build`, `verify:dist`) | Local Node 24.22.3 | **19/19 pruebas aprobadas**, build y subruta Pages OK | [frontend-y-entorno.log](pruebas/frontend-y-entorno.log) |
| Integración real MongoDB cloud + RabbitMQ + Worker | **Nube real** (colección sintética `biblioteca_yoel_final_3be394f489e7431c`) | Revisión 40, **10/10 avisos procesados** por worker vía escritor único, reinicio real íntegro, **0 secretos** en Mongo, limpieza confirmada (`total=0`) | [nube-final.log](mongodb-nube/nube-final.log), [checkpoint-final.json](mongodb-nube/checkpoint-final.json), [worker.log](mongodb-nube/worker.log) |
| Demostración en navegador real (escritorio + móvil + Pages) | **Navegador Chrome 157** contra API + RabbitMQ + Worker + Vite | Flujo Admin/Ana/Bruno verificado, aislamiento comprobado, 8 capturas auténticas | [navegador.log](navegador/navegador.log), capturas abajo |
| GitHub Pages publicado | **Sitio estático remoto** (`https://yrojasar-ui.github.io/Sistema-de-gestion-Academica/`) | `HTTP/2 200` en HTML y assets bajo `/Sistema-de-gestion-Academica/`; sin `VITE_API_URL` muestra *"Servidor de biblioteca pendiente de configuración"* | [08-github-pages-publicado.png](navegador/08-github-pages-publicado.png) |

## 2. Capturas auténticas de navegador (Chrome 157.0.8103.80)

- [01 — Admin: Catálogo y árbol B+ validado (1280×800)](navegador/01-admin-catalogo-bplus-desktop.png)
- [02 — Ana: Mis avisos procesados por RabbitMQ + worker (1280×800)](navegador/02-ana-mis-avisos-desktop.png)
- [03 — Ana: Mis pagos simulados y comprobante PG-1 (1280×800)](navegador/03-ana-pagos-simulados-desktop.png)
- [04 — Admin: Panel de Avisos y mensajería RabbitMQ (1280×800)](navegador/04-admin-avisos-rabbitmq-desktop.png)
- [05 — Admin: Estructuras reales y validación de los cinco índices (1280×800)](navegador/05-admin-estructuras-desktop.png)
- [06 — Ana: Mis avisos en vista móvil (390×844)](navegador/06-ana-mis-avisos-movil.png)
- [07 — Admin: Panel de Avisos en vista móvil (390×844)](navegador/07-admin-avisos-movil.png)
- [08 — GitHub Pages publicado sin backend remoto configurado (1280×800)](navegador/08-github-pages-publicado.png)

## 3. Distinción explícita de alcances y límites

- **Broker real:** las 11 garantías asincrónicas (`rabbitmq_worker_real`), el recorrido de navegador y la prueba cloud MongoDB ejecutaron contra **RabbitMQ 4.0.5 real**.
- **Nube real vs. fixtures:** la persistencia v2 + avisos + worker + reinicio + limpieza se comprobó en la API docente real; timeouts, escrituras parciales, HTML ambiguo y bloqueos de recuperación se probaron con **fixtures deterministas** (`mongo_biblioteca_recuperacion`), sin inducir fallos en el servicio compartido.
- **Docker:** `docker-compose.yml` y los `Dockerfile` se prepararon y validaron sintácticamente, pero el daemon de Docker Desktop no estaba activo en WSL (`dockerDesktopLinuxEngine` no disponible); no se reporta una ejecución de contenedores.
- **Backend HTTPS remoto:** pendiente de elegir alojamiento con TLS y volumen persistente (ver [DESPLIEGUE_HTTPS_DOCKER.md](../../DESPLIEGUE_HTTPS_DOCKER.md)). GitHub Pages sirve únicamente el frontend estático.
