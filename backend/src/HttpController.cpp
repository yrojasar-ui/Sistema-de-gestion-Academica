#include "biblioteca/HttpController.hpp"
#include "biblioteca/DomainJson.hpp"
#include <algorithm>
#include <charconv>
#include <regex>
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
    if (tipo.substr(0, tipo.find(';')) != "application/json")
        throw ErrorDominio(415, "Se requiere application/json");
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
    const auto limite = numero(req, "limite", clave == "libros" ? 10000U : 100U, 10000);
    const auto inicio = numero(req, "desplazamiento", 0, 10000);
    auto items = Json::array();
    for (auto i = inicio; i < filas.size() && i - inicio < limite; ++i) items.push_back(Json(filas[i]));
    responder(res, 200, {{clave, std::move(items)}, {"total", filas.size()}, {"limite", limite}, {"desplazamiento", inicio}});
}

void mismoId(const Json& j, const std::string& campo, const Request& req) {
    if (j.at(campo).get<std::string>() != req.matches[1].str())
        throw std::invalid_argument("ID de cuerpo y ruta diferentes");
}

template<class T, class F>
Json detalle(const std::vector<T>& filas, const std::string& id, F identificador) {
    auto it = std::find_if(filas.begin(), filas.end(), [&](const auto& v) { return identificador(v) == id; });
    if (it == filas.end()) throw ErrorDominio(404, "Registro ausente");
    return Json(*it);
}

bool ipValida(const std::string& ip) {
    if (ip.size() < 2 || ip.size() > 45) return false;
    return std::all_of(ip.begin(), ip.end(), [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '.' || c == ':';
    });
}

std::string recortarEspacios(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

// Solo lee X-Forwarded-For / X-Real-IP si la conexión TCP proviene de un proxy explícitamente confiable.
std::string clientIpForRateLimit(const Request& req, const std::vector<std::string>& trustedProxies) {
    if (std::find(trustedProxies.begin(), trustedProxies.end(), req.remote_addr) == trustedProxies.end())
        return req.remote_addr;
    if (req.has_header("X-Forwarded-For")) {
        const auto raw = req.get_header_value("X-Forwarded-For");
        std::vector<std::string> hops;
        std::stringstream ss(raw);
        std::string item;
        while (std::getline(ss, item, ',')) {
            auto cleaned = recortarEspacios(item);
            if (!cleaned.empty()) hops.push_back(std::move(cleaned));
        }
        for (auto it = hops.rbegin(); it != hops.rend(); ++it) {
            if (!ipValida(*it)) return req.remote_addr;
            if (std::find(trustedProxies.begin(), trustedProxies.end(), *it) == trustedProxies.end())
                return *it;
        }
    }
    if (req.has_header("X-Real-IP")) {
        auto realIp = recortarEspacios(req.get_header_value("X-Real-IP"));
        if (ipValida(realIp)) return realIp;
    }
    return req.remote_addr;
}

bool esRutaInterna(const std::string& path) {
    return path.rfind("/api/interno/", 0) == 0;
}

} // namespace

