"""Prueba de integración local con un archivo y un puerto exclusivos de la ejecución."""
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

binary, initial = map(Path, sys.argv[1:3])
initial_bytes = initial.read_bytes()


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    working = root / "catalogo.json"
    port = free_port()
    base = f"http://127.0.0.1:{port}/api/"
    args = [str(binary), "--initial", str(initial), "--working", str(working)]

    def request(route, method="GET", body=None):
        payload = None if body is None else json.dumps(body).encode()
        req = Request(base + route, data=payload, method=method,
                      headers={"Content-Type": "application/json"})
        try:
            response = urlopen(req, timeout=2)
        except HTTPError as error:
            response = error
        with response:
            data = response.read()
            return response.status, json.loads(data) if data else None

    def start():
        process = subprocess.Popen(args + ["--port", str(port)])
        try:
            for _ in range(100):
                if process.poll() is not None:
                    raise RuntimeError("La API terminó durante el arranque")
                try:
                    if request("libros")[0] == 200:
                        return process
                except URLError:
                    time.sleep(0.03)
            raise RuntimeError("La API no quedó lista")
        except BaseException:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=3)
            raise

    book = {"idLibro": "PRUEBA-HTTP", "titulo": "Obra temporal", "autor": "Autor", "temas": []}
    process = start()
    try:
        assert len(request("libros")[1]["libros"]) == 16
        assert subprocess.run(args + ["--port", str(free_port())], timeout=3).returncode != 0
        assert request("libros", "POST", book)[0] == 201
        assert request("libros", "POST", book)[0] == 409
        assert request("libros?prefijo=OBRA")[1]["libros"] == [book]
        assert request("arbol/validacion")[1]["valido"]
        assert request("libros/ausente", "DELETE")[0] == 404
        assert request("libros", "POST", {"titulo": "incompleto"})[0] == 400
        # Fuerza un fallo de escritura en un directorio privado de la prueba.
        root.chmod(0o500)
        try:
            if os.geteuid() != 0:
                assert request("libros/PRUEBA-HTTP", "DELETE")[0] == 503
                assert request("libros?titulo=Obra%20temporal")[1]["libros"] == [book]
        finally:
            root.chmod(0o700)
    finally:
        process.terminate()
        process.wait(timeout=3)

    process = start()
    try:
        assert request("libros?titulo=Obra%20temporal")[1]["libros"] == [book]
        assert request("libros/PRUEBA-HTTP", "DELETE")[0] == 204
        assert request("libros?prefijo=Obra")[1]["libros"] == []
    finally:
        process.terminate()
        process.wait(timeout=3)

    working.write_text("[]")
    process = start()
    try:
        assert request("libros")[1]["libros"] == []
    finally:
        process.terminate()
        process.wait(timeout=3)
    working.write_text("{")
    assert subprocess.run(args + ["--port", str(port)], timeout=3).returncode != 0
    assert working.read_text() == "{"
    assert initial.read_bytes() == initial_bytes
    print("API HTTP: CRUD, errores, recuperación, reinicio y catálogo vacío comprobados.")
