#include "PruebaEstructura.hpp"
#include "biblioteca/ColaReservas.hpp"
#include <algorithm>
#include <set>

using namespace biblioteca;
using namespace prueba;
namespace {
void probar(Caso& c) {
    ColaReservas cola("L1"), otra("L2");
    std::vector<std::pair<std::uint64_t,std::string>> modelo;
    std::set<std::string> canceladas;
    auto verificar = [&] {
        std::vector<std::string> ids;
        for (const auto& turno : modelo) ids.push_back(turno.second);
        c.exigir(cola.recorrer() == ids, "Recorrido FIFO");
        auto s = json::parse(cola.snapshot());
        c.exigir(s.at("cantidad") == ids.size() && s.at("nodos").size() == ids.size(), "Tamaño de cola");
        c.exigir(s.at("inicio") == (ids.empty() ? json(nullptr) : json(ids.front()))
            && s.at("fin") == (ids.empty() ? json(nullptr) : json(ids.back())), "Inicio/fin");
        c.exigir(cola.frente() == (ids.empty() ? std::nullopt : std::optional(ids.front())), "Frente");
        for (std::size_t i = 0; i < ids.size(); ++i) {
            auto& nodo = s.at("nodos").at(i);
            c.exigir(nodo.at("id") == ids[i] && nodo.at("llegada") == modelo[i].first &&
                nodo.at("siguiente") == (i+1 == ids.size() ? json(nullptr) : json(ids[i+1])), "Enlace de turno");
        }
    };
    auto encolar = [&](const std::string& id, std::uint64_t llegada) {
        ++c.operaciones;
        bool existe = std::any_of(modelo.begin(), modelo.end(), [&](const auto& par) { return par.second == id; });
        c.exigir(cola.encolar(id, llegada) == !existe, "Duplicado");
        if (!existe) {
            modelo.emplace_back(llegada, id);
            std::sort(modelo.begin(), modelo.end());
            canceladas.erase(id);
        }
        verificar();
    };
    auto atender = [&](bool descartar) {
        ++c.operaciones;
        std::optional<std::string> esperado;
        while (!modelo.empty()) {
            auto id = modelo.front().second;
            modelo.erase(modelo.begin());
            if (!descartar || !canceladas.count(id)) { esperado = id; break; }
        }
        auto real = descartar ? cola.atender([&](const auto& id) { return canceladas.count(id) != 0; }) : cola.desencolar();
        c.exigir(real == esperado, "Turno atendido");
        verificar();
        return real;
    };
    atender(true);
    encolar("R1", 1); encolar("R3", 2); encolar("R2", 2); encolar("R1", 9);
    canceladas.insert("R1");
    otra.encolar("R4", 1);
    std::cout << "Ejemplo=" << cola.snapshot() << '\n';
    c.exigir(atender(true) == "R2" && otra.frente() == "R4", "Cancelar primera conserva la siguiente y aísla libros");
    canceladas.insert("R3");
    c.exigir(!atender(true), "Todas canceladas");
    for (unsigned i = 0; i < 700; ++i) {
        auto id = "R"+std::to_string(c.azar()%30);
        switch (c.azar()%5) {
            case 0: case 1: encolar(id, c.azar()%20); break;
            case 2: canceladas.insert(id); verificar(); break;
            case 3: atender(true); break;
            default: atender(false);
        }
    }
    while (!modelo.empty()) atender(false);
    encolar("R1", 9);
}
}
int main() { return ejecutar(20261117, probar); }
