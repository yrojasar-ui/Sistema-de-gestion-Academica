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

# Terminal 1 · RabbitMQ 4.0.5 real en loopback con carpeta propia de esta demo
bash scripts/arrancar_rabbitmq_wsl.sh "$DEMO_DIR/rabbitmq-vivo"

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

*(El script deja creados `Admin`, `Ana`, `Bruno` y `Carla`, pagos pendientes/pagados/anulados, préstamos vencido/próximo a vencer/vigentes/devueltos/anulado, reservas en los 4 estados, ≥ 8 avisos procesados por el worker, los ejemplares disponibles `E-DEF-5` (`L005`) y `E-DEF-6` (`L006`), y el cargo pendiente `P-DEF-VIG` de Ana listo para la demostración en vivo.)*

### Comprobación en vivo con dos ventanas (Ana y Admin)

1. Abrir **Ventana 1** (`http://127.0.0.1:5173/`) como `Admin` → **Pagos simulados** → filtrar por **Pagados**.
2. Abrir **Ventana 2** (ventana privada/incógnito en `http://127.0.0.1:5173/`) como `Ana` → **Mis pagos simulados** → pulsar **Simular pago** en `P-DEF-VIG` y confirmar.
3. Verificar en Ventana 2 que aparece *«Usted ha pagado. Pago simulado registrado.»* y anotar el identificador del comprobante emitido por el servidor (`PG-n`, conservado al recargar).
4. En Ventana 1 (`Admin`), pulsar **Actualizar**: el filtro **Pagados** se conserva y debe aparecer el **mismo comprobante** emitido en la ventana de Ana para `P-DEF-VIG` con *«Pago simulado realizado»*.

---

## Recorrido guiado paso a paso (5–7 minutos)

| Tiempo | Rol / Terminal | Acción concreta | Resultado esperado a señalar al jurado |
|---|---|---|---|
| **0:00–1:00** | Navegador · `Admin` | Iniciar sesión en `http://127.0.0.1:5173/`. En **Libros**, editar título y temas de `L005`, abrir **Ver árbol B+** y pulsar **Validar invariantes**. En **Ejemplares**, comprobar que `E-DEF-5` (`L005`) ya figura `disponible`. | El B+ refleja el título editado de `L005` en orden normalizado e informa *Invariantes válidas*; el grafo actualiza aristas; `E-DEF-5` está listo y `disponible` para el flujo Ana → Bruno. |
| **1:00–2:00** | Navegador · `Ana` | En Ventana 2 (o tras cerrar sesión) ingresar como `Ana`. Buscar `L005` por el prefijo del título editado y pulsar **Solicitar préstamo**. Abrir **Mi biblioteca** y luego **Mis avisos**. | La solicitud sobre `L005` queda `apartada` con `E-DEF-5` asignado (sin préstamo activo todavía). En **Mis avisos** aparece *Reserva disponible para retirar* procesada realmente por RabbitMQ + `biblioteca_worker` (visible en Terminal 3). |
| **2:00–3:00** | Dos ventanas · `Admin` y `Ana` | En `Admin` → **Solicitudes** → **Registrar entrega** para la solicitud apartada de `Ana` (`E-DEF-5`, vencimiento futuro). Luego en `Admin` → **Pagos simulados** seleccionar filtro **Pagados**. En `Ana` → **Mis pagos simulados** → **Simular pago** en `P-DEF-VIG` y confirmar; en `Admin` pulsar **Actualizar**. | El nuevo préstamo sobre `E-DEF-5` entra al Rojo-Negro y la copia pasa a `prestado`. `Ana` ve *«Usted ha pagado. Pago simulado registrado.»* con el comprobante emitido (`₡100,00 · SIMULADO`); `Admin` conserva el filtro **Pagados** y ve exactamente el **mismo comprobante** con *«Pago simulado realizado»*, sin liberar la copia. |
| **3:00–4:00** | Navegador · `Bruno` → `Admin` | Entrar como `Bruno` → buscar `L005` y pulsar **Solicitar préstamo** (queda `pendiente`, posición `#1` en FIFO de `L005`) y abrir **Mi biblioteca** y **Mis avisos**. Luego en `Admin` → **Préstamos** → **Registrar devolución** del préstamo de `Ana` sobre `E-DEF-5`. | Bruno **no ve** ningún préstamo, pago ni aviso de Ana (aislamiento). Al devolver `E-DEF-5`, Ana conserva su historial devuelto con fecha de cierre, el Rojo-Negro actualiza los activos, `E-DEF-5` pasa automáticamente a `apartada` para Bruno (FIFO) y el worker genera avisos para ambos. |
| **4:00–5:00** | Terminal 1 (RabbitMQ) + Navegador · `Admin` | Detener brevemente Terminal 1 (`Ctrl+C` en RabbitMQ) y registrar una renovación o solicitud; mostrar en **Avisos** que el broker aparece *Sin conexión temporal* y el aviso queda en *Pendiente de publicación*. Reiniciar Terminal 1 con `bash scripts/arrancar_rabbitmq_wsl.sh "$DEMO_DIR/rabbitmq-vivo"`. | Catálogo y préstamos siguen operativos sin RabbitMQ; no se simula éxito. Al volver RabbitMQ en su carpeta propia, `AvisoOutboxRelay` reconecta, publica y el worker procesa a *Procesado* automáticamente. |
| **5:00–6:00** | Terminal 2 (`catalogo_api`) + Navegador | Detener Terminal 2 con `Ctrl+C` y volver a ejecutar el mismo comando de arranque. Reingresar como `Ana` → **Mi biblioteca**, **Mis pagos simulados** y **Mis avisos**. | La sesión anterior queda inválida (exige login); historial, el comprobante emitido para `P-DEF-VIG` (*«Usted ha pagado. Pago simulado registrado.»*) y las notificaciones procesadas se conservan íntegros tras el reinicio. |
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
