#include "biblioteca/GrafoTemas.hpp"
#include <algorithm>
#include <deque>
#include <nlohmann/json.hpp>
#include <set>

namespace biblioteca {
namespace {
bool compartenTema(const std::vector<std::string>& a, const std::vector<std::string>& b) {
    auto i = a.begin(), j = b.begin();
    while (i != a.end() && j != b.end()) {
        if (*i == *j) return true;
        if (*i < *j) ++i;
        else ++j;
    }
    return false;
}
void quitarVecino(std::vector<std::string>& lista, const std::string& id) {
    lista.erase(std::lower_bound(lista.begin(), lista.end(), id));
}
}
void GrafoTemas::actualizar(const Libro& libro) {
    validateBook(libro);
    Vertice nuevo;
    for (const auto& tema : libro.temas) nuevo.temas.push_back(normalizeTitle(tema));
    std::sort(nuevo.temas.begin(), nuevo.temas.end());
    nuevo.temas.erase(std::unique(nuevo.temas.begin(), nuevo.temas.end()), nuevo.temas.end());

    // Preparar las dos direcciones antes de publicar una modificación.
    std::vector<std::pair<Vertice*, std::vector<std::string>>> cambios;
    for (auto& [id, otro] : vertices_) {
        if (id == libro.idLibro) continue;
        const bool enlazar = compartenTema(nuevo.temas, otro.temas);
        if (enlazar) nuevo.vecinos.push_back(id);
        auto pos = std::lower_bound(otro.vecinos.begin(), otro.vecinos.end(), libro.idLibro);
        const bool enlazado = pos != otro.vecinos.end() && *pos == libro.idLibro;
        if (enlazar == enlazado) continue;
        auto& lista = cambios.emplace_back(&otro, otro.vecinos).second;
        auto destino = lista.begin() + (pos - otro.vecinos.begin());
        if (enlazar) lista.insert(destino, libro.idLibro);
        else lista.erase(destino);
    }
    vertices_.insert_or_assign(libro.idLibro, std::move(nuevo));
    for (auto& [vertice, lista] : cambios) vertice->vecinos.swap(lista);
}
bool GrafoTemas::insertar(const Libro& libro) {
    if (vertices_.count(libro.idLibro)) return false;
    actualizar(libro);
    return true;
}
bool GrafoTemas::modificar(const Libro& libro) {
    if (!vertices_.count(libro.idLibro)) return false;
    actualizar(libro);
    return true;
}
bool GrafoTemas::eliminar(const std::string& id) {
    auto it = vertices_.find(id);
    if (it == vertices_.end()) return false;
    for (const auto& vecino : it->second.vecinos) quitarVecino(vertices_.at(vecino).vecinos, id);
    vertices_.erase(it);
    return true;
}
std::vector<std::string> GrafoTemas::vecinos(const std::string& id) const {
    return vertices_.at(id).vecinos;
}
std::vector<std::string> GrafoTemas::bfs(const std::string& id) const {
    if (!vertices_.count(id)) return {};
    std::vector<std::string> orden;
    std::set<std::string> vistos{id};
    std::deque<std::string> pendientes{id};
    while (!pendientes.empty()) {
        auto actual = std::move(pendientes.front());
        pendientes.pop_front();
        orden.push_back(actual);
        for (const auto& vecino : vertices_.at(actual).vecinos)
            if (vistos.insert(vecino).second) pendientes.push_back(vecino);
    }
    return orden;
}
std::vector<std::string> GrafoTemas::dfs(const std::string& id) const {
    if (!vertices_.count(id)) return {};
    std::vector<std::string> orden, pendientes{id};
    std::set<std::string> vistos;
    while (!pendientes.empty()) {
        auto actual = std::move(pendientes.back());
        pendientes.pop_back();
        if (!vistos.insert(actual).second) continue;
        orden.push_back(actual);
        const auto& lista = vertices_.at(actual).vecinos;
        pendientes.insert(pendientes.end(), lista.rbegin(), lista.rend());
    }
    return orden;
}
std::string GrafoTemas::snapshot() const {
    using nlohmann::json;
    auto vertices = json::array();
    std::size_t enlaces = 0;
    for (const auto& [id, v] : vertices_) {
        vertices.push_back({{"id", id}, {"temas", v.temas}, {"vecinos", v.vecinos}});
        enlaces += v.vecinos.size();
    }
    return json{{"vertices", std::move(vertices)}, {"aristas", enlaces / 2}}.dump();
}
} // namespace biblioteca
