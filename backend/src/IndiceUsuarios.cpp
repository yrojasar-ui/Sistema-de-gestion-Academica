#include "biblioteca/IndiceUsuarios.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace biblioteca {
namespace {
template<class Lista>
auto localizar(Lista& usuarios, const std::string& id) {
    return std::find_if(usuarios.begin(), usuarios.end(), [&](const auto& u) { return u.idUsuario == id; });
}
}
IndiceUsuarios::IndiceUsuarios(std::size_t capacidad, unsigned limite) : capacidad_(capacidad), limite_(limite) {
    if (!capacidad || limite > 16) throw std::invalid_argument("Capacidad positiva y profundidad máxima de 0 a 16");
}
std::uint64_t IndiceUsuarios::hash(const std::string& id) {
    std::uint64_t valor = 14695981039346656037ULL;
    for (unsigned char c : id) {
        valor ^= c;
        valor *= 1099511628211ULL;
    }
    return valor;
}
std::size_t IndiceUsuarios::posicion(const std::string& id) const {
    return static_cast<std::size_t>(hash(id)) & (directorio_.size() - 1);
}
void IndiceUsuarios::dividir(std::size_t indice) {
    const auto& anterior = cubetas_[indice];
    const unsigned nuevaProf = anterior.profundidad + 1;
    const auto bit = std::size_t{1} << anterior.profundidad;
    Cubeta izquierda{nuevaProf, {}}, derecha{nuevaProf, {}};
    for (const auto& usuario : anterior.usuarios) {
        auto& destino = (hash(usuario.idUsuario) & bit) ? derecha : izquierda;
        destino.usuarios.push_back(usuario);
    }
    // Reservar antes de cambiar profundidades o referencias compartidas.
    if (nuevaProf > profundidad_) directorio_.reserve(directorio_.size() * 2);
    const auto nuevoIndice = cubetas_.size();
    cubetas_.push_back(std::move(derecha));
    cubetas_[indice] = std::move(izquierda);
    if (nuevaProf > profundidad_) {
        const auto mitad = directorio_.size();
        directorio_.resize(mitad * 2);
        std::copy_n(directorio_.begin(), mitad, directorio_.begin() + static_cast<std::ptrdiff_t>(mitad));
        ++profundidad_;
    }
    for (std::size_t i = 0; i < directorio_.size(); ++i)
        if (directorio_[i] == indice && (i & bit)) directorio_[i] = nuevoIndice;
}
bool IndiceUsuarios::insertar(const UsuarioIndice& usuario) {
    if (usuario.idUsuario.empty()) throw std::invalid_argument("idUsuario vacío");
    auto indice = directorio_[posicion(usuario.idUsuario)];
    const auto& usuarios = cubetas_[indice].usuarios;
    if (localizar(usuarios, usuario.idUsuario) != usuarios.end()) return false;
    while (cubetas_[indice].usuarios.size() >= capacidad_ && cubetas_[indice].profundidad < limite_) {
        dividir(indice);
        indice = directorio_[posicion(usuario.idUsuario)];
    }
    cubetas_[indice].usuarios.push_back(usuario);
    return true;
}
std::optional<UsuarioIndice> IndiceUsuarios::buscar(const std::string& id) const {
    const auto& usuarios = cubetas_[directorio_[posicion(id)]].usuarios;
    auto it = localizar(usuarios, id);
    if (it == usuarios.end()) return std::nullopt;
    return *it;
}
bool IndiceUsuarios::modificar(const UsuarioIndice& usuario) {
    auto& usuarios = cubetas_[directorio_[posicion(usuario.idUsuario)]].usuarios;
    auto it = localizar(usuarios, usuario.idUsuario);
    if (it == usuarios.end()) return false;
    it->nombre = usuario.nombre;
    it->activo = usuario.activo;
    return true;
}
bool IndiceUsuarios::eliminar(const std::string& id) {
    auto& usuarios = cubetas_[directorio_[posicion(id)]].usuarios;
    auto it = localizar(usuarios, id);
    if (it == usuarios.end()) return false;
    usuarios.erase(it);
    return true;
}
std::string IndiceUsuarios::snapshot() const {
    using nlohmann::json;
    auto cubetas = json::array();
    for (const auto& cubeta : cubetas_) {
        auto ids = json::array();
        for (const auto& usuario : cubeta.usuarios) ids.push_back(usuario.idUsuario);
        cubetas.push_back({{"id", cubetas.size()}, {"profundidadLocal", cubeta.profundidad},
            {"usuarios", std::move(ids)}, {"ocupacion", cubeta.usuarios.size()}});
    }
    return json{{"profundidadGlobal", profundidad_}, {"profundidadMaxima", limite_}, {"capacidad", capacidad_},
        {"directorio", directorio_}, {"cubetas", std::move(cubetas)}}.dump();
}
std::string IndiceUsuarios::traza(const std::string& id) const {
    const auto pos = posicion(id);
    const auto& usuarios = cubetas_[directorio_[pos]].usuarios;
    return nlohmann::json{{"posicion", pos}, {"cubeta", directorio_[pos]},
        {"encontrado", localizar(usuarios, id) != usuarios.end()}}.dump();
}
} // namespace biblioteca
