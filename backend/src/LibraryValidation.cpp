#include "biblioteca/LibraryService.hpp"
#include "biblioteca/DomainJson.hpp"
#include <algorithm>
#include <set>

namespace biblioteca {
namespace {
void comprobar(bool valido, const std::string& detalle) {
    if (!valido) throw ErrorDominio(409, "Estado incompatible: " + detalle);
}
template<class T, class F>
void idsUnicos(const std::vector<T>& filas, F id) {
    if (filas.size() > 10000) throw std::invalid_argument("Máximo 10000 registros por entidad");
    std::set<std::string> vistos;
    for (const auto& fila : filas) {
        validarId(id(fila));
        comprobar(vistos.insert(id(fila)).second, "ID repetido");
    }
}
}
void LibraryService::validar(const Runtime& e) const {
    const auto& d = e.datos;
    if (Json(d).dump().size() > 1024 * 1024)
        throw std::invalid_argument("El estado del dominio supera 1 MiB; exportación debe poder restaurarse");
    idsUnicos(d.usuarios, [](const auto& x) { return x.idUsuario; });
    idsUnicos(d.ejemplares, [](const auto& x) { return x.idEjemplar; });
    idsUnicos(d.prestamos, [](const auto& x) { return x.idPrestamo; });
    idsUnicos(d.reservas, [](const auto& x) { return x.idReserva; });
    idsUnicos(d.avisos, [](const auto& x) { return x.idAviso; });
    std::string razon;
    comprobar(catalogo_.validate(razon), razon);
    comprobar(e.activos.validar(), "invariantes Rojo-Negro");
    for (const auto& u : d.usuarios) {
        comprobar(u.nombre.size() <= 256 && !normalizeTitle(u.nombre).empty(), "nombre de usuario");
        const auto desdeHash = usuario(e, u.idUsuario);
        comprobar(desdeHash.nombre == u.nombre && desdeHash.activo == u.activo, "registro/hash");
    }
    auto ejemplar = [&](const std::string& id) -> const Ejemplar& {
        auto it = std::find_if(d.ejemplares.begin(), d.ejemplares.end(), [&](const auto& c) { return c.idEjemplar == id; });
        comprobar(it != d.ejemplares.end(), "referencia a ejemplar");
        return *it;
    };
    std::set<std::string> prestados, apartados;
    std::vector<ClavePrestamo> claves;
    for (const auto& p : d.prestamos) {
        validarFecha(p.vencimiento);
        validarInstante(p.inicio);
        comprobar(p.vencimiento > p.inicio.substr(0,10), "vencimiento anterior al préstamo");
        ejemplar(p.idEjemplar);
        auto u = usuario(e, p.idUsuario);
        if (p.devolucion.empty()) {
            comprobar(u.activo && prestados.insert(p.idEjemplar).second, "usuario inactivo o doble préstamo");
            claves.emplace_back(p.vencimiento, p.idPrestamo);
        } else {
            validarInstante(p.devolucion);
            comprobar(p.devolucion >= p.inicio, "devolución anterior al préstamo");
        }
    }
    std::sort(claves.begin(), claves.end());
    comprobar(claves == e.activos.enOrden(), "préstamos/Rojo-Negro");
    std::set<std::pair<std::string, std::string>> turnos;
    std::set<std::uint64_t> llegadas;
    std::map<std::string, std::vector<std::pair<std::uint64_t, std::string>>> pendientes;
    comprobar(d.siguienteLlegada > 0 && d.siguienteLlegada <= 9007199254740991ULL, "secuencia inválida");
    for (const auto& r : d.reservas) {
        comprobar(catalogo_.find(r.idLibro).has_value(), "libro de reserva ausente");
        auto u = usuario(e, r.idUsuario);
        validarInstante(r.creada);
        comprobar(r.llegada > 0 && r.llegada < d.siguienteLlegada && llegadas.insert(r.llegada).second, "turno de reserva inválido");
        comprobar(r.estado == "pendiente" || r.estado == "apartada" || r.estado == "cancelada" || r.estado == "atendida", "estado de reserva");
        if (pendiente(r)) comprobar(u.activo && turnos.emplace(r.idLibro, r.idUsuario).second, "reserva duplicada o usuario inactivo");
        if (r.estado == "pendiente") {
            comprobar(r.idEjemplar.empty(), "reserva pendiente con copia");
            pendientes[r.idLibro].emplace_back(r.llegada, r.idReserva);
        }
        if (r.estado == "apartada" || r.estado == "atendida") comprobar(!r.idEjemplar.empty(), "reserva sin copia asignada");
        if (!r.idEjemplar.empty()) comprobar(ejemplar(r.idEjemplar).idLibro == r.idLibro, "reserva de otro libro");
        if (r.estado == "apartada") {
            comprobar(apartados.insert(r.idEjemplar).second, "doble apartado");
            for (const auto& anterior : d.reservas)
                comprobar(anterior.idLibro != r.idLibro || anterior.estado != "pendiente" || anterior.llegada > r.llegada,
                          "reserva adelantada al turno");
        }
        if (r.estado == "atendida") {
            comprobar(std::any_of(d.prestamos.begin(), d.prestamos.end(), [&](const auto& p) {
                return p.idEjemplar == r.idEjemplar && p.idUsuario == r.idUsuario && p.inicio >= r.creada;
            }), "reserva atendida sin préstamo");
        }
    }
    for (const auto& c : d.ejemplares) {
        comprobar(catalogo_.find(c.idLibro).has_value(), "libro de ejemplar ausente");
        comprobar(c.estado == "disponible" || c.estado == "prestado" || c.estado == "apartado" || c.estado == "retirado", "estado de ejemplar");
        comprobar((prestados.count(c.idEjemplar) != 0) == (c.estado == "prestado"), "copia/préstamo");
        comprobar((apartados.count(c.idEjemplar) != 0) == (c.estado == "apartado"), "copia/apartado");
        comprobar(c.estado != "disponible" || pendientes[c.idLibro].empty(), "copia disponible con reservas pendientes");
    }
    for (const auto& [libro, cola] : e.colas) {
        std::vector<std::string> vigentes;
        for (const auto& id : cola.recorrer()) {
            auto it = std::find_if(d.reservas.begin(), d.reservas.end(), [&](const auto& r) { return r.idReserva == id; });
            comprobar(it != d.reservas.end() && it->idLibro == libro, "referencia FIFO");
            comprobar(it->estado == "pendiente" || it->estado == "cancelada", "turno consumido en FIFO");
            if (it->estado == "pendiente") vigentes.push_back(id);
        }
        auto orden = pendientes[libro];
        std::sort(orden.begin(), orden.end());
        std::vector<std::string> esperado;
        for (const auto& [turno, id] : orden) { (void)turno; esperado.push_back(id); }
        comprobar(vigentes == esperado, "orden FIFO");
    }
    for (const auto& [libro, orden] : pendientes)
        comprobar(orden.empty() || e.colas.count(libro) != 0, "FIFO ausente");
    std::size_t numero = 0;
    for (const auto& a : d.avisos) {
        comprobar(a.idAviso == "AV-" + std::to_string(++numero), "secuencia de avisos");
        usuario(e, a.idUsuario);
        validarInstante(a.creado);
        comprobar(a.estado == "pendiente", "la publicación de avisos requiere un publicador");
        if (a.tipo == "reserva_apartada") {
            comprobar(a.idPrestamo.empty() && std::any_of(d.reservas.begin(), d.reservas.end(), [&](const auto& r) {
                return r.idReserva == a.idReserva && r.idUsuario == a.idUsuario && !r.idEjemplar.empty();
            }), "referencia del aviso de reserva");
        } else {
            comprobar(a.tipo == "devolucion" && a.idReserva.empty() &&
                std::any_of(d.prestamos.begin(), d.prestamos.end(), [&](const auto& p) {
                    return p.idPrestamo == a.idPrestamo && p.idUsuario == a.idUsuario && !p.devolucion.empty();
                }), "referencia del aviso de devolución");
        }
    }
    for (const auto& b : catalogo_.all()) {
        std::vector<std::string> vecinos;
        for (const auto& otro : catalogo_.all()) {
            if (b.idLibro == otro.idLibro) continue;
            bool comparte = false;
            for (const auto& tema : b.temas)
                for (const auto& temaOtro : otro.temas) comparte |= normalizeTitle(tema) == normalizeTitle(temaOtro);
            if (comparte) vecinos.push_back(otro.idLibro);
        }
        std::sort(vecinos.begin(), vecinos.end());
        comprobar(vecinos == e.grafo.vecinos(b.idLibro), "grafo/temas");
    }
}
} // namespace biblioteca