void registrarHttp(httplib::Server& s, CatalogService& catalogo, LibraryService& b,
                   AuthService& auth, const HttpOptions& opciones) {
    auto* mtx = opciones.stateMutex;
    const auto* serviceIdentity = opciones.serviceIdentity;
    const auto origenes = opciones.origenes;
    const auto recover = opciones.recover;
    const auto brokerStatus = opciones.brokerStatus;
    const auto wakeOutbox = opciones.wakeOutbox;
    const auto trustedProxies = opciones.trustedProxies;

    auto lockState = [mtx]() {
        return mtx ? std::unique_lock<std::mutex>(*mtx) : std::unique_lock<std::mutex>();
    };

    auto guard = [lockState](auto fn) {
        return [lockState, fn = std::move(fn)](const Request& req, Response& res) {
            auto lk = lockState();
            fn(req, res);
        };
    };

    s.set_pre_routing_handler([origenes, &b, &auth, serviceIdentity, lockState](const Request& req, Response& res) {
        auto lk = lockState();
        const auto origen = req.get_header_value("Origin");
        if (!origen.empty()) {
            if (esRutaInterna(req.path) || std::find(origenes.begin(), origenes.end(), origen) == origenes.end()) {
                responder(res, 403, {{"error", "Origen no permitido"}});
                return httplib::Server::HandlerResponse::Handled;
            }
            res.set_header("Access-Control-Allow-Origin", origen);
            res.set_header("Vary", "Origin");
        }
        if (esRutaInterna(req.path)) {
            if (!serviceIdentity || !serviceIdentity->configured()) {
                responder(res, 503, {{"error", "Identidad interna de servicio no configurada"}});
                return httplib::Server::HandlerResponse::Handled;
            }
            if (req.get_header_value_count("Authorization") != 1 ||
                !serviceIdentity->verifyAuthorization(req.get_header_value("Authorization"))) {
                responder(res, 403, {{"error", "Credencial de servicio requerida"}});
                return httplib::Server::HandlerResponse::Handled;
            }
            if (!b.repository().ready()) {
                responder(res, 503, {{"error", "Biblioteca bloqueada; consulte /readyz y recupere explícitamente"},
                                    {"resultado", b.repository().outcome()}});
                return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        }
        if (req.method != "OPTIONS" && req.path != "/healthz" && req.path != "/readyz" &&
            !(req.path == "/api/sesion/login" && req.method == "POST")) {
            try {
                if (req.get_header_value_count("Authorization") != 1)
                    throw ErrorDominio(401, "Sesión requerida o vencida");
                const auto authHeader = req.get_header_value("Authorization");
                // La identidad de servicio jamás tiene permisos en rutas humanas/administrativas.
                if (serviceIdentity && serviceIdentity->verifyAuthorization(authHeader))
                    throw ErrorDominio(403, "La identidad de servicio no tiene permisos en esta ruta");
                const auto user = auth.authenticate(authHeader);
                if (b.repository().ready()) {
                    bool active = false;
                    try { active = b.usuario(user.idUsuario).activo; } catch (const ErrorDominio&) {}
                    if (!active) throw ErrorDominio(401, "Sesión requerida o vencida");
                } else if (user.rol != "ADMIN") {
                    throw ErrorDominio(503, "Biblioteca no disponible");
                }
                if (user.rol != "ADMIN") {
                    static const std::regex readCatalog(R"(/api/libros(/[A-Za-z0-9_-]{1,64}(/relacionados)?)?)");
                    static const std::regex ownDetail(R"(/api/mis-(prestamos|reservas|pagos|avisos)/[A-Za-z0-9_-]{1,64})");
                    static const std::regex cancel(R"(/api/mis-reservas/[A-Za-z0-9_-]{1,64}/cancelacion)");
                    static const std::regex pay(R"(/api/mis-pagos/[A-Za-z0-9_-]{1,64}/simulacion)");
                    const bool allowed =
                        (req.method == "GET" && (std::regex_match(req.path, readCatalog) ||
                         std::regex_match(req.path, ownDetail) || req.path == "/api/mi-biblioteca" ||
                         req.path == "/api/mis-avisos" || req.path == "/api/disponibilidad" || req.path == "/api/sesion")) ||
                        (req.method == "POST" && (req.path == "/api/solicitudes" || req.path == "/api/sesion/logout" ||
                         std::regex_match(req.path, cancel) || std::regex_match(req.path, pay)));
                    if (!allowed) throw ErrorDominio(403, "Sin permisos para esta operación");
                }
            } catch (const ErrorDominio& e) {
                responder(res, e.status, {{"error", e.what()}});
                return httplib::Server::HandlerResponse::Handled;
            }
        }
        if (!b.repository().ready() && req.method != "OPTIONS" &&
            req.path != "/healthz" && req.path != "/readyz" && req.path != "/api/recuperacion" &&
            req.path != "/api/almacenamiento" && req.path.rfind("/api/sesion", 0) != 0) {
            responder(res, 503, {{"error", "Biblioteca bloqueada; consulte /readyz y recupere explícitamente"},
                                {"resultado", b.repository().outcome()}});
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    s.Options(R"(/.*)", [](const Request&, Response& res) {
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    });

    s.Get("/healthz", [](const Request&, Response& res) { responder(res, 200, {{"proceso", "activo"}}); });
    s.Get("/readyz", guard([&](const Request&, Response& res) {
        const auto& repo = b.repository();
        if (repo.ready()) b.validar();
        responder(res, repo.ready() ? 200 : 503, {{"listo", repo.ready()}});
    }));
    s.Post("/api/recuperacion", guard([&, recover, wakeOutbox](const Request& req, Response& res) {
        parametros(req, {}); campos(cuerpo(req), {});
        if (recover) recover();
        b.validar();
        if (wakeOutbox) wakeOutbox();
        responder(res, 200, {{"listo", b.repository().ready()}});
    }));

    // Interfaz interna autenticada exclusivamente para el worker (escritor único).
    s.Post(R"(/api/interno/avisos/([A-Za-z0-9_-]{1,64})/procesar)", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req);
        campos(j, {"idEvento", "titulo", "mensaje", "intentos"});
        if (!j.at("intentos").is_number_unsigned()) throw std::invalid_argument("Intentos debe ser entero sin signo");
        const auto resultado = b.procesarAviso(req.matches[1], j.at("idEvento"), j.at("titulo"),
                                               j.at("mensaje"), j.at("intentos").get<std::uint32_t>());
        responder(res, 200, {{"notificacion", resultado.notificacion},
                             {"duplicado", resultado.duplicado},
                             {"estado", "procesado"}});
    }));
    s.Post(R"(/api/interno/avisos/([A-Za-z0-9_-]{1,64})/fallo)", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req);
        campos(j, {"idEvento", "intentos", "error", "definitivo"});
        if (!j.at("intentos").is_number_unsigned() || !j.at("definitivo").is_boolean())
            throw std::invalid_argument("Campos inválidos para fallo de aviso");
        b.fallarAviso(req.matches[1], j.at("idEvento"), j.at("intentos").get<std::uint32_t>(),
                      j.at("error"), j.at("definitivo").get<bool>());
        const auto actual = detalle(b.estado().avisos, req.matches[1], [](const auto& x) { return x.idAviso; });
        responder(res, 200, {{"ok", true}, {"estado", actual.at("estado")}});
    }));

    s.Post("/api/sesion/login", guard([&, trustedProxies](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"idUsuario", "password"});
        const auto ip = clientIpForRateLimit(req, trustedProxies);
        const auto result = auth.login(j.at("idUsuario"), j.at("password"), ip, [&](const Identidad& i) {
            if (!b.repository().ready()) return i.rol == "ADMIN";
            try { return b.usuario(i.idUsuario).activo; } catch (const ErrorDominio&) { return false; }
        });
        responder(res, 200, result);
    }));
    s.Get("/api/sesion", guard([&](const Request& req, Response& res) {
        auto i = auth.authenticate(req.get_header_value("Authorization"));
        responder(res, 200, {{"idUsuario", i.idUsuario}, {"rol", i.rol}});
    }));
    s.Post("/api/sesion/logout", guard([&](const Request& req, Response& res) {
        campos(cuerpo(req), {}); auth.logout(req.get_header_value("Authorization")); res.status = 204;
    }));

    s.Get("/api/cuentas", guard([&](const Request&, Response& res) { responder(res, 200, {{"items", auth.accounts()}}); }));
    s.Post("/api/cuentas", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"idUsuario", "rol", "password"});
        const auto id = j.at("idUsuario").get<std::string>();
        if (!b.usuario(id).activo) throw ErrorDominio(409, "Usuario inactivo");
        auth.create(id, j.at("rol"), j.at("password"));
        responder(res, 201, {{"idUsuario", id}, {"rol", j.at("rol")}, {"activa", true}});
    }));
    s.Put(R"(/api/cuentas/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"rol", "activa", "password"});
        if (j.at("activa").get<bool>() && !b.usuario(req.matches[1]).activo) throw ErrorDominio(409, "Usuario inactivo");
        auth.update(req.matches[1], j.at("rol"), j.at("activa"), j.at("password"));
        responder(res, 200, {{"ok", true}});
    }));

    s.Get("/api/almacenamiento", guard([&, brokerStatus](const Request&, Response& res) {
        const auto& repo = b.repository();
        const auto st = brokerStatus ? brokerStatus() : BrokerStatus{};
        responder(res, 200, {
            {"listo", repo.ready()}, {"almacenamiento", repo.storage()},
            {"recuperacion", repo.recovery()}, {"resultado", repo.outcome()},
            {"broker", {{"configurado", st.configurado}, {"conectado", st.conectado}, {"ultimoError", st.ultimoError}}}
        });
    }));

    s.Get("/api/disponibilidad", guard([&](const Request&, Response& res) {
        auto rows = Json::array();
        for (const auto& libro : catalogo.all()) {
            int disponibles = 0, apartados = 0, prestados = 0;
            for (const auto& e : b.estado().ejemplares) if (e.idLibro == libro.idLibro) {
                disponibles += e.estado == "disponible";
                apartados += e.estado == "apartado";
                prestados += e.estado == "prestado";
            }
            rows.push_back({{"idLibro", libro.idLibro}, {"disponibles", disponibles},
                            {"apartados", apartados}, {"prestados", prestados}});
        }
        responder(res, 200, {{"items", rows}});
    }));

    s.Get("/api/mi-biblioteca", guard([&](const Request& req, Response& res) {
        auto i = auth.authenticate(req.get_header_value("Authorization"));
        auto owned = [&](const auto& rows) {
            auto out = Json::array();
            for (const auto& row : rows) if (row.idUsuario == i.idUsuario) out.push_back(row);
            return out;
        };
        auto libroResumen = [&](const std::string& id) {
            const auto libro = catalogo.find(id);
            if (!libro) throw ErrorDominio(500, "Referencia de libro incompatible");
            return Json{{"idLibro", libro->idLibro}, {"titulo", libro->titulo}, {"autor", libro->autor}};
        };
        auto prestamosPropios = [&](const auto& rows) {
            Json out = owned(rows);
            for (auto& row : out) {
                const Json ejemplar = detalle(b.estado().ejemplares, row.at("idEjemplar").get<std::string>(),
                                              [](const auto& e) { return e.idEjemplar; });
                row["libro"] = libroResumen(ejemplar.at("idLibro").get<std::string>());
            }
            return out;
        };
        auto reservas = owned(b.estado().reservas);
        for (auto& row : reservas) row["libro"] = libroResumen(row.at("idLibro").get<std::string>());
        std::size_t pendientes = 0, publicados = 0, fallidos = 0;
        for (const auto& a : b.estado().avisos) {
            if (a.idUsuario != i.idUsuario) continue;
            if (a.estado == "pendiente") ++pendientes;
            else if (a.estado == "publicado") ++publicados;
            else if (a.estado == "fallido") ++fallidos;
        }
        responder(res, 200, {
            {"usuario", b.usuario(i.idUsuario)}, {"activos", prestamosPropios(b.activos())},
            {"prestamos", prestamosPropios(b.estado().prestamos)}, {"reservas", reservas},
            {"cargos", owned(b.estado().cargos)}, {"comprobantes", owned(b.estado().comprobantes)},
            {"notificaciones", owned(b.estado().notificaciones)},
            {"avisosEnCurso", {{"pendientes", pendientes}, {"publicados", publicados}, {"fallidos", fallidos}}}
        });
    }));

    s.Get("/api/mis-avisos", guard([&](const Request& req, Response& res) {
        parametros(req, {});
        const auto i = auth.authenticate(req.get_header_value("Authorization"));
        auto items = Json::array();
        for (const auto& n : b.estado().notificaciones)
            if (n.idUsuario == i.idUsuario) items.push_back(Json(n));
        std::size_t pendientes = 0, publicados = 0, fallidos = 0;
        for (const auto& a : b.estado().avisos) {
            if (a.idUsuario != i.idUsuario) continue;
            if (a.estado == "pendiente") ++pendientes;
            else if (a.estado == "publicado") ++publicados;
            else if (a.estado == "fallido") ++fallidos;
        }
        responder(res, 200, {
            {"items", std::move(items)},
            {"pendientes", pendientes},
            {"publicados", publicados},
            {"fallidos", fallidos}
        });
    }));

    s.Get(R"(/api/mis-(prestamos|reservas|pagos|avisos)/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        const auto i = auth.authenticate(req.get_header_value("Authorization"));
        const std::string type = req.matches[1], id = req.matches[2];
        Json row;
        if (type == "prestamos") row = detalle(b.estado().prestamos, id, [](const auto& x) { return x.idPrestamo; });
        else if (type == "reservas") row = detalle(b.estado().reservas, id, [](const auto& x) { return x.idReserva; });
        else if (type == "pagos") row = detalle(b.estado().cargos, id, [](const auto& x) { return x.idPrestamo; });
        else row = detalle(b.estado().notificaciones, id, [](const auto& x) { return x.idNotificacion; });
        if (row.at("idUsuario") != i.idUsuario) throw ErrorDominio(404, "Registro ausente");
        responder(res, 200, row);
    }));

    s.Post("/api/solicitudes", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"idLibro"});
        const auto i = auth.authenticate(req.get_header_value("Authorization"));
        const auto id = "SOL-" + std::to_string(b.estado().siguienteLlegada);
        b.reservar(id, j.at("idLibro"), i.idUsuario);
        responder(res, 201, detalle(b.estado().reservas, id, [](const auto& r) { return r.idReserva; }));
    }));
    s.Post(R"(/api/mis-reservas/([A-Za-z0-9_-]{1,64})/cancelacion)", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"motivo"});
        const auto i = auth.authenticate(req.get_header_value("Authorization"));
        const auto r = detalle(b.estado().reservas, req.matches[1], [](const auto& x) { return x.idReserva; });
        if (r.at("idUsuario") != i.idUsuario) throw ErrorDominio(404, "Registro ausente");
        b.cancelar(req.matches[1], j.at("motivo"), i.idUsuario);
        responder(res, 200, {{"ok", true}});
    }));
    s.Get("/api/pagos", guard([&](const Request&, Response& res) {
        responder(res, 200, {{"cargos", b.estado().cargos}, {"comprobantes", b.estado().comprobantes}});
    }));
    s.Post(R"(/api/mis-pagos/([A-Za-z0-9_-]{1,64})/simulacion)", guard([&](const Request& req, Response& res) {
        campos(cuerpo(req), {});
        auto i = auth.authenticate(req.get_header_value("Authorization"));
        auto c = detalle(b.estado().cargos, req.matches[1], [](const auto& x) { return x.idPrestamo; });
        if (c.at("idUsuario") != i.idUsuario) throw ErrorDominio(404, "Registro ausente");
        responder(res, 200, b.pagar(req.matches[1]));
    }));
    s.Post(R"(/api/prestamos/([A-Za-z0-9_-]{1,64})/anulacion)", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"motivo", "copiaRecuperada"});
        const auto i = auth.authenticate(req.get_header_value("Authorization"));
        b.anular(req.matches[1], j.at("motivo"), i.idUsuario, j.at("copiaRecuperada"));
        responder(res, 200, {{"ok", true}});
    }));

    s.Get("/api/libros", guard([&](const Request& req, Response& res) {
        parametros(req, {"titulo", "prefijo", "limite", "desplazamiento"});
        if (req.has_param("titulo") && req.has_param("prefijo")) throw std::invalid_argument("Use título o prefijo");
        std::vector<Libro> filas;
        if (req.has_param("titulo") || req.has_param("prefijo")) {
            const bool porTitulo = req.has_param("titulo");
            const auto valor = req.get_param_value(porTitulo ? "titulo" : "prefijo");
            if (valor.size() > 512 || normalizeTitle(valor).empty())
                throw std::invalid_argument("Consulta vacía o demasiado larga");
            filas = porTitulo ? catalogo.byTitle(valor) : catalogo.byPrefix(valor);
        } else {
            filas = catalogo.all();
        }
        pagina(res, req, filas, "libros");
    }));
    s.Get(R"(/api/libros/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        auto libro = catalogo.find(req.matches[1]);
        if (!libro) throw ErrorDominio(404, "Libro ausente");
        responder(res, 200, Json(*libro));
    }));
    s.Post("/api/libros", guard([&](const Request& req, Response& res) {
        auto libro = cuerpo(req).get<Libro>();
        b.crearLibro(libro);
        responder(res, 201, Json(libro));
    }));
    s.Put(R"(/api/libros/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); mismoId(j, "idLibro", req);
        auto libro = j.get<Libro>();
        b.editarLibro(libro);
        responder(res, 200, Json(libro));
    }));
    s.Delete(R"(/api/libros/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        b.borrarLibro(req.matches[1]); res.status = 204;
    }));
    s.Get(R"(/api/libros/([A-Za-z0-9_-]{1,64})/relacionados)", guard([&](const Request& req, Response& res) {
        parametros(req, {"limite", "desplazamiento"});
        pagina(res, req, b.relacionados(req.matches[1]));
    }));
    s.Get("/api/arbol", guard([&](const Request&, Response& res) {
        std::ostringstream salida; catalogo.visualize(salida);
        responder(res, 200, {{"estructura", salida.str()}});
    }));
    s.Get("/api/arbol/validacion", guard([&](const Request&, Response& res) {
        std::string razon; const auto valido = catalogo.validate(razon);
        responder(res, 200, {{"valido", valido}, {"detalle", razon}});
    }));
    s.Get("/api/estructuras", guard([&](const Request&, Response& res) {
        responder(res, 200, Json::parse(b.snapshots()));
    }));
    s.Get("/api/estructuras/validacion", guard([&](const Request&, Response& res) {
        b.validar(); responder(res, 200, {{"valido", true}});
    }));
    s.Get("/api/dominio", guard([&](const Request&, Response& res) { responder(res, 200, Json(b.estado())); }));
    s.Put("/api/dominio", guard([&, wakeOutbox](const Request& req, Response& res) {
        auto next = cuerpo(req).get<EstadoBiblioteca>();
        auth.validateAdministrators(next);
        b.reconstruir(next);
        for (const auto& a : auth.accounts()) {
            const auto id = a.at("idUsuario").get<std::string>();
            try { if (!b.usuario(id).activo) auth.invalidate(id); } catch (const ErrorDominio&) { auth.invalidate(id); }
        }
        if (wakeOutbox) wakeOutbox();
        responder(res, 200, {{"ok", true}});
    }));
    s.Get("/api/usuarios", guard([&](const Request& req, Response& res) {
        parametros(req, {"limite", "desplazamiento"}); pagina(res, req, b.estado().usuarios);
    }));
    s.Get(R"(/api/usuarios/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        responder(res, 200, Json(b.usuario(req.matches[1])));
    }));
    s.Post("/api/usuarios", guard([&](const Request& req, Response& res) {
        auto u = cuerpo(req).get<Usuario>(); b.crearUsuario(u); responder(res, 201, Json(u));
    }));
    s.Put(R"(/api/usuarios/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); mismoId(j, "idUsuario", req);
        auto u = j.get<Usuario>();
        if (!u.activo && auth.admin(u.idUsuario)) throw ErrorDominio(409, "Deshabilite primero la cuenta administrativa");
        b.editarUsuario(u, auth.authenticate(req.get_header_value("Authorization")).idUsuario);
        if (!u.activo) auth.invalidate(u.idUsuario);
        responder(res, 200, Json(u));
    }));
    s.Delete(R"(/api/usuarios/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        if (auth.admin(req.matches[1])) throw ErrorDominio(409, "Deshabilite primero la cuenta administrativa");
        b.borrarUsuario(req.matches[1]); auth.invalidate(req.matches[1]); responder(res, 200, {{"ok", true}});
    }));
    s.Get("/api/ejemplares", guard([&](const Request& req, Response& res) {
        parametros(req, {"limite", "desplazamiento"}); pagina(res, req, b.estado().ejemplares);
    }));
    s.Post("/api/ejemplares", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"idEjemplar", "idLibro"});
        b.crearEjemplar(j.at("idEjemplar"), j.at("idLibro"));
        responder(res, 201, detalle(b.estado().ejemplares, j.at("idEjemplar"), [](const auto& v) { return v.idEjemplar; }));
    }));
    s.Get(R"(/api/ejemplares/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        responder(res, 200, detalle(b.estado().ejemplares, req.matches[1], [](const auto& v) { return v.idEjemplar; }));
    }));
    s.Put(R"(/api/ejemplares/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"estado"});
        b.estadoEjemplar(req.matches[1], j.at("estado")); responder(res, 200, {{"ok", true}});
    }));
    s.Delete(R"(/api/ejemplares/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        b.borrarEjemplar(req.matches[1]); responder(res, 200, {{"ok", true}});
    }));
    s.Get("/api/prestamos", guard([&](const Request& req, Response& res) {
        parametros(req, {"estado", "limite", "desplazamiento"});
        const auto estado = req.has_param("estado") ? req.get_param_value("estado") : "historial";
        if (estado != "activos" && estado != "historial") throw std::invalid_argument("Estado activos o historial requerido");
        pagina(res, req, estado == "activos" ? b.activos() : b.estado().prestamos);
    }));
    s.Get("/api/vencimientos", guard([&](const Request& req, Response& res) {
        parametros(req, {"hasta", "limite", "desplazamiento"});
        if (req.has_param("hasta")) validarFecha(req.get_param_value("hasta"));
        pagina(res, req, b.activos(req.get_param_value("hasta")));
    }));
    s.Post("/api/prestamos", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"idPrestamo", "idEjemplar", "idUsuario", "vencimiento"});
        b.prestar(j.at("idPrestamo"), j.at("idEjemplar"), j.at("idUsuario"), j.at("vencimiento"));
        responder(res, 201, detalle(b.estado().prestamos, j.at("idPrestamo"), [](const auto& v) { return v.idPrestamo; }));
    }));
    s.Get(R"(/api/prestamos/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        responder(res, 200, detalle(b.estado().prestamos, req.matches[1], [](const auto& v) { return v.idPrestamo; }));
    }));
    s.Post(R"(/api/prestamos/([A-Za-z0-9_-]{1,64})/devolucion)", guard([&](const Request& req, Response& res) {
        campos(cuerpo(req), {}); b.devolver(req.matches[1]); responder(res, 200, {{"ok", true}});
    }));
    s.Post(R"(/api/prestamos/([A-Za-z0-9_-]{1,64})/renovacion)", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"vencimiento"});
        b.renovar(req.matches[1], j.at("vencimiento")); responder(res, 200, {{"ok", true}});
    }));
    s.Get("/api/reservas", guard([&](const Request& req, Response& res) {
        parametros(req, {"limite", "desplazamiento"}); pagina(res, req, b.estado().reservas);
    }));
    s.Post("/api/reservas", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"idReserva", "idLibro", "idUsuario"});
        b.reservar(j.at("idReserva"), j.at("idLibro"), j.at("idUsuario"));
        responder(res, 201, detalle(b.estado().reservas, j.at("idReserva"), [](const auto& v) { return v.idReserva; }));
    }));
    s.Get(R"(/api/reservas/([A-Za-z0-9_-]{1,64}))", guard([&](const Request& req, Response& res) {
        responder(res, 200, detalle(b.estado().reservas, req.matches[1], [](const auto& v) { return v.idReserva; }));
    }));
    s.Post(R"(/api/reservas/([A-Za-z0-9_-]{1,64})/cancelacion)", guard([&](const Request& req, Response& res) {
        auto j = cuerpo(req); campos(j, {"motivo"});
        b.cancelar(req.matches[1], j.at("motivo"), auth.authenticate(req.get_header_value("Authorization")).idUsuario);
        responder(res, 200, {{"ok", true}});
    }));
    s.Get("/api/avisos", guard([&, brokerStatus](const Request& req, Response& res) {
        parametros(req, {"limite", "desplazamiento"});
        const auto& filas = b.estado().avisos;
        const auto limite = numero(req, "limite", 100U, 10000);
        const auto inicio = numero(req, "desplazamiento", 0, 10000);
        auto items = Json::array();
        for (auto i = inicio; i < filas.size() && i - inicio < limite; ++i) items.push_back(Json(filas[i]));
        std::size_t pendienteCount = 0, publicadoCount = 0, procesadoCount = 0, fallidoCount = 0;
        for (const auto& a : filas) {
            if (a.estado == "pendiente") ++pendienteCount;
            else if (a.estado == "publicado") ++publicadoCount;
            else if (a.estado == "procesado") ++procesadoCount;
            else if (a.estado == "fallido") ++fallidoCount;
        }
        const auto st = brokerStatus ? brokerStatus() : BrokerStatus{};
        responder(res, 200, {
            {"items", std::move(items)}, {"total", filas.size()}, {"limite", limite}, {"desplazamiento", inicio},
            {"resumen", {{"pendiente", pendienteCount}, {"publicado", publicadoCount},
                         {"procesado", procesadoCount}, {"fallido", fallidoCount}}},
            {"broker", {{"configurado", st.configurado}, {"conectado", st.conectado}, {"ultimoError", st.ultimoError}}}
        });
    }));
    s.Post(R"(/api/avisos/([A-Za-z0-9_-]{1,64})/reintento)", guard([&, wakeOutbox](const Request& req, Response& res) {
        parametros(req, {}); campos(cuerpo(req), {});
        b.reintentarAviso(req.matches[1]);
        if (wakeOutbox) wakeOutbox();
        responder(res, 200, {{"ok", true}});
    }));

    s.set_exception_handler([](const Request& req, Response& res, std::exception_ptr error) {
        int codigo = 503;
        std::string mensaje;
        try { std::rethrow_exception(error); }
        catch (const PersistenceError& e) {
            responder(res, 503, {{"error", e.what()}, {"resultado", outcomeName(e.outcome)}});
            return;
        }
        catch (const ErrorDominio& e) { codigo = e.status; mensaje = e.what(); }
        catch (const DuplicateBookId& e) { codigo = 409; mensaje = e.what(); }
        catch (const std::invalid_argument& e) { codigo = 400; mensaje = e.what(); }
        catch (const Json::exception&) { codigo = 400; mensaje = "JSON inválido o tipo incorrecto"; }
        catch (const std::exception&) { mensaje = "Operación no disponible"; }
        if (req.path == "/readyz") codigo = 503;
        responder(res, codigo, {{"error", mensaje}});
    });
    s.set_error_handler([](const Request&, Response& res) {
        if (res.body.empty()) responder(res, res.status, {{"error", "Solicitud o ruta no válida"}});
    });
}
} // namespace biblioteca
