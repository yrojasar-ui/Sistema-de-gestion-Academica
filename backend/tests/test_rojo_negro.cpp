#include "PruebaEstructura.hpp"
#include "biblioteca/PrestamosActivos.hpp"
#include <set>

using namespace biblioteca;
using namespace prueba;
namespace {
void probar(Caso& c) {
    PrestamosActivos arbol;
    std::set<ClavePrestamo> modelo;
    auto verificar = [&] {
        c.exigir(arbol.validar(), "Color, orden, padres o altura negra");
        c.exigir(arbol.enOrden() == std::vector<ClavePrestamo>(modelo.begin(), modelo.end()), "Orden frente a set");
    };
    auto insertar = [&](const ClavePrestamo& k) {
        ++c.operaciones;
        c.exigir(arbol.insertar(k) == modelo.insert(k).second, "Alta o duplicado");
        verificar();
    };
    auto quitar = [&](const ClavePrestamo& k) {
        ++c.operaciones;
        c.exigir(arbol.eliminar(k) == (modelo.erase(k) == 1), "Borrado o ausente");
        verificar();
    };
    verificar();
    quitar({"2026-11-16", "P1"});
    for (const auto& id : {"P2", "P1", "P3"}) insertar({"2026-11-16", id});
    auto s = json::parse(arbol.snapshot());
    c.exigir(s.at("raiz") == json(ClavePrestamo("2026-11-16", "P2")) && s.at("alturaNegra") == 3, "Raíz/altura del ejemplo");
    const auto& raiz = s.at("nodos").at(0);
    c.exigir(raiz.at("padre").is_null() && raiz.at("color") == "negro"
        && raiz.at("izq") == s.at("nodos")[1].at("clave") && raiz.at("der") == s.at("nodos")[2].at("clave"), "Enlaces exportados");
    std::vector<ClavePrestamo> traza;
    c.exigir(arbol.contiene({"2026-11-16", "P3"}, &traza) && traza.size() == 2 && traza.back().second == "P3", "Traza");
    std::cout << "Ejemplo=" << s.dump() << '\n';
    const ClavePrestamo original{"2026-11-16", "P2"}, nueva{"2026-11-24", "P2"};
    ++c.operaciones;
    c.exigir(arbol.cambiarVencimiento(original, nueva.first), "Reindexar la raíz con dos hijos");
    modelo.erase(original);
    modelo.insert(nueva);
    verificar();
    insertar(original);
    ++c.operaciones;
    c.exigir(!arbol.cambiarVencimiento(original, nueva.first), "Proteger clave destino existente");
    verificar();
    quitar(original);
    insertar({"2026-11-24", "P2"});
    for (unsigned i = 0; i < 3000; ++i) {
        const ClavePrestamo k{c.azar()%2 ? "2026-11-16" : "2026-11-24", "P" + std::to_string(c.azar()%80)};
        switch (c.azar()%3) {
            case 0: insertar(k); break;
            case 1: quitar(k); break;
            default:
                ++c.operaciones;
                c.exigir(arbol.contiene(k) == (modelo.count(k) == 1), "Búsqueda");
                verificar();
        }
    }
    // Borrar la raíz real repetidamente, incluyendo último nodo y árbol vacío.
    while (!modelo.empty()) quitar(json::parse(arbol.snapshot()).at("raiz").get<ClavePrestamo>());
    c.exigir(json::parse(arbol.snapshot()).at("raiz").is_null(), "Raíz vacía");
    insertar({"2026-11-24", "P1"});
}
}
int main() { return ejecutar(20261114, probar); }
