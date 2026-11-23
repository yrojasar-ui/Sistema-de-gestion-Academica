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
        if (activo(p)) exigir(nuevo->activos.insertar({p.vencimiento, p.idPrestamo}), "Préstamo repetido");
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
    const auto avisosPrevios = actual_->datos.avisos.size();
    bool hayPendienteNuevo = false;
    auto nuevo = std::make_unique<Runtime>(*actual_);
    operacion(*nuevo);
    validar(*nuevo);
    if (nuevo->datos.avisos.size() > avisosPrevios) hayPendienteNuevo = true;
    else {
        for (std::size_t i = 0; i < nuevo->datos.avisos.size(); ++i) {
            if (nuevo->datos.avisos[i].estado == "pendiente" && actual_->datos.avisos[i].estado != "pendiente") {
                hayPendienteNuevo = true;
                break;
            }
        }
    }
    repositorio_.replace(nuevo->datos);
    actual_.swap(nuevo);
    if (hayPendienteNuevo && onAvisoCreado_) onAvisoCreado_();
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
void LibraryService::editarUsuario(const Usuario& u, const std::string& responsable) {
    validarId(responsable);
    nombreValido(u);
    cambiar([&](Runtime& e) {
        usuario(e, u.idUsuario);
        if (!u.activo) {
            for (const auto& p : e.datos.prestamos)
                exigir(p.idUsuario != u.idUsuario || !activo(p), "Usuario con préstamo activo");
            for (auto& r : e.datos.reservas)
                if (r.idUsuario == u.idUsuario && pendiente(r)) { liberar(e, r); r.cancelacion.responsable = responsable; }
        }
        e.usuarios.modificar(u);
        obtener(e.datos.usuarios, u.idUsuario, [](const auto& x) { return x.idUsuario; }) = u;
        for (const auto& b : catalogo_.all()) asignar(e, b.idLibro);
    });
}
void LibraryService::borrarUsuario(const std::string& id) {
    validarId(id);
    cambiar([&](Runtime& e) {
        auto u = usuario(e, id);
        for (const auto& p : e.datos.prestamos)
            exigir(p.idUsuario != id || !activo(p), "Usuario con préstamo activo");
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
    validarId(id);
    validarId(libro);
    exigir(catalogo_.find(libro).has_value(), "Libro ausente", 404);
    cambiar([&](Runtime& e) {
        unico(e.datos.ejemplares, id, [](const auto& x) { return x.idEjemplar; });
        e.datos.ejemplares.push_back({id, libro, "disponible"});
        asignar(e, libro);
    });
}
void LibraryService::estadoEjemplar(const std::string& id, const std::string& estado) {
    validarId(id);
    exigir(estado == "disponible" || estado == "retirado", "Estado editable disponible o retirado", 400);
    cambiar([&](Runtime& e) {
        auto& c = copia(e.datos, id);
        exigir(c.estado == "disponible" || c.estado == "retirado", "Ejemplar comprometido");
        c.estado = estado;
        asignar(e, c.idLibro);
    });
}
void LibraryService::borrarEjemplar(const std::string& id) {
    validarId(id);
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
    validarId(id);
    validarId(ejemplar);
    validarId(lector);
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
        e.datos.prestamos.push_back({id, ejemplar, lector, ahoraUTC(), vence, "", {}});
        exigir(e.activos.insertar({vence, id}), "Clave de préstamo duplicada");
        e.datos.cargos.push_back({id, lector, importeDemo_, monedaDemo_, "pendiente"});
        aviso(e, "prestamo_registrado", lector, "", id);
    });
}
void LibraryService::devolver(const std::string& id) {
    validarId(id);
    cambiar([&](Runtime& e) {
        auto& p = prestamo(e.datos, id);
        exigir(activo(p), "Préstamo ya devuelto");
        p.devolucion = ahoraUTC();
        exigir(e.activos.eliminar({p.vencimiento, id}), "Índice activo inconsistente");
        auto& c = copia(e.datos, p.idEjemplar);
        c.estado = "disponible";
        aviso(e, "devolucion", p.idUsuario, "", id);
        asignar(e, c.idLibro);
    });
}
void LibraryService::renovar(const std::string& id, const std::string& vence) {
    validarId(id);
    fechaFutura(vence);
    cambiar([&](Runtime& e) {
        auto& p = prestamo(e.datos, id);
        exigir(activo(p), "Préstamo devuelto");
        exigir(vence > p.vencimiento, "Renovación debe ampliar el vencimiento", 400);
        exigir(e.activos.cambiarVencimiento({p.vencimiento, id}, vence), "Índice activo inconsistente");
        p.vencimiento = vence;
        aviso(e, "prestamo_renovado", p.idUsuario, "", id);
    });
}
void LibraryService::reservar(const std::string& id, const std::string& libro, const std::string& lector) {
    validarId(id);
    validarId(libro);
    validarId(lector);
    exigir(catalogo_.find(libro).has_value(), "Libro ausente", 404);
    cambiar([&](Runtime& e) {
        unico(e.datos.reservas, id, [](const auto& x) { return x.idReserva; });
        exigir(usuario(e, lector).activo, "Usuario inactivo");
        for (const auto& r : e.datos.reservas)
            exigir(!(pendiente(r) && r.idLibro == libro && r.idUsuario == lector), "Reserva pendiente duplicada");
        exigir(e.datos.siguienteLlegada < 9007199254740991ULL, "Secuencia agotada");
        const auto turno = e.datos.siguienteLlegada++;
        e.datos.reservas.push_back({id, libro, lector, turno, "pendiente", "", ahoraUTC(), {}});
        e.colas.try_emplace(libro, libro).first->second.encolar(id, turno);
        asignar(e, libro);
    });
}
void LibraryService::liberar(Runtime& e, Reserva& r) {
    if (r.estado == "apartada") copia(e.datos, r.idEjemplar).estado = "disponible";
    r.cancelacion = {ahoraUTC(), "Desactivación del usuario", "servicio"};
    r.estado = "cancelada"; // Su ID puede permanecer en FIFO hasta descartarse al atender.
    aviso(e, "reserva_cancelada", r.idUsuario, r.idReserva, "");
}
void LibraryService::cancelar(const std::string& id, const std::string& motivo, const std::string& responsable) {
    validarId(id);
    validarId(responsable);
    exigir(!normalizeTitle(motivo).empty() && motivo.size() <= 512, "Motivo requerido (máximo 512 bytes)", 400);
    cambiar([&](Runtime& e) {
        auto& r = reserva(e.datos, id);
        exigir(pendiente(r), "Reserva ya finalizada");
        liberar(e, r);
        r.cancelacion = {ahoraUTC(), motivo, responsable};
        asignar(e, r.idLibro);
    });
}
void LibraryService::configurarCargo(std::int64_t importe, const std::string& moneda) {
    exigir(importe >= 0 && importe <= 100000000, "Importe demo fuera de rango", 400);
    exigir(moneda.size() == 3 && std::all_of(moneda.begin(), moneda.end(), [](char c) { return c >= 'A' && c <= 'Z'; }),
           "Moneda ISO de tres letras", 400);
    importeDemo_ = importe; monedaDemo_ = moneda;
}
void LibraryService::anular(const std::string& id, const std::string& motivo, const std::string& responsable, bool recuperada) {
    validarId(id);
    validarId(responsable);
    exigir(recuperada, "Confirme que la copia no salió o fue recuperada", 400);
    exigir(!normalizeTitle(motivo).empty() && motivo.size() <= 512, "Motivo requerido (máximo 512 bytes)", 400);
    cambiar([&](Runtime& e) {
        auto& p = prestamo(e.datos, id);
        exigir(activo(p), "Solo se anulan préstamos activos");
        p.cancelacion = {ahoraUTC(), motivo, responsable};
        exigir(e.activos.eliminar({p.vencimiento, id}), "Índice activo inconsistente");
        auto& c = copia(e.datos, p.idEjemplar);
        c.estado = "disponible";
        for (auto& cargo : e.datos.cargos) if (cargo.idPrestamo == id) cargo.estado = "anulado";
        aviso(e, "prestamo_anulado", p.idUsuario, "", id);
        asignar(e, c.idLibro);
    });
}
Comprobante LibraryService::pagar(const std::string& id) {
    validarId(id);
    // Verificación idempotente antes de mutar: evita reescrituras innecesarias en doble clic o reintento
    // y garantiza un único comprobante y un único aviso de pago simulado.
    const auto cargoActual = std::find_if(actual_->datos.cargos.begin(), actual_->datos.cargos.end(),
                                          [&](const auto& c) { return c.idPrestamo == id; });
    if (cargoActual == actual_->datos.cargos.end()) throw ErrorDominio(404, "Registro ausente: " + id);
    exigir(cargoActual->estado != "anulado", "Cargo anulado");
    if (cargoActual->estado == "pagado") {
        const auto existente = std::find_if(actual_->datos.comprobantes.begin(), actual_->datos.comprobantes.end(),
                                            [&](const auto& c) { return c.idPrestamo == id; });
        if (existente != actual_->datos.comprobantes.end()) return *existente;
    }
    Comprobante resultado;
    cambiar([&](Runtime& e) {
        auto& cargo = obtener(e.datos.cargos, id, [](const auto& c) { return c.idPrestamo; });
        exigir(cargo.estado != "anulado", "Cargo anulado");
        for (const auto& c : e.datos.comprobantes)
            if (c.idPrestamo == id) { resultado = c; return; }
        resultado = {"PG-" + std::to_string(e.datos.comprobantes.size() + 1), ahoraUTC(), cargo.idUsuario,
                     id, cargo.importe, cargo.moneda, "SIMULADO"};
        e.datos.comprobantes.push_back(resultado);
        cargo.estado = "pagado";
        aviso(e, "pago_simulado", cargo.idUsuario, "", id);
    });
    return resultado;
}
void LibraryService::marcarAvisoPublicado(const std::string& idAviso) {
    validarId(idAviso);
    // Si el worker ya procesó el aviso antes de que el outbox marcara publicado, no retroceder.
    auto actual = std::find_if(actual_->datos.avisos.begin(), actual_->datos.avisos.end(),
                               [&](const auto& a) { return a.idAviso == idAviso; });
    if (actual == actual_->datos.avisos.end()) throw ErrorDominio(404, "Aviso ausente: " + idAviso);
    if (actual->estado == "procesado" || actual->estado == "publicado") return;
    if (actual->estado != "pendiente") return;
    cambiar([&](Runtime& e) {
        auto& a = obtener(e.datos.avisos, idAviso, [](const auto& x) { return x.idAviso; });
        if (a.estado == "pendiente") {
            a.estado = "publicado";
            a.publicadoEn = ahoraUTC();
        }
    });
}
ResultadoProcesoAviso LibraryService::procesarAviso(const std::string& idAviso, const std::string& idEvento,
                                                    const std::string& titulo, const std::string& mensaje,
                                                    std::uint32_t intentos) {
    validarId(idAviso);
    validarId(idEvento);
    exigir(!normalizeTitle(titulo).empty() && titulo.size() <= 128, "Título de notificación requerido (máximo 128 bytes)", 400);
    exigir(!normalizeTitle(mensaje).empty() && mensaje.size() <= 512, "Mensaje de notificación requerido (máximo 512 bytes)", 400);
    // Deduplicación persistente e idempotente: si ya fue procesado, devolver la notificación existente sin duplicar.
    auto existente = std::find_if(actual_->datos.avisos.begin(), actual_->datos.avisos.end(),
                                  [&](const auto& a) { return a.idAviso == idAviso; });
    if (existente == actual_->datos.avisos.end()) throw ErrorDominio(404, "Aviso ausente: " + idAviso);
    exigir(existente->idEvento == idEvento, "ID de evento no coincide con el aviso", 400);
    if (existente->estado == "procesado") {
        auto notif = std::find_if(actual_->datos.notificaciones.begin(), actual_->datos.notificaciones.end(),
                                  [&](const auto& n) { return n.idAviso == idAviso; });
        if (notif == actual_->datos.notificaciones.end()) throw ErrorDominio(500, "Notificación procesada ausente");
        return {*notif, true};
    }
    ResultadoProcesoAviso resultado;
    cambiar([&](Runtime& e) {
        auto& a = obtener(e.datos.avisos, idAviso, [](const auto& x) { return x.idAviso; });
        exigir(a.idEvento == idEvento, "ID de evento no coincide con el aviso", 400);
        if (a.estado == "procesado") {
            resultado.notificacion = obtener(e.datos.notificaciones, idAviso, [](const auto& n) { return n.idAviso; });
            resultado.duplicado = true;
            return;
        }
        const auto instante = ahoraUTC();
        a.estado = "procesado";
        a.procesadoEn = instante;
        a.intentos = std::max(a.intentos, std::max<std::uint32_t>(1U, intentos));
        a.ultimoError.clear();
        Notificacion n{"NT-" + a.idAviso, a.idAviso, a.idEvento, a.idUsuario, a.tipo,
                       titulo, mensaje, a.idReserva, a.idPrestamo, instante};
        e.datos.notificaciones.push_back(n);
        resultado = {n, false};
    });
    return resultado;
}
void LibraryService::fallarAviso(const std::string& idAviso, const std::string& idEvento,
                                 std::uint32_t intentos, const std::string& error, bool definitivo) {
    validarId(idAviso);
    validarId(idEvento);
    exigir(!normalizeTitle(error).empty() && error.size() <= 512, "Detalle de error requerido (máximo 512 bytes)", 400);
    auto existente = std::find_if(actual_->datos.avisos.begin(), actual_->datos.avisos.end(),
                                  [&](const auto& a) { return a.idAviso == idAviso; });
    if (existente == actual_->datos.avisos.end()) throw ErrorDominio(404, "Aviso ausente: " + idAviso);
    exigir(existente->idEvento == idEvento, "ID de evento no coincide con el aviso", 400);
    if (existente->estado == "procesado") return;
    cambiar([&](Runtime& e) {
        auto& a = obtener(e.datos.avisos, idAviso, [](const auto& x) { return x.idAviso; });
        if (a.estado == "procesado") return;
        a.intentos = std::max(a.intentos, std::max<std::uint32_t>(1U, intentos));
        a.ultimoError = error;
        if (definitivo) a.estado = "fallido";
    });
}
void LibraryService::reintentarAviso(const std::string& idAviso) {
    validarId(idAviso);
    cambiar([&](Runtime& e) {
        auto& a = obtener(e.datos.avisos, idAviso, [](const auto& x) { return x.idAviso; });
        exigir(a.estado == "fallido", "Solo se pueden reencolar avisos en estado fallido");
        a.estado = "pendiente";
        a.publicadoEn.clear();
        a.ultimoError.clear();
    });
}
void LibraryService::aviso(Runtime& e, const std::string& tipo, const std::string& lector,
                           const std::string& reservaId, const std::string& prestamoId) {
    const auto id = "AV-" + std::to_string(e.datos.avisos.size() + 1);
    e.datos.avisos.push_back({id, "EV-" + id, tipo, lector, reservaId, prestamoId, ahoraUTC(), "pendiente", "", "", 0U, ""});
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
    validarId(id);
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
    validarId(id);
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
