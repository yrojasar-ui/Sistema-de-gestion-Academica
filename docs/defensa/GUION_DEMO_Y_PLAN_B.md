# Guion de demostración en vivo (5–7 minutos) y Plan B sin Internet

## Preparación previa a la defensa (2 minutos antes)

Desde la raíz `/home/yoerojas/proyectos/Sistema-de-gestion-Academica` en WSL, con `build/debug` compilado y Node `24.22.3` en `PATH`:

```bash
# 1) Generar una instancia aislada nueva con dataset completo (no modifica cuentas
#    personales ni la API docente y rechaza directorios existentes sin borrar nada):
DEMO_DIR="build/demo-defensa-$(date -u +%Y%m%d-%H%M%S)"
python3 scripts/preparar_demo_defensa.py --dir "$DEMO_DIR"
# Opcional: añadir --verify para comprobar dos sesiones, reinicio y aislamiento Bruno.
# Las contraseñas sintéticas exclusivas quedan únicamente en "$DEMO_DIR/privado/credenciales_demo.json" (0600).
```

Abrir **4 terminales WSL** apuntando al mismo `$DEMO_DIR` (sin imprimir contraseñas en pantalla):

```bash
# En cada terminal exportar la ruta generada:
export DEMO_DIR="build/demo-defensa-<AAAAMMDD-HHMMSS>"

# Terminal 1 · RabbitMQ 4.0.5 real en loopback (puerto 127.0.0.1:5672)
bash scripts/arrancar_rabbitmq_wsl.sh

# Terminal 2 · API C++17 (único escritor, con outbox RabbitMQ y token de servicio)
./build/debug/catalogo_api --storage local \
  --initial data/libros_iniciales.json \
  --working "$DEMO_DIR/datos/defensa-local.json" \
  --auth-store "$DEMO_DIR/privado/cuentas.json" \
  --service-token-file "$DEMO_DIR/privado/service.token" \
  --amqp-host 127.0.0.1 --amqp-port 5672 \
  --amqp-credentials-file "$DEMO_DIR/privado/amqp.creds" \
  --outbox-poll-ms 300 --demo-charge 10000 --currency CRC --port 8080

# Terminal 3 · Worker independiente C++17
./build/debug/biblioteca_worker --api-url http://127.0.0.1:8080 \
  --service-token-file "$DEMO_DIR/privado/service.token" \
  --amqp-host 127.0.0.1 --amqp-port 5672 \
  --amqp-credentials-file "$DEMO_DIR/privado/amqp.creds"

# Terminal 4 · Frontend Vite (http://127.0.0.1:5173/)
export PATH="$PWD/build/catalogo-prep/node-v24.22.3-linux-x64/bin:$PATH"
cd frontend && npm run dev
```

*(El script deja creados `Admin`, `Ana`, `Bruno` y `Carla`, pagos pendientes/pagados/anulados, préstamos vencido/próximo a vencer/vigentes/devueltos/anulado, reservas en los 4 estados, ≥ 8 avisos procesados por el worker, los ejemplares disponibles `E-DEF-5` y `E-DEF-6`, y el cargo pendiente `P-DEF-VIG` de Ana listo para la demostración en vivo.)*

### Comprobación en vivo con dos ventanas (Ana y Admin)

1. Abrir **Ventana 1** (`http://127.0.0.1:5173/`) como `Admin` → **Pagos simulados** → filtrar por **Pagados**.
2. Abrir **Ventana 2** (ventana privada/incógnito en `http://127.0.0.1:5173/`) como `Ana` → **Mis pagos simulados** → pulsar **Simular pago** en `P-DEF-VIG` y confirmar.
3. Verificar en Ventana 2 que aparece *«Usted ha pagado. Pago simulado registrado.»* y el nuevo comprobante (`PG-4`, conservado al recargar).
4. En Ventana 1 (`Admin`), pulsar **Actualizar**: el filtro **Pagados** se conserva y aparece el mismo comprobante `PG-4` con *«Pago simulado realizado»*.

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
