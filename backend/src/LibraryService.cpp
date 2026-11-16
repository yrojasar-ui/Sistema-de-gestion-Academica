#include "biblioteca/LibraryService.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <set>

namespace biblioteca {
namespace {
void exigir(bool condicion, const std::string& detalle, int status = 409) {
    if (!condicion) throw ErrorDominio(status, detalle);
}
template<class T, class F>
T& obtener(std::vector<T>& filas, const std::string& id, F campo) {
    auto i = std::find_if(filas.begin(), filas.end(), [&](const auto& f) { return campo(f) == id; });
    if (i == filas.end()) throw ErrorDominio(404, "Registro ausente: " + id);
    return *i;
}
Ejemplar& copia(EstadoBiblioteca& e, const std::string& id) {
    return obtener(e.ejemplares, id, [](const auto& x) { return x.idEjemplar; });
}
Reserva& reserva(EstadoBiblioteca& e, const std::string& id) {
    return obtener(e.reservas, id, [](const auto& x) { return x.idReserva; });
}
Prestamo& prestamo(EstadoBiblioteca& e, const std::string& id) {
    return obtener(e.prestamos, id, [](const auto& x) { return x.idPrestamo; });
}
template<class T, class F>
void unico(const std::vector<T>& filas, const std::string& id, F campo) {
    validarId(id);
    exigir(std::none_of(filas.begin(), filas.end(), [&](const auto& f) { return campo(f) == id; }), "ID duplicado");
}
template<class T, class F>
void borrar(std::vector<T>& filas, const std::string& id, F campo) {
    filas.erase(std::remove_if(filas.begin(), filas.end(), [&](const auto& f) { return campo(f) == id; }), filas.end());
}
void nombreValido(const Usuario& u) {
    validarId(u.idUsuario);
    if (u.nombre.size() > 256 || normalizeTitle(u.nombre).empty()) throw std::invalid_argument("Nombre vacío o demasiado largo");
}
void fechaFutura(const std::string& fecha) {
    validarFecha(fecha);
    if (fecha <= ahoraUTC().substr(0,10)) throw std::invalid_argument("El vencimiento debe ser posterior a hoy UTC");
}
}
LibraryService::LibraryService(CatalogService& catalogo, IDomainRepository& repositorio, bool deferLoad)
    : catalogo_(catalogo), repositorio_(repositorio),
      actual_(deferLoad ? std::make_unique<Runtime>() : construir(repositorio.load())) {}
std::unique_ptr<LibraryService::Runtime> LibraryService::construir(const EstadoBiblioteca& estado) const {
    auto nuevo = std::make_unique<Runtime>();
    nuevo->datos = estado;
    for (const auto& libro : catalogo_.all()) nuevo->grafo.insertar(libro);
    for (const auto& u : estado.usuarios) exigir(nuevo->usuarios.insertar(u), "Usuario repetido");
    for (const auto& p : estado.prestamos)
        if (p.devolucion.empty()) exigir(nuevo->activos.insertar({p.vencimiento, p.idPrestamo}), "Préstamo repetido");
    for (const auto& r : estado.reservas) {
        auto& cola = nuevo->colas.try_emplace(r.idLibro, r.idLibro).first->second;
        if (r.estado == "pendiente") exigir(cola.encolar(r.idReserva, r.llegada), "Reserva repetida");
    }
    validar(*nuevo);
    return nuevo;
}
void LibraryService::reconstruir(const EstadoBiblioteca& estado) {
    auto nuevo = construir(estado);
    repositorio_.replace(nuevo->datos);
    actual_.swap(nuevo);
}
void LibraryService::cambiar(const std::function<void(Runtime&)>& operacion) {
    // Copia de nodos y enlaces: mantiene la topología real y usa sus algoritmos al mutar.
    auto nuevo = std::make_unique<Runtime>(*actual_);
    operacion(*nuevo);
    validar(*nuevo);
    repositorio_.replace(nuevo->datos);
    actual_.swap(nuevo);
}
Usuario LibraryService::usuario(const Runtime& estado, const std::string& id) {
    auto u = estado.usuarios.buscar(id);
    if (!u) throw ErrorDominio(404, "Usuario ausente");
    return *u;
}
Usuario LibraryService::usuario(const std::string& id) const { return usuario(*actual_, id); }
void LibraryService::crearUsuario(const Usuario& u) {
    nombreValido(u);
    cambiar([&](Runtime& e) {
        exigir(e.usuarios.insertar(u), "Usuario duplicado");
        e.datos.usuarios.push_back(u);
    });
}
void LibraryService::editarUsuario(const Usuario& u) {
    nombreValido(u);
    cambiar([&](Runtime& e) {
        usuario(e, u.idUsuario);
        if (!u.activo) {
            for (const auto& p : e.datos.prestamos)
                exigir(p.idUsuario != u.idUsuario || !p.devolucion.empty(), "Usuario con préstamo activo");
            for (auto& r : e.datos.reservas)
                if (r.idUsuario == u.idUsuario && pendiente(r)) liberar(e, r);
        }
        e.usuarios.modificar(u);
        obtener(e.datos.usuarios, u.idUsuario, [](const auto& x) { return x.idUsuario; }) = u;
        for (const auto& b : catalogo_.all()) asignar(e, b.idLibro);
    });
}
void LibraryService::borrarUsuario(const std::string& id) {
    cambiar([&](Runtime& e) {
        auto u = usuario(e, id);
        for (const auto& p : e.datos.prestamos)
            exigir(p.idUsuario != id || !p.devolucion.empty(), "Usuario con préstamo activo");
        for (const auto& r : e.datos.reservas)
            exigir(r.idUsuario != id || !pendiente(r), "Usuario con reserva pendiente");
        bool historial = false;
        for (const auto& p : e.datos.prestamos) historial |= p.idUsuario == id;
        for (const auto& r : e.datos.reservas) historial |= r.idUsuario == id;
        if (historial) {
            u.activo = false;
            e.usuarios.modificar(u);
            obtener(e.datos.usuarios, id, [](const auto& x) { return x.idUsuario; }).activo = false;
        } else {
            e.usuarios.eliminar(id);
            borrar(e.datos.usuarios, id, [](const auto& x) { return x.idUsuario; });
        }
    });
}
void LibraryService::crearEjemplar(const std::string& id, const std::string& libro) {
    exigir(catalogo_.find(libro).has_value(), "Libro ausente", 404);
    cambiar([&](Runtime& e) {
        unico(e.datos.ejemplares, id, [](const auto& x) { return x.idEjemplar; });
        e.datos.ejemplares.push_back({id, libro, "disponible"});
        asignar(e, libro);
    });
}
void LibraryService::estadoEjemplar(const std::string& id, const std::string& estado) {
    exigir(estado == "disponible" || estado == "retirado", "Estado editable disponible o retirado", 400);
    cambiar([&](Runtime& e) {
        auto& c = copia(e.datos, id);
        exigir(c.estado == "disponible" || c.estado == "retirado", "Ejemplar comprometido");
        c.estado = estado;
        asignar(e, c.idLibro);
    });
}
void LibraryService::borrarEjemplar(const std::string& id) {
    cambiar([&](Runtime& e) {
        auto& c = copia(e.datos, id);
        exigir(c.estado == "disponible" || c.estado == "retirado", "Ejemplar comprometido");
        bool historial = false;
        for (const auto& p : e.datos.prestamos) historial |= p.idEjemplar == id;
        for (const auto& r : e.datos.reservas) historial |= r.idEjemplar == id;
        if (historial) c.estado = "retirado";
        else borrar(e.datos.ejemplares, id, [](const auto& x) { return x.idEjemplar; });
    });
}
void LibraryService::prestar(const std::string& id, const std::string& ejemplar, const std::string& lector, const std::string& vence) {
    fechaFutura(vence);
    cambiar([&](Runtime& e) {
        unico(e.datos.prestamos, id, [](const auto& x) { return x.idPrestamo; });
        exigir(usuario(e, lector).activo, "Usuario inactivo");
        auto& c = copia(e.datos, ejemplar);
        exigir(c.estado == "disponible" || c.estado == "apartado", "Ejemplar no disponible");
        auto apartado = std::find_if(e.datos.reservas.begin(), e.datos.reservas.end(), [&](const auto& r) {
            return r.idEjemplar == ejemplar && r.estado == "apartada";
        });
        if (c.estado == "apartado") {
            exigir(apartado != e.datos.reservas.end() && apartado->idUsuario == lector, "Copia apartada para otro lector");
            apartado->estado = "atendida";
        }
        c.estado = "prestado";
        e.datos.prestamos.push_back({id, ejemplar, lector, ahoraUTC(), vence, ""});
        exigir(e.activos.insertar({vence, id}), "Clave de préstamo duplicada");
    });
}
void LibraryService::devolver(const std::string& id) {
    cambiar([&](Runtime& e) {
        auto& p = prestamo(e.datos, id);
        exigir(p.devolucion.empty(), "Préstamo ya devuelto");
        p.devolucion = ahoraUTC();
        exigir(e.activos.eliminar({p.vencimiento, id}), "Índice activo inconsistente");
        auto& c = copia(e.datos, p.idEjemplar);
        c.estado = "disponible";
        aviso(e, "devolucion", p.idUsuario, "", id);
        asignar(e, c.idLibro);
    });
}
void LibraryService::renovar(const std::string& id, const std::string& vence) {
    fechaFutura(vence);
    cambiar([&](Runtime& e) {
        auto& p = prestamo(e.datos, id);
        exigir(p.devolucion.empty(), "Préstamo devuelto");
        exigir(vence > p.vencimiento, "Renovación debe ampliar el vencimiento", 400);
        exigir(e.activos.cambiarVencimiento({p.vencimiento, id}, vence), "Índice activo inconsistente");
        p.vencimiento = vence;
    });
}
void LibraryService::reservar(const std::string& id, const std::string& libro, const std::string& lector) {
    exigir(catalogo_.find(libro).has_value(), "Libro ausente", 404);
    cambiar([&](Runtime& e) {
        unico(e.datos.reservas, id, [](const auto& x) { return x.idReserva; });
        exigir(usuario(e, lector).activo, "Usuario inactivo");
        for (const auto& r : e.datos.reservas)
            exigir(!(pendiente(r) && r.idLibro == libro && r.idUsuario == lector), "Reserva pendiente duplicada");
        exigir(e.datos.siguienteLlegada < 9007199254740991ULL, "Secuencia agotada");
        const auto turno = e.datos.siguienteLlegada++;
        e.datos.reservas.push_back({id, libro, lector, turno, "pendiente", "", ahoraUTC()});
        e.colas.try_emplace(libro, libro).first->second.encolar(id, turno);
        asignar(e, libro);
    });
}
void LibraryService::liberar(Runtime& e, Reserva& r) {
    if (r.estado == "apartada") copia(e.datos, r.idEjemplar).estado = "disponible";
    r.estado = "cancelada"; // Su ID puede permanecer en FIFO hasta descartarse al atender.
}
void LibraryService::cancelar(const std::string& id) {
    cambiar([&](Runtime& e) {
        auto& r = reserva(e.datos, id);
        exigir(pendiente(r), "Reserva ya finalizada");
        liberar(e, r);
        asignar(e, r.idLibro);
    });
}
void LibraryService::aviso(Runtime& e, const std::string& tipo, const std::string& lector,
                           const std::string& reservaId, const std::string& prestamoId) {
    const auto id = "AV-" + std::to_string(e.datos.avisos.size() + 1);
    e.datos.avisos.push_back({id, tipo, lector, reservaId, prestamoId, ahoraUTC(), "pendiente"});
}
void LibraryService::asignar(Runtime& e, const std::string& libro) {
    auto it = e.colas.find(libro);
    if (it == e.colas.end()) return;
    for (auto& c : e.datos.ejemplares) {
        if (c.idLibro != libro || c.estado != "disponible") continue;
        auto siguiente = it->second.atender([&](const auto& id) {
            const auto& r = reserva(e.datos, id);
            return r.estado != "pendiente";
        });
        if (!siguiente) break;
        auto& r = reserva(e.datos, *siguiente);
        r.estado = "apartada";
        r.idEjemplar = c.idEjemplar;
        c.estado = "apartado";
        aviso(e, "reserva_apartada", r.idUsuario, r.idReserva, "");
    }
}
void LibraryService::crearLibro(const Libro& libro) {
    auto nuevo = std::make_unique<Runtime>(*actual_);
    exigir(nuevo->grafo.insertar(libro), "Libro duplicado");
    catalogo_.commit([&](auto& c) { c.insert(libro); });
    actual_.swap(nuevo);
}
void LibraryService::editarLibro(const Libro& libro) {
    auto nuevo = std::make_unique<Runtime>(*actual_);
    exigir(nuevo->grafo.modificar(libro), "Libro ausente", 404);
    catalogo_.commit([&](auto& c) { c.update(libro); });
    actual_.swap(nuevo);
}
void LibraryService::borrarLibro(const std::string& id) {
    exigir(catalogo_.find(id).has_value(), "Libro ausente", 404);
    for (const auto& c : estado().ejemplares) exigir(c.idLibro != id, "Libro referenciado por un ejemplar");
    for (const auto& r : estado().reservas) exigir(r.idLibro != id, "Libro referenciado por reservas o historial");
    auto nuevo = std::make_unique<Runtime>(*actual_);
    nuevo->grafo.eliminar(id);
    nuevo->colas.erase(id);
    catalogo_.commit([&](auto& c) { c.erase(id); });
    actual_.swap(nuevo);
}
std::vector<Prestamo> LibraryService::activos(const std::string& hasta) const {
    if (!hasta.empty()) validarFecha(hasta);
    std::vector<Prestamo> salida;
    // El Rojo-Negro determina qué préstamos están activos y su orden de vencimiento.
    for (const auto& clave : actual_->activos.enOrden()) {
        if (!hasta.empty() && clave.first > hasta) break;
        auto it = std::find_if(estado().prestamos.begin(), estado().prestamos.end(), [&](const auto& p) { return p.idPrestamo == clave.second; });
        salida.push_back(*it);
    }
    return salida;
}
std::vector<Libro> LibraryService::relacionados(const std::string& id) const {
    exigir(catalogo_.find(id).has_value(), "Libro ausente", 404);
    std::vector<Libro> salida;
    for (const auto& vecino : actual_->grafo.vecinos(id)) salida.push_back(*catalogo_.find(vecino));
    return salida;
}
void LibraryService::validar() const { validar(*actual_); }
std::string LibraryService::snapshots() const {
    using nlohmann::json;
    auto colas = json::array();
    for (const auto& [id, cola] : actual_->colas) { (void)id; colas.push_back(json::parse(cola.snapshot())); }
    return json{{"bplus", json::parse(catalogo_.snapshot())}, {"usuarios", json::parse(actual_->usuarios.snapshot())},
        {"prestamos", json::parse(actual_->activos.snapshot())}, {"grafo", json::parse(actual_->grafo.snapshot())},
        {"reservas", std::move(colas)}}.dump();
}
} // namespace biblioteca
