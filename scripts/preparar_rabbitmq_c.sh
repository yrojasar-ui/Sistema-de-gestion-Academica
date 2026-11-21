#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
version=0.15.0
sha=7b652df52c0de4d19ca36c798ed81378cba7a03a0f0c5d498881ae2d79b241c2
dest="$PWD/build/deps/rabbitmq-c-$version"
archive="$dest.tar.gz"
mkdir -p build/deps
if [[ ! -f "$archive" ]]; then
  curl --fail --location --proto '=https' --tlsv1.2 \
    "https://github.com/alanxz/rabbitmq-c/archive/refs/tags/v$version.tar.gz" -o "$archive"
fi
echo "$sha  $archive" | sha256sum --check
if [[ ! -f "$dest/install/lib/librabbitmq.a" ]]; then
  tar -xzf "$archive" -C build/deps
  cmake -S "$dest" -B "$dest/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_STATIC_LIBS=ON \
    -DENABLE_SSL_SUPPORT=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_TOOLS=OFF \
    -DCMAKE_INSTALL_PREFIX="$dest/install"
  cmake --build "$dest/build" -j2
  ctest --test-dir "$dest/build" --output-on-failure
  cmake --install "$dest/build"
fi
echo "rabbitmq-c $version: $dest/install"
