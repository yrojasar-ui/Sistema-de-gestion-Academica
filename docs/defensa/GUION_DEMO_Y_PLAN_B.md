# Guion de demostración en vivo (5–7 minutos) y Plan B sin Internet

## Preparación previa a la defensa (2 minutos antes)

Desde la raíz `/home/yoerojas/proyectos/Sistema-de-gestion-Academica` en WSL, con `build/debug` compilado y Node `24.22.3` en `PATH`:

```bash
# Preparación única (solo si no existe el archivo de demo y credenciales):
mkdir -p data/privado-demo && chmod 700 data/privado-demo
./build/debug/catalogo_api --bootstrap-service-token data/privado-demo/service.token
printf 'user=guest\npassword=guest\nvhost=/\n' > data/privado-demo/amqp.creds
chmod 600 data/privado-demo/amqp.creds
./build/debug/catalogo_api --storage local \
  --initial data/libros_iniciales.json --working data/demo-defensa.json \
  --auth-store data/privado-demo/cuentas.json \
  --bootstrap-admin Admin --admin-name "Administrador"
```

Abrir **4 terminales WSL** (sin imprimir contraseñas en pantalla):

```bash
# Terminal 1 · RabbitMQ 4.0.5 real en loopback (persiste en data/rabbitmq-wsl)
bash scripts/arrancar_rabbitmq_wsl.sh

# Terminal 2 · API C++17 (único escritor, con outbox RabbitMQ y token de servicio)
./build/debug/catalogo_api --storage local \
  --working data/demo-defensa.json --auth-store data/privado-demo/cuentas.json \
  --service-token-file data/privado-demo/service.token \
  --amqp-host 127.0.0.1 --amqp-port 5672 --amqp-credentials-file data/privado-demo/amqp.creds \
  --demo-charge 10000 --currency CRC --port 8080

# Terminal 3 · Worker independiente C++17
./build/debug/biblioteca_worker --api-url http://127.0.0.1:8080 \
  --service-token-file data/privado-demo/service.token \
  --amqp-host 127.0.0.1 --amqp-port 5672 --amqp-credentials-file data/privado-demo/amqp.creds

# Terminal 4 · Frontend Vite (http://127.0.0.1:5173/)
export PATH="$PWD/build/catalogo-prep/node-v24.22.3-linux-x64/bin:$PATH"
cd frontend && npm run dev
```

*(Tener creados previamente los usuarios sintéticos `Ana` y `Bruno` con sus cuentas `USUARIO` desde Admin → Usuarios, usando contraseñas propias de al menos 12 caracteres.)*

---

## Recorrido guiado paso a paso (5–7 minutos)

