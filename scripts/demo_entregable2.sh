#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
binary=${1:-"$root/build/debug/biblioteca"}
if [[ ! -x "$binary" ]]; then
  cmake -S "$root" -B "$root/build/debug" -DCMAKE_BUILD_TYPE=Debug
  cmake --build "$root/build/debug" -j2
fi
binary=$(realpath -- "$binary")
temp=$(mktemp -d "${TMPDIR:-/tmp}/biblioteca-demo-XXXXXX")
# Limpiar exclusivamente el directorio que mktemp acaba de crear para esta demo.
trap 'rm -rf -- "$temp"' EXIT
initial="$root/data/libros_iniciales.json"
before=$(sha256sum -- "$initial")
"$binary" --initial "$initial" --working "$temp/trabajo.json" --order 3 > "$temp/sesion1.log" <<'INPUT'
1
1
2
DEMO-01
Árbol de demostración
Autor Sintético Demo
estructuras;prueba
3
ARBOL DE DEMOSTRACION
6
7
4
Árbol de demostración
DEMO-01
SI
3
Árbol de demostración
2
PERSIST-01
Persistencia de prueba
Autor Sintético Demo
prueba
8
0
INPUT
"$binary" --initial "$initial" --working "$temp/trabajo.json" --order 3 > "$temp/sesion2.log" <<'INPUT'
1
2
3
Persistencia de prueba
3
Árbol de demostración
5
7
0
INPUT
cat "$temp/sesion1.log" "$temp/sesion2.log"
grep -q 'Cargados: 16' "$temp/sesion1.log"
grep -q 'Insertado: DEMO-01' "$temp/sesion1.log"
grep -q '\[DEMO-01\] Árbol de demostración' "$temp/sesion1.log"
grep -q 'Borrado: DEMO-01' "$temp/sesion1.log"
grep -q 'Resultados: 0' "$temp/sesion1.log"
grep -q 'Cargados: 17' "$temp/sesion2.log"
grep -q '\[PERSIST-01\] Persistencia de prueba' "$temp/sesion2.log"
grep -q 'Resultados: 0' "$temp/sesion2.log"
grep -q 'Invariantes OK' "$temp/sesion1.log"
grep -q 'Invariantes OK' "$temp/sesion2.log"
if grep -q 'Error:\|FALLO:\|SIN GUARDAR' "$temp/sesion1.log" "$temp/sesion2.log"; then exit 1; fi
[[ "$before" == "$(sha256sum -- "$initial")" ]]
printf '\nDEMO OK: carga -> inserción -> búsqueda -> borrado -> ausencia -> guardado -> reinicio; fixture intacto.\n'
