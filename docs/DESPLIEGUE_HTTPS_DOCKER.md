# Despliegue, Docker Compose y preparación HTTPS

Este documento detalla la ejecución con Docker Compose, el arranque nativo en WSL con RabbitMQ real, el estado verificado de GitHub Pages y el procedimiento listo para publicar el backend detrás de un proxy inverso HTTPS.

## 1. Estado real de GitHub Pages y backend HTTPS

| Componente | Estado real comprobado (20 de noviembre de 2026) | Evidencia y límites |
|---|---|---|
| Frontend estático en GitHub Pages | **Publicado y accesible** (`HTTP/2 200` en `https://yrojasar-ui.github.io/Sistema-de-gestion-Academica/`), sirviendo `index.html`, JS y CSS bajo `/Sistema-de-gestion-Academica/assets/`. | **Frontend publicado ≠ aplicación remota funcional.** Pages solo sirve archivos estáticos (no ejecuta C++ ni RabbitMQ). Sin `VITE_API_URL`, muestra *"Servidor de biblioteca pendiente de configuración"* y no intenta conectar a `localhost`. |
| Backend C++17 + RabbitMQ + Worker en nube HTTPS | **Pendiente de decisión de alojamiento/proveedor.** | No existe un servidor remoto contratado ni se creó ningún servicio de pago ni una URL pública ficticia. Todo el código, CORS, proxy inverso, `trusted-proxy`, bloqueo de `/api/interno/*` y volúmenes persistentes quedan preparados y probados localmente. |

### Decisión necesaria para habilitar la aplicación remota

Para pasar de **frontend publicado** a **aplicación remota funcional** hace falta:
1. Elegir un servidor Linux con HTTPS público y volumen persistente (por ejemplo un VPS con Caddy/Nginx, una VM académica o un servicio de contenedores con disco persistente) manteniendo **una única instancia escritora** (`replicas: 1`).
2. Configurar DNS y certificado TLS (ver [deploy/Caddyfile.https.example](../deploy/Caddyfile.https.example) o [deploy/nginx-https.conf.example](../deploy/nginx-https.conf.example)).
3. Arrancar `catalogo_api` en loopback (`127.0.0.1:8080`) con `--origin https://yrojasar-ui.github.io --trusted-proxy 127.0.0.1` y conservar en volumen privado el almacén `cuentas.json`, `service.token`, el negocio/diario Mongo y los datos de RabbitMQ.
4. Configurar la variable pública `VITE_API_URL=https://<su-dominio-api>` en **Settings → Secrets and variables → Actions → Variables** (sin `/api` al final) y reejecutar el workflow de `main` para recompilar Pages.

## 2. Garantías de red y seguridad detrás del proxy

- **CORS exacto:** `--origin https://yrojasar-ui.github.io` (sin subruta), permitiendo `GET, POST, PUT, DELETE, OPTIONS` y cabeceras `Content-Type, Authorization`.
- **Sesiones en RAM:** el navegador conserva el token Bearer únicamente en memoria (sin cookies ni `localStorage`).
- **Rate-limit por IP real sin confiar en cabeceras arbitrarias:** `catalogo_api` ignora `X-Forwarded-For` / `X-Real-IP` por defecto. Solo cuando la conexión TCP proviene de una IP listada en `--trusted-proxy`, toma la IP del cliente más a la derecha fuera de los proxies confiables. Así los usuarios detrás de Nginx/Caddy no comparten el mismo límite de intentos de login por IP, y un cliente directo no puede falsificar su IP.
- **Superficie interna cerrada:** tanto `HttpController` (mediante `ServiceIdentity` con token `0600` independiente de cuentas humanas) como las plantillas Nginx/Caddy bloquean `/api/interno/*` desde Internet (`403`). RabbitMQ (`5672` / `15672`) permanece en red interna/loopback y nunca se publica a Internet.

## 3. Docker y Docker Compose (demostración local)

