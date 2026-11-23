"""Pruebas de regresión de validaciones, excepciones, pagos idempotentes,
fallos de persistencia y fallos de RabbitMQ."""
import json
import os
import secrets
import socket
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import Request, urlopen

api_bin = Path(sys.argv[1]).resolve()
worker_bin = Path(sys.argv[2]).resolve()
initial_json = Path(sys.argv[3]).resolve()


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_until(fn, timeout=10.0, msg="condición"):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        try:
            v = fn()
            if v:
                return v
        except Exception as e:
            last = e
        time.sleep(0.1)
    raise AssertionError(f"Timeout esperando {msg}: {last}")


def main():
    passwords = {u: secrets.token_urlsafe(20) for u in ("Admin", "Ana", "Bruno")}
    today_utc = datetime.now(timezone.utc).date()
    future1 = (today_utc + timedelta(days=5)).isoformat()
    future2 = (today_utc + timedelta(days=10)).isoformat()
    past_day = (today_utc - timedelta(days=2)).isoformat()

    # 1. Validación estricta de AMQP_PORT en variable de entorno (catalogo_api y biblioteca_worker)
    for bad_port in ("abc", "0", "99999"):
        env = os.environ.copy()
        env["AMQP_PORT"] = bad_port
        r_api = subprocess.run([str(api_bin)], env=env, text=True, capture_output=True)
        assert r_api.returncode != 0 and "AMQP_PORT" in r_api.stderr, r_api.stderr
        r_wrk = subprocess.run([str(worker_bin)], env=env, text=True, capture_output=True)
        assert r_wrk.returncode != 0 and "AMQP_PORT" in r_wrk.stderr, r_wrk.stderr
    print("1. Validación de AMQP_PORT inválido en catalogo_api y biblioteca_worker OK.")

    with tempfile.TemporaryDirectory(prefix="biblioteca-reg-") as folder:
        root = Path(folder)
        working = root / "biblioteca.json"
        auth_store = root / "privado" / "cuentas.json"
        svc_token = root / "privado" / "service.token"
        api_port = free_port()
        closed_amqp_port = free_port()
        base = f"http://127.0.0.1:{api_port}"

        subprocess.run([
            str(api_bin), "--initial", str(initial_json), "--working", str(working),
            "--auth-store", str(auth_store), "--bootstrap-admin", "Admin", "--admin-name", "Admin Reg"
        ], input=passwords["Admin"] + "\n", text=True, capture_output=True, check=True)
        subprocess.run([str(api_bin), "--bootstrap-service-token", str(svc_token)],
                       text=True, capture_output=True, check=True)

        log_path = root / "api.log"
        log_file = open(log_path, "w+")
        proc = None

        def call(path, method="GET", body=None, token="", status=200, content_type="application/json"):
            h = {}
            if content_type is not None:
                h["Content-Type"] = content_type
            if token:
                h["Authorization"] = "Bearer " + token
            data = None if body is None else (body.encode() if isinstance(body, str) else json.dumps(body).encode())
            try:
                resp = urlopen(Request(base + path, data=data, method=method, headers=h), timeout=10)
            except HTTPError as err:
                resp = err
            with resp:
                raw = resp.read()
                val = json.loads(raw) if raw else None
                assert resp.status == status, f"{method} {path}: {resp.status} != {status}: {val}"
                return val

        def start_api(with_closed_broker=False):
            nonlocal proc
            cmd = [
                str(api_bin), "--initial", str(initial_json), "--working", str(working),
                "--auth-store", str(auth_store), "--service-token-file", str(svc_token),
                "--port", str(api_port),
            ]
            if with_closed_broker:
                cmd += ["--amqp-host", "127.0.0.1", "--amqp-port", str(closed_amqp_port), "--outbox-poll-ms", "200"]
            proc = subprocess.Popen(cmd, stdout=log_file, stderr=log_file)
            wait_until(lambda: call("/readyz")["listo"] is True, timeout=10, msg="API readyz")

        def stop_api():
            nonlocal proc
            if proc and proc.poll() is None:
                proc.terminate()
                proc.wait(timeout=10)
            proc = None

        def login(u):
            return call("/api/sesion/login", "POST", {"idUsuario": u, "password": passwords[u]})["token"]

        try:
            start_api(with_closed_broker=True)
            admin = login("Admin")
            for u in ("Ana", "Bruno"):
                call("/api/usuarios", "POST", {"idUsuario": u, "nombre": u + " Demo", "activo": True}, admin, 201)
                call("/api/cuentas", "POST", {"idUsuario": u, "rol": "USUARIO", "password": passwords[u]}, admin, 201)
            ana = login("Ana")
            bruno = login("Bruno")

            # 2. Validación de campos obligatorios, IDs, fechas y tipos JSON en servidor
            call("/api/ejemplares", "POST", {"idEjemplar": "E malo!", "idLibro": "L001"}, admin, 400)
            call("/api/ejemplares", "POST", {"idEjemplar": "E1", "idLibro": "L malo!"}, admin, 400)
            call("/api/ejemplares", "POST", {"idEjemplar": "E1", "idLibro": "L001"}, admin, 201)
            call("/api/ejemplares", "POST", {"idEjemplar": "E2", "idLibro": "L002"}, admin, 201)

            # Fechas inválidas / hoy / pasadas
            for bad_date in ("2026-02-30", "2026/12/01", today_utc.isoformat(), past_day, ""):
                call("/api/prestamos", "POST",
                     {"idPrestamo": "P-BAD", "idEjemplar": "E1", "idUsuario": "Ana", "vencimiento": bad_date}, admin, 400)
            # ID inválido en préstamo y solicitud
            call("/api/prestamos", "POST",
                 {"idPrestamo": "P malo!", "idEjemplar": "E1", "idUsuario": "Ana", "vencimiento": future1}, admin, 400)
            call("/api/solicitudes", "POST", {"idLibro": "L con espacio"}, ana, 400)
            # Content-Type incorrecto y JSON repetido/malformado
            call("/api/solicitudes", "POST", '{"idLibro":"L001"}', ana, 415, content_type="text/plain")
            call("/api/solicitudes", "POST", '{"idLibro":"L001","idLibro":"L002"}', ana, 400)
            call("/api/solicitudes", "POST", "{malformado", ana, 400)
            print("2. Validaciones de campos obligatorios, IDs, fechas, JSON y Content-Type OK.")

            # 3. Transiciones prohibidas: ejemplar ocupado, cancelación repetida, pago sobre cargo anulado
            call("/api/prestamos", "POST",
                 {"idPrestamo": "P1", "idEjemplar": "E1", "idUsuario": "Ana", "vencimiento": future1}, admin, 201)
            # Ejemplar ya prestado -> 409
            call("/api/prestamos", "POST",
                 {"idPrestamo": "P1-DUP", "idEjemplar": "E1", "idUsuario": "Bruno", "vencimiento": future1}, admin, 409)

            # Cola FIFO sobre L001: Ana en posición 1, Bruno en posición 2 -> validar posicionCola y aislamiento
            sol_ana = call("/api/solicitudes", "POST", {"idLibro": "L001"}, ana, 201)
            sol_bruno = call("/api/solicitudes", "POST", {"idLibro": "L001"}, bruno, 201)
            mib_ana = call("/api/mi-biblioteca", token=ana)
            mib_bruno = call("/api/mi-biblioteca", token=bruno)
            assert mib_ana["reservas"][0]["posicionCola"] == 1
            assert mib_bruno["reservas"][0]["posicionCola"] == 2
            assert all(r["idUsuario"] == "Bruno" for r in mib_bruno["reservas"])

            # Cancelación repetida de reserva -> primera 200, segunda 409
            call(f"/api/mis-reservas/{sol_bruno['idReserva']}/cancelacion", "POST", {"motivo": "Ya no lo necesito"}, bruno, 200)
            call(f"/api/mis-reservas/{sol_bruno['idReserva']}/cancelacion", "POST", {"motivo": "Repetida"}, bruno, 409)
            # Cancelación con motivo vacío -> 400
            call(f"/api/mis-reservas/{sol_ana['idReserva']}/cancelacion", "POST", {"motivo": "   "}, ana, 400)
            # Verificar fechaCierre y motivo en reserva cancelada de Bruno
            mib_bruno2 = call("/api/mi-biblioteca", token=bruno)
            assert mib_bruno2["reservas"][0]["fechaCierre"] and mib_bruno2["reservas"][0]["cancelacion"]["motivo"] == "Ya no lo necesito"

            # Pago sobre cargo anulado -> 409, sin comprobante ni aviso de pago
            call("/api/prestamos", "POST",
                 {"idPrestamo": "P-ANUL", "idEjemplar": "E2", "idUsuario": "Ana", "vencimiento": future1}, admin, 201)
            call("/api/prestamos/P-ANUL/anulacion", "POST", {"motivo": "Error de registro", "copiaRecuperada": True}, admin, 200)
            call("/api/mis-pagos/P-ANUL/simulacion", "POST", {}, ana, 409)
            assert all(c["idPrestamo"] != "P-ANUL" for c in call("/api/mi-biblioteca", token=ana)["comprobantes"])
            print("3. Transiciones prohibidas (ejemplar ocupado, cancelación repetida, pago en cargo anulado) OK.")

            # 4. Fallo de guardado en disco (solo lectura -> 503) NO adelanta estado en memoria
            root.chmod(0o500)
            try:
                err503 = call("/api/mis-pagos/P1/simulacion", "POST", {}, ana, 503)
                assert err503["resultado"] == "rechazado"
                mib_after_fail = call("/api/mi-biblioteca", token=ana)
                cargo_p1 = next(c for c in mib_after_fail["cargos"] if c["idPrestamo"] == "P1")
                assert cargo_p1["estado"] == "pendiente"
                assert all(c["idPrestamo"] != "P1" for c in mib_after_fail["comprobantes"])
            finally:
                root.chmod(0o700)

            # 4b. Corrupción externa del archivo JSON en disco -> HTTP 503 PersistenceError (NO 400 Json::exception)
            backup_bytes = working.read_bytes()
            working.write_text("{corrupto_en_disco")
            try:
                err_corrupt = call("/api/mis-pagos/P1/simulacion", "POST", {}, ana, 503)
                assert err_corrupt["resultado"] == "rechazado"
                cargo_p1 = next(c for c in call("/api/mi-biblioteca", token=ana)["cargos"] if c["idPrestamo"] == "P1")
                assert cargo_p1["estado"] == "pendiente"
            finally:
                working.write_bytes(backup_bytes)
            print("4. Fallos de persistencia (solo lectura y JSON corrupto) -> 503 sin adelantar estado en memoria OK.")

            # 5. Pago idempotente (doble clic) + RabbitMQ caído tras guardar: conserva pago y deja aviso pendiente
            pg1 = call("/api/mis-pagos/P1/simulacion", "POST", {}, ana, 200)
            pg1_retry = call("/api/mis-pagos/P1/simulacion", "POST", {}, ana, 200)
            assert pg1["idComprobante"] == pg1_retry["idComprobante"]
            mib_paid = call("/api/mi-biblioteca", token=ana)
            assert len([c for c in mib_paid["comprobantes"] if c["idPrestamo"] == "P1"]) == 1
            avisos_pago = [a for a in call("/api/avisos", token=admin)["items"] if a["tipo"] == "pago_simulado" and a["idPrestamo"] == "P1"]
            assert len(avisos_pago) == 1 and avisos_pago[0]["estado"] == "pendiente"

            # Anular P1 DESPUÉS de pagarlo -> cargo pasa a anulado y comprobante PG-1 se conserva en historial; reintentar pago da 409
            call("/api/prestamos/P1/anulacion", "POST", {"motivo": "Corrección posterior al pago", "copiaRecuperada": True}, admin, 200)
            mib_post_anul = call("/api/mi-biblioteca", token=ana)
            assert next(c for c in mib_post_anul["cargos"] if c["idPrestamo"] == "P1")["estado"] == "anulado"
            assert any(c["idComprobante"] == pg1["idComprobante"] for c in mib_post_anul["comprobantes"])
            call("/api/mis-pagos/P1/simulacion", "POST", {}, ana, 409)

            # 6. Aislamiento entre lectores y permisos
            call("/api/mis-pagos/P1", "GET", token=bruno, status=404)
            call("/api/mis-pagos/P1/simulacion", "POST", {}, bruno, 404)
            call("/api/pagos", "GET", token=ana, status=403)
            call("/api/avisos", "GET", token=ana, status=403)
            call("/api/prestamos", "POST",
                 {"idPrestamo": "P-NO", "idEjemplar": "E2", "idUsuario": "Ana", "vencimiento": future2}, ana, 403)

            # 7. Reinicio conserva comprobante, estado anulado y aviso pendiente sin secretos en logs
            stop_api()
            start_api(with_closed_broker=True)
            ana = login("Ana")
            admin = login("Admin")
            mib_restarted = call("/api/mi-biblioteca", token=ana)
            assert any(c["idComprobante"] == pg1["idComprobante"] for c in mib_restarted["comprobantes"])
            assert next(c for c in mib_restarted["cargos"] if c["idPrestamo"] == "P1")["estado"] == "anulado"
            stop_api()

            log_file.flush()
            log_txt = log_path.read_text(errors="replace")
            assert "[api] Error de persistencia" in log_txt, "Se esperaba traza técnica sanitizada en log del servidor"
            for pw in passwords.values():
                assert pw not in log_txt, "Contraseña filtrada en api.log"
            assert svc_token.read_text().strip() not in log_txt, "Token de servicio filtrado en api.log"
            print("5-7. Pago idempotente, cargo pagado luego anulado, broker caído, aislamiento, reinicio y logs limpios OK.")
        finally:
            root.chmod(0o700)
            stop_api()
            log_file.close()


if __name__ == "__main__":
    main()
