#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
binary=${1:-"$root/build/debug/catalogo_api"}
exec python3 "$root/backend/tests/test_library_api.py" "$binary" "$root/data/libros_iniciales.json" --demo