Archivos preparados:
- [deploy/Dockerfile.backend](../deploy/Dockerfile.backend): multi-stage `ubuntu:24.04`, verifica `third_party/SHA256SUMS`, compila `libsodium 1.0.22` y `rabbitmq-c 0.15.0` fijados, construye `catalogo_api` y `biblioteca_worker`, ejecuta como usuario sin privilegios `biblioteca` (`UID 10001`).
- [deploy/Dockerfile.frontend](../deploy/Dockerfile.frontend): multi-stage `node:24.22.3-slim` + `nginxinc/nginx-unprivileged:1.27-alpine` sirviendo la SPA y proxying `/api/` hacia `api:8080` (bloqueando `/api/interno/`).
- [docker-compose.yml](../docker-compose.yml): levanta `rabbitmq` (`4.0-alpine`), `api` (`replicas: 1`), `worker` (monta `biblioteca_privado` en solo lectura `:ro`) y `web` en `127.0.0.1:5173`.
- [.env.example](../.env.example): variables de ejemplo sin secretos reales.

> **Estado en este entorno WSL:** Docker Desktop está instalado en Windows, pero la integración WSL2 y el daemon Linux no estaban activos durante esta ejecución (`dockerDesktopLinuxEngine` no disponible). Por ello los manifiestos se validaron sintácticamente y por inspección contra el build nativo, **sin inventar una ejecución de contenedores**.

### Comandos de uso cuando Docker esté activo

```bash
cp .env.example .env
# Editar .env y colocar una contraseña local para RABBITMQ_DEFAULT_PASS (nunca subir .env a Git).

# 1. Compilar imágenes
docker compose build

# 2. Aprovisionamiento único (token de servicio 0600 y primer ADMIN, conservado en volumen biblioteca_privado)
docker compose run --rm --no-deps api --bootstrap-service-token /var/lib/biblioteca/privado/service.token
docker compose run --rm --no-deps api --storage local \
  --initial /app/data/libros_iniciales.json \
  --working /var/lib/biblioteca/datos/biblioteca-local.json \
  --auth-store /var/lib/biblioteca/privado/cuentas.json \
  --bootstrap-admin Admin --admin-name "Administrador"

# 3. Arrancar toda la pila (interfaz en http://127.0.0.1:5173/)
docker compose up -d
docker compose ps

# 4. Detener o reiniciar SIN borrar volúmenes (NUNCA usar down -v en demostraciones normales)
docker compose stop
docker compose up -d
```

## 4. Arranque nativo en WSL (conservado y verificado)

Se conserva íntegro el arranque en dos terminales (API + Vite) y se añaden dos procesos opcionales para mensajería RabbitMQ real + worker C++:

```bash
# Terminal 1 · RabbitMQ real en loopback (conserva datos en data/rabbitmq-wsl)
bash scripts/arrancar_rabbitmq_wsl.sh

# Preparación única de credenciales privadas (0600, nunca en Git)
./build/debug/catalogo_api --bootstrap-service-token data/privado/service.token
printf 'user=guest\npassword=guest\nvhost=/\n' > data/privado/amqp.creds && chmod 600 data/privado/amqp.creds

# Terminal 2 · API C++17 (único escritor)
./build/debug/catalogo_api --storage local \
  --working data/biblioteca-local.json --auth-store data/privado/cuentas.json \
  --service-token-file data/privado/service.token \
  --amqp-host 127.0.0.1 --amqp-port 5672 --amqp-credentials-file data/privado/amqp.creds \
  --demo-charge 10000 --currency CRC --port 8080

# Terminal 3 · Worker independiente C++17
./build/debug/biblioteca_worker --api-url http://127.0.0.1:8080 \
  --service-token-file data/privado/service.token \
  --amqp-host 127.0.0.1 --amqp-port 5672 --amqp-credentials-file data/privado/amqp.creds

# Terminal 4 · Frontend Vite (http://127.0.0.1:5173/)
cd frontend && npm run dev
```
