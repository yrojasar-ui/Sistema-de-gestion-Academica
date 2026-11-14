#pragma once
#include "biblioteca/Libro.hpp"
#include <map>
#include <string>
#include <vector>

namespace biblioteca {
class GrafoTemas {
public:
    bool insertar(const Libro& libro);
    bool modificar(const Libro& libro);
    bool eliminar(const std::string& id);
    std::vector<std::string> vecinos(const std::string& id) const;
    std::vector<std::string> bfs(const std::string& id) const;
    std::vector<std::string> dfs(const std::string& id) const;
    std::string snapshot() const;
private:
    struct Vertice {
        std::vector<std::string> temas, vecinos;
    };
    // Índice auxiliar por ID. Cada vértice posee su lista de adyacencia real.
    std::map<std::string, Vertice> vertices_;
    void actualizar(const Libro& libro);
};
} // namespace biblioteca
