"""Recorrido con sesiones reales y archivos privados temporales; nunca usa nube."""
import json
import secrets
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

binary, initial = (Path(x).resolve() for x in sys.argv[1:3])
passwords = {u: secrets.token_urlsafe(24) for u in ("Admin", "Ana", "Bruno", "Carla")}
original = initial.read_bytes()
count = 0
observed = []

with tempfile.TemporaryDirectory(prefix="biblioteca-acceso-") as folder:
    root = Path(folder)
    working = root / "biblioteca.json"
    private = root / "privado" / "cuentas.json"
    args = [str(binary), "--initial", str(initial), "--working", str(working), "--auth-store", str(private)]
    bootstrap = subprocess.run(args + ["--bootstrap-admin", "Admin", "--admin-name", "Administración sintética"],
                               input=passwords["Admin"]+"\n", text=True, capture_output=True)
    assert bootstrap.returncode == 0, bootstrap.stderr
    assert private.stat().st_mode & 0o777 == 0o600
    assert private.parent.stat().st_mode & 0o777 == 0o700
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    base = f"http://127.0.0.1:{port}"
    output = open(root / "api.log", "w+")
    process = None

    def call(path, method="GET", body=None, token="", status=200, headers=None):
        global count
        count += 1
        h = {"Content-Type": "application/json", **(headers or {})}
        if token:
            h["Authorization"] = "Bearer "+token
        data = None if body is None else json.dumps(body).encode()
        try:
            response = urlopen(Request(base+path, data=data, method=method, headers=h), timeout=10)
        except HTTPError as error:
            response = error
        with response:
            raw = response.read()
            value = json.loads(raw) if raw else None
            if path != "/api/sesion/login":
                observed.append({"path":path.split("?",1)[0],"method":method.lower(),"status":str(response.status),"body":value})
            assert response.status == status, f"{method} {path}: {response.status}, esperaba {status}: {value}"
            return value

    def start(ttl=1800):
        global process
        process = subprocess.Popen(args+["--port",str(port),"--session-seconds",str(ttl)], stdout=output, stderr=output)
        for _ in range(200):
            assert process.poll() is None, "Backend terminó"
            try:
                call("/readyz")
                return
            except URLError:
                time.sleep(.03)
        raise AssertionError("No arrancó")

    def stop():
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=5)

    def login(user):
        result = call("/api/sesion/login", "POST", {"idUsuario":user,"password":passwords[user]})
        assert result["rol"] == ("ADMIN" if user == "Admin" else "USUARIO")
        return result["token"]

    def post(path, data, token, status=200):
        return call("/api/"+path, "POST", data, token, status)

    def own(token):
        return call("/api/mi-biblioteca", token=token)

    def snapshot():
        return call("/api/dominio",token=admin)

    def unchanged(path, data, token, status):
        before = snapshot()
        indexes = call("/api/estructuras",token=admin)
        disk = working.read_bytes()
        post(path,data,token,status)
        assert before == snapshot() and disk == working.read_bytes()
        assert indexes == call("/api/estructuras",token=admin)

    def valid():
        assert call("/api/estructuras/validacion",token=admin)["valido"]

    try:
        start(1)
        expired = login("Admin")
        time.sleep(1.1)
        call("/api/sesion",token=expired,status=401)
        stop()
        start()
        call("/api/sesion",token=expired,status=401)
        admin = login("Admin")
        assert call("/readyz") == {"listo":True}
        assert call("/healthz") == {"proceso":"activo"}
        call("/api/dominio",status=401)
        for user in ("Ana","Bruno","Carla"):
            post("usuarios",{"idUsuario":user,"nombre":user+" sintético","activo":True},admin,201)
            post("cuentas",{"idUsuario":user,"rol":"USUARIO","password":passwords[user]},admin,201)
        a, b, c = (login(u) for u in ("Ana","Bruno","Carla"))
        for route, method, data in (
            ("usuarios","GET",None),("cuentas","GET",None),("ejemplares","GET",None),
            ("estructuras","GET",None),("arbol","GET",None),("arbol/validacion","GET",None),
            ("dominio","GET",None),("dominio","PUT",{}),("recuperacion","POST",{}),
            ("libros","POST",{}),("prestamos","POST",{}),("reservas","GET",None),("pagos","GET",None),
            ("usuarios/Ana","DELETE",None),("almacenamiento","GET",None),("avisos","GET",None)
        ):
            call("/api/"+route,method,data,a,403)
        book = {"idLibro":"DEMO","titulo":"Atlas del campus","autor":"Autor sintético","temas":["campus"]}
        post("libros",book,admin,201)
        post("libros",{**book,"idLibro":"REL","titulo":"Campus relacionado"},admin,201)
        assert call("/api/libros/DEMO/relacionados",token=a)["total"] == 1
        edited={**book,"titulo":"Nuevo atlas del campus","temas":["nuevo"]}
        call("/api/libros/DEMO","PUT",edited,admin)
        assert call("/api/libros?prefijo=Nuevo",token=a)["libros"] == [edited]
        assert call("/api/libros?titulo=Atlas%20del%20campus",token=a)["total"] == 0
        assert call("/api/libros/DEMO/relacionados",token=a)["total"] == 0
        post("ejemplares",{"idEjemplar":"E1","idLibro":"DEMO"},admin,201)
        ra = post("solicitudes",{"idLibro":"DEMO"},a,201)
        assert ra["estado"] == "apartada" and ra["idUsuario"] == "Ana"
        unchanged("solicitudes",{"idLibro":"DEMO","idUsuario":"Bruno"},a,400)
        unchanged("solicitudes",{"idLibro":"DEMO"},a,409)
        pa={"idPrestamo":"PA","idEjemplar":"E1","idUsuario":"Ana","vencimiento":"2099-12-01"}
        post("prestamos",pa,admin,201)
        rb=post("solicitudes",{"idLibro":"DEMO"},b,201)
        rc=post("solicitudes",{"idLibro":"DEMO"},c,201)
        assert rb["llegada"] < rc["llegada"] and rb["estado"] == rc["estado"] == "pendiente"
        unchanged("prestamos",{**pa,"idPrestamo":"DOBLE","idUsuario":"Bruno"},admin,409)
        for route in ("mis-prestamos/PA","mis-pagos/PA","mis-reservas/"+ra["idReserva"]):
            call("/api/"+route,token=b,status=404)
        unchanged("mis-pagos/PA/simulacion",{},b,404)
        unchanged("mis-reservas/"+rb["idReserva"]+"/cancelacion",{"motivo":"Ajena"},a,404)
        receipt=post("mis-pagos/PA/simulacion",{},a)
        assert receipt["importe"] == 10000 and receipt["moneda"] == "CRC" and receipt["etiqueta"] == "SIMULADO"
        assert post("mis-pagos/PA/simulacion",{},a) == receipt
        assert snapshot()["ejemplares"][0]["estado"] == "prestado"
        assert len(own(a)["activos"]) == 1 and own(b)["activos"] == []
        assert len(own(b)["reservas"]) == 1 and own(b)["comprobantes"] == []
        valid()
        before=snapshot()
        stop(); start()
        call("/api/sesion",token=a,status=401)
        admin=login("Admin"); a=login("Ana"); b=login("Bruno"); c=login("Carla")
        assert snapshot() == before
        valid()
        print("Reinicio 1: cuentas, préstamo activo, pago único, turnos y cinco índices conservados.")
        post("prestamos/PA/devolucion",{},admin)
        assert own(b)["reservas"][0]["estado"] == "apartada"
        assert own(a)["prestamos"][0]["estado"] == "devuelto"
        post("mis-reservas/"+rb["idReserva"]+"/cancelacion",{"motivo":"Cambio de planes"},b)
        cancelled=own(b)["reservas"][0]
        assert cancelled["estado"] == "cancelada" and cancelled["cancelacion"]["responsable"] == "Bruno"
        assert own(c)["reservas"][0]["estado"] == "apartada"
        post("prestamos",{**pa,"idPrestamo":"PC","idUsuario":"Carla"},admin,201)
        post("mis-pagos/PC/simulacion",{},c)
        unchanged("prestamos/PC/anulacion",{"motivo":"Error","copiaRecuperada":False},admin,400)
        unchanged("prestamos/PC/anulacion",{"motivo":"Error","copiaRecuperada":True},c,403)
        post("prestamos/PC/anulacion",{"motivo":"La copia no salió del mostrador","copiaRecuperada":True},admin)
        assert own(c)["prestamos"][0]["estado"] == "cancelado" and own(c)["prestamos"][0]["devolucion"] == ""
        assert own(c)["cargos"][0]["estado"] == "anulado" and len(own(c)["comprobantes"]) == 1
        unchanged("prestamos/PC/devolucion",{},admin,409)
        unchanged("mis-pagos/PC/simulacion",{},c,409)
        post("prestamos",{**pa,"idPrestamo":"PC2","idUsuario":"Carla"},admin,201)
        post("prestamos/PC2/renovacion",{"vencimiento":"2100-01-01"},admin)
        post("prestamos/PC2/devolucion",{},admin)
        valid()
        before=snapshot()
        root.chmod(0o500)
        try:
            unchanged("solicitudes",{"idLibro":"DEMO"},a,503)
            unchanged("mis-pagos/PC2/simulacion",{},c,503)
        finally:
            root.chmod(0o700)
        assert snapshot() == before
        accounts_before=call("/api/cuentas",token=admin)
        private_before=private.read_bytes()
        private.parent.chmod(0o500)
        try:
            call("/api/cuentas/Ana","PUT",{"rol":"USUARIO","activa":True,"password":secrets.token_urlsafe(24)},admin,503)
        finally:
            private.parent.chmod(0o700)
        assert call("/api/cuentas",token=admin) == accounts_before and private.read_bytes() == private_before
        assert call("/api/sesion",token=a)["idUsuario"] == "Ana"
        call("/api/usuarios/Bruno","PUT",{"idUsuario":"Bruno","nombre":"Bruno sintético","activo":False},admin)
        call("/api/mi-biblioteca",token=b,status=401)
        call("/api/sesion/login","POST",{"idUsuario":"Bruno","password":passwords["Bruno"]},status=401)
        call("/api/usuarios/Admin","PUT",{"idUsuario":"Admin","nombre":"Admin","activo":False},admin,409)
        post("sesion/logout",{},a,204)
        call("/api/sesion",token=a,status=401)
        before=snapshot(); stop(); start(); admin=login("Admin"); c=login("Carla")
        assert snapshot() == before and len(own(c)["prestamos"]) == 2
        assert snapshot()["ejemplares"][0]["estado"] == "disponible"
        valid()
        print("Reinicio 2: devolución, cancelación FIFO, anulación auditada y comprobantes conservados.")
        generic1=post("sesion/login",{"idUsuario":"NoExiste","password":passwords["Ana"]},"",401)
        generic2=post("sesion/login",{"idUsuario":"Ana","password":"incorrecta"},"",401)
        assert generic1 == generic2
        for _ in range(3):
            post("sesion/login",{"idUsuario":"Ana","password":"incorrecta"},"",401)
        post("sesion/login",{"idUsuario":"Ana","password":"incorrecta"},"",429)
        business=json.dumps(snapshot())
        for secret in [*passwords.values(),a,b,c,admin]:
            assert secret not in business
        assert "argon2" not in business and initial.read_bytes() == original
        legacy=root/"legacy.json"
        legacy.write_bytes(original)
        old_args=[str(binary),"--initial",str(initial),"--working",str(legacy),
                  "--auth-store",str(root/"privado-migracion"/"cuentas.json"),
                  "--bootstrap-admin","Admin","--admin-name","Admin de migración"]
        rejected=subprocess.run(old_args,input=passwords["Admin"]+"\n",text=True,capture_output=True)
        assert rejected.returncode!=0 and legacy.read_bytes()==original
        migrated=subprocess.run(old_args+["--migrate"],input=passwords["Admin"]+"\n",text=True,capture_output=True)
        assert migrated.returncode==0,migrated.stderr
        recovered=json.loads(legacy.read_text())
        assert recovered["version"]==2 and len(recovered["libros"])==16 and recovered["dominio"]["usuarios"][0]["idUsuario"]=="Admin"
        print("Migración local explícita: rechazo conserva archivo; actualización conserva las 16 obras.")
        print(f"OK: {count} solicitudes HTTP; autenticación, aislamiento, FIFO, pagos, anulación, reinicios y fallos sin cambios parciales.")
        if "--contract-log" in sys.argv:
            Path(sys.argv[sys.argv.index("--contract-log")+1]).write_text(json.dumps(observed))
    finally:
        root.chmod(0o700)
        stop()
        output.seek(0)
        log=output.read()
        output.close()
        assert "ERROR: AddressSanitizer" not in log and "runtime error:" not in log
        for secret in passwords.values():
            assert secret not in log
