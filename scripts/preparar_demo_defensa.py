#!/usr/bin/env python3
"""Prepara una instancia aislada y repetible de Biblioteca Académica para la defensa.

Características:
- Nunca modifica cuentas existentes ni la API docente MongoDB.
- Genera datos sintéticos completos:
  * Pagos pendientes (1 reservado para la demo en vivo), pagados y anulado con comprobante histórico.
  * Préstamos: vencido (fixture histórico válido sin relajar altas), próximo a vencer, vigente,
    dos devoluciones y una anulación con motivo.
  * Reservas: pendientes (con cola FIFO), apartada, atendida (con fecha de cierre) y cancelada (con motivo).
  * Al menos 8 avisos procesados realmente por biblioteca_worker vía RabbitMQ.
  * Ejemplares disponibles (E-DEF-5 y E-DEF-6) y un cargo pendiente (P-DEF-VIG) listos para vivo.
- Con --verify comprueba el flujo de dos sesiones (Ana paga, Admin ve el mismo comprobante,
  Ana lo vuelve a ver tras recargar y reiniciar la API, y Bruno no accede a pagos ni avisos de Ana).
"""
import argparse
import json
import os
import secrets
import shutil
import signal
import socket
import subprocess
import sys
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "backend" / "tests"))


def free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def find_rabbitmq():
    local_rt = REPO_ROOT / "build" / "deps" / "rabbitmq-runtime"
    local_sbin = local_rt / "usr" / "lib" / "rabbitmq" / "lib"
    if local_sbin.exists():
        matches = sorted(local_sbin.glob("rabbitmq_server-*/sbin/rabbitmq-server"))
        if matches:
            sbin = matches[-1].parent
            erl_bin = local_rt / "usr" / "lib" / "erlang" / "bin"
            return str(sbin / "rabbitmq-server"), str(sbin / "rabbitmqctl"), f"{erl_bin}:{sbin}:{local_rt / 'usr' / 'bin'}"
    for srv_p, ctl_p in [(Path("/usr/lib/rabbitmq/bin/rabbitmq-server"), Path("/usr/lib/rabbitmq/bin/rabbitmqctl"))]:
        if srv_p.exists() and ctl_p.exists():
            return str(srv_p), str(ctl_p), str(srv_p.parent)
    raise RuntimeError("RabbitMQ no disponible en el entorno local")


class IsolatedBroker:
    def __init__(self, base_dir: Path, port: int):
        self.base_dir = base_dir
        self.port = port
        self.epmd_port = free_port()
        self.dist_port = free_port()
        self.nodename = f"bib_defensa_{port}@localhost"
        self.srv, self.ctl, self.extra_path = find_rabbitmq()
        self.proc = None
        self.log_path = base_dir / "rabbitmq.log"
        self.env = os.environ.copy()
        if self.extra_path:
            self.env["PATH"] = self.extra_path + ":" + self.env.get("PATH", "")
        for sub in ("home", "mnesia", "log", "etc", "plugins", "schema", "generated"):
            (base_dir / sub).mkdir(parents=True, exist_ok=True)
        home = base_dir / "home"
        home.chmod(0o700)
        cookie = home / ".erlang.cookie"
        if not cookie.exists():
            cookie.write_text(secrets.token_hex(16))
            cookie.chmod(0o400)
        cfg = base_dir / "etc" / "rabbitmq.conf"
        cfg.write_text(f"listeners.tcp.1 = 127.0.0.1:{port}\nloopback_users = none\n")
        env_f = base_dir / "etc" / "rabbitmq-env.conf"
        env_f.write_text("")
        plg = base_dir / "etc" / "enabled_plugins"
        plg.write_text("[].\n")
        self.env.update({
            "HOME": str(home),
            "ERL_EPMD_ADDRESS": "127.0.0.1",
            "ERL_EPMD_PORT": str(self.epmd_port),
            "RABBITMQ_NODENAME": self.nodename,
            "RABBITMQ_NODE_IP_ADDRESS": "127.0.0.1",
            "RABBITMQ_NODE_PORT": str(port),
            "RABBITMQ_DIST_PORT": str(self.dist_port),
            "RABBITMQ_MNESIA_BASE": str(base_dir / "mnesia"),
            "RABBITMQ_LOG_BASE": str(base_dir / "log"),
            "RABBITMQ_CONFIG_FILE": str(cfg),
            "RABBITMQ_CONF_ENV_FILE": str(env_f),
            "RABBITMQ_ENABLED_PLUGINS_FILE": str(plg),
            "RABBITMQ_PLUGINS_EXPAND_DIR": str(base_dir / "plugins"),
            "RABBITMQ_PID_FILE": str(base_dir / "rabbitmq.pid"),
            "RABBITMQ_SCHEMA_DIR": str(base_dir / "schema"),
            "RABBITMQ_GENERATED_CONFIG_DIR": str(base_dir / "generated"),
            "RABBITMQ_FEATURE_FLAGS_FILE": str(base_dir / "feature_flags"),
        })

    def start(self):
        with open(self.log_path, "ab") as logf:
            self.proc = subprocess.Popen([self.srv], env=self.env, stdout=logf, stderr=logf, start_new_session=True)
        deadline = time.monotonic() + 45.0
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                raise RuntimeError("RabbitMQ terminó inesperadamente:\n" + self.log_path.read_text(errors="replace")[-2000:])
            chk = subprocess.run([self.ctl, "-n", self.nodename, "await_startup"],
                                 env=self.env, capture_output=True, text=True, timeout=15)
            if chk.returncode == 0:
                return
            time.sleep(0.3)
        raise RuntimeError("Timeout esperando RabbitMQ de defensa")

    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                subprocess.run([self.ctl, "-n", self.nodename, "stop"], env=self.env, capture_output=True, timeout=20)
                self.proc.wait(timeout=15)
            except Exception:
                try:
                    os.killpg(self.proc.pid, signal.SIGTERM)
                    self.proc.wait(timeout=5)
                except Exception:
                    try:
                        os.killpg(self.proc.pid, signal.SIGKILL)
                    except OSError:
                        pass
        self.proc = None


