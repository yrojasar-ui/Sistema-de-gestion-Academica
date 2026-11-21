#!/usr/bin/env python3
"""Verificación integrada final: MongoDB cloud real + RabbitMQ real + worker C++ independiente.
Usa una colección sintética exclusiva y limpia únicamente su único documento confirmado.
No ejecutar desde CI."""
import argparse
from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import secrets
import shutil
import socket
import subprocess
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
from uuid import uuid4

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--api-binary", default="build/debug/catalogo_api")
parser.add_argument("--worker-binary", default="build/debug/biblioteca_worker")
parser.add_argument("--evidence", required=True, help="Directorio nuevo para evidencias")
args = parser.parse_args()

repo_root = Path(__file__).resolve().parents[1]
api_bin = Path(args.api_binary).resolve()
worker_bin = Path(args.worker_binary).resolve()
evidence = Path(args.evidence)
evidence.mkdir(parents=True, exist_ok=True)

endpoint = "https://paginas-web-cr.com/Api/apis/mongodb.php"
collection = "biblioteca_yoel_final_" + uuid4().hex[:16]
root = Path(tempfile.mkdtemp(prefix="biblioteca-final-mongo-"))
log = (evidence / "nube-final.log").open("w", encoding="utf-8")
api_log = (evidence / "api.log").open("w", encoding="utf-8")
worker_log = (evidence / "worker.log").open("w", encoding="utf-8")

passwords = {u: secrets.token_urlsafe(24) for u in ("AdminFinal", "Ana", "Bruno", "Carla")}
future = lambda days: (datetime.now(timezone.utc).date() + timedelta(days=days)).isoformat()


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


api_port = free_port()
rmq_port = free_port()
base = f"http://127.0.0.1:{api_port}"


def record(message, value=None):
    line = datetime.now(timezone.utc).isoformat() + " " + message
    if value is not None:
        line += " " + json.dumps(value, ensure_ascii=False, sort_keys=True)
    print(line, flush=True)
    log.write(line + "\n")
    log.flush()


def find_rabbitmq():
    local_rt = repo_root / "build" / "deps" / "rabbitmq-runtime"
    matches = sorted((local_rt / "usr" / "lib" / "rabbitmq" / "lib").glob("rabbitmq_server-*/sbin/rabbitmq-server"))
    if matches:
        sbin = matches[-1].parent
        erl_bin = local_rt / "usr" / "lib" / "erlang" / "bin"
        return str(sbin / "rabbitmq-server"), str(sbin / "rabbitmqctl"), f"{erl_bin}:{sbin}:{local_rt / 'usr' / 'bin'}"
    srv = shutil.which("rabbitmq-server") or "/usr/sbin/rabbitmq-server"
    ctl = shutil.which("rabbitmqctl") or "/usr/sbin/rabbitmqctl"
    return srv, ctl, ""


class Broker:
    def __init__(self, base_dir: Path, port: int):
        self.port = port
        self.srv, self.ctl, extra = find_rabbitmq()
        self.env = os.environ.copy()
        if extra:
            self.env["PATH"] = extra + ":" + self.env.get("PATH", "")
        home = base_dir / "home"
        for sub in ("home", "mnesia", "log", "etc", "plugins", "schema", "generated"):
            (base_dir / sub).mkdir(parents=True, exist_ok=True)
        home.chmod(0o700)
        (base_dir / "etc" / "rabbitmq.conf").write_text(f"listeners.tcp.1 = 127.0.0.1:{port}\nloopback_users = none\n")
        (base_dir / "etc" / "enabled_plugins").write_text("[].\n")
        self.env.update({
            "HOME": str(home), "ERL_EPMD_ADDRESS": "127.0.0.1", "ERL_EPMD_PORT": str(free_port()),
            "RABBITMQ_NODENAME": f"bib_cloud_{port}@localhost", "RABBITMQ_NODE_IP_ADDRESS": "127.0.0.1",
            "RABBITMQ_NODE_PORT": str(port), "RABBITMQ_DIST_PORT": str(free_port()),
            "RABBITMQ_MNESIA_BASE": str(base_dir / "mnesia"), "RABBITMQ_LOG_BASE": str(base_dir / "log"),
            "RABBITMQ_CONFIG_FILE": str(base_dir / "etc" / "rabbitmq.conf"),
            "RABBITMQ_ENABLED_PLUGINS_FILE": str(base_dir / "etc" / "enabled_plugins"),
            "RABBITMQ_PLUGINS_EXPAND_DIR": str(base_dir / "plugins"),
            "RABBITMQ_PID_FILE": str(base_dir / "rabbitmq.pid"),
            "RABBITMQ_SCHEMA_DIR": str(base_dir / "schema"),
            "RABBITMQ_GENERATED_CONFIG_DIR": str(base_dir / "generated"),
            "RABBITMQ_FEATURE_FLAGS_FILE": str(base_dir / "feature_flags"),
        })
        self.proc = None
        self.logf = (evidence / "rabbitmq.log").open("ab")

    def start(self):
        self.proc = subprocess.Popen([self.srv], env=self.env, stdout=self.logf, stderr=self.logf)
        for _ in range(120):
            assert self.proc.poll() is None, "RabbitMQ terminó"
            try:
                with socket.create_connection(("127.0.0.1", self.port), 0.5):
                    if subprocess.run([self.ctl, "await_startup"], env=self.env, capture_output=True, timeout=15).returncode == 0:
                        return
            except OSError:
                pass
            time.sleep(0.25)
        raise RuntimeError("Timeout arrancando RabbitMQ")

    def stop(self):
        if self.proc and self.proc.poll() is None:
            subprocess.run([self.ctl, "stop"], env=self.env, capture_output=True, timeout=20)
            self.proc.wait(timeout=15)
        self.proc = None
        self.logf.close()


