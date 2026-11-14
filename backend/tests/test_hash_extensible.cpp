#include "PruebaEstructura.hpp"
#include "biblioteca/IndiceUsuarios.hpp"
#include <map>
#include <set>

using namespace biblioteca;
using namespace prueba;
namespace {
void probar(Caso& c) {
    IndiceUsuarios indice(1, 3);
    std::map<std::string, UsuarioIndice> modelo;
    auto verificar = [&] {
        const auto s = json::parse(indice.snapshot());
        const auto global = s.at("profundidadGlobal").get<unsigned>();
        c.exigir(global <= 3 && s.at("directorio").size() == (1U << global), "Tamaño del directorio");
        std::set<std::string> presentes;
        for (const auto& b : s.at("cubetas")) {
            auto local = b.at("profundidadLocal").get<unsigned>();
            const auto& d = s.at("directorio");
            c.exigir(local <= global && std::count(d.begin(), d.end(), b.at("id")) == (1 << (global-local)), "Referencias por cubeta");
            c.exigir(b.at("usuarios").size() <= 1 || local == 3, "Desborde prematuro");
            for (const auto& id : b.at("usuarios")) c.exigir(presentes.insert(id.get<std::string>()).second, "Registro repetido");
        }
        c.exigir(presentes.size() == modelo.size(), "Conteo de usuarios");
        for (const auto& [id, u] : modelo) {
            auto real = indice.buscar(id);
            c.exigir(presentes.count(id) && real && real->nombre == u.nombre && real->activo == u.activo, "Registro perdido o mal dirigido");
        }
        c.exigir(s.dump().find("privado") == std::string::npos, "Snapshot contiene un nombre");
    };
    auto alta = [&](const UsuarioIndice& u) {
        ++c.operaciones;
        c.exigir(indice.insertar(u) == modelo.emplace(u.idUsuario, u).second, "Alta/duplicado");
        verificar();
    };
    auto baja = [&](const std::string& id) {
        ++c.operaciones;
        c.exigir(indice.eliminar(id) == (modelo.erase(id) == 1), "Baja");
        verificar();
    };
    verificar();
    for (const auto& id : {"U1", "U2", "U3"}) alta({id, "privado", true});
    auto ejemplo = json::parse(indice.snapshot());
    c.exigir(ejemplo["profundidadGlobal"] == 2 && ejemplo["cubetas"].size() == 3, "Split y alias");
    c.exigir(json::parse(indice.traza("U2")).at("encontrado") == true, "Traza del directorio");
    std::cout << "Ejemplo=" << ejemplo.dump() << '\n';
    // Más de ocho usuarios en ocho prefijos posibles: fuerza el desbordamiento.
    for (unsigned i = 0; i < 20; ++i) alta({"U"+std::to_string(i), "privado", true});
    for (unsigned i = 0; i < 600; ++i) {
        UsuarioIndice u{"U"+std::to_string(c.azar()%32), "privado"+std::to_string(i), c.azar()%2 == 1};
        switch (c.azar()%4) {
            case 0: alta(u); break;
            case 1: baja(u.idUsuario); break;
            case 2:
                ++c.operaciones;
                c.exigir(indice.modificar(u) == (modelo.count(u.idUsuario) == 1), "Modificación");
                if (modelo.count(u.idUsuario)) modelo.at(u.idUsuario) = u;
                verificar();
                break;
            default:
                ++c.operaciones;
                c.exigir(indice.buscar(u.idUsuario).has_value() == (modelo.count(u.idUsuario) == 1), "Búsqueda");
        }
    }
    for (unsigned i = 0; i < 32; ++i) baja("U"+std::to_string(i));
    alta({"U1", "privado", true});
    IndiceUsuarios sinBits(1, 0);
    for (const auto& id : {"A", "B", "C"}) {
        ++c.operaciones;
        c.exigir(sinBits.insertar({id, "", true}), "Colisión en única cubeta");
    }
    c.exigir(json::parse(sinBits.snapshot()).at("cubetas")[0].at("ocupacion") == 3, "Profundidad cero");
}
}
int main() { return ejecutar(20261115, probar); }
