"""Pruebas de integración reales con RabbitMQ, outbox C++, worker independiente C++,
identidad de servicio mínima, aislamiento de avisos y proxy de confianza."""
import json
import os
import secrets
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

api_bin = Path(sys.argv[1]).resolve()
worker_bin = Path(sys.argv[2]).resolve()
outbox_test_bin = Path(sys.argv[3]).resolve()
initial_json = Path(sys.argv[4]).resolve()
repo_root = Path(__file__).resolve().parents[2]


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _is_debian_wrapper(path: Path) -> bool:
    try:
        return path.resolve().name == "rabbitmq-script-wrapper"
    except OSError:
        return False


def find_rabbitmq_binaries():
    local_rt = repo_root / "build" / "deps" / "rabbitmq-runtime"
    local_sbin = local_rt / "usr" / "lib" / "rabbitmq" / "lib"
    if local_sbin.exists():
        matches = sorted(local_sbin.glob("rabbitmq_server-*/sbin/rabbitmq-server"))
        if matches:
            sbin = matches[-1].parent
            erl_bin = local_rt / "usr" / "lib" / "erlang" / "bin"
            return str(sbin / "rabbitmq-server"), str(sbin / "rabbitmqctl"), f"{erl_bin}:{sbin}:{local_rt / 'usr' / 'bin'}"
    # Preferir siempre los binarios directos de /usr/lib/rabbitmq/ en vez del wrapper /usr/sbin/*
    # (en Debian/Ubuntu /usr/sbin/rabbitmqctl rechaza usuarios no-root y descarta sus argumentos).
    direct_candidates = [
        (Path("/usr/lib/rabbitmq/bin/rabbitmq-server"), Path("/usr/lib/rabbitmq/bin/rabbitmqctl")),
    ]
    sys_lib = Path("/usr/lib/rabbitmq/lib")
    if sys_lib.exists():
        for srv_match in sorted(sys_lib.glob("rabbitmq_server-*/sbin/rabbitmq-server"), reverse=True):
            direct_candidates.append((srv_match, srv_match.parent / "rabbitmqctl"))
    for srv_p, ctl_p in direct_candidates:
        if srv_p.exists() and ctl_p.exists() and not _is_debian_wrapper(srv_p) and not _is_debian_wrapper(ctl_p):
            return str(srv_p), str(ctl_p), f"{srv_p.parent}"
    sys_srv = shutil.which("rabbitmq-server") or ("/usr/sbin/rabbitmq-server" if Path("/usr/sbin/rabbitmq-server").exists() else None)
    sys_ctl = shutil.which("rabbitmqctl") or ("/usr/sbin/rabbitmqctl" if Path("/usr/sbin/rabbitmqctl").exists() else None)
    if sys_srv and sys_ctl:
        return sys_srv, sys_ctl, ""
    return None, None, ""


