#include "PruebaEstructura.hpp"
#include "biblioteca/GrafoTemas.hpp"
#include <set>

using namespace biblioteca;
using namespace prueba;
namespace {
void probar(Caso& c) {
    GrafoTemas grafo;
    // Oráculo por intersección de conjuntos, recalculado sin listas persistentes.
    std::map<std::string, std::set<std::string>> modelo;
    auto verificar = [&] {
        auto s = json::parse(grafo.snapshot());
        c.exigir(s["vertices"].size() == modelo.size(), "Cantidad de vértices");
        std::size_t enlaces = 0;
        for (const auto& v : s["vertices"]) {
            const auto id = v.at("id").get<std::string>();
            std::vector<std::string> esperados;
            for (const auto& [otro, temas] : modelo) {
                if (otro == id) continue;
                for (const auto& t : temas)
                    if (modelo.at(id).count(t)) { esperados.push_back(otro); break; }
            }
            c.exigir(grafo.vecinos(id) == esperados && v["vecinos"] == esperados, "Simetría, unicidad y etiquetas de vecinos");
            enlaces += esperados.size();
        }
        c.exigir(s["aristas"] == enlaces/2, "Cantidad de aristas");
    };
    auto escribir = [&](const std::string& id, const std::vector<std::string>& temas, bool alta) {
        Libro libro{id, id, "Autor", temas};
        const bool existe = modelo.count(id) != 0;
        const bool ok = alta ? grafo.insertar(libro) : grafo.modificar(libro);
        ++c.operaciones;
        c.exigir(ok == (alta != existe), "Alta o modificación");
        if (ok) {
            auto& conjunto = modelo[id];
            conjunto.clear();
            for (const auto& t : temas) conjunto.insert(normalizeTitle(t));
        }
        verificar();
    };
    auto quitar = [&](const std::string& id) {
        ++c.operaciones;
        c.exigir(grafo.eliminar(id) == (modelo.erase(id) == 1), "Eliminar vértice");
        verificar();
    };
    verificar();
    c.exigir(grafo.bfs("A").empty() && grafo.dfs("A").empty(), "Origen ausente");
    escribir("A", {"Árbol", "árbol", "web"}, true);
    escribir("B", {"arbol", "red"}, true);
    escribir("C", {"WEB"}, true);
    escribir("D", {"red"}, true);
    escribir("E", {}, true);
    c.exigir(grafo.bfs("A") == std::vector<std::string>({"A","B","C","D"}), "BFS por niveles");
    c.exigir(grafo.dfs("A") == std::vector<std::string>({"A","B","D","C"}), "DFS en profundidad");
    quitar("B");
    std::cout << "Ejemplo=" << grafo.snapshot() << '\n';
    for (unsigned i = 0; i < 500; ++i) {
        auto id = "L"+std::to_string(c.azar()%12);
        auto op = c.azar()%3;
        if (op == 0) quitar(id);
        else escribir(id, {"T"+std::to_string(c.azar()%5), "T"+std::to_string(c.azar()%5)}, op == 1);
    }
    while (!modelo.empty()) { auto id = modelo.begin()->first; quitar(id); }
    escribir("A", {"web"}, true);
}
}
int main() { return ejecutar(20261116, probar); }
