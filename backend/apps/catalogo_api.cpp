#include "biblioteca/AmqpBroker.hpp"
#include "biblioteca/HttpController.hpp"
#include "biblioteca/JsonBookRepository.hpp"
#include "biblioteca/LocalLibraryRepository.hpp"
#include "biblioteca/ServiceIdentity.hpp"
#include <httplib.h>
#ifdef BIBLIOTECA_MONGO
#include "biblioteca/MongoLibraryRepository.hpp"
#endif
#include <atomic>
#include <charconv>
#include <csignal>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <limits>
#include <mutex>
#include <sys/file.h>
#include <termios.h>
#include <unistd.h>

using namespace biblioteca;
namespace fs = std::filesystem;

namespace {
std::atomic<httplib::Server*> g_server{nullptr};
void onShutdownSignal(int) {
    if (auto* s = g_server.load()) s->stop();
}

std::string envOr(const char* name, const std::string& fallback = "") {
    const char* v = std::getenv(name);
    return v ? std::string(v) : fallback;
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
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string storage = "local", endpoint, collection;
    fs::path mongoState, authStore = "data/privado/cuentas.json";
    fs::path serviceTokenFile, bootstrapServiceToken;
    fs::path amqpCredentialsFile;
    AmqpConfig amqp;
    int outboxPollMs = 1000;
    std::string bootstrap, adminName, moneda = "CRC";
    int sessionSeconds = 1800;
    std::int64_t charge = 10000;
    bool initialize = false, migrate = false;
    std::vector<std::string> origins{"http://localhost:5173", "http://127.0.0.1:5173"};
    std::vector<std::string> trustedProxies;
};

Settings settings(int argc, char** argv) {
    Settings cfg;
    cfg.amqp.host = envOr("AMQP_HOST", "");
    if (auto p = envOr("AMQP_PORT"); !p.empty()) {
        int parsedPort = 0;
        const auto end = p.data() + p.size();
        const auto parsed = std::from_chars(p.data(), end, parsedPort);
        if (parsed.ec != std::errc{} || parsed.ptr != end || parsedPort < 1 || parsedPort > 65535)
            throw std::invalid_argument("Variable de entorno AMQP_PORT inválida (1..65535)");
        cfg.amqp.port = parsedPort;
    }
    cfg.amqp.vhost = envOr("AMQP_VHOST", "/");
    cfg.amqp.user = envOr("AMQP_USER", "guest");
    cfg.amqp.password = envOr("AMQP_PASSWORD", "guest");
    if (auto f = envOr("SERVICE_TOKEN_FILE"); !f.empty()) cfg.serviceTokenFile = f;
    if (auto f = envOr("AMQP_CREDENTIALS_FILE"); !f.empty()) cfg.amqpCredentialsFile = f;

    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        if (flag == "--migrate") { cfg.migrate = true; continue; }
        if (flag == "--initialize") { cfg.initialize = true; continue; }
        if (++i == argc) throw std::invalid_argument("Falta valor para " + flag);
        std::string value = argv[i];
        if (flag == "--auth-store") cfg.authStore = value;
        else if (flag == "--service-token-file") cfg.serviceTokenFile = value;
        else if (flag == "--bootstrap-service-token") cfg.bootstrapServiceToken = value;
        else if (flag == "--amqp-host") cfg.amqp.host = value;
        else if (flag == "--amqp-vhost") cfg.amqp.vhost = value;
        else if (flag == "--amqp-credentials-file") cfg.amqpCredentialsFile = value;
        else if (flag == "--bootstrap-admin") cfg.bootstrap = value;
        else if (flag == "--admin-name") cfg.adminName = value;
        else if (flag == "--currency") cfg.moneda = value;
        else if (flag == "--host") cfg.host = value;
        else if (flag == "--trusted-proxy") cfg.trustedProxies.push_back(value);
        else if (flag == "--demo-charge" || flag == "--session-seconds" || flag == "--outbox-poll-ms") {
            std::int64_t n = 0;
            auto r = std::from_chars(value.data(), value.data() + value.size(), n);
            if (r.ec != std::errc{} || r.ptr != value.data() + value.size() || n < 0 || n > 100000000)
                throw std::invalid_argument("Importe o duración inválida");
            if (flag == "--demo-charge") cfg.charge = n;
            else if (flag == "--session-seconds") cfg.sessionSeconds = static_cast<int>(n);
            else cfg.outboxPollMs = std::max(100, static_cast<int>(n));
        }
        else if (flag == "--storage") cfg.storage = value;
        else if (flag == "--endpoint") cfg.endpoint = value;
        else if (flag == "--collection") cfg.collection = value;
        else if (flag == "--mongo-state") cfg.mongoState = value;
        else if (flag == "--initial") cfg.initial = value;
        else if (flag == "--working") cfg.working = value;
        else if (flag == "--origin") cfg.origins.push_back(value);
        else if (flag == "--port" || flag == "--amqp-port") {
            int parsedPort = 0;
            const auto end = value.data() + value.size();
            auto parsed = std::from_chars(value.data(), end, parsedPort);
            if (parsed.ec != std::errc{} || parsed.ptr != end || parsedPort < 1 || parsedPort > 65535)
                throw std::invalid_argument("Puerto inválido");
            if (flag == "--port") cfg.port = parsedPort; else cfg.amqp.port = parsedPort;
        } else throw std::invalid_argument("Opción desconocida: " + flag);
    }
    if (!cfg.amqpCredentialsFile.empty()) loadAmqpCredentialsFile(cfg.amqpCredentialsFile, cfg.amqp);
    if (cfg.storage != "local" && cfg.storage != "mongo") throw std::invalid_argument("Use --storage local o mongo");
    if (cfg.storage == "mongo" && (cfg.endpoint.empty() || cfg.collection.empty() || cfg.mongoState.empty()))
        throw std::invalid_argument("Mongo requiere --endpoint, --collection y --mongo-state");
    if (cfg.storage == "local" && (cfg.initialize || !cfg.endpoint.empty() || !cfg.collection.empty() || !cfg.mongoState.empty()))
        throw std::invalid_argument("Opciones Mongo requieren --storage mongo");
    return cfg;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto cfg = settings(argc, argv);
        if (!cfg.bootstrapServiceToken.empty()) {
            ServiceIdentity::generateTokenFile(cfg.bootstrapServiceToken);
            std::cout << "Token de servicio generado en archivo privado 0600 (no mostrado).\n";
            return 0;
        }
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
        AuthService auth(cfg.authStore, cfg.sessionSeconds);
        ServiceIdentity serviceIdentity(cfg.serviceTokenFile);
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
            catch (const std::exception& e) { std::cerr << "Mongo no listo: " << sanitizeErrorText(e.what()) << '\n'; }
        }