class LocalRabbitBroker:
    def __init__(self, base_dir: Path, port: int):
        self.base_dir = base_dir
        self.port = port
        self.epmd_port = free_port()
        self.dist_port = free_port()
        self.nodename = f"bib_test_{port}@localhost"
        self.srv, self.ctl, self.extra_path = find_rabbitmq_binaries()
        if not self.srv:
            raise RuntimeError("RabbitMQ no disponible en el entorno de pruebas")
        self.proc = None
        self.log_path = base_dir / "rabbitmq.log"
        self.last_await_info = "await_startup no ejecutado"
        self.env = os.environ.copy()
        if self.extra_path:
            self.env["PATH"] = self.extra_path + ":" + self.env.get("PATH", "")
        home = base_dir / "home"
        for sub in ("home", "mnesia", "log", "etc", "plugins", "schema", "generated"):
            (base_dir / sub).mkdir(parents=True, exist_ok=True)
        home.chmod(0o700)
        cookie_file = home / ".erlang.cookie"
        if not cookie_file.exists():
            cookie_file.write_text(secrets.token_hex(16))
            cookie_file.chmod(0o400)
        cfg_file = base_dir / "etc" / "rabbitmq.conf"
        cfg_file.write_text(f"listeners.tcp.1 = 127.0.0.1:{port}\nloopback_users = none\n")
        env_file = base_dir / "etc" / "rabbitmq-env.conf"
        env_file.write_text("")
        plugins_file = base_dir / "etc" / "enabled_plugins"
        plugins_file.write_text("[].\n")
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
            "RABBITMQ_CONFIG_FILE": str(cfg_file),
            "RABBITMQ_CONF_ENV_FILE": str(env_file),
            "RABBITMQ_ENABLED_PLUGINS_FILE": str(plugins_file),
            "RABBITMQ_PLUGINS_EXPAND_DIR": str(base_dir / "plugins"),
            "RABBITMQ_PID_FILE": str(base_dir / "rabbitmq.pid"),
            "RABBITMQ_SCHEMA_DIR": str(base_dir / "schema"),
            "RABBITMQ_GENERATED_CONFIG_DIR": str(base_dir / "generated"),
            "RABBITMQ_FEATURE_FLAGS_FILE": str(base_dir / "feature_flags"),
        })

    def _collect_logs(self, tail_chars: int = 3000) -> str:
        parts = [
            f"srv={self.srv} ctl={self.ctl} nodename={self.nodename} port={self.port} epmd={self.epmd_port}",
            f"último await_startup: {self.last_await_info}",
        ]
        if self.log_path.exists():
            txt = self.log_path.read_text(errors="replace")
            parts.append(f"--- {self.log_path.name} (últimos {tail_chars} chars) ---\n{txt[-tail_chars:]}")
        log_dir = self.base_dir / "log"
        if log_dir.exists():
            for p in sorted(log_dir.glob("*.log")):
                txt = p.read_text(errors="replace")
                if txt.strip():
                    parts.append(f"--- log/{p.name} (últimos {tail_chars} chars) ---\n{txt[-tail_chars:]}")
        return "\n".join(parts)

    def dump_logs_to_artifact_dir(self, extra_files=None):
        dest_raw = os.environ.get("RABBITMQ_TEST_LOG_DIR", "").strip()
        if not dest_raw:
            return
        dest = Path(dest_raw)
        dest.mkdir(parents=True, exist_ok=True)
        (dest / "broker-diagnostic.txt").write_text(self._collect_logs(tail_chars=16000))
        if self.log_path.exists():
            shutil.copy2(self.log_path, dest / "rabbitmq-stdout.log")
        log_dir = self.base_dir / "log"
        if log_dir.exists():
            for p in sorted(log_dir.glob("*.log")):
                shutil.copy2(p, dest / f"rabbitmq-{p.name}")
        for name, path in (extra_files or {}).items():
            if path.exists():
                shutil.copy2(path, dest / name)

    def start(self):
        logf = open(self.log_path, "ab")
        self.proc = subprocess.Popen(
            [self.srv], env=self.env, stdout=logf, stderr=logf, start_new_session=True
        )
        logf.close()
        deadline = time.monotonic() + 45.0
        port_open = False
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                self.last_await_info = f"proceso terminó con código {self.proc.returncode}"
                self.dump_logs_to_artifact_dir()
                raise RuntimeError("RabbitMQ terminó inesperadamente:\n" + self._collect_logs())
            try:
                with socket.create_connection(("127.0.0.1", self.port), 0.5):
                    port_open = True
            except OSError:
                port_open = False
            if port_open:
                try:
                    chk = subprocess.run(
                        [self.ctl, "-n", self.nodename, "await_startup"],
                        env=self.env, capture_output=True, text=True, timeout=15
                    )
                    self.last_await_info = (
                        f"rc={chk.returncode} stdout={chk.stdout.strip()!r} stderr={chk.stderr.strip()!r}"
                    )
                    if chk.returncode == 0:
                        return
                except subprocess.TimeoutExpired as exc:
                    self.last_await_info = f"timeout await_startup: stdout={exc.stdout!r} stderr={exc.stderr!r}"
            time.sleep(0.25)
        self.dump_logs_to_artifact_dir()
        raise RuntimeError(
            f"Timeout esperando arranque de RabbitMQ (puerto abierto={port_open}):\n" + self._collect_logs()
        )

    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                subprocess.run(
                    [self.ctl, "-n", self.nodename, "stop"],
                    env=self.env, capture_output=True, timeout=20
                )
            except Exception:
                pass
            try:
                self.proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(self.proc.pid, signal.SIGTERM)
                    self.proc.wait(timeout=5)
                except Exception:
                    try:
                        os.killpg(self.proc.pid, signal.SIGKILL)
                    except OSError:
                        self.proc.kill()
                    self.proc.wait(timeout=5)
        if self.proc is not None:
            try:
                os.killpg(self.proc.pid, signal.SIGTERM)
            except OSError:
                pass
        self.proc = None

    def queue_messages(self, queue_name: str) -> int:
        res = subprocess.run(
            [self.ctl, "-n", self.nodename, "list_queues", "name", "messages", "--silent"],
            env=self.env, capture_output=True, text=True, timeout=15
        )
        assert res.returncode == 0, f"list_queues falló (rc={res.returncode}): {res.stderr}\n{res.stdout}"
        for line in res.stdout.splitlines():
            parts = line.split()
            if len(parts) == 2 and parts[0] == queue_name:
                return int(parts[1])
        return 0