| Tiempo | Rol / Terminal | Acción concreta | Resultado esperado a señalar al jurado |
|---|---|---|---|
| **0:00–1:00** | Navegador · `Admin` | Iniciar sesión en `http://127.0.0.1:5173/`. En **Libros**, editar título y temas de `L001`, abrir **Ver árbol B+** y pulsar **Validar invariantes**. En **Ejemplares**, crear `E-DEF-1` para `L001`. | El B+ refleja el título editado en orden normalizado e informa *Invariantes válidas*; el grafo actualiza aristas; el ejemplar figura `disponible`. |
| **1:00–2:00** | Navegador · `Ana` | Cerrar sesión e ingresar como `Ana`. Buscar la obra editada por prefijo y pulsar **Solicitar préstamo**. Abrir **Mi biblioteca** y luego **Mis avisos**. | La solicitud queda `apartada` (sin préstamo activo todavía). En **Mis avisos** aparece *Reserva disponible para retirar* procesada realmente por RabbitMQ + `biblioteca_worker` (visible en los logs de Terminal 3). |
| **2:00–3:00** | Navegador · `Admin` → `Ana` | Ingresar como `Admin` → **Solicitudes** → **Registrar entrega** para Ana (vencimiento futuro). Volver a entrar como `Ana` → **Mis pagos simulados** → **Simular pago**. | Préstamo activo entra al Rojo-Negro; copia pasa a `prestado`. Ana obtiene comprobante único `PG-1` (`₡100,00 · SIMULADO`), con aviso de simulación y sin liberar la copia. |
| **3:00–4:00** | Navegador · `Bruno` → `Admin` | Entrar como `Bruno` → solicitar el mismo libro (queda `pendiente`, turno siguiente en FIFO) y abrir **Mi biblioteca** y **Mis avisos**. Luego entrar como `Admin` → **Préstamos** → **Registrar devolución** del préstamo de Ana. | Bruno **no ve** ningún préstamo, pago ni aviso de Ana (aislamiento). Al devolver, Ana conserva su historial devuelto, el Rojo-Negro se vacía, la copia pasa automáticamente a `apartada` para Bruno (FIFO) y el worker genera avisos para ambos. |
| **4:00–5:00** | Terminal 1 (RabbitMQ) + Navegador · `Admin` | Detener brevemente Terminal 1 (`Ctrl+C` en RabbitMQ) y registrar otra acción (o mostrar en **Avisos** que el broker aparece *Sin conexión temporal* y el aviso queda en *Pendiente de publicación*). Reiniciar Terminal 1. | Catálogo y préstamos siguen operativos sin RabbitMQ; no se simula éxito. Al volver RabbitMQ, `AvisoOutboxRelay` reconecta, publica y el worker procesa a *Procesado* automáticamente. |
| **5:00–6:00** | Terminal 2 (`catalogo_api`) + Navegador | Detener Terminal 2 con `Ctrl+C` y volver a ejecutar el mismo comando de arranque. Entrar como `Ana` → **Mi biblioteca** y **Mis avisos**. | La sesión anterior queda inválida (exige login); historial, comprobante `PG-1` y notificaciones procesadas se conservan íntegros tras el reinicio. |
| **6:00–7:00** | Navegador · `Admin` | Abrir **Estructuras** → pulsar **Validar estructuras** y recorrer los snapshots JSON de B+, Rojo-Negro, Hash Extensible, Grafo y FIFO. Abrir **Avisos** para enseñar los 4 estados y el conteo final. | Mensaje *Invariantes válidas en las cinco estructuras*; snapshots reales emitidos por C++. |

---

## Plan alternativo 100 % local si falla Internet (Plan B)

Todo el flujo anterior **no depende de Internet**:
1. **RabbitMQ 4.0.5**, **`catalogo_api` (`--storage local`)**, **`biblioteca_worker`** y **Vite** operan íntegramente sobre `127.0.0.1` en WSL.
2. **Para evidenciar MongoDB cloud sin Internet:** mostrar los artefactos reales ya capturados y versionados en `docs/evidencias/final/mongodb-nube/nube-final.log`, `checkpoint-final.json` y `worker.log` (colección `biblioteca_yoel_final_3be394f489e7431c`, revisión 40, 10/10 avisos procesados y limpieza `total=0`), y ejecutar en vivo la suite de fixtures deterministas offline:
   ```bash
   ctest --test-dir build/debug -R '^(mongo_biblioteca_recuperacion|outbox_unidad|rabbitmq_worker_real)$' -V
   ```
3. **Para evidenciar GitHub Pages sin Internet:** ejecutar localmente el preview de producción con la subruta exacta del repositorio y mostrar la captura real [08-github-pages-publicado.png](../evidencias/final/navegador/08-github-pages-publicado.png):
   ```bash
   cd frontend && VITE_API_URL= npm run build && npm run verify:dist && npm run preview -- --port 4173
   # Abrir http://127.0.0.1:4173/Sistema-de-gestion-Academica/
   ```
4. **Si fallara el navegador:** ejecutar las demos automatizadas por consola (`bash scripts/demo_acceso.sh build/debug/catalogo_api` y `ctest --test-dir build/debug -R '^rabbitmq_worker_real$' -V`).