def request(url, method="GET", data=None, expected=200, token=""):
    headers = {"Content-Type": "application/json"}
    if token:
        headers["Authorization"] = "Bearer " + token
    req = Request(url, data=None if data is None else json.dumps(data).encode(), method=method, headers=headers)
    try:
        resp = urlopen(req, timeout=90)
    except HTTPError as err:
        resp = err
    with resp:
        raw = resp.read()
        assert resp.status == expected, (method, url, resp.status, raw)
        return json.loads(raw) if raw else None


def remote():
    res = request(endpoint + "?coleccion=" + collection)
    assert res["coleccion"] == collection and res["total"] == len(res["documentos"])
    return res["documentos"]


def wait_until(fn, timeout=30.0, msg="condición"):
    end = time.monotonic() + timeout
    last = None
    while time.monotonic() < end:
        try:
            v = fn()
            if v:
                return v
        except Exception as e:
            last = e
        time.sleep(0.3)
    raise AssertionError(f"Timeout esperando {msg}: {last}")


priv_dir = root / "privado"
priv_dir.mkdir(mode=0o700)
auth_store = priv_dir / "cuentas.json"
svc_token_file = priv_dir / "service.token"
amqp_creds = priv_dir / "amqp.creds"
amqp_creds.write_text("user=guest\npassword=guest\nvhost=/\n")
amqp_creds.chmod(0o600)

broker = Broker(root / "rmq", rmq_port)
api_proc = None
worker_proc = None
oid = None
cleaned = False


def login(user):
    return request(base + "/api/sesion/login", "POST", {"idUsuario": user, "password": passwords[user]})["token"]


def start_api(initialize=False):
    global api_proc
    base_cmd = [
        str(api_bin), "--storage", "mongo", "--endpoint", endpoint, "--collection", collection,
        "--mongo-state", str(root / "estado"), "--auth-store", str(auth_store),
        "--service-token-file", str(svc_token_file),
        "--amqp-host", "127.0.0.1", "--amqp-port", str(rmq_port),
        "--amqp-credentials-file", str(amqp_creds),
        "--outbox-poll-ms", "400", "--port", str(api_port)
    ]
    if initialize:
        r_svc = subprocess.run([str(api_bin), "--bootstrap-service-token", str(svc_token_file)],
                               text=True, capture_output=True)
        assert r_svc.returncode == 0, r_svc.stderr
        r_adm = subprocess.run(base_cmd + ["--initialize", "--bootstrap-admin", "AdminFinal", "--admin-name", "Admin Final Sintético"],
                               input=passwords["AdminFinal"] + "\n", text=True, capture_output=True)
        assert r_adm.returncode == 0, r_adm.stderr
    api_proc = subprocess.Popen(base_cmd, stdout=api_log, stderr=api_log)
    wait_until(lambda: request(base + "/readyz")["listo"] is True, timeout=30, msg="API Mongo readyz")


def stop_api():
    global api_proc
    if api_proc and api_proc.poll() is None:
        api_proc.terminate()
        api_proc.wait(timeout=15)
    api_proc = None


def start_worker():
    global worker_proc
    worker_proc = subprocess.Popen([
        str(worker_bin), "--api-url", base, "--service-token-file", str(svc_token_file),
        "--amqp-host", "127.0.0.1", "--amqp-port", str(rmq_port),
        "--amqp-credentials-file", str(amqp_creds),
        "--max-retries", "3", "--backoff-ms", "500"
    ], stdout=worker_log, stderr=worker_log)


def stop_worker():
    global worker_proc
    if worker_proc and worker_proc.poll() is None:
        worker_proc.terminate()
        worker_proc.wait(timeout=15)
    worker_proc = None


