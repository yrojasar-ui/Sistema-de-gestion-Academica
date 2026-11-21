# Dependencia JSON

`nlohmann/json` **3.12.0**, licencia MIT conservada en `nlohmann/LICENSE.MIT`.
Se incluye el encabezado oficial para compilar sin descargas durante CMake.
El B+ y su validador son propios; esta biblioteca solo analiza/serializa JSON.

Origen consultado el 9 de noviembre de 2026:
- https://github.com/nlohmann/json/tree/v3.12.0
- https://json.nlohmann.me/integration/index.html

Recuperación desde la raíz del repositorio:

```bash
curl --fail --location https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp -o third_party/nlohmann/json.hpp
curl --fail --location https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT -o third_party/nlohmann/LICENSE.MIT
sha256sum -c third_party/SHA256SUMS
```

## Servidor HTTP local

`cpp-httplib` 0.66.0 (licencia MIT), encabezado de la versión oficial
[publicada en GitHub](https://github.com/yhirose/cpp-httplib/tree/v0.66.0).
El ejecutable `catalogo_api` lo utiliza para HTTP en loopback. La licencia se
conserva junto al encabezado. Las sumas se verifican con
`sha256sum -c third_party/SHA256SUMS` desde la raíz.

## Autenticación

libsodium **1.0.22**, licencia ISC incluida en el archivo fuente oficial.
scripts/preparar_sodium.sh descarga la versión exacta, verifica SHA-256
adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349,
compila y ejecuta make check antes de instalar en build/deps.
CMake enlaza ese archivo estático. No sustituye ninguna estructura académica.
Origen: https://download.libsodium.org/libsodium/releases/libsodium-1.0.22.tar.gz
API oficial: https://doc.libsodium.org/password_hashing/default_phf
y https://doc.libsodium.org/generating_random_data.

## Cliente AMQP 0-9-1 (RabbitMQ)

`rabbitmq-c` **0.15.0**, licencia MIT incluida en el archivo fuente oficial.
`scripts/preparar_rabbitmq_c.sh` descarga el paquete oficial, verifica SHA-256
`7b652df52c0de4d19ca36c798ed81378cba7a03a0f0c5d498881ae2d79b241c2`,
compila la biblioteca estática `librabbitmq.a` y ejecuta su suite CTest antes de
instalar en `build/deps/rabbitmq-c-0.15.0/install`. Se utiliza únicamente para
transporte AMQP con RabbitMQ (colas durables, publisher confirms, detección de
`basic.return` y ACK manual). No sustituye la cola FIFO académica por libro.
Origen: https://github.com/alanxz/rabbitmq-c/archive/refs/tags/v0.15.0.tar.gz
