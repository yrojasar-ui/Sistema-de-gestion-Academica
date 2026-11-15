#include "biblioteca/HttpController.hpp"
#include "biblioteca/DomainJson.hpp"
#include <algorithm>
#include <charconv>
#include <set>
#include <sstream>

namespace biblioteca {
namespace {
using Request = httplib::Request;
using Response = httplib::Response;
void responder(Response& res, int codigo, const Json& valor) {
    res.status = codigo;
    res.set_content(valor.dump(), "application/json; charset=utf-8");
}
Json cuerpo(const Request& req) {
    const auto tipo = req.get_header_value("Content-Type");
    if (tipo.substr(0,tipo.find(';')) != "application/json") throw ErrorDominio(415, "Se requiere application/json");
    std::vector<std::set<std::string>> objetos;
    return Json::parse(req.body, [&](int profundidad, Json::parse_event_t evento, Json& dato) {
        if (profundidad > 16) throw std::invalid_argument("JSON demasiado anidado");
        if (evento == Json::parse_event_t::object_start) objetos.emplace_back();
        if (evento == Json::parse_event_t::key && !objetos.back().insert(dato.get<std::string>()).second)
            throw std::invalid_argument("Campo JSON repetido");
        if (evento == Json::parse_event_t::object_end) objetos.pop_back();
        return true;
    });
}
void parametros(const Request& req, std::initializer_list<std::string> aceptados) {
    std::set<std::string> vistos;
    for (const auto& [clave, valor] : req.params) {
        (void)valor;
        if (std::find(aceptados.begin(), aceptados.end(), clave) == aceptados.end() || !vistos.insert(clave).second)
            throw std::invalid_argument("Parámetro desconocido o repetido");
    }
}
std::size_t numero(const Request& req, const std::string& clave, std::size_t predeterminado, std::size_t maximo) {
    if (!req.has_param(clave)) return predeterminado;
    auto texto = req.get_param_value(clave);
    std::size_t valor = 0;
    auto fin = texto.data() + texto.size();
    auto r = std::from_chars(texto.data(), fin, valor);
    if (r.ec != std::errc{} || r.ptr != fin || valor > maximo || (clave == "limite" && valor == 0))
        throw std::invalid_argument("Paginación inválida");
    return valor;
}
template<class T>
void pagina(Response& res, const Request& req, const std::vector<T>& filas, const std::string& clave = "items") {
    // El catálogo conserva el listado completo por defecto para la pantalla existente.
    const auto limite = numero(req, "limite", clave == "libros" ? 10000U : 100U, 10000);
    const auto inicio = numero(req, "desplazamiento", 0, 10000);
    auto items = Json::array();
    for (auto i = inicio; i < filas.size() && i - inicio < limite; ++i) items.push_back(Json(filas[i]));
    responder(res, 200, {{clave, std::move(items)}, {"total", filas.size()}, {"limite", limite}, {"desplazamiento", inicio}});
}
void mismoId(const Json& j, const std::string& campo, const Request& req) {
    if (j.at(campo).get<std::string>() != req.matches[1].str()) throw std::invalid_argument("ID de cuerpo y ruta diferentes");
}
template<class T, class F>
Json detalle(const std::vector<T>& filas, const std::string& id, F identificador) {
    auto it = std::find_if(filas.begin(), filas.end(), [&](const auto& v) { return identificador(v) == id; });
    if (it == filas.end()) throw ErrorDominio(404, "Registro ausente");
    return Json(*it);
}
}
void registrarHttp(httplib::Server& s, CatalogService& catalogo, LibraryService& b, const std::vector<std::string>& origenes) {
    s.set_pre_routing_handler([origenes](const Request& req, Response& res) {
        const auto origen = req.get_header_value("Origin");
        if (!origen.empty()) {
            if (std::find(origenes.begin(), origenes.end(), origen) == origenes.end()) {
                responder(res, 403, {{"error","Origen no permitido"}});
                return httplib::Server::HandlerResponse::Handled;
            }
            res.set_header("Access-Control-Allow-Origin", origen);
            res.set_header("Vary", "Origin");
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });
    s.Options(R"(/.*)", [](const Request&, Response& res) {
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });
    s.Get("/healthz", [](const Request&, Response& res) { responder(res, 200, {{"proceso","activo"}}); });
    s.Get("/readyz", [&](const Request&, Response& res) {
        b.validar();
        responder(res, 200, {{"listo",true},{"libros","json"},{"dominio","memoria"},{"eventos","pendientes"}});
    });
    s.Get("/api/libros", [&](const Request& req, Response& res) {
        parametros(req, {"titulo","prefijo","limite","desplazamiento"});
        if (req.has_param("titulo") && req.has_param("prefijo")) throw std::invalid_argument("Use título o prefijo");
        auto filas = catalogo.all();
        for (const auto* tipo : {"titulo","prefijo"}) {
            if (!req.has_param(tipo)) continue;
            const auto valor = req.get_param_value(tipo);
            if (valor.size() > 512 || normalizeTitle(valor).empty()) throw std::invalid_argument("Consulta vacía o demasiado larga");
            filas = std::string(tipo) == "titulo" ? catalogo.byTitle(valor) : catalogo.byPrefix(valor);
        }
        pagina(res, req, filas, "libros");
    });
    s.Get(R"(/api/libros/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        auto libro = catalogo.find(req.matches[1]);
        if (!libro) throw ErrorDominio(404, "Libro ausente");
        responder(res, 200, Json(*libro));
    });
    s.Post("/api/libros", [&](const Request& req, Response& res) {
        auto libro = cuerpo(req).get<Libro>();
        b.crearLibro(libro);
        responder(res, 201, Json(libro));
    });
    s.Put(R"(/api/libros/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        auto j = cuerpo(req); mismoId(j, "idLibro", req);
        auto libro = j.get<Libro>();
        b.editarLibro(libro);
        responder(res, 200, Json(libro));
    });
    s.Delete(R"(/api/libros/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        b.borrarLibro(req.matches[1]); res.status = 204;
    });
    s.Get(R"(/api/libros/([A-Za-z0-9_-]{1,64})/relacionados)", [&](const Request& req, Response& res) {
        parametros(req, {"limite","desplazamiento"});
        pagina(res, req, b.relacionados(req.matches[1]));
    });
    s.Get("/api/arbol", [&](const Request&, Response& res) {
        std::ostringstream salida; catalogo.visualize(salida);
        responder(res, 200, {{"estructura",salida.str()}});
    });
    s.Get("/api/arbol/validacion", [&](const Request&, Response& res) {
        std::string razon; const auto valido = catalogo.validate(razon);
        responder(res, 200, {{"valido",valido},{"detalle",razon}});
    });
    s.Get("/api/estructuras", [&](const Request&, Response& res) { responder(res, 200, Json::parse(b.snapshots())); });
    s.Get("/api/estructuras/validacion", [&](const Request&, Response& res) {
        b.validar(); responder(res, 200, {{"valido",true}});
    });
    s.Get("/api/dominio", [&](const Request&, Response& res) { responder(res, 200, Json(b.estado())); });
    s.Put("/api/dominio", [&](const Request& req, Response& res) {
        b.reconstruir(cuerpo(req).get<EstadoBiblioteca>());
        responder(res, 200, {{"ok",true}});
    });
    s.Get("/api/usuarios", [&](const Request& req, Response& res) {
        parametros(req, {"limite","desplazamiento"}); pagina(res, req, b.estado().usuarios);
    });
    s.Get(R"(/api/usuarios/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        responder(res, 200, Json(b.usuario(req.matches[1])));
    });
    s.Post("/api/usuarios", [&](const Request& req, Response& res) {
        auto u = cuerpo(req).get<Usuario>(); b.crearUsuario(u); responder(res, 201, Json(u));
    });
    s.Put(R"(/api/usuarios/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        auto j = cuerpo(req); mismoId(j, "idUsuario", req);
        auto u = j.get<Usuario>(); b.editarUsuario(u); responder(res, 200, Json(u));
    });
    s.Delete(R"(/api/usuarios/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        b.borrarUsuario(req.matches[1]); responder(res, 200, {{"ok",true}});
    });
    s.Get("/api/ejemplares", [&](const Request& req, Response& res) {
        parametros(req, {"limite","desplazamiento"}); pagina(res, req, b.estado().ejemplares);
    });
    s.Post("/api/ejemplares", [&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"idEjemplar","idLibro"});
        b.crearEjemplar(j.at("idEjemplar"), j.at("idLibro"));
        responder(res, 201, detalle(b.estado().ejemplares, j.at("idEjemplar"), [](const auto& v) { return v.idEjemplar; }));
    });
    s.Get(R"(/api/ejemplares/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        responder(res, 200, detalle(b.estado().ejemplares, req.matches[1], [](const auto& v) { return v.idEjemplar; }));
    });
    s.Put(R"(/api/ejemplares/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"estado"});
        b.estadoEjemplar(req.matches[1], j.at("estado")); responder(res, 200, {{"ok",true}});
    });
    s.Delete(R"(/api/ejemplares/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        b.borrarEjemplar(req.matches[1]); responder(res, 200, {{"ok",true}});
    });
    s.Get("/api/prestamos", [&](const Request& req, Response& res) {
        parametros(req, {"estado","limite","desplazamiento"});
        const auto estado = req.has_param("estado") ? req.get_param_value("estado") : "historial";
        if (estado != "activos" && estado != "historial") throw std::invalid_argument("Estado activos o historial requerido");
        pagina(res, req, estado == "activos" ? b.activos() : b.estado().prestamos);
    });
    s.Get("/api/vencimientos", [&](const Request& req, Response& res) {
        parametros(req, {"hasta","limite","desplazamiento"});
        if (req.has_param("hasta")) validarFecha(req.get_param_value("hasta"));
        pagina(res, req, b.activos(req.get_param_value("hasta")));
    });
    s.Post("/api/prestamos", [&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"idPrestamo","idEjemplar","idUsuario","vencimiento"});
        b.prestar(j.at("idPrestamo"), j.at("idEjemplar"), j.at("idUsuario"), j.at("vencimiento"));
        responder(res, 201, detalle(b.estado().prestamos, j.at("idPrestamo"), [](const auto& v) { return v.idPrestamo; }));
    });
    s.Get(R"(/api/prestamos/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        responder(res, 200, detalle(b.estado().prestamos, req.matches[1], [](const auto& v) { return v.idPrestamo; }));
    });
    s.Post(R"(/api/prestamos/([A-Za-z0-9_-]{1,64})/devolucion)", [&](const Request& req, Response& res) {
        campos(cuerpo(req), {}); b.devolver(req.matches[1]); responder(res, 200, {{"ok",true}});
    });
    s.Post(R"(/api/prestamos/([A-Za-z0-9_-]{1,64})/renovacion)", [&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"vencimiento"});
        b.renovar(req.matches[1], j.at("vencimiento")); responder(res, 200, {{"ok",true}});
    });
    s.Get("/api/reservas", [&](const Request& req, Response& res) {
        parametros(req, {"limite","desplazamiento"}); pagina(res, req, b.estado().reservas);
    });
    s.Post("/api/reservas", [&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"idReserva","idLibro","idUsuario"});
        b.reservar(j.at("idReserva"), j.at("idLibro"), j.at("idUsuario"));
        responder(res, 201, detalle(b.estado().reservas, j.at("idReserva"), [](const auto& v) { return v.idReserva; }));
    });
    s.Get(R"(/api/reservas/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
        responder(res, 200, detalle(b.estado().reservas, req.matches[1], [](const auto& v) { return v.idReserva; }));
    });
    s.Post(R"(/api/reservas/([A-Za-z0-9_-]{1,64})/cancelacion)", [&](const Request& req, Response& res) {
        campos(cuerpo(req), {}); b.cancelar(req.matches[1]); responder(res, 200, {{"ok",true}});
    });
    s.Get("/api/avisos", [&](const Request& req, Response& res) {
        parametros(req, {"limite","desplazamiento"}); pagina(res, req, b.estado().avisos);
    });
    s.set_exception_handler([](const Request& req, Response& res, std::exception_ptr error) {
        int codigo = 503;
        std::string mensaje;
        try { std::rethrow_exception(error); }
        catch (const ErrorDominio& e) { codigo = e.status; mensaje = e.what(); }
        catch (const DuplicateBookId& e) { codigo = 409; mensaje = e.what(); }
        catch (const std::invalid_argument& e) { codigo = 400; mensaje = e.what(); }
        catch (const Json::exception&) { codigo = 400; mensaje = "JSON inválido o tipo incorrecto"; }
        catch (const std::exception& e) { mensaje = e.what(); }
        if (req.path == "/readyz") codigo = 503;
        responder(res, codigo, {{"error",mensaje}});
    });
    s.set_error_handler([](const Request&, Response& res) {
        if (res.body.empty()) responder(res, res.status, {{"error","Solicitud o ruta no válida"}});
    });
}
} // namespace biblioteca
