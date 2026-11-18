#include "biblioteca/HttpController.hpp"
#include "biblioteca/JsonBookRepository.hpp"
#include "biblioteca/LocalLibraryRepository.hpp"
#include <httplib.h>
#ifdef BIBLIOTECA_MONGO
#include "biblioteca/MongoLibraryRepository.hpp"
#endif
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <charconv>
#include <iostream>
#include <termios.h>
#include <limits>

using namespace biblioteca;
namespace fs = std::filesystem;

namespace {
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
    std::string storage = "local", endpoint, collection;
    fs::path mongoState, authStore = "data/privado/cuentas.json";
    std::string bootstrap, adminName, moneda = "CRC";
    int sessionSeconds = 1800;
    std::int64_t charge = 10000;
    bool initialize = false, migrate = false;
    std::vector<std::string> origins{"http://localhost:5173", "http://127.0.0.1:5173"};
};
Settings settings(int argc, char** argv) {
    Settings cfg;
    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--migrate") { cfg.migrate = true; continue; }
        if (flag == "--initialize") { cfg.initialize = true; continue; }
        if (++i == argc) throw std::invalid_argument("Falta valor para " + flag);
        std::string value = argv[i];
        if (flag == "--auth-store") cfg.authStore = value;
        else if (flag == "--bootstrap-admin") cfg.bootstrap = value;
        else if (flag == "--admin-name") cfg.adminName = value;
        else if (flag == "--currency") cfg.moneda = value;
        else if (flag == "--demo-charge" || flag == "--session-seconds") {
            std::int64_t n = 0;
            auto r = std::from_chars(value.data(),value.data()+value.size(),n);
            if (r.ec != std::errc{} || r.ptr != value.data()+value.size() || n < 0 || n > 100000000)
                throw std::invalid_argument("Importe o duración inválida");
            if (flag == "--demo-charge") cfg.charge = n; else cfg.sessionSeconds = static_cast<int>(n);
        }
        else if (flag == "--storage") cfg.storage = value;
        else if (flag == "--endpoint") cfg.endpoint = value;
        else if (flag == "--collection") cfg.collection = value;
        else if (flag == "--mongo-state") cfg.mongoState = value;
        else if (flag == "--initial") cfg.initial = value;
        else if (flag == "--working") cfg.working = value;
        else if (flag == "--origin") cfg.origins.push_back(value);
        else if (flag == "--port") {
            const auto end = value.data() + value.size();
            auto parsed = std::from_chars(value.data(), end, cfg.port);
            if (parsed.ec != std::errc{} || parsed.ptr != end || cfg.port < 1 || cfg.port > 65535)
                throw std::invalid_argument("Puerto inválido");
        } else throw std::invalid_argument("Opción desconocida: " + flag);
    }
    if (cfg.storage != "local" && cfg.storage != "mongo") throw std::invalid_argument("Use --storage local o mongo");
    if (cfg.storage == "mongo" && (cfg.endpoint.empty() || cfg.collection.empty() || cfg.mongoState.empty()))
        throw std::invalid_argument("Mongo requiere --endpoint, --collection y --mongo-state");
    if (cfg.storage == "local" && (cfg.initialize || !cfg.endpoint.empty() || !cfg.collection.empty() || !cfg.mongoState.empty()))
        throw std::invalid_argument("Opciones Mongo requieren --storage mongo");
    return cfg;
}

}

