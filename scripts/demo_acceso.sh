#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
exec python3 "$root/backend/tests/test_acceso_api.py" "${1:-$root/build/debug/catalogo_api}" "$root/data/libros_iniciales.json"