def wait_until(fn, timeout=15.0, msg="condición"):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            v = fn()
            if v:
                return v
        except Exception as e:
            last = e
        time.sleep(0.15)
    raise AssertionError(f"Timeout esperando {msg}: {last}")


def validate_prerequisites(api_bin: Path, worker_bin: Path, initial_json: Path) -> None:
    missing = []
    if not api_bin.is_file() or not os.access(api_bin, os.X_OK):
        missing.append(f"Binario catalogo_api ausente o no ejecutable: {api_bin}")
    if not worker_bin.is_file() or not os.access(worker_bin, os.X_OK):
        missing.append(f"Binario biblioteca_worker ausente o no ejecutable: {worker_bin}")
    if not initial_json.is_file():
        missing.append(f"Catálogo inicial ausente: {initial_json}")
    try:
        srv, ctl, _ = find_rabbitmq()
        if not os.access(srv, os.X_OK) or not os.access(ctl, os.X_OK):
            missing.append(f"Binarios de RabbitMQ no ejecutables: {srv}, {ctl}")
    except RuntimeError as exc:
        missing.append(str(exc))
    if missing:
        raise RuntimeError(
            "Validación previa de binarios falló (no se creó ni modificó ningún directorio):\n  - "
            + "\n  - ".join(missing)
            + "\nCompile primero con: cmake --build build/debug --parallel 4"
        )


def resolve_new_output_dir(raw_dir: str | None) -> Path:
    if raw_dir:
        candidate = Path(raw_dir).resolve()
        if candidate.exists() or candidate.is_symlink():
            raise FileExistsError(
                f"El directorio destino ya existe y no se sobrescribe ni borra: {candidate}. "
                "Indique una ruta nueva con --dir o omita --dir para crear un directorio único."
            )
        return candidate
    stamp = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
    for _ in range(16):
        candidate = (REPO_ROOT / "build" / f"demo-defensa-{stamp}-{secrets.token_hex(2)}").resolve()
        if not candidate.exists():
            return candidate
    raise RuntimeError("No se pudo reservar un directorio nuevo bajo build/")