#endif
        biblioteca.configurarCargo(cfg.charge, cfg.moneda);
        if (!cfg.bootstrap.empty()) {
            if (!auth.empty()) throw std::runtime_error("Bootstrap solo admite almacén de cuentas vacío");
            if (!domain->ready()) throw std::runtime_error("Bootstrap requiere biblioteca validada");
            validarId(cfg.bootstrap);
            if (normalizeTitle(cfg.adminName).empty()) throw std::invalid_argument("--admin-name requerido");
            std::cerr << "Contraseña nueva (12–256 bytes): " << std::flush;
            termios previous{};
            const bool terminal = ::isatty(STDIN_FILENO) && ::tcgetattr(STDIN_FILENO, &previous) == 0;
            if (terminal) {
                auto hidden = previous;
                hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
                if (::tcsetattr(STDIN_FILENO, TCSAFLUSH, &hidden))
                    throw std::runtime_error("No se pudo ocultar la contraseña");
            }
            std::string password;
            std::getline(std::cin, password);
            if (terminal) ::tcsetattr(STDIN_FILENO, TCSAFLUSH, &previous);
            std::cerr << '\n';
            if (password.size() < 12 || password.size() > 256)
                throw std::invalid_argument("Contraseña de 12 a 256 bytes requerida");
            bool exists = false;
            try { if (!biblioteca.usuario(cfg.bootstrap).activo) throw std::runtime_error("Usuario de bootstrap inactivo"); exists = true; }
            catch (const ErrorDominio& e) { if (e.status != 404) throw; }
            if (!exists) biblioteca.crearUsuario({cfg.bootstrap, cfg.adminName, true});
            auth.create(cfg.bootstrap, "ADMIN", password);
            std::fill(password.begin(), password.end(), '\0');
            std::cout << "Administrador creado. Inicie la API sin --bootstrap-admin.\n";
            return 0;
        }
        if (auth.empty()) throw std::runtime_error("Sin cuentas: ejecute primero --bootstrap-admin ID --admin-name Nombre");
        if (!server.bind_to_port(cfg.host, cfg.port)) throw std::runtime_error("Puerto local no disponible");

        std::mutex stateMutex;
        std::unique_ptr<RabbitEventPublisher> publisher;
        std::unique_ptr<AvisoOutboxRelay> relay;
        if (cfg.amqp.enabled()) {
            publisher = std::make_unique<RabbitEventPublisher>(cfg.amqp);
            relay = std::make_unique<AvisoOutboxRelay>(stateMutex, biblioteca, *publisher, cfg.outboxPollMs);
            biblioteca.setOnAvisoCreado([r = relay.get()] { r->notify(); });
        }

        HttpOptions httpOpts;
        httpOpts.origenes = cfg.origins;
        httpOpts.recover = recover;
        httpOpts.stateMutex = &stateMutex;
        httpOpts.serviceIdentity = &serviceIdentity;
        httpOpts.trustedProxies = cfg.trustedProxies;
        if (relay) {
            httpOpts.brokerStatus = [r = relay.get()] { return r->status(); };
            httpOpts.wakeOutbox = [r = relay.get()] { r->notify(); };
        }
        registrarHttp(server, service, biblioteca, auth, httpOpts);

        g_server = &server;
        std::signal(SIGINT, onShutdownSignal);
        std::signal(SIGTERM, onShutdownSignal);
        if (relay) relay->start();

        std::cout << "API de catálogo en http://" << cfg.host << ":" << cfg.port << " · " << cfg.storage << " · "
                  << (cfg.storage == "mongo" ? cfg.collection : cfg.working.string())
                  << (cfg.amqp.enabled() ? (" · RabbitMQ " + cfg.amqp.host + ":" + std::to_string(cfg.amqp.port)) : " · sin broker AMQP")
                  << std::endl;
        const bool ok = server.listen_after_bind();
        g_server = nullptr;
        if (relay) relay->stop();
        if (publisher) publisher->close();
        return ok ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << sanitizeErrorText(e.what()) << '\n';
        return 1;
    }
}
