#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
version=1.0.22
sha=adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349
dest="$PWD/build/deps/libsodium-$version"
archive="$dest.tar.gz"
mkdir -p build/deps
if [[ ! -f "$archive" ]]; then
  curl --fail --location --proto '=https' --tlsv1.2 "https://download.libsodium.org/libsodium/releases/libsodium-$version.tar.gz" -o "$archive"
fi
echo "$sha  $archive" | sha256sum --check
if [[ ! -f "$dest/install/lib/libsodium.a" ]]; then
  tar -xzf "$archive" -C build/deps
  (cd "$dest"; ./configure --prefix="$dest/install" --disable-shared; make -j2; make check -j2; make install)
fi
echo "libsodium $version: $dest/install"