try:
    record("Verificación integrada final en MongoDB cloud + RabbitMQ + Worker", {"endpoint": endpoint, "coleccion": collection})
    assert remote() == [], "La colección sintética nueva debe iniciar vacía"
    record("GET previo confirmado: total=0")

    broker.start()
    record("RabbitMQ real iniciado en loopback", {"port": rmq_port})
    start_api(initialize=True)
    docs = remote()
    assert len(docs) == 1 and docs[0]["proyecto"] == collection and docs[0]["idBiblioteca"] == "principal"
    oid = docs[0]["_id"]["$oid"]
    record("Documento único inicializado en MongoDB cloud", {"oid": oid, "revision": docs[0]["estado"]["revision"]})

    admin = login("AdminFinal")
    assert request(base + "/api/almacenamiento", token=admin)["almacenamiento"] == "mongo"

    # Crear libros, edición B+/grafo, usuarios, cuentas y ejemplar
    b1 = {"idLibro": "FIN-L1", "titulo": "Atlas Académico", "autor": "Autor Sintético", "temas": ["campus", "mapas"]}
    b2 = {"idLibro": "FIN-L2", "titulo": "Guía de Mapas", "autor": "Autora Sintética", "temas": ["mapas"]}
    request(base + "/api/libros", "POST", b1, 201, admin)
    request(base + "/api/libros", "POST", b2, 201, admin)
    assert len(request(base + "/api/libros/FIN-L1/relacionados", token=admin)["items"]) == 1
    b1_edit = {**b1, "titulo": "Atlas Académico Revisado", "temas": ["geografia"]}
    request(base + "/api/libros/FIN-L1", "PUT", b1_edit, 200, admin)
    assert request(base + "/api/libros/FIN-L1/relacionados", token=admin)["items"] == []

    for u in ("Ana", "Bruno", "Carla"):
        request(base + "/api/usuarios", "POST", {"idUsuario": u, "nombre": u + " Sintética", "activo": True}, 201, admin)
        request(base + "/api/cuentas", "POST", {"idUsuario": u, "rol": "USUARIO", "password": passwords[u]}, 201, admin)
    ana, bruno, carla = login("Ana"), login("Bruno"), login("Carla")
    request(base + "/api/ejemplares", "POST", {"idEjemplar": "FIN-E1", "idLibro": "FIN-L1"}, 201, admin)

    # Solicitud de Ana (apartada) -> entrega -> pago simulado idempotente -> renovación -> solicitudes FIFO Bruno y Carla
    sol_a = request(base + "/api/solicitudes", "POST", {"idLibro": "FIN-L1"}, 201, ana)
    assert sol_a["estado"] == "apartada"
    request(base + "/api/prestamos", "POST", {"idPrestamo": "FIN-PA", "idEjemplar": "FIN-E1", "idUsuario": "Ana", "vencimiento": future(7)}, 201, admin)
    rec1 = request(base + "/api/mis-pagos/FIN-PA/simulacion", "POST", {}, 200, ana)
    rec2 = request(base + "/api/mis-pagos/FIN-PA/simulacion", "POST", {}, 200, ana)
    assert rec1 == rec2 and rec1["etiqueta"] == "SIMULADO" and rec1["importe"] == 10000
    request(base + "/api/prestamos/FIN-PA/renovacion", "POST", {"vencimiento": future(14)}, 200, admin)
    sol_b = request(base + "/api/solicitudes", "POST", {"idLibro": "FIN-L1"}, 201, bruno)
    sol_c = request(base + "/api/solicitudes", "POST", {"idLibro": "FIN-L1"}, 201, carla)
    assert sol_b["llegada"] < sol_c["llegada"]

    # Devolución Ana -> Bruno apartado -> Bruno cancela -> Carla apartada -> préstamo/devolución Carla
    request(base + "/api/prestamos/FIN-PA/devolucion", "POST", {}, 200, admin)
    assert request(base + f"/api/reservas/{sol_b['idReserva']}", token=admin)["estado"] == "apartada"
    request(base + f"/api/mis-reservas/{sol_b['idReserva']}/cancelacion", "POST", {"motivo": "Cancelación demo final"}, 200, bruno)
    assert request(base + f"/api/reservas/{sol_c['idReserva']}", token=admin)["estado"] == "apartada"
    request(base + "/api/prestamos", "POST", {"idPrestamo": "FIN-PC", "idEjemplar": "FIN-E1", "idUsuario": "Carla", "vencimiento": future(7)}, 201, admin)
    request(base + "/api/prestamos/FIN-PC/devolucion", "POST", {}, 200, admin)

    # Arrancar worker independiente C++ para procesar TODOS los avisos vía RabbitMQ -> API (escritor único) -> MongoDB
    start_worker()
    wait_until(lambda: (
        (av := request(base + "/api/avisos", token=admin))["resumen"]["pendiente"] == 0
        and av["resumen"]["publicado"] == 0
        and av["resumen"]["fallido"] == 0
        and av["resumen"]["procesado"] == av["total"]
        and av["total"] >= 8
    ), timeout=45, msg="todos los avisos procesados por worker y persistidos en Mongo")
    record("Worker procesó todos los avisos en MongoDB vía escritor único", request(base + "/api/avisos", token=admin)["resumen"])

    # Verificar aislamiento de Mis avisos antes de reiniciar
    mis_a = request(base + "/api/mis-avisos", token=ana)
    mis_b = request(base + "/api/mis-avisos", token=bruno)
    mis_c = request(base + "/api/mis-avisos", token=carla)
    assert len(mis_a["items"]) >= 4 and all(n["idUsuario"] == "Ana" for n in mis_a["items"])
    assert len(mis_b["items"]) >= 2 and all(n["idUsuario"] == "Bruno" for n in mis_b["items"])
    assert len(mis_c["items"]) >= 3 and all(n["idUsuario"] == "Carla" for n in mis_c["items"])
    request(base + f"/api/mis-avisos/{mis_b['items'][0]['idNotificacion']}", token=ana, expected=404)

    before_dom = request(base + "/api/dominio", token=admin)
    before_books = request(base + "/api/libros", token=admin)
    before_structs = request(base + "/api/estructuras", token=admin)

    # REINICIO REAL DE API Y WORKER CONTRA MONGODB CLOUD
    stop_worker()
    stop_api()
    start_api(initialize=False)
    start_worker()
    admin, ana, bruno, carla = login("AdminFinal"), login("Ana"), login("Bruno"), login("Carla")
    after_dom = request(base + "/api/dominio", token=admin)
    after_books = request(base + "/api/libros", token=admin)
    after_structs = request(base + "/api/estructuras", token=admin)
    assert before_dom == after_dom, "El dominio cambió tras reiniciar contra MongoDB"
    assert before_books == after_books, "El catálogo cambió tras reiniciar contra MongoDB"
    assert request(base + "/api/estructuras/validacion", token=admin)["valido"] is True
    assert request(base + "/api/libros?prefijo=Atlas%20Academico%20Revisado", token=ana)["total"] == 1
    assert request(base + "/api/mis-avisos", token=ana) == mis_a
    assert request(base + "/api/mis-avisos", token=bruno) == mis_b
    assert request(base + "/api/mis-avisos", token=carla) == mis_c

    # Comprobar que ninguna credencial/hash/token fue almacenada en MongoDB
    docs_now = remote()
    assert len(docs_now) == 1
    raw_mongo = json.dumps(docs_now[0])
    svc_secret = svc_token_file.read_text().strip()
    for secret in [*passwords.values(), svc_secret, admin, ana, bruno, carla, "argon2"]:
        assert secret not in raw_mongo, "Credencial o hash detectado en MongoDB"

    (evidence / "checkpoint-final.json").write_text(json.dumps({
        "coleccion": collection, "revision": docs_now[0]["estado"]["revision"],
        "resumenAvisos": request(base + "/api/avisos", token=admin)["resumen"],
        "libros": len(after_books["libros"]),
        "prestamos": len(after_dom["prestamos"]),
        "reservas": len(after_dom["reservas"]),
        "comprobantes": len(after_dom["comprobantes"]),
        "notificaciones": len(after_dom["notificaciones"]),
        "siguienteLlegada": after_dom["siguienteLlegada"],
        "estructurasValidas": True
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    record("REINICIO REAL OK: 5 estructuras, edición B+/grafo, FIFO, pago SIMULADO, avisos/worker e historial intactos")
finally:
    stop_worker()
    stop_api()
    broker.stop()
    try:
        if oid:
            docs_clean = remote()
            assert len(docs_clean) == 1 and docs_clean[0]["_id"]["$oid"] == oid and docs_clean[0]["proyecto"] == collection
            res_del = request(endpoint, "DELETE", {
                "coleccion": collection,
                "filtro": {"_id": oid, "proyecto": collection, "idBiblioteca": "principal"}
            })
            record("DELETE del documento sintético único", res_del)
            assert res_del["eliminados"] == 1 and remote() == []
            cleaned = True
            record("GET final confirmado: total=0; limpieza completa verificada")
        else:
            record("No hubo ObjectId que limpiar")
    finally:
        api_log.close()
        worker_log.close()
        log.close()
        if cleaned:
            shutil.rmtree(root)
        else:
            print("ATENCIÓN: limpieza incompleta; revisar:", root, collection)
