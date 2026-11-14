#include "PruebaEstructura.hpp"
#include "biblioteca/BPlusTree.hpp"

using namespace biblioteca;
using namespace prueba;
namespace {
void probar(Caso& c) {
    BPlusTree arbol(3);
    auto vacio = json::parse(arbol.snapshot());
    c.exigir(vacio["cantidad"] == 0 && vacio["nodos"][0]["claves"].empty(), "Hoja vacía");
    for (const auto& id : {"A", "B", "C", "D", "E", "F", "G"}) {
        ++c.operaciones;
        c.exigir(arbol.insert({id, id, "Autor", {}}), "Alta");
    }
    const BookKey clave{"e", "E"};
    auto s = json::parse(arbol.snapshot(clave));
    const auto& nodos = s.at("nodos");
    auto traza = s.at("traza").get<std::vector<std::size_t>>();
    c.exigir(traza.size() >= 3 && traza.front() == s.at("raiz"), "Traza multinivel");
    for (std::size_t i = 1; i < traza.size(); ++i) {
        const auto& hijos = nodos[traza[i-1]].at("hijos");
        c.exigir(std::count(hijos.begin(), hijos.end(), traza[i]) == 1, "Paso a un hijo real");
    }
    const auto& hoja = nodos[traza.back()];
    c.exigir(hoja["hoja"] && std::count(hoja["claves"].begin(), hoja["claves"].end(), json(clave)) == 1, "Clave en hoja");
    json actual = s.at("raiz"), anterior = nullptr;
    while (!nodos[actual.get<std::size_t>()].at("hoja").get<bool>()) actual = nodos[actual.get<std::size_t>()].at("hijos")[0];
    std::vector<BookKey> recorrido;
    while (!actual.is_null()) {
        const auto& n = nodos.at(actual.get<std::size_t>());
        c.exigir(n["anterior"] == anterior, "Enlace de retorno");
        for (const auto& k : n["claves"]) recorrido.push_back(k.get<BookKey>());
        anterior = actual;
        actual = n["siguiente"];
    }
    std::vector<BookKey> esperadas;
    for (const auto& libro : arbol.all()) esperadas.push_back(keyOf(libro));
    c.exigir(recorrido == esperadas, "Cadena de hojas");
}
}
int main() { return ejecutar(20261118, probar); }
