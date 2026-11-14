#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace biblioteca {
struct UsuarioIndice {
    std::string idUsuario, nombre;
    bool activo;
};

class IndiceUsuarios {
public:
    explicit IndiceUsuarios(std::size_t capacidad = 4, unsigned limite = 12);
    bool insertar(const UsuarioIndice& usuario);
    std::optional<UsuarioIndice> buscar(const std::string& id) const;
    bool modificar(const UsuarioIndice& usuario);
    bool eliminar(const std::string& id);
    std::string snapshot() const;
    std::string traza(const std::string& id) const;
private:
    struct Cubeta {
        unsigned profundidad;
        std::vector<UsuarioIndice> usuarios;
    };
    static std::uint64_t hash(const std::string& id);
    std::size_t posicion(const std::string& id) const;
    void dividir(std::size_t indice);
    std::size_t capacidad_;
    unsigned limite_, profundidad_ = 0;
    // Varias entradas del directorio pueden referenciar la misma cubeta.
    std::vector<std::size_t> directorio_{0};
    std::vector<Cubeta> cubetas_{{0, {}}};
};
} // namespace biblioteca
