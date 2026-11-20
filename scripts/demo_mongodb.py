#!/usr/bin/env python3
"""Demostración optativa contra nube real; no la ejecuta CTest ni CI.
Solo colección nueva con nombre generado y entidades sintéticas; elimina su único documento.
"""
import argparse
from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import shutil
import socket
import secrets
import subprocess
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen
from uuid import uuid4

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--binary", default="build/debug/catalogo_api")
parser.add_argument("--evidence", required=True, help="Directorio nuevo para registros y checkpoints")
args = parser.parse_args()
binary = Path(args.binary).resolve()
evidence = Path(args.evidence)
evidence.mkdir(parents=True, exist_ok=False)
endpoint = "https://paginas-web-cr.com/Api/apis/mongodb.php"
collection = "biblioteca_yoel_p4_" + uuid4().hex
root = Path(tempfile.mkdtemp(prefix="biblioteca-mongo-demo-"))
log = (evidence / "nube.log").open("w", encoding="utf-8")
server_log = (evidence / "backend.log").open("w", encoding="utf-8")
process = None
password = secrets.token_urlsafe(24)
token = ""
oid = None
cleaned = False
with socket.socket() as sock:
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
base = f"http://127.0.0.1:{port}"
future = lambda days: (datetime.now(timezone.utc).date() + timedelta(days=days)).isoformat()


def record(message, value=None):
    line = datetime.now(timezone.utc).isoformat() + " " + message
    if value is not None:
        line += " " + json.dumps(value, ensure_ascii=False, sort_keys=True)
    print(line, flush=True)
    log.write(line + "\n")
    log.flush()


def request(url, method="GET", data=None, expected=200, authorization=""):
    req = Request(url, data=None if data is None else json.dumps(data).encode(),
                  method=method, headers={"Content-Type": "application/json", **({"Authorization":"Bearer "+authorization} if authorization else {})})
    try:
        response = urlopen(req, timeout=90)
    except HTTPError as error:
        response = error
    with response:
        body = response.read()
        assert response.status == expected, (method, url, response.status, body)
        if body:
            assert response.headers.get_content_type() == "application/json"
            return json.loads(body)


def remote():
    result = request(endpoint + "?coleccion=" + collection)
    assert result["coleccion"] == collection and result["total"] == len(result["documentos"])
    return result["documentos"]


def call(path, method="GET", data=None, expected=200):
    result = request(base + path, method, data, expected, token)
    record(f"{method} {path} HTTP {expected}", result)
    return result


def start(initialize=False):
    global process, token
    command = [str(binary), "--storage", "mongo", "--endpoint", endpoint,
               "--collection", collection, "--mongo-state", str(root / "estado"),
               "--port", str(port), "--auth-store", str(root / "privado" / "cuentas.json")]
    if initialize:
        bootstrap=subprocess.run(command+["--initialize","--bootstrap-admin","DemoAdmin","--admin-name","Administrador sintético"],
                                 input=password+"\n",text=True,capture_output=True)
        assert bootstrap.returncode==0, "Falló bootstrap privado (no se registran credenciales)"
    process = subprocess.Popen(command, stdout=server_log, stderr=server_log)
    for _ in range(300):
        assert process.poll() is None, "Backend terminó durante el arranque"
        try:
            ready = call("/readyz")
            assert ready["listo"]
            # No pasar login por record(): su respuesta contiene la sesión efímera.
            token=request(base+"/api/sesion/login","POST",{"idUsuario":"DemoAdmin","password":password})["token"]
            assert call("/api/almacenamiento")["almacenamiento"] == "mongo"
            return
        except URLError:
            time.sleep(.1)
    raise RuntimeError("Timeout arrancando backend")


def stop():
    if process and process.poll() is None:
        process.terminate()
        process.wait(timeout=10)