def wait_until(predicate, timeout=15.0, interval=0.15, msg="condición"):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            res = predicate()
            if res:
                return res
        except Exception as e:
            last = e
        time.sleep(interval)
    raise AssertionError(f"Timeout esperando {msg} (último error: {last})")


def main():
    passwords = {u: secrets.token_urlsafe(24) for u in ("Admin", "Ana", "Bruno")}
    with tempfile.TemporaryDirectory(prefix="biblioteca-rmq-") as folder:
        root = Path(folder)
        working = root / "biblioteca.json"
        auth_store = root / "privado" / "cuentas.json"
        svc_token_file = root / "privado" / "service.token"
        amqp_creds_file = root / "privado" / "amqp.creds"
        api_port = free_port()
        rmq_port = free_port()
        base = f"http://127.0.0.1:{api_port}"

        # Bootstrap Admin y token de servicio independiente (sin exponer secretos)
        boot_admin = subprocess.run([
            str(api_bin), "--initial", str(initial_json), "--working", str(working),
            "--auth-store", str(auth_store), "--bootstrap-admin", "Admin", "--admin-name", "Admin Prueba"
        ], input=passwords["Admin"] + "\n", text=True, capture_output=True)
        assert boot_admin.returncode == 0, boot_admin.stderr

        boot_svc = subprocess.run([str(api_bin), "--bootstrap-service-token", str(svc_token_file)],
                                  text=True, capture_output=True)
        assert boot_svc.returncode == 0, boot_svc.stderr
        assert (svc_token_file.stat().st_mode & 0o777) == 0o600
        svc_token = svc_token_file.read_text().strip()
        assert svc_token not in boot_svc.stdout and svc_token not in boot_svc.stderr

        amqp_creds_file.write_text("user=guest\npassword=guest\nvhost=/\n")
        amqp_creds_file.chmod(0o600)

        api_log = open(root / "api.log", "w+")
        worker_log = open(root / "worker.log", "w+")
        api_proc = None
        worker_proc = None
        broker = LocalRabbitBroker(root / "rmq", rmq_port)

        def call(path, method="GET", body=None, token="", status=200, extra_headers=None):
            h = {"Content-Type": "application/json", **(extra_headers or {})}
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

        def start_api(trusted_proxy=None):
            nonlocal api_proc
            cmd = [
                str(api_bin), "--initial", str(initial_json), "--working", str(working),
                "--auth-store", str(auth_store), "--service-token-file", str(svc_token_file),
                "--amqp-host", "127.0.0.1", "--amqp-port", str(rmq_port),
                "--amqp-credentials-file", str(amqp_creds_file),
                "--outbox-poll-ms", "200", "--port", str(api_port)
            ]
            if trusted_proxy:
                cmd += ["--trusted-proxy", trusted_proxy]
            api_proc = subprocess.Popen(cmd, stdout=api_log, stderr=api_log)
            wait_until(lambda: call("/readyz")["listo"] is True, timeout=10, msg="API readyz")

        def stop_api():
            nonlocal api_proc
            if api_proc and api_proc.poll() is None:
                api_proc.terminate()
                api_proc.wait(timeout=10)
            api_proc = None

        def start_worker(extra=None):
            nonlocal worker_proc
            cmd = [
                str(worker_bin), "--api-url", base, "--service-token-file", str(svc_token_file),
                "--amqp-host", "127.0.0.1", "--amqp-port", str(rmq_port),
                "--amqp-credentials-file", str(amqp_creds_file),
                "--max-retries", "2", "--backoff-ms", "150"
            ] + (extra or [])
            worker_proc = subprocess.Popen(cmd, stdout=worker_log, stderr=worker_log)

        def stop_worker():
            nonlocal worker_proc
            if worker_proc and worker_proc.poll() is None:
                worker_proc.terminate()
                worker_proc.wait(timeout=10)
            worker_proc = None

        def login(u, extra_headers=None):
            return call("/api/sesion/login", "POST", {"idUsuario": u, "password": passwords[u]},
                        extra_headers=extra_headers)["token"]

        try:
            # === 1. BROKER CAÍDO AL PRODUCIR AVISOS ===
            start_api()
            admin = login("Admin")
            for u in ("Ana", "Bruno"):
                call("/api/usuarios", "POST", {"idUsuario": u, "nombre": u + " Lector", "activo": True}, admin, 201)
                call("/api/cuentas", "POST", {"idUsuario": u, "rol": "USUARIO", "password": passwords[u]}, admin, 201)
            ana = login("Ana")
            bruno = login("Bruno")

            call("/api/ejemplares", "POST", {"idEjemplar": "E1", "idLibro": "L001"}, admin, 201)
            sol_ana = call("/api/solicitudes", "POST", {"idLibro": "L001"}, ana, 201)
            assert sol_ana["estado"] == "apartada"
            # Catálogo y préstamos siguen operativos con RabbitMQ caído
            assert call("/api/libros", token=ana)["total"] == 16
            call("/api/prestamos", "POST", {"idPrestamo": "P1", "idEjemplar": "E1", "idUsuario": "Ana", "vencimiento": "2099-12-01"}, admin, 201)
            sol_bruno = call("/api/solicitudes", "POST", {"idLibro": "L001"}, bruno, 201)
            assert sol_bruno["estado"] == "pendiente"

            time.sleep(0.5)
            avisos_caido = call("/api/avisos", token=admin)
            assert avisos_caido["broker"]["configurado"] is True
            assert avisos_caido["broker"]["conectado"] is False
            assert avisos_caido["resumen"]["pendiente"] >= 2
            assert avisos_caido["resumen"]["publicado"] == 0
            assert avisos_caido["resumen"]["procesado"] == 0
            # Sin notificaciones falsas cuando RabbitMQ está caído
            mis_ana = call("/api/mis-avisos", token=ana)
            assert mis_ana["items"] == [] and mis_ana["pendientes"] >= 2
            print("1. Broker caído: catálogo/préstamos operativos, avisos pendientes, sin éxito simulado OK.")

            # === 2. IDENTIDAD DE SERVICIO CON PERMISOS MÍNIMOS ===
            for ruta in ("/api/libros", "/api/usuarios", "/api/cuentas", "/api/dominio", "/api/avisos", "/api/mi-biblioteca", "/api/estructuras"):
                call(ruta, token=svc_token, status=403)
            # Cuentas humanas NO pueden invocar rutas internas del worker
            call("/api/interno/avisos/AV-1/procesar", "POST",
                 {"idEvento": "EV-AV-1", "titulo": "x", "mensaje": "y", "intentos": 1}, admin, 403)
            call("/api/interno/avisos/AV-1/procesar", "POST",
                 {"idEvento": "EV-AV-1", "titulo": "x", "mensaje": "y", "intentos": 1}, ana, 403)
            call("/api/interno/avisos/AV-1/procesar", "POST",
                 {"idEvento": "EV-AV-1", "titulo": "x", "mensaje": "y", "intentos": 1}, "", 403)
            print("2. Identidad de servicio aislada de rutas administrativas y cuentas humanas OK.")

            # === 3. RECUPERACIÓN DEL BROKER Y PUBLICACIÓN DE PENDIENTES (WORKER AÚN DETENIDO) ===
            broker.start()
            # Probar detección de mensaje sin ruta (basic.return) contra RabbitMQ REAL
            ret_test = subprocess.run([str(outbox_test_bin), "--test-basic-return", "127.0.0.1", str(rmq_port), "guest", "guest"],
                                      text=True, capture_output=True)
            assert ret_test.returncode == 0, ret_test.stderr + ret_test.stdout
            print("3a. Detección real de basic.return (mandatory sin ruta) OK.")

            wait_until(lambda: call("/api/avisos", token=admin)["resumen"]["pendiente"] == 0
                       and call("/api/avisos", token=admin)["resumen"]["publicado"] >= 2,
                       timeout=12, msg="publicación automática de pendientes")
            estado_pub = call("/api/avisos", token=admin)
            assert estado_pub["broker"]["conectado"] is True
            # Publisher confirm NO equivale a aviso procesado/visible para el usuario
            assert call("/api/mis-avisos", token=ana)["items"] == []
            assert call("/api/mis-avisos", token=ana)["publicados"] >= 2
            print("3b. Recuperación del broker y publicación confirmada (aún sin notificación hasta worker) OK.")

            # === 4. REINICIO DEL BROKER CON MENSAJES PENDIENTES DURABLES ===
            assert broker.queue_messages("biblioteca.avisos") >= 2
            broker.stop()
            broker.start()
            assert broker.queue_messages("biblioteca.avisos") >= 2
            print("4. Reinicio del broker RabbitMQ conserva mensajes persistentes en cola durable OK.")

            # === 5. WORKER DETENIDO Y POSTERIOR PROCESAMIENTO ===
            start_worker()
            wait_until(lambda: call("/api/avisos", token=admin)["resumen"]["procesado"] >= 2
                       and call("/api/avisos", token=admin)["resumen"]["publicado"] == 0,
                       timeout=12, msg="procesamiento por worker")
            mis_ana = call("/api/mis-avisos", token=ana)
            assert len(mis_ana["items"]) >= 2
            assert all(n["idUsuario"] == "Ana" for n in mis_ana["items"])
            print("5. Worker procesa mensajes acumulados y persiste bandeja Mis avisos OK.")

            # === 6. CAÍDA DESPUÉS DE PERSISTIR EFECTO Y ANTES DEL ACK + REDELIVERY SIN DUPLICADO ===
            stop_worker()
            # Devolver P1 -> genera AV de devolución para Ana y AV de reserva_apartada para Bruno
            call("/api/prestamos/P1/devolucion", "POST", {}, admin)
            wait_until(lambda: call("/api/avisos", token=admin)["resumen"]["pendiente"] == 0,
                       timeout=10, msg="publicación de avisos tras devolución")
            avisos_all = call("/api/avisos", token=admin)["items"]
            av_bruno = next(a for a in avisos_all if a["idUsuario"] == "Bruno" and a["tipo"] == "reserva_apartada")
            # Arrancar worker con caída controlada pre-ACK en av_bruno
            start_worker(["--test-fail-before-ack", av_bruno["idAviso"]])
            rc = worker_proc.wait(timeout=15)
            worker_proc = None
            assert rc == 42, f"Se esperaba salida controlada 42 pre-ACK, obtuvo {rc}"
            # La API ya persistió el efecto durable de av_bruno
            mis_bruno_1 = call("/api/mis-avisos", token=bruno)
            assert len([n for n in mis_bruno_1["items"] if n["idAviso"] == av_bruno["idAviso"]]) == 1
            # Al reiniciar el worker, RabbitMQ redelivers el mensaje sin ACK y la API deduplica
            start_worker()
            wait_until(lambda: broker.queue_messages("biblioteca.avisos") == 0
                       and call("/api/avisos", token=admin)["resumen"]["publicado"] == 0,
                       timeout=12, msg="redelivery procesado y confirmado")
            mis_bruno_2 = call("/api/mis-avisos", token=bruno)
            assert len([n for n in mis_bruno_2["items"] if n["idAviso"] == av_bruno["idAviso"]]) == 1
            # Duplicado directo sobre endpoint interno devuelve duplicado=True sin duplicar notificación
            dup = call(f"/api/interno/avisos/{av_bruno['idAviso']}/procesar", "POST",
                       {"idEvento": av_bruno["idEvento"], "titulo": "Reserva disponible para retirar",
                        "mensaje": "Duplicado directo", "intentos": 3}, svc_token, 200)
            assert dup["duplicado"] is True
            assert len(call("/api/mis-avisos", token=bruno)["items"]) == len(mis_bruno_2["items"])
            print("6. Caída pre-ACK + redelivery + duplicado directo sin segunda notificación OK.")

            # === 7. USUARIO A SIN ACCESO A AVISOS DE B ===
            notif_bruno_id = mis_bruno_2["items"][0]["idNotificacion"]
            assert call(f"/api/mis-avisos/{notif_bruno_id}", token=bruno)["idUsuario"] == "Bruno"
            call(f"/api/mis-avisos/{notif_bruno_id}", token=ana, status=404)
            assert all(n["idUsuario"] == "Ana" for n in call("/api/mis-avisos", token=ana)["items"])
            call("/api/avisos", token=ana, status=403)
            call(f"/api/avisos/{av_bruno['idAviso']}/reintento", "POST", {}, ana, 403)
            print("7. Aislamiento por propietario en Mis avisos (A no accede a B) OK.")

            # === 8. MENSAJE INVÁLIDO -> DLQ ===
            dlq_before = broker.queue_messages("biblioteca.avisos.fallidos")
            pub_bad = subprocess.run([
                str(outbox_test_bin), "--publish-raw", "127.0.0.1", str(rmq_port),
                "guest", "guest", "aviso.creado", "{json_invalido"
            ], text=True, capture_output=True)
            assert pub_bad.returncode == 0, pub_bad.stderr
            wait_until(lambda: broker.queue_messages("biblioteca.avisos.fallidos") == dlq_before + 1,
                       timeout=10, msg="mensaje inválido en DLQ")
            print("8. Mensaje malformado enrutado a DLQ biblioteca.avisos.fallidos OK.")

            # === 9. FALLO 503 DEL ESCRITOR -> REINTENTOS AGOTADOS -> RECONCILIACIÓN DURABLE -> REINTENTO ADMIN ===
            stop_worker()
            call("/api/prestamos", "POST", {"idPrestamo": "P2", "idEjemplar": "E1", "idUsuario": "Bruno", "vencimiento": "2099-12-01"}, admin, 201)
            wait_until(lambda: call("/api/avisos", token=admin)["resumen"]["pendiente"] == 0, timeout=10, msg="AV P2 publicado")
            av_p2 = next(a for a in call("/api/avisos", token=admin)["items"] if a["idPrestamo"] == "P2")
            # Forzar fallo de escritura (directorio solo lectura -> escritor devuelve 503 tanto al procesar como al reportar fallo final)
            root.chmod(0o500)
            dlq_before2 = broker.queue_messages("biblioteca.avisos.fallidos")
            start_worker()
            try:
                wait_until(
                    lambda: broker.queue_messages("biblioteca.avisos.fallidos") >= dlq_before2 + 1
                    and broker.queue_messages("biblioteca.avisos.fallo_pendiente") >= 1,
                    timeout=15, msg="reintentos agotados -> DLQ + reconciliación durable pendiente"
                )
                # Mientras el escritor sigue en 503, el aviso aún aparece publicado y la reconciliación
                # sobrevive a un reinicio del worker porque reside en la cola durable del broker.
                av_p2_mid = next(a for a in call("/api/avisos", token=admin)["items"] if a["idAviso"] == av_p2["idAviso"])
                assert av_p2_mid["estado"] == "publicado"
                stop_worker()
                assert broker.queue_messages("biblioteca.avisos.fallo_pendiente") >= 1
            finally:
                root.chmod(0o700)
            # Restablecido el escritor, al volver el worker debe reconciliar automáticamente el fallo
            # SIN ninguna llamada manual al endpoint interno: el aviso deja de estar atrapado en publicado.
            start_worker()
            wait_until(
                lambda: next(a for a in call("/api/avisos", token=admin)["items"] if a["idAviso"] == av_p2["idAviso"])["estado"] == "fallido"
                and broker.queue_messages("biblioteca.avisos.fallo_pendiente") == 0,
                timeout=15, msg="reconciliación automática del fallo tras restablecer el escritor"
            )
            av_p2_failed = next(a for a in call("/api/avisos", token=admin)["items"] if a["idAviso"] == av_p2["idAviso"])
            assert av_p2_failed["estado"] == "fallido" and av_p2_failed["ultimoError"]
            # Procedimiento administrativo de revisión y reintento (sin borrado silencioso)
            call(f"/api/avisos/{av_p2['idAviso']}/reintento", "POST", {}, admin, 200)
            wait_until(lambda: next(a for a in call("/api/avisos", token=admin)["items"] if a["idAviso"] == av_p2["idAviso"])["estado"] == "procesado",
                       timeout=12, msg="reintento administrativo procesado")
            assert any(n["idAviso"] == av_p2["idAviso"] for n in call("/api/mis-avisos", token=bruno)["items"])
            print("9. Escritor 503 -> DLQ + reconciliación durable tras reinicio -> fallido automático y recuperación ADMIN OK.")

            # === 10. REINICIO DE API CONSERVA NOTIFICACIONES, AVISOS E ÍNDICES ===
            before_dom = call("/api/dominio", token=admin)
            before_bruno = call("/api/mis-avisos", token=bruno)
            stop_api()
            start_api(trusted_proxy="127.0.0.1")
            admin = login("Admin")
            bruno = login("Bruno")
            assert call("/api/dominio", token=admin) == before_dom
            assert call("/api/mis-avisos", token=bruno) == before_bruno
            assert call("/api/estructuras/validacion", token=admin)["valido"] is True
            print("10. Reinicio de API conserva avisos, notificaciones procesadas y 5 estructuras OK.")

            # === 11. TRUSTED PROXY RATE LIMITING POR IP REAL ===
            # Con --trusted-proxy 127.0.0.1, dos clientes detrás del proxy NO comparten el límite por IP
            for _ in range(5):
                call("/api/sesion/login", "POST", {"idUsuario": "NoExiste1", "password": "incorrecta-password"},
                     status=401, extra_headers={"X-Forwarded-For": "198.51.100.10"})
            call("/api/sesion/login", "POST", {"idUsuario": "NoExiste1", "password": "incorrecta-password"},
                 status=429, extra_headers={"X-Forwarded-For": "198.51.100.10"})
            # Otro usuario con otra IP real detrás del proxy sí puede iniciar sesión
            tok_ana_proxy = login("Ana", extra_headers={"X-Forwarded-For": "198.51.100.20"})
            assert call("/api/sesion", token=tok_ana_proxy)["idUsuario"] == "Ana"
            print("11. Proxy de confianza separa límite de login por IP real sin compartir bloqueo OK.")

            # Verificar que ningún secreto se filtró a logs
            stop_worker()
            stop_api()
            for log_f in (api_log, worker_log):
                log_f.seek(0)
                txt = log_f.read()
                assert "ERROR: AddressSanitizer" not in txt and "runtime error:" not in txt, txt
                assert svc_token not in txt, "Token de servicio filtrado en log"
                for pw in passwords.values():
                    assert pw not in txt, "Contraseña filtrada en log"
            print("OK rabbitmq_worker_real: todas las garantías asincrónicas y de seguridad verificadas.")
        except Exception:
            api_log.flush()
            worker_log.flush()
            broker.dump_logs_to_artifact_dir({"api.log": root / "api.log", "worker.log": root / "worker.log"})
            raise
        finally:
            root.chmod(0o700)
            stop_worker()
            stop_api()
            broker.stop()
            api_log.close()
            worker_log.close()


if __name__ == "__main__":
    main()
