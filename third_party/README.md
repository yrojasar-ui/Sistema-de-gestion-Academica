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
