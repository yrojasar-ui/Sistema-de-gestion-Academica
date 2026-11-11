#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"
evidence=${1:-"$root/docs/evidencias/e2a/$(TZ=America/Guatemala date +%Y%m%d-%H%M%S)"}
mkdir -p "$evidence"
evidence=$(realpath -- "$evidence")
{
  TZ=America/Guatemala date --iso-8601=seconds
  uname -a
  cat /etc/os-release
  g++ --version
  cmake --version
  make --version
  git --version
  git status --short --branch
  git remote -v
  git rev-parse --verify HEAD || true
} > "$evidence/entorno.log" 2>&1
printf 'paso\tcodigo\n' > "$evidence/resultados.tsv"
# Identificar fuentes incluso si una compilación o prueba falla después.
find backend data scripts third_party -type f -print0 | sort -z | xargs -0 sha256sum > "$evidence/fuentes.sha256"
sha256sum CMakeLists.txt >> "$evidence/fuentes.sha256"
run_step() {
  local name=$1
  shift
  printf 'Comando:' > "$evidence/$name.log"
  printf ' %q' "$@" >> "$evidence/$name.log"
  printf '\n' >> "$evidence/$name.log"
  set +e
  "$@" 2>&1 | tee -a "$evidence/$name.log"
  local statuses=("${PIPESTATUS[@]}")
  set -e
  printf '%s\t%s\n' "$name" "${statuses[0]}" >> "$evidence/resultados.tsv"
  printf '\nCódigo de retorno: %s\n' "${statuses[0]}" >> "$evidence/$name.log"
  [[ ${statuses[0]} -eq 0 && ${statuses[1]} -eq 0 ]]
}
run_step debug-configure cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
run_step debug-build cmake --build build/debug -j2
run_step debug-ctest ctest --test-dir build/debug -V --output-on-failure
run_step asan-configure cmake -S . -B build/asan -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DENABLE_SANITIZERS=ON
run_step asan-build cmake --build build/asan -j2
run_step asan-ctest env ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build/asan -V --output-on-failure
run_step demo bash scripts/demo_entregable2.sh "$root/build/debug/biblioteca"
printf 'Evidencia: %s\n' "$evidence"