def main():
    parser = argparse.ArgumentParser(description="Prepara instancia aislada de defensa con datos completos sin borrar directorios existentes.")
    parser.add_argument("--dir", default=None, help="Directorio NUEVO (no existente) para la instancia aislada; si se omite se crea uno único en build/")
    parser.add_argument("--api-bin", default=str(REPO_ROOT / "build" / "debug" / "catalogo_api"))
    parser.add_argument("--worker-bin", default=str(REPO_ROOT / "build" / "debug" / "biblioteca_worker"))
    parser.add_argument("--verify", action="store_true", help="Ejecutar verificación de dos sesiones, reinicio y aislamiento Bruno")
    args = parser.parse_args()

    api_bin = Path(args.api_bin).resolve()
    worker_bin = Path(args.worker_bin).resolve()
    initial_json = REPO_ROOT / "data" / "libros_iniciales.json"

    try:
        # Validar todos los binarios y el destino ANTES de crear cualquier archivo o directorio.
        validate_prerequisites(api_bin, worker_bin, initial_json)
        out_dir = resolve_new_output_dir(args.dir)
    except (FileExistsError, RuntimeError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(2)

    out_dir.mkdir(parents=True, exist_ok=False)
    priv = out_dir / "privado"
    priv.mkdir(mode=0o700, parents=True, exist_ok=True)
    os.chmod(priv, 0o700)
    datos = out_dir / "datos"
    datos.mkdir(parents=True, exist_ok=True)
    logs = out_dir / "logs"
    logs.mkdir(parents=True, exist_ok=True)

    working = datos / "defensa-local.json"
    auth_store = priv / "cuentas.json"
    svc_token = priv / "service.token"
    amqp_creds = priv / "amqp.creds"
    creds_file = priv / "credenciales_demo.json"

    passwords = {
        "Admin": "DefAdm-" + secrets.token_urlsafe(12),
        "Ana": "DefAna-" + secrets.token_urlsafe(12),
        "Bruno": "DefBru-" + secrets.token_urlsafe(12),
        "Carla": "DefCar-" + secrets.token_urlsafe(12),
    }
    creds_file.write_text(json.dumps(passwords, indent=2) + "\n")
    os.chmod(creds_file, 0o600)
    amqp_creds.write_text("user=guest\npassword=guest\nvhost=/\n")
    os.chmod(amqp_creds, 0o600)

    subprocess.run([str(api_bin), "--bootstrap-service-token", str(svc_token)],
                   text=True, capture_output=True, check=True)
    os.chmod(svc_token, 0o600)
    subprocess.run([
        str(api_bin), "--storage", "local", "--initial", str(initial_json),
        "--working", str(working), "--auth-store", str(auth_store),
        "--bootstrap-admin", "Admin", "--admin-name", "Administrador Defensa"
    ], input=passwords["Admin"] + "\n", text=True, capture_output=True, check=True)

    api_port = free_port()
    rmq_port = free_port()
    base = f"http://127.0.0.1:{api_port}"
    broker = IsolatedBroker(out_dir / "rabbitmq", rmq_port)
    api_log = open(logs / "api.log", "w+")
    worker_log = open(logs / "worker.log", "w+")
    api_proc = None
    worker_proc = None

    def call(path, method="GET", body=None, token="", status=200):
        h = {"Content-Type": "application/json"}
        if token:
            h["Authorization"] = "Bearer " + token
        data = None if body is None else json.dumps(body).encode()
        try:
            resp = urlopen(Request(base + path, data=data, method=method, headers=h), timeout=10)
        except HTTPError as err:
            resp = err
        with resp:
            raw = resp.read()
            val = json.loads(raw) if raw else None
            assert resp.status == status, f"{method} {path}: {resp.status} != {status}: {val}"
            return val

    def start_api():
        nonlocal api_proc
        api_proc = subprocess.Popen([
            str(api_bin), "--storage", "local", "--initial", str(initial_json),
            "--working", str(working), "--auth-store", str(auth_store),
            "--service-token-file", str(svc_token),
            "--amqp-host", "127.0.0.1", "--amqp-port", str(rmq_port),
            "--amqp-credentials-file", str(amqp_creds),
            "--outbox-poll-ms", "200", "--demo-charge", "10000", "--currency", "CRC",
            "--port", str(api_port),
        ], stdout=api_log, stderr=api_log)
        wait_until(lambda: call("/readyz")["listo"] is True, timeout=10, msg="API readyz")

    def stop_api():
        nonlocal api_proc
        if api_proc and api_proc.poll() is None:
            api_proc.terminate()
            api_proc.wait(timeout=10)
        api_proc = None

    def start_worker():
        nonlocal worker_proc
        worker_proc = subprocess.Popen([
            str(worker_bin), "--api-url", base, "--service-token-file", str(svc_token),
            "--amqp-host", "127.0.0.1", "--amqp-port", str(rmq_port),
            "--amqp-credentials-file", str(amqp_creds),
            "--max-retries", "3", "--backoff-ms", "200",
        ], stdout=worker_log, stderr=worker_log)

    def stop_worker():
        nonlocal worker_proc
        if worker_proc and worker_proc.poll() is None:
            worker_proc.terminate()
            worker_proc.wait(timeout=10)
        worker_proc = None

    def login(u):
        return call("/api/sesion/login", "POST", {"idUsuario": u, "password": passwords[u]})["token"]

    today_utc = datetime.now(timezone.utc).date()
    due_soon = (today_utc + timedelta(days=2)).isoformat()
    due_valid_1 = (today_utc + timedelta(days=12)).isoformat()
    due_valid_2 = (today_utc + timedelta(days=9)).isoformat()
    hist_start = (datetime.now(timezone.utc) - timedelta(days=12)).strftime("%Y-%m-%dT%H:%M:%SZ")
    hist_overdue = (today_utc - timedelta(days=2)).isoformat()

    try:
        broker.start()
        start_api()
        start_worker()

        admin = login("Admin")
        for uid, nombre in (("Ana", "Ana Morales"), ("Bruno", "Bruno Castro"), ("Carla", "Carla Vega")):
            call("/api/usuarios", "POST", {"idUsuario": uid, "nombre": nombre, "activo": True}, admin, 201)
            call("/api/cuentas", "POST", {"idUsuario": uid, "rol": "USUARIO", "password": passwords[uid]}, admin, 201)

        ana = login("Ana")
        bruno = login("Bruno")
        carla = login("Carla")

        # Crear ejemplares físicos (dejando E-DEF-5 y E-DEF-6 disponibles al final para demo en vivo)
        for eid, lid in (
            ("E-DEF-1", "L001"), ("E-DEF-2", "L002"), ("E-DEF-3", "L003"),
            ("E-DEF-4", "L004"), ("E-DEF-5", "L005"), ("E-DEF-6", "L006"), ("E-DEF-7", "L007"),
        ):
            call("/api/ejemplares", "POST", {"idEjemplar": eid, "idLibro": lid}, admin, 201)

        # 1) Reserva atendida -> préstamo próximo a vencer (Ana, L002) + pago confirmado PG-1
        sol_atend = call("/api/solicitudes", "POST", {"idLibro": "L002"}, ana, 201)
        assert sol_atend["estado"] == "apartada"
        call("/api/prestamos", "POST",
             {"idPrestamo": "P-DEF-PRONTO", "idEjemplar": "E-DEF-2", "idUsuario": "Ana", "vencimiento": due_soon}, admin, 201)
        pg_pronto = call("/api/mis-pagos/P-DEF-PRONTO/simulacion", "POST", {}, ana, 200)

        # 2) Préstamo vigente (Ana, L003) -> cargo PENDIENTE reservado para la demostración en vivo
        call("/api/prestamos", "POST",
             {"idPrestamo": "P-DEF-VIG", "idEjemplar": "E-DEF-3", "idUsuario": "Ana", "vencimiento": due_valid_1}, admin, 201)

        # 3) Reserva cancelada con motivo (Ana, L003)
        sol_canc = call("/api/solicitudes", "POST", {"idLibro": "L003"}, ana, 201)
        call(f"/api/mis-reservas/{sol_canc['idReserva']}/cancelacion", "POST",
             {"motivo": "Cambio de bibliografía del curso"}, ana, 200)

        # 4) Reserva apartada lista para retirar (Ana, L004)
        sol_apart = call("/api/solicitudes", "POST", {"idLibro": "L004"}, ana, 201)
        assert sol_apart["estado"] == "apartada"

        # 5) Dos devoluciones históricas (Ana sobre E-DEF-5 y Bruno sobre E-DEF-6 -> ambos quedan disponibles)
        call("/api/prestamos", "POST",
             {"idPrestamo": "P-DEF-DEV1", "idEjemplar": "E-DEF-5", "idUsuario": "Ana", "vencimiento": due_valid_1}, admin, 201)
        call("/api/mis-pagos/P-DEF-DEV1/simulacion", "POST", {}, ana, 200)
        call("/api/prestamos/P-DEF-DEV1/devolucion", "POST", {}, admin, 200)

        call("/api/prestamos", "POST",
             {"idPrestamo": "P-DEF-DEV2", "idEjemplar": "E-DEF-6", "idUsuario": "Bruno", "vencimiento": due_valid_2}, admin, 201)
        call("/api/prestamos/P-DEF-DEV2/devolucion", "POST", {}, admin, 200)

        # 6) Anulación con motivo de un préstamo cuyo cargo YA fue pagado (conserva comprobante histórico)
        call("/api/prestamos", "POST",
             {"idPrestamo": "P-DEF-ANUL", "idEjemplar": "E-DEF-6", "idUsuario": "Ana", "vencimiento": due_valid_1}, admin, 201)
        pg_anul = call("/api/mis-pagos/P-DEF-ANUL/simulacion", "POST", {}, ana, 200)
        call("/api/prestamos/P-DEF-ANUL/anulacion", "POST",
             {"motivo": "Error de registro en mostrador", "copiaRecuperada": True}, admin, 200)

        # 7) Préstamo vigente propio de Bruno (L007)
        call("/api/prestamos", "POST",
             {"idPrestamo": "P-DEF-BRU", "idEjemplar": "E-DEF-7", "idUsuario": "Bruno", "vencimiento": due_valid_2}, admin, 201)

        # 8) Préstamo vencido mediante fixture histórico válido (conservando la validación de altas futuras)
        # Confirmar primero que POST /api/prestamos rechaza vencimiento pasado (400)
        call("/api/prestamos", "POST",
             {"idPrestamo": "P-NO-PASADO", "idEjemplar": "E-DEF-1", "idUsuario": "Ana", "vencimiento": hist_overdue}, admin, 400)
        # Crear el préstamo inicialmente válido y actualizar su inicio/vencimiento históricos vía PUT /api/dominio validado
        call("/api/prestamos", "POST",
             {"idPrestamo": "P-DEF-VENC", "idEjemplar": "E-DEF-1", "idUsuario": "Ana", "vencimiento": due_valid_1}, admin, 201)
        dom = call("/api/dominio", token=admin)
        for p in dom["prestamos"]:
            if p["idPrestamo"] == "P-DEF-VENC":
                p["inicio"] = hist_start
                p["vencimiento"] = hist_overdue
        call("/api/dominio", "PUT", dom, admin, 200)

        # 9) Cola FIFO pendiente sobre L001 (Bruno #1, Carla #2)
        sol_p1 = call("/api/solicitudes", "POST", {"idLibro": "L001"}, bruno, 201)
        sol_p2 = call("/api/solicitudes", "POST", {"idLibro": "L001"}, carla, 201)
        assert sol_p1["estado"] == "pendiente" and sol_p2["estado"] == "pendiente"

        # Esperar a que biblioteca_worker procese TODOS los avisos (al menos 8 procesados)
        wait_until(
            lambda: call("/api/avisos", token=admin)["resumen"]["pendiente"] == 0
            and call("/api/avisos", token=admin)["resumen"]["publicado"] == 0
            and call("/api/avisos", token=admin)["resumen"]["procesado"] >= 8,
            timeout=20, msg="procesamiento de >=8 avisos por worker"
        )
        resumen_av = call("/api/avisos", token=admin)["resumen"]
        assert resumen_av["procesado"] >= 8 and resumen_av["fallido"] == 0, resumen_av

        # Verificación opcional de dos sesiones + reinicio + aislamiento Bruno
        if args.verify:
            # Ana paga el cargo pendiente de P-DEF-VENC (P-DEF-VIG queda pendiente para la demo en vivo!)
            pg_venc = call("/api/mis-pagos/P-DEF-VENC/simulacion", "POST", {}, ana, 200)
            # Doble clic / reintento -> mismo comprobante y 1 solo aviso de pago para P-DEF-VENC
            pg_venc_dup = call("/api/mis-pagos/P-DEF-VENC/simulacion", "POST", {}, ana, 200)
            assert pg_venc["idComprobante"] == pg_venc_dup["idComprobante"]

            wait_until(
                lambda: any(n["idPrestamo"] == "P-DEF-VENC" and n["tipo"] == "pago_simulado"
                            for n in call("/api/mis-avisos", token=ana)["items"]),
                timeout=15, msg="aviso pago_simulado procesado para P-DEF-VENC"
            )
            avisos_pago_venc = [a for a in call("/api/avisos", token=admin)["items"]
                                if a["idPrestamo"] == "P-DEF-VENC" and a["tipo"] == "pago_simulado"]
            assert len(avisos_pago_venc) == 1

            # Admin consulta el mismo comprobante en /api/pagos
            pagos_admin = call("/api/pagos", token=admin)
            assert any(c["idComprobante"] == pg_venc["idComprobante"] and c["idPrestamo"] == "P-DEF-VENC"
                       for c in pagos_admin["comprobantes"])
            # Cargo anulado conserva comprobante histórico pg_anul
            assert any(c["idPrestamo"] == "P-DEF-ANUL" and c["estado"] == "anulado" for c in pagos_admin["cargos"])
            assert any(c["idComprobante"] == pg_anul["idComprobante"] for c in pagos_admin["comprobantes"])
            # Cargo pendiente para demo en vivo sigue pendiente
            assert any(c["idPrestamo"] == "P-DEF-VIG" and c["estado"] == "pendiente" for c in pagos_admin["cargos"])

            # Bruno NO puede consultar el pago ni el aviso de Ana
            notif_ana_pago = next(n for n in call("/api/mis-avisos", token=ana)["items"]
                                  if n["idPrestamo"] == "P-DEF-VENC" and n["tipo"] == "pago_simulado")
            call("/api/mis-pagos/P-DEF-VENC", "GET", token=bruno, status=404)
            call(f"/api/mis-avisos/{notif_ana_pago['idNotificacion']}", "GET", token=bruno, status=404)
            assert all(n["idUsuario"] == "Bruno" for n in call("/api/mis-avisos", token=bruno)["items"])

            # Reinicio real de catalogo_api: Ana vuelve a iniciar sesión y conserva comprobante y avisos ordenados desc
            stop_api()
            start_api()
            ana = login("Ana")
            admin = login("Admin")
            mib_ana = call("/api/mi-biblioteca", token=ana)
            assert any(c["idComprobante"] == pg_venc["idComprobante"] for c in mib_ana["comprobantes"])
            assert next(c for c in mib_ana["cargos"] if c["idPrestamo"] == "P-DEF-VENC")["estado"] == "pagado"
            assert next(c for c in mib_ana["cargos"] if c["idPrestamo"] == "P-DEF-VIG")["estado"] == "pendiente"
            assert [p["idPrestamo"] for p in mib_ana["activos"]] == ["P-DEF-VENC", "P-DEF-PRONTO", "P-DEF-VIG"]
            mis_av = call("/api/mis-avisos", token=ana)["items"]
            fechas = [n["procesadaEn"] for n in mis_av]
            assert fechas == sorted(fechas, reverse=True), f"Mis avisos no ordenados desc: {fechas}"
            assert call("/api/estructuras/validacion", token=admin)["valido"] is True
            print(f"VERIFICACIÓN 2 SESIONES + REINICIO + AISLAMIENTO OK (comprobante={pg_venc['idComprobante']}, avisos procesados={call('/api/avisos', token=admin)['resumen']['procesado']}).")

        # Resumen final del dataset preparado
        dom_final = call("/api/dominio", token=admin)
        av_final = call("/api/avisos", token=admin)["resumen"]
        assert call("/api/estructuras/validacion", token=admin)["valido"] is True
        print("DATASET DEFENSA LISTO en:", out_dir)
        print(json.dumps({
            "working": str(working),
            "auth_store": str(auth_store),
            "service_token": str(svc_token),
            "credenciales_privadas": str(creds_file),
            "prestamos": {
                "activos": len([p for p in dom_final["prestamos"] if p["estado"] == "activo"]),
                "devueltos": len([p for p in dom_final["prestamos"] if p["estado"] == "devuelto"]),
                "anulados": len([p for p in dom_final["prestamos"] if p["estado"] == "cancelado"]),
            },
            "reservas": {
                est: len([r for r in dom_final["reservas"] if r["estado"] == est])
                for est in ("pendiente", "apartada", "atendida", "cancelada")
            },
            "cargos": {
                est: len([c for c in dom_final["cargos"] if c["estado"] == est])
                for est in ("pendiente", "pagado", "anulado")
            },
            "comprobantes": len(dom_final["comprobantes"]),
            "avisos": av_final,
            "ejemplares_disponibles_para_vivo": [e["idEjemplar"] for e in dom_final["ejemplares"] if e["estado"] == "disponible"],
            "cargo_pendiente_ana_para_vivo": "P-DEF-VIG",
        }, indent=2, ensure_ascii=False))
    finally:
        stop_worker()
        stop_api()
        broker.stop()
        api_log.close()
        worker_log.close()
        # Verificar cero secretos en logs
        for lp in (logs / "api.log", logs / "worker.log"):
            if lp.exists():
                txt = lp.read_text(errors="replace")
                for pw in passwords.values():
                    assert pw not in txt, f"Contraseña filtrada en {lp}"
                assert svc_token.read_text().strip() not in txt, f"Token filtrado en {lp}"


if __name__ == "__main__":
    main()
