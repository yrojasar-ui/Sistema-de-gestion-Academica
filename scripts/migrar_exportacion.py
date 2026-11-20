#!/usr/bin/env python3
"""Convierte una exportación v1 a v2 sin sobrescribir archivos. La API valida referencias al importar."""
import json
import os
import sys
from pathlib import Path

source, target = map(Path,sys.argv[1:3])
if source.is_symlink() or not source.is_file() or source.stat().st_size > 1024*1024:
    raise SystemExit("Origen inválido")
state=json.loads(source.read_text())
if set(state)!={"version","usuarios","ejemplares","prestamos","reservas","avisos","siguienteLlegada"} or state["version"]!=1:
    raise SystemExit("Se requiere exportación de dominio v1")
empty={"fecha":"","motivo":"","responsable":""}
for p in state["prestamos"]:
    if set(p)!={"idPrestamo","idEjemplar","idUsuario","inicio","vencimiento","devolucion"}:
        raise SystemExit("Préstamo incompatible")
    p["estado"]="devuelto" if p["devolucion"] else "activo"
    p["cancelacion"]=empty.copy()
for r in state["reservas"]:
    if set(r)!={"idReserva","idLibro","idUsuario","llegada","estado","idEjemplar","creada"}:
        raise SystemExit("Reserva incompatible")
    r["cancelacion"]={"fecha":r["creada"],"motivo":"Migración v1: fecha y responsable originales no registrados","responsable":"migracion"} if r["estado"]=="cancelada" else empty.copy()
state.update(version=2,cargos=[],comprobantes=[])
with os.fdopen(os.open(target,os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600),"w") as f:
    json.dump(state,f,ensure_ascii=False,indent=2)
    f.write("\n")
print("Convertido; conserve el original. Importe como ADMIN sobre su catálogo correspondiente para validar todas las referencias.")
