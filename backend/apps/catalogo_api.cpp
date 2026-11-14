#include "biblioteca/CatalogService.hpp"
#include "biblioteca/JsonBookRepository.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <charconv>
#include <iostream>
#include <sstream>

using namespace biblioteca;
using Json = nlohmann::json;
namespace fs = std::filesystem;

namespace {
using Request = httplib::Request;
using Response = httplib::Response;

void reply(Response& res, int status, const Json& body) {
    res.status = status;
    res.set_content(body.dump(), "application/json; charset=utf-8");
}
void fail(Response& res, int status, const std::string& message) {
    reply(res, status, {{"error", message}});
}

// El bloqueo sobre un archivo estable sobrevive al rename atómico del JSON.
class WriterLock {
public:
    explicit WriterLock(const fs::path& working) {
        const auto path = working.string() + ".lock";
        fd_ = ::open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd_ < 0) throw std::runtime_error("No se pudo abrir el bloqueo del archivo de trabajo");
        if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) {
            ::close(fd_);
            throw std::runtime_error("El archivo de trabajo ya está en uso por otra API");
        }
    }
    ~WriterLock() { ::close(fd_); }
    WriterLock(const WriterLock&) = delete;
    WriterLock& operator=(const WriterLock&) = delete;
private:
    int fd_;
};

struct Settings {
    fs::path initial = "data/libros_iniciales.json";
    fs::path working = "data/catalogo_web.json";
    int port = 8080;
};
Settings settings(int argc, char** argv) {
    Settings cfg;
    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        if (++i == argc) throw std::invalid_argument("Falta valor para " + flag);
        std::string value = argv[i];
        if (flag == "--initial") cfg.initial = value;
        else if (flag == "--working") cfg.working = value;
        else if (flag == "--port") {
            const auto end = value.data() + value.size();
            auto parsed = std::from_chars(value.data(), end, cfg.port);
            if (parsed.ec != std::errc{} || parsed.ptr != end || cfg.port < 1 || cfg.port > 65535)
                throw std::invalid_argument("Puerto inválido");
        } else throw std::invalid_argument("Opción desconocida: " + flag);
    }
    return cfg;
}

Libro readBook(const std::string& body) {
    const auto json = Json::parse(body, [](int depth, Json::parse_event_t, Json&) {
        if (depth > 8) throw std::invalid_argument("Documento demasiado anidado");
        return true;
    });
    if (!json.is_object() || json.size() != 4 || !json.contains("idLibro") ||
        !json.contains("titulo") || !json.contains("autor") || !json.contains("temas"))
        throw std::invalid_argument("Campos requeridos: idLibro, titulo, autor y temas");
    return {json.at("idLibro").get<std::string>(), json.at("titulo").get<std::string>(),
            json.at("autor").get<std::string>(), json.at("temas").get<std::vector<std::string>>()};
}
Json bookJson(const Libro& book) {
    return {{"idLibro", book.idLibro}, {"titulo", book.titulo},
            {"autor", book.autor}, {"temas", book.temas}};
}
void listBooks(const CatalogService& service, const Request& req, Response& res) {
    std::vector<Libro> books;
    if (req.params.empty()) books = service.all();
    else {
        if (req.params.size() != 1) throw std::invalid_argument("Utilice título o prefijo");
        const auto& [key, value] = *req.params.begin();
        if (value.size() > 512 || normalizeTitle(value).empty())
            throw std::invalid_argument("Consulta vacía o mayor de 512 bytes");
        if (key == "titulo") books = service.byTitle(value);
        else if (key == "prefijo") books = service.byPrefix(value);
        else throw std::invalid_argument("Parámetro desconocido");
    }
    Json rows = Json::array();
    for (const auto& book : books) rows.push_back(bookJson(book));
    reply(res, 200, {{"libros", std::move(rows)}});
}
}

int main(int argc, char** argv) {
    try {
        const auto cfg = settings(argc, argv);
        JsonBookRepository repository(cfg.initial, cfg.working);
        WriterLock writer(repository.workingPath());
        CatalogService service(repository);
        httplib::Server server;
        // Un único trabajador serializa mutación, guardado y recuperación con las lecturas.
        server.new_task_queue = [] { return new httplib::ThreadPool(1, 1, 64); };
        server.set_socket_options([](int fd) { httplib::set_socket_opt(fd, SOL_SOCKET, SO_REUSEADDR, 1); });
        server.set_payload_max_length(32 * 1024);
        server.set_read_timeout(5);
        server.set_write_timeout(5);
        server.set_keep_alive_max_count(1);
        server.set_default_headers({{"Cache-Control", "no-store"}});
        bool ready = false;
        server.set_pre_routing_handler([&](const Request&, Response& res) {
            if (ready) return httplib::Server::HandlerResponse::Unhandled;
            fail(res, 503, "No se pudo recuperar el catálogo. Revise el JSON de trabajo y reinicie la API.");
            return httplib::Server::HandlerResponse::Handled;
        });
        auto persist = [&] {
            try { service.save(); }
            catch (const std::exception& e) {
                std::cerr << "Guardado: " << e.what() << '\n';
                ready = false;
                try { service.load(cfg.working); ready = true; }
                catch (const std::exception& recovery) { std::cerr << "Recarga: " << recovery.what() << '\n'; }
                throw std::runtime_error("No se pudo guardar la operación en el archivo de trabajo");
            }
        };
        server.Get("/api/libros", [&](const Request& req, Response& res) { listBooks(service, req, res); });
        server.Post("/api/libros", [&](const Request& req, Response& res) {
            const auto type = req.get_header_value("Content-Type");
            if (type.substr(0, type.find(';')) != "application/json") {
                fail(res, 415, "Se requiere Content-Type application/json"); return;
            }
            Libro book = readBook(req.body);
            service.insert(book);
            persist();
            reply(res, 201, bookJson(book));
        });
        server.Delete(R"(/api/libros/([A-Za-z0-9_-]{1,64}))", [&](const Request& req, Response& res) {
            if (!service.erase(req.matches[1].str())) { fail(res, 404, "El libro ya no está en el catálogo"); return; }
            persist();
            res.status = 204;
        });
        server.Get("/api/arbol", [&](const Request&, Response& res) {
            std::ostringstream out;
            service.visualize(out);
            reply(res, 200, {{"estructura", out.str()}});
        });
        server.Get("/api/arbol/validacion", [&](const Request&, Response& res) {
            std::string reason;
            const bool valid = service.validate(reason);
            reply(res, 200, {{"valido", valid}, {"detalle", reason}});
        });
        server.set_exception_handler([](const Request&, Response& res, std::exception_ptr error) {
            try { std::rethrow_exception(error); }
            catch (const DuplicateBookId& e) { fail(res, 409, e.what()); }
            catch (const std::invalid_argument& e) { fail(res, 400, e.what()); }
            catch (const Json::exception&) { fail(res, 400, "JSON inválido o tipo de campo incorrecto"); }
            catch (const std::exception& e) { fail(res, 503, e.what()); }
        });
        server.set_error_handler([](const Request&, Response& res) {
            if (res.body.empty()) fail(res, res.status, res.status == 413 ? "Cuerpo demasiado grande" : "Solicitud o ruta no válida");
        });
        if (!server.bind_to_port("127.0.0.1", cfg.port)) throw std::runtime_error("Puerto local no disponible");
        if (fs::exists(cfg.working)) service.load(cfg.working);
        else { service.load(cfg.initial); service.save(); }
        ready = true;
        std::cout << "API de catálogo en http://127.0.0.1:" << cfg.port << " · " << cfg.working.string() << std::endl;
        return server.listen_after_bind() ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
