# Diagramas editables — Biblioteca Académica (Entrega Final)

Cada diagrama se conserva en dos formatos editables:
1. Fuente **Mermaid** (`.mmd`) editable en cualquier editor compatible.
2. Vectorial **SVG** (`.svg`) limpio, de alta resolución y reproducible sin conexión para el informe PDF y la presentación.

## Índice
- `01-arquitectura-componentes.svg` / `.mmd`: Arquitectura por componentes (React/Vite, API C++ escritor único, AuthService, 5 estructuras, Outbox Relay, RabbitMQ 4.0.5, Worker C++17, JSON local y MongoDB).
- `02-casos-uso-clases.svg` / `.mmd`: Casos de uso por rol (`ADMIN`, `USUARIO`, `SERVICIO`) y modelo de clases de dominio v2 (`Libro`, `Ejemplar`, `Usuario`, `Prestamo`, `Reserva`, `Aviso`, `Notificacion`, `Cargo`, `Comprobante`, `CuentaPrivada`).
- `03-estructuras-entidades.svg` / `.mmd`: Correspondencia entre las 5 estructuras académicas C++17 propias y las entidades/consultas de negocio.
- `04-secuencia-prestamo-asincrona.svg` / `.mmd`: Secuencia completa de devolución, asignación FIFO, persistencia confirmada, publicación outbox con publisher confirm, consumo del worker, confirmación interna idempotente y ACK manual.
- `05-despliegue.svg` / `.mmd`: Topología de despliegue (GitHub Pages estático, proxy inverso HTTPS con `trusted-proxy` y bloqueo `/api/interno/*`, red privada con API escritor único, RabbitMQ durable, Worker C++17 y almacenamiento persistente / MongoDB cloud).