int main(int argc, char** argv) {
    try {
        const auto cfg = settings(argc, argv);
        WriterLock writer(cfg.storage == "mongo" ? cfg.mongoState : cfg.working);
        std::unique_ptr<LocalLibraryRepository> repository;
        if (cfg.storage == "local") repository = std::make_unique<LocalLibraryRepository>(cfg.initial, cfg.working, cfg.migrate);
        IBookRepository* books = repository.get();
        IDomainRepository* domain = repository.get();
#ifdef BIBLIOTECA_MONGO
        std::unique_ptr<MongoLibraryRepository> remote;
        if (cfg.storage == "mongo") {
            remote = std::make_unique<MongoLibraryRepository>(cfg.endpoint, cfg.collection,
                                                             cfg.mongoState.string() + ".pending.json");
            remote->allowMigration(cfg.migrate);
            books = remote.get(); domain = remote.get();
        }
#else
        if (cfg.storage == "mongo") throw std::invalid_argument("Compilación sin soporte Mongo");
#endif
        AuthService auth(cfg.authStore,cfg.sessionSeconds);
        CatalogService service(*books);
        httplib::Server server;
        // Un único trabajador serializa mutación, guardado y recuperación con las lecturas.
        server.new_task_queue = [] { return new httplib::ThreadPool(1, 1, 64); };
        server.set_socket_options([](int fd) { httplib::set_socket_opt(fd, SOL_SOCKET, SO_REUSEADDR, 1); });
        server.set_payload_max_length(1024 * 1024);
        server.set_read_timeout(5);
        server.set_write_timeout(5);
        server.set_keep_alive_max_count(1);
        server.set_default_headers({{"Cache-Control", "no-store"}});

        if (cfg.storage == "local") {
            service.load(repository->workingPath());
        }
        LibraryService biblioteca(service, *domain, cfg.storage == "mongo");
        std::function<void()> recover;
#ifdef BIBLIOTECA_MONGO
        auto rebuild = [&](bool initialize) {
            try {
                remote->refresh(initialize);
                if (cfg.migrate) remote->migrateDomain();
                CatalogService candidate(*remote);
                candidate.load(remote->workingPath());
                LibraryService candidateLibrary(candidate, *remote);
                candidateLibrary.validar();
                service.swapState(candidate);
                biblioteca.swapState(candidateLibrary);
                remote->accept();
            } catch (const std::exception& e) {
                remote->block(e.what());
                throw;
            }
        };
        if (remote) {
            recover = [&] {
                try { rebuild(false); }
                catch (const std::exception& e) { throw PersistenceError(
                    remote->outcome() == "incierto" ? WriteOutcome::incierto : WriteOutcome::rechazado, e.what()); }
            };
            try { rebuild(cfg.initialize); }
            catch (const std::exception& e) { std::cerr << "Mongo no listo: " << e.what() << '\n'; }
        }
#endif
        biblioteca.configurarCargo(cfg.charge,cfg.moneda);
        if (!cfg.bootstrap.empty()) {
            if (!auth.empty()) throw std::runtime_error("Bootstrap solo admite almacén de cuentas vacío");
            if (!domain->ready()) throw std::runtime_error("Bootstrap requiere biblioteca validada");
            validarId(cfg.bootstrap);
            if (normalizeTitle(cfg.adminName).empty()) throw std::invalid_argument("--admin-name requerido");
            std::cerr << "Contraseña nueva (12–256 bytes): " << std::flush;
            termios previous{};
            const bool terminal = ::isatty(STDIN_FILENO) && ::tcgetattr(STDIN_FILENO,&previous)==0;
            if (terminal) { auto hidden=previous; hidden.c_lflag &= static_cast<tcflag_t>(~ECHO); if (::tcsetattr(STDIN_FILENO,TCSAFLUSH,&hidden)) throw std::runtime_error("No se pudo ocultar la contraseña"); }
            std::string password;
            std::getline(std::cin,password);
            if (terminal) ::tcsetattr(STDIN_FILENO,TCSAFLUSH,&previous);
            std::cerr << '\n';
            if (password.size()<12 || password.size()>256) throw std::invalid_argument("Contraseña de 12 a 256 bytes requerida");
            bool exists=false;
            try { if (!biblioteca.usuario(cfg.bootstrap).activo) throw std::runtime_error("Usuario de bootstrap inactivo"); exists=true; }
            catch (const ErrorDominio& e) { if (e.status!=404) throw; }
            if (!exists) biblioteca.crearUsuario({cfg.bootstrap,cfg.adminName,true});
            // Aprovisionamiento reanudable: si falla el almacén privado, el usuario no tiene acceso.
            auth.create(cfg.bootstrap,"ADMIN",password);
            std::fill(password.begin(),password.end(),'\0');
            std::cout << "Administrador creado. Inicie la API sin --bootstrap-admin.\n";
            return 0;
        }
        if (auth.empty()) throw std::runtime_error("Sin cuentas: ejecute primero --bootstrap-admin ID --admin-name Nombre");
        if (!server.bind_to_port("127.0.0.1", cfg.port)) throw std::runtime_error("Puerto local no disponible");
        registrarHttp(server, service, biblioteca, auth, cfg.origins, recover);
        std::cout << "API de catálogo en http://127.0.0.1:" << cfg.port << " · " << cfg.storage << " · " << (cfg.storage == "mongo" ? cfg.collection : cfg.working.string()) << std::endl;
        return server.listen_after_bind() ? 0 : 1;
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