def integrity(label):
    state = call("/api/dominio")
    snapshot = call("/api/estructuras")
    assert call("/api/estructuras/validacion")["valido"]
    active = [p for p in state["prestamos"] if p["estado"] == "activo"]
    assert sorted(n["clave"] for n in snapshot["prestamos"]["nodos"]) == sorted(
        [p["vencimiento"], p["idPrestamo"]] for p in active)
    assert sorted(u for b in snapshot["usuarios"]["cubetas"] for u in b["usuarios"]) == sorted(
        u["idUsuario"] for u in state["usuarios"])
    reservations = {r["idReserva"]: r for r in state["reservas"]}
    for queue in snapshot["reservas"]:
        actual = [(n["llegada"], n["id"]) for n in queue["nodos"]
                  if reservations[n["id"]]["estado"] == "pendiente"]
        expected = sorted((r["llegada"], r["idReserva"]) for r in state["reservas"]
                          if r["idLibro"] == queue["idLibro"] and r["estado"] == "pendiente")
        assert actual == expected, (actual, expected)
    assert len({a["idAviso"] for a in state["avisos"]}) == len(state["avisos"])
    (evidence / (label + ".json")).write_text(json.dumps(
        {"dominio": state, "estructuras": snapshot}, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return state


def mutation(path, method="POST", data=None, expected=200):
    value = call("/api/" + path, method, data, expected)
    assert call("/api/estructuras/validacion")["valido"]
    return value


def restart(label):
    before = call("/api/dominio"), call("/api/libros")
    stop()
    start()
    after = call("/api/dominio"), call("/api/libros")
    assert before == after, "El reinicio cambió entidades, secuencias, avisos o libros"
    state = integrity(label)
    record("REINICIO OK: " + label, {"prestamos": len(state["prestamos"]),
           "activos": sum(not p["devolucion"] for p in state["prestamos"]),
           "reservas": [(r["idUsuario"], r["estado"], r["llegada"]) for r in state["reservas"]],
           "siguienteLlegada": state["siguienteLlegada"], "avisos": [a["idAviso"] for a in state["avisos"]]})
    return state


try:
    record("Demo real; ámbito sintético exclusivo", {"endpoint": endpoint, "coleccion": collection})
    assert remote() == [], "La colección debe estar vacía antes de inicializar"
    record("GET previo: HTTP 200 application/json, total=0")
    start(True)
    docs = remote()
    assert len(docs) == 1 and docs[0]["proyecto"] == collection and docs[0]["idBiblioteca"] == "principal"
    oid = docs[0]["_id"]["$oid"]
    book = {"idLibro": "P4-L", "titulo": "Atlas del campus", "autor": "Autor sintético", "temas": ["campus"]}
    mutation("libros", data=book, expected=201)
    for name in ("Ana", "Bruno", "Carla"):
        mutation("usuarios", data={"idUsuario": name, "nombre": name + " sintética", "activo": True}, expected=201)
    mutation("ejemplares", data={"idEjemplar": "P4-E", "idLibro": "P4-L"}, expected=201)
    mutation("prestamos", data={"idPrestamo": "P4-PA", "idEjemplar": "P4-E", "idUsuario": "Ana",
                                "vencimiento": future(7)}, expected=201)
    for name in ("Bruno", "Carla"):
        mutation("reservas", data={"idReserva": "P4-R-" + name, "idLibro": "P4-L", "idUsuario": name}, expected=201)
    before = call("/api/dominio")
    call("/api/prestamos", "POST", {"idPrestamo": "RECHAZADO", "idEjemplar": "P4-E",
         "idUsuario": "Bruno", "vencimiento": future(7)}, 409)
    assert call("/api/dominio") == before
    # Edición y renovación quedan incluidas en el primer reinicio real.
    mutation("prestamos/P4-PA/renovacion", data={"vencimiento": future(14)})
    related = {**book, "idLibro": "P4-REL", "titulo": "Plano sintético"}
    mutation("libros", data=related, expected=201)
    assert call("/api/libros/P4-L/relacionados")["items"] == [related]
    edited = {**book, "titulo": "Geografía del campus", "temas": ["geografia"]}
    mutation("libros/P4-L", "PUT", edited)
    first = restart("reinicio-1-prestamo")
    assert first["ejemplares"][0]["estado"] == "prestado"
    assert [(r["idUsuario"], r["llegada"]) for r in first["reservas"]] == [("Bruno", 1), ("Carla", 2)]
    assert first["prestamos"][0]["vencimiento"] == future(14)
    assert call("/api/libros?titulo=Atlas%20del%20campus")["total"] == 0
    assert call("/api/libros?prefijo=geografia")["libros"] == [edited]
    assert call("/api/libros/P4-L/relacionados")["items"] == []
    assert call("/api/libros/P4-REL/relacionados")["items"] == []
    mutation("prestamos/P4-PA/devolucion", data={})
    second = restart("reinicio-2-apartado")
    assert second["prestamos"][0]["devolucion"]
    assert second["ejemplares"][0]["estado"] == "apartado"
    assert second["reservas"][0]["estado"] == "apartada" and second["reservas"][1]["estado"] == "pendiente"
    assert any(a["idReserva"] == "P4-R-Bruno" and a["estado"] == "pendiente" for a in second["avisos"])
    mutation("reservas/P4-R-Bruno/cancelacion", data={"motivo":"Cancelación sintética de demostración"})
    assert call("/api/reservas/P4-R-Carla")["estado"] == "apartada"
    mutation("prestamos", data={"idPrestamo": "P4-PC", "idEjemplar": "P4-E", "idUsuario": "Carla",
                                "vencimiento": future(7)}, expected=201)
    mutation("prestamos/P4-PC/devolucion", data={})
    third = restart("reinicio-3-historial")
    assert third["ejemplares"][0]["estado"] == "disponible"
    assert len(third["prestamos"]) == 2 and all(p["devolucion"] for p in third["prestamos"])
    assert all(a["estado"] == "pendiente" for a in third["avisos"])
    mutation("reservas", data={"idReserva": "P4-R-Nueva", "idLibro": "P4-L", "idUsuario": "Ana"}, expected=201)
    fourth = restart("reinicio-4-secuencia")
    assert fourth["reservas"][-1]["llegada"] == 3 and fourth["siguienteLlegada"] == 4
    assert call("/api/reservas/P4-R-Nueva")["estado"] == "apartada"
    record("ACEPTACIÓN COMPLETA: cuatro reinicios automáticos sin importación/exportación; índices válidos")
finally:
    stop()
    try:
        # Nunca borrar toda la colección ni registros que no se identificaron como propios.
        if oid:
            docs = remote()
            assert len(docs) == 1 and docs[0]["_id"]["$oid"] == oid and docs[0]["proyecto"] == collection
            result = request(endpoint, "DELETE", {"coleccion": collection,
                "filtro": {"_id": oid, "proyecto": collection, "idBiblioteca": "principal"}})
            record("DELETE del único documento creado por esta demo", result)
            assert result["eliminados"] == 1 and remote() == []
            cleaned = True
            record("GET final: total=0; limpieza propia confirmada")
        else:
            record("No hay ObjectId confirmado: no se ejecuta DELETE. Revisar ámbito y diario", str(root))
    finally:
        server_log.close()
        log.close()
        if cleaned:
            shutil.rmtree(root)
        else:
            print("Se conserva diario para recuperación:", root)
