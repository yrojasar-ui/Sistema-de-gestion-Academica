"""Aceptación HTTP aislada. Sin API externa, con puertos y archivos temporales."""
import copy
from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import socket
import secrets
import subprocess
import sys
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

binary, initial = map(lambda p: Path(p).resolve(), sys.argv[1:3])
demo = "--demo" in sys.argv[3:]
original = initial.read_bytes()
requests = 0
observed = []
future = lambda days: (datetime.now(timezone.utc).date() + timedelta(days=days)).isoformat()


def check(condition, message):
    if not condition:
        raise AssertionError(message)


with tempfile.TemporaryDirectory(prefix="biblioteca-dominio-") as folder:
    root = Path(folder)
    working = root / "libros.json"
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    base = f"http://127.0.0.1:{port}"
    password = secrets.token_urlsafe(24)
    auth_store = root / "privado" / "cuentas.json"
    args = [str(binary), "--initial", str(initial), "--working", str(working), "--auth-store", str(auth_store)]
    subprocess.run(args + ["--bootstrap-admin", "Admin", "--admin-name", "Administrador sintético"],
                   input=password+"\n", text=True, check=True, capture_output=True)
    token = ""
    process = None
    output = open(root / "servidor.log", "w+")

    def call(route, method="GET", data=None, expected=200, headers=None):
        global requests
        requests += 1
        payload = None if data is None else json.dumps(data).encode()
        req = Request(base + route, method=method, data=payload,
                      headers={"Content-Type": "application/json", "Authorization":"Bearer "+token, **(headers or {})})
        try:
            result = urlopen(req, timeout=8)
        except HTTPError as e:
            result = e
        with result:
            raw = result.read()
            value = json.loads(raw) if raw else None
            if route != "/api/sesion/login": observed.append({"path":route.split("?",1)[0], "method":method.lower(), "status":str(result.status), "body":value})
            check(result.status == expected, f"{method} {route}: {result.status} {value}, esperado {expected}")
            return value

    def start():
        global process, token
        process = subprocess.Popen(args + ["--port", str(port)], stdout=output, stderr=output)
        for _ in range(200):
            if process.poll() is not None:
                raise RuntimeError("El backend terminó durante el arranque")
            try:
                call("/readyz")
                token=call("/api/sesion/login","POST",{"idUsuario":"Admin","password":password})["token"]
                return
            except URLError:
                time.sleep(.03)
        raise RuntimeError("Timeout de arranque")

    def stop():
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=5)

    def domain():
        return call("/api/dominio")

    def integrity():
        d = domain()
        s = call("/api/estructuras")
        check(call("/api/estructuras/validacion")["valido"], "Validador del servicio")
        users = {u["idUsuario"]: u for u in d["usuarios"]}
        copies = {c["idEjemplar"]: c for c in d["ejemplares"]}
        loans = [p for p in d["prestamos"] if not p["devolucion"]]
        check(len({p["idEjemplar"] for p in loans}) == len(loans), "Dos préstamos de la copia")
        rn = s["prestamos"]
        keys = sorted([p["vencimiento"], p["idPrestamo"]] for p in loans)
        check(sorted(n["clave"] for n in rn["nodos"]) == keys, "Préstamos/RN")
        nodes = {tuple(n["clave"]): n for n in rn["nodos"]}

        def black_height(key, parent=None, low=None, high=None):
            if key is None:
                return 1
            node = nodes[tuple(key)]
            check(node["padre"] == parent, "Padres RN")
            check((low is None or low < key) and (high is None or key < high), "Orden RN")
            if node["color"] == "rojo":
                for child in (node["izq"], node["der"]):
                    check(child is None or nodes[tuple(child)]["color"] == "negro", "Rojo/rojo")
            left = black_height(node["izq"], key, low, key)
            right = black_height(node["der"], key, key, high)
            check(left == right, "Altura negra")
            return left + (node["color"] == "negro")

        check(black_height(rn["raiz"]) == rn["alturaNegra"], "Altura exportada")
        check(rn["raiz"] is None or nodes[tuple(rn["raiz"])]["color"] == "negro", "Raíz negra")
        check(call("/api/prestamos?estado=activos")["items"] == sorted(loans, key=lambda p: (p["vencimiento"], p["idPrestamo"])), "Consulta por RN")
        hashed = [u for b in s["usuarios"]["cubetas"] for u in b["usuarios"]]
        check(sorted(hashed) == sorted(users), "Usuarios/hash")
        for uid, u in users.items():
            check(call("/api/usuarios/" + uid) == u, "Consulta individual por hash")
        reservations = {r["idReserva"]: r for r in d["reservas"]}
        for p in loans:
            check(users[p["idUsuario"]]["activo"] and copies[p["idEjemplar"]]["estado"] == "prestado", "Referencias préstamo")
        for c in copies.values():
            active = [p for p in loans if p["idEjemplar"] == c["idEjemplar"]]
            held = [r for r in reservations.values() if r["estado"] == "apartada" and r["idEjemplar"] == c["idEjemplar"]]
            check((c["estado"] == "prestado") == bool(active), "Estado prestado")
            check((c["estado"] == "apartado") == bool(held), "Estado apartado")
        queues = {q["idLibro"]: q for q in s["reservas"]}
        for book in {r["idLibro"] for r in reservations.values()}:
            expected = sorted((r["llegada"], r["idReserva"]) for r in reservations.values()
                              if r["idLibro"] == book and r["estado"] == "pendiente")
            actual = [(n["llegada"], n["id"]) for n in queues[book]["nodos"]
                      if reservations[n["id"]]["estado"] == "pendiente"]
            check(actual == expected, "Turnos FIFO")
        return d

    def mutate(route, method, data=None, status=200):
        result = call("/api/" + route, method, data, status)
        integrity()
        return result

    def reject(route, method, data=None, status=409):
        before = domain(), call("/api/estructuras"), working.read_bytes()
        call("/api/" + route, method, data, status)
        check(before == (domain(), call("/api/estructuras"), working.read_bytes()), "El rechazo cambió datos o índices")

    def person(uid, active=True):
        return {"idUsuario": uid, "nombre": uid, "activo": active}

    def loan(pid, uid, eid="E1", days=7):
        return {"idPrestamo": pid, "idEjemplar": eid, "idUsuario": uid, "vencimiento": future(days)}

    try:
        start()
        check(len(call("/api/libros")["libros"]) == 16, "Catálogo original")
        book = {"idLibro": "DEMO-L", "titulo": "Atlas del campus", "autor": "Autor sintético", "temas": ["mapas"]}
        mutate("libros", "POST", book, 201)
        for uid in ("Ana", "Bruno", "Carla"):
            mutate("usuarios", "POST", person(uid), 201)
        mutate("ejemplares", "POST", {"idEjemplar": "E1", "idLibro": "DEMO-L"}, 201)
        mutate("prestamos", "POST", loan("P-Ana", "Ana"), 201)
        print("A. Ana recibe E1; un préstamo activo en el Rojo-Negro.")
        for rid, uid in (("R-Bruno", "Bruno"), ("R-Carla", "Carla")):
            mutate("reservas", "POST", {"idReserva": rid, "idLibro": "DEMO-L", "idUsuario": uid}, 201)
        check([r["llegada"] for r in domain()["reservas"]] == [1, 2], "Orden asignado por servicio")
        print("B. Bruno reserva con turno 1 y Carla con turno 2.")
        reject("prestamos", "POST", loan("P-Doble", "Bruno"))
        print("C. Segundo préstamo rechazado con 409; datos, archivo e índices intactos.")

        # Reconstruir después de reiniciar, con filas en orden inverso.
        exported = domain()
        (root / "dominio.json").write_text(json.dumps(exported))
        stop()
        start()
        check(domain() == exported, "Repositorio local persistente tras reinicio")
        check(call("/api/libros/DEMO-L") == book, "Libro persistido en JSON")
        imported = json.loads((root / "dominio.json").read_text())
        imported["reservas"].reverse()
        mutate("dominio", "PUT", imported)
        check(call("/api/estructuras")["reservas"][0]["inicio"] == "R-Bruno", "Recuperar el orden por llegada")

        mutate("prestamos/P-Ana/devolucion", "POST", {})
        d = domain()
        check(d["prestamos"][0]["devolucion"] and d["ejemplares"][0]["estado"] == "apartado", "Devolución conserva historial")
        check(call("/api/reservas/R-Bruno")["estado"] == "apartada", "Bruno recibe copia")
        print("D. Ana devuelve; historial conservado, RN vacío y E1 apartada para Bruno.")
        mutate("reservas/R-Bruno/cancelacion", "POST", {"motivo":"Cambio de planes"})
        check(call("/api/reservas/R-Carla")["estado"] == "apartada", "Carla recibe siguiente turno")
        print("E. Bruno cancela; E1 queda apartada para Carla.")
        mutate("prestamos", "POST", loan("P-Carla", "Carla"), 201)
        check(call("/api/reservas/R-Carla")["estado"] == "atendida", "Reserva atendida")
        mutate("prestamos/P-Carla/devolucion", "POST", {})
        check(call("/api/ejemplares/E1")["estado"] == "disponible", "Copia devuelta")
        check(len(call("/api/prestamos")["items"]) == 2 and call("/api/prestamos?estado=activos")["total"] == 0, "Dos históricos y cero activos")
        print("F. Carla recibe y devuelve; dos préstamos históricos y E1 disponible.")
        print("G. Estados, referencias, hash, Rojo-Negro y FIFO comprobados tras cada operación.")

        if not demo:
            mutate("usuarios", "POST", person("Inactivo", False), 201)
            reject("prestamos", "POST", loan("P-Inactivo", "Inactivo"))
            reject("reservas", "POST", {"idReserva": "R-I", "idLibro": "DEMO-L", "idUsuario": "Inactivo"})
            reject("prestamos", "POST", {**loan("P-Fecha", "Ana"), "vencimiento": "2026-02-30"}, 400)
            reject("prestamos", "POST", loan("P-Hoy", "Ana", days=0), 400)
            mutate("prestamos", "POST", loan("P-Nueva", "Ana"), 201)
            reject("usuarios/Ana", "DELETE")
            reject("usuarios/Ana", "PUT", person("Ana", False))
            reject("ejemplares/E1", "DELETE")
            reject("libros/DEMO-L", "DELETE")
            for rid, uid in (("R2-B", "Bruno"), ("R2-C", "Carla")):
                mutate("reservas", "POST", {"idReserva": rid, "idLibro": "DEMO-L", "idUsuario": uid}, 201)
            reject("reservas", "POST", {"idReserva": "R-Dup", "idLibro": "DEMO-L", "idUsuario": "Bruno"})
            reject("reservas", "POST", {"idReserva": "R-Turno", "idLibro": "DEMO-L", "idUsuario": "Ana", "llegada": 0}, 400)
            mutate("ejemplares", "POST", {"idEjemplar": "E2", "idLibro": "DEMO-L"}, 201)
            check(call("/api/reservas/R2-B")["idEjemplar"] == "E2", "Segunda copia atiende primer turno")
            reject("prestamos", "POST", loan("P-FueraTurno", "Carla", "E2"))
            mutate("usuarios/Bruno", "PUT", person("Bruno", False))
            check(call("/api/reservas/R2-C")["idEjemplar"] == "E2", "Desactivación libera apartado")
            mutate("prestamos/P-Nueva/renovacion", "POST", {"vencimiento": future(14)})
            check(call("/api/vencimientos?hasta=" + future(10))["total"] == 0, "Clave vieja retirada")
            check(call("/api/vencimientos?hasta=" + future(14))["total"] == 1, "Clave nueva insertada")
            reject("prestamos/P-Nueva/renovacion", "POST", {"vencimiento": future(7)}, 400)
            mutate("prestamos", "POST", loan("P-E2", "Carla", "E2"), 201)
            check(call("/api/prestamos?estado=activos")["total"] == 2, "Dos copias del mismo libro prestadas")
            reject("reservas", "POST", {"idReserva": "X", "idLibro": "DEMO-L", "idUsuario": "ausente"}, 404)

            related = {**book, "idLibro": "RELACION", "titulo": "Plano"}
            mutate("libros", "POST", related, 201)
            check(call("/api/libros/DEMO-L/relacionados")["items"] == [related], "Grafo temas compartidos")
            edited = {**book, "titulo": "Geografía del campus", "temas": ["geografia"]}
            mutate("libros/DEMO-L", "PUT", edited)
            check(call("/api/libros?titulo=Atlas%20del%20campus")["libros"] == [], "Título viejo ausente")
            check(call("/api/libros?prefijo=geografia")["libros"] == [edited], "Título nuevo vía B+")
            check(call("/api/libros/DEMO-L/relacionados")["items"] == [], "Arista retirada")
            check(call("/api/libros/RELACION/relacionados")["items"] == [], "Simetría tras edición")
            mutate("libros/RELACION", "DELETE", status=204)

            bad = copy.deepcopy(domain())
            bad["siguienteLlegada"] = 1
            reject("dominio", "PUT", bad)
            bad = copy.deepcopy(domain())
            bad["prestamos"].append({**bad["prestamos"][-1], "idPrestamo": "P-Duplicado"})
            reject("dominio", "PUT", bad)
            mutate("prestamos/P-Nueva/devolucion", "POST", {})
            mutate("prestamos/P-E2/devolucion", "POST", {})
            reject("prestamos/P-Nueva/devolucion", "POST", {})
            reject("prestamos/P-Nueva/renovacion", "POST", {"vencimiento": future(21)})
            mutate("ejemplares/E1", "DELETE")
            check(call("/api/ejemplares/E1")["estado"] == "retirado", "Retirada conserva historial")
            reject("prestamos", "POST", loan("P-Retirada", "Ana"))
            mutate("usuarios/Ana", "DELETE")
            check(not call("/api/usuarios/Ana")["activo"], "Desactivación conserva historial")
            mutate("usuarios/Inactivo", "DELETE")
            call("/api/usuarios/Inactivo", expected=404)
            empty_book = {**book, "idLibro": "BORRABLE"}
            mutate("libros", "POST", empty_book, 201)
            mutate("ejemplares", "POST", {"idEjemplar": "E-B", "idLibro": "BORRABLE"}, 201)
            mutate("ejemplares/E-B", "DELETE")
            call("/api/ejemplares/E-B", expected=404)
            mutate("libros/BORRABLE", "DELETE", status=204)
            call("/api/libros/BORRABLE", expected=404)
            check(call("/api/usuarios?limite=1&desplazamiento=1")["items"] == [domain()["usuarios"][1]], "Paginación")
            call("/api/usuarios?limite=0", expected=400)
            call("/api/usuarios?campo=otro", expected=400)
            call("/healthz", headers={"Origin": "https://ajeno.invalid"}, expected=403)
            call("/healthz", headers={"Origin": "http://localhost:5173"})
            # Fallo de guardado de libros conserva también grafo, B+ y dominio.
            root.chmod(0o500)
            try:
                if os.geteuid() != 0:
                    reject("libros/DEMO-L", "PUT", book, 503)
            finally:
                root.chmod(0o700)
            print("Adicionales OK: inactivos, duplicados, dos copias, desactivación, renovación, edición, referencias, importación inválida, errores y guardado fallido.")
        check(initial.read_bytes() == original, "Fixture inicial alterado")
        if "--contract-log" in sys.argv:
            Path(sys.argv[sys.argv.index("--contract-log") + 1]).write_text(json.dumps(observed))
        print(f"OK solicitudes HTTP={requests}; datos temporales; avisos pendientes persistentes.")
    finally:
        stop()
        output.seek(0)
        server_log = output.read()
        output.close()
        if "ERROR: AddressSanitizer" in server_log or "runtime error:" in server_log:
            raise RuntimeError(server_log)
        if sys.exc_info()[0]:
            print(server_log, file=sys.stderr)
