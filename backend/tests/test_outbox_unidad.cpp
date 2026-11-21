#include "biblioteca/AmqpBroker.hpp"
#include "biblioteca/LocalLibraryRepository.hpp"
#include "biblioteca/ServiceIdentity.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>
#include <thread>
#include <vector>

using namespace biblioteca;
namespace fs = std::filesystem;

namespace {
void check(bool ok, const std::string& msg) {
    if (!ok) throw std::runtime_error("FALLO: " + msg);
}

class MockPublisher final : public IEventPublisher {
public:
    std::atomic<bool> fail{true};
    std::mutex mtx;
    std::vector<std::string> published;
    void publish(const Aviso& a) override {
        if (fail.load()) throw std::runtime_error("Broker caído simulado amqp://user:secreto123@127.0.0.1:5672");
        std::lock_guard<std::mutex> lk(mtx);
        published.push_back(a.idAviso);
    }
};
} // namespace

int main(int argc, char** argv) {
    // Modo opcional: --test-basic-return <host> <port> <user> <pass>
    // Verifica con RabbitMQ REAL que un mensaje mandatory sin ruta produce basic.return y error explícito.
    if (argc == 6 && std::string(argv[1]) == "--test-basic-return") {
        AmqpConfig cfg;
        cfg.host = argv[2];
        cfg.port = std::stoi(argv[3]);
        cfg.user = argv[4];
        cfg.password = argv[5];
        cfg.declareQueues = false; // Solo exchange, sin cola enlazada para la clave inexistente.
        RabbitEventPublisher pub(cfg);
        bool caughtReturn = false;
        std::string errMsg;
        try {
            pub.publishRaw("clave.sin.ruta.inexistente", R"({"prueba":"sin-ruta"})", "EV-SIN-RUTA", "AV-SIN-RUTA", 0);
        } catch (const std::exception& e) {
            errMsg = e.what();
            caughtReturn = errMsg.find("basic.return") != std::string::npos;
        }
        pub.close();
        if (!caughtReturn) {
            std::cerr << "No se detectó basic.return: " << errMsg << '\n';
            return 1;
        }
        std::cout << "OK basic.return detectado en broker real: " << errMsg << '\n';
        return 0;
    }
    if (argc == 8 && std::string(argv[1]) == "--publish-raw") {
        AmqpConfig cfg;
        cfg.host = argv[2];
        cfg.port = std::stoi(argv[3]);
        cfg.user = argv[4];
        cfg.password = argv[5];
        RabbitEventPublisher pub(cfg);
        pub.publishRaw(argv[6], argv[7], "EV-RAW", "AV-RAW", 0);
        pub.close();
        return 0;
    }

    auto pattern = (fs::temp_directory_path() / "biblioteca-outbox-XXXXXX").string();
    std::vector<char> buf(pattern.begin(), pattern.end());
    buf.push_back('\0');
    const char* dir = ::mkdtemp(buf.data());
    check(dir != nullptr, "mkdtemp");
    const fs::path root(dir);

    try {
        // 1. Sanitización de errores con credenciales
        const auto sanitized = sanitizeErrorText("Fallo en amqp://admin:SuperSecreto99@broker.local:5672/vhost\nlinea2");
        check(sanitized.find("SuperSecreto99") == std::string::npos, "No debe exponer contraseñas");
        check(sanitized.find("***@broker.local") != std::string::npos, "Debe redactar credenciales en URI");

        // 2. Identidad de servicio independiente (0700/0600)
        const auto tokenPath = root / "priv" / "service.token";
        ServiceIdentity::generateTokenFile(tokenPath);
        struct stat st{};
        check(::lstat(tokenPath.c_str(), &st) == 0 && (st.st_mode & 0777) == 0600, "Token 0600");
        check(::lstat(tokenPath.parent_path().c_str(), &st) == 0 && (st.st_mode & 0777) == 0700, "Dir 0700");
        const auto rawToken = ServiceIdentity::readSecretFile(tokenPath);
        ServiceIdentity sid(tokenPath);
        check(sid.configured(), "ServiceIdentity configurada");
        check(sid.verifyAuthorization("Bearer " + rawToken), "Verifica token válido");
        check(!sid.verifyAuthorization("Bearer " + std::string(64, 'a')), "Rechaza token incorrecto");
        check(!sid.verifyAuthorization(rawToken), "Exige prefijo Bearer");

        // Permisos inseguros deben ser rechazados
        ::chmod(tokenPath.c_str(), 0644);
        bool rejectedInsecure = false;
        try { ServiceIdentity bad(tokenPath); } catch (const std::exception&) { rejectedInsecure = true; }
        check(rejectedInsecure, "Rechaza token con permisos inseguros");
        ::chmod(tokenPath.c_str(), 0600);

        // 3. Ciclo de vida de Aviso + deduplicación persistente + carrera worker/outbox
        const auto initial = root / "inicial.json";
        {
            std::ofstream out(initial);
            out << R"([{"idLibro":"L1","titulo":"Estructuras","autor":"Autor","temas":["datos"]}])";
        }
        LocalLibraryRepository repo(initial, root / "trabajo.json");
        CatalogService catalog(repo);
        catalog.load(repo.workingPath());
        LibraryService lib(catalog, repo);
        lib.crearUsuario({"Ana", "Ana Sintética", true});
        lib.crearEjemplar("E1", "L1");
        lib.reservar("R1", "L1", "Ana"); // Genera AV-1 reserva_apartada
        check(lib.estado().avisos.size() == 1 && lib.estado().avisos[0].estado == "pendiente", "AV-1 pendiente");
        check(lib.estado().avisos[0].idEvento == "EV-AV-1", "idEvento estable");
        check(lib.estado().notificaciones.empty(), "Sin notificación antes del worker");

        // 4. Outbox relay con broker caído y posterior recuperación
        std::mutex mtx;
        MockPublisher mockPub;
        AvisoOutboxRelay relay(mtx, lib, mockPub, 100);
        relay.start();
        relay.notify();
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        {
            std::lock_guard<std::mutex> lk(mtx);
            check(lib.estado().avisos[0].estado == "pendiente", "Broker caído conserva aviso pendiente");
            check(!relay.status().conectado, "Estado de broker refleja desconexión");
            check(relay.status().ultimoError.find("secreto123") == std::string::npos, "Error de broker sanitizado");
        }
        mockPub.fail = false;
        relay.notify();
        for (int i = 0; i < 30; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            std::lock_guard<std::mutex> lk(mtx);
            if (lib.estado().avisos[0].estado == "publicado") break;
        }
        relay.stop();
        check(lib.estado().avisos[0].estado == "publicado" && !lib.estado().avisos[0].publicadoEn.empty(),
              "Outbox recupera y marca publicado");
        check(relay.status().conectado, "Broker marcado conectado tras confirmación");
        check(lib.estado().notificaciones.empty(), "Publisher confirm no crea notificación visible");

        // 5. Procesamiento idempotente + deduplicación persistente
        const auto r1 = lib.procesarAviso("AV-1", "EV-AV-1", "Reserva disponible", "Tu ejemplar E1 está listo", 1);
        check(!r1.duplicado && r1.notificacion.idNotificacion == "NT-AV-1", "Primera entrega procesada");
        check(lib.estado().notificaciones.size() == 1 && lib.estado().avisos[0].estado == "procesado", "1 notificación");
        const auto r2 = lib.procesarAviso("AV-1", "EV-AV-1", "Reserva disponible", "Tu ejemplar E1 está listo", 2);
        check(r2.duplicado && r2.notificacion.idNotificacion == "NT-AV-1", "Redelivery detectado como duplicado");
        check(lib.estado().notificaciones.size() == 1, "No duplica notificación visible");

        // Carrera: marcarAvisoPublicado sobre aviso ya procesado NO debe retrocederlo
        lib.marcarAvisoPublicado("AV-1");
        check(lib.estado().avisos[0].estado == "procesado", "No retrocede procesado a publicado");

        // 6. Fallo definitivo y reintento administrativo explícito
        lib.prestar("P1", "E1", "Ana", "2099-12-01"); // Genera AV-2 prestamo_registrado
        check(lib.estado().avisos.size() == 2 && lib.estado().avisos[1].idAviso == "AV-2", "AV-2 creado");
        lib.fallarAviso("AV-2", "EV-AV-2", 3, "Reintentos agotados: timeout escritor", true);
        check(lib.estado().avisos[1].estado == "fallido" && lib.estado().avisos[1].intentos == 3, "AV-2 fallido");
        lib.reintentarAviso("AV-2");
        check(lib.estado().avisos[1].estado == "pendiente" && lib.estado().avisos[1].ultimoError.empty(),
              "Reintento admin devuelve a pendiente");
        lib.validar();

        std::cout << "OK outbox_unidad: ServiceIdentity, sanitización, outbox, idempotencia, carrera y reintento.\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        fs::remove_all(root);
        return 1;
    }
    fs::remove_all(root);
    return 0;
}
