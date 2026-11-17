#include "biblioteca/MongoLibraryRepository.hpp"
#include <fstream>
#include "biblioteca/DomainMigration.hpp"
#include "biblioteca/LibraryService.hpp"
#include <limits>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

namespace biblioteca {
namespace {
constexpr std::size_t maxSnapshot = 1024U * 1024U;
// fsync del archivo y directorio antes de enviar la solicitud remota.
void durableWrite(const std::filesystem::path& path, const std::string& text) {
    auto name = path.string() + ".tmp.XXXXXX";
    std::vector<char> buffer(name.begin(), name.end()); buffer.push_back('\0');
    int fd = ::mkstemp(buffer.data());
    if (fd < 0) throw std::runtime_error("No se pudo crear el diario pendiente");
    const std::filesystem::path temp(buffer.data());
    try {
        std::size_t offset = 0;
        while (offset < text.size()) {
            auto count = ::write(fd, text.data() + offset, text.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) throw std::runtime_error("Error escribiendo el diario");
            offset += static_cast<std::size_t>(count);
        }
        if (::fsync(fd) != 0) throw std::runtime_error("Error sincronizando el diario");
        const int closed = ::close(fd); fd = -1;
        if (closed != 0) throw std::runtime_error("Error cerrando el diario");
        std::filesystem::rename(temp, path);
        const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path(".");
        const int dir = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dir < 0) throw std::runtime_error("No se pudo abrir el directorio del diario");
        const auto synced = ::fsync(dir); ::close(dir);
        if (synced != 0) throw std::runtime_error("Error sincronizando el directorio del diario");
    } catch (...) {
        if (fd >= 0) ::close(fd);
        std::error_code ignored; std::filesystem::remove(temp, ignored);
        throw;
    }
}
bool matches(Json observed, const Json& expected) {
    if (observed.is_null()) return false;
    // Mongo asigna ObjectId únicamente al crear la biblioteca.
    if (!expected.contains("_id")) observed.erase("_id");
    return observed == expected;
}
}
MongoLibraryRepository::MongoLibraryRepository(std::string endpoint, std::string collection,
        std::filesystem::path journal, HttpTransport transport)
    : endpoint_(std::move(endpoint)), collection_(std::move(collection)),
      journal_(std::move(journal)), request_(std::move(transport)) {
    mongo::validateConfig(endpoint_, collection_);
    if (journal_.empty()) throw std::invalid_argument("Se requiere un diario Mongo estable");
}
void MongoLibraryRepository::validateDocument(const Json& doc) const {
    campos(doc, {"_id","proyecto","idBiblioteca","versionEsquema","estado"});
    if (doc.at("proyecto") != collection_ || doc.at("idBiblioteca") != "principal" || doc.at("versionEsquema") != 1 ||
        !mongo::validOid(doc.at("_id").at("$oid").get<std::string>()))
        throw std::runtime_error("Ámbito, identidad o versión Mongo incompatible");
    campos(doc.at("_id"), {"$oid"});
    const auto& state = doc.at("estado");
    campos(state, {"revision","libros","dominio"});
    if (!state.at("revision").is_number_unsigned() || state.at("revision").get<std::uint64_t>() == 0 ||
        !state.at("libros").is_array() || state.at("libros").size() > 10000 || doc.dump().size() > maxSnapshot)
        throw std::runtime_error("Revisión o tamaño de instantánea incompatible");
    for (const auto& book : state.at("libros")) validateBook(book.get<Libro>());
    if (state.at("dominio").at("version") == 1 && migration_)
        (void)migrarDominioV1(state.at("dominio")).get<EstadoBiblioteca>();
    else (void)state.at("dominio").get<EstadoBiblioteca>(); // Referencias e índices se validan en los servicios.
}
Json MongoLibraryRepository::read() const {
    const auto response = mongo::responseJson(request_("GET", endpoint_ + "?coleccion=" + collection_, ""));
    const auto& docs = response.at("documentos");
    if (response.at("coleccion") != collection_ || !docs.is_array() || !response.at("total").is_number_unsigned() ||
        response.at("total") != docs.size() || docs.size() > 1)
        throw std::runtime_error("Lectura Mongo incompleta o biblioteca duplicada");
    if (docs.empty()) return nullptr;
    validateDocument(docs.front());
    return docs.front();
}
Json MongoLibraryRepository::intent() const {
    if (!std::filesystem::exists(journal_)) return nullptr;
    if (std::filesystem::is_symlink(journal_) || !std::filesystem::is_regular_file(journal_) ||
        std::filesystem::file_size(journal_) > 3 * maxSnapshot)
        throw std::runtime_error("Diario pendiente inválido");
    std::ifstream file(journal_);
    auto value = Json::parse(file, [](int depth, Json::parse_event_t, Json&) {
        if (depth > 32) throw std::runtime_error("Diario demasiado anidado");
        return true;
    });
    campos(value, {"endpoint","coleccion","anterior","esperado"});
    if (value.at("endpoint") != endpoint_ || value.at("coleccion") != collection_)
        throw std::runtime_error("Diario pertenece a otro ámbito Mongo");
    return value;
}
void MongoLibraryRepository::clearIntent() const {
    std::filesystem::remove(journal_);
    // Si una caída recupera un diario antiguo, la reconciliación es solo GET e idempotente.
}
void MongoLibraryRepository::fail(WriteOutcome result, const std::string& message) const {
    outcome_ = result; block(message);
    throw PersistenceError(result, message);
}
void MongoLibraryRepository::refresh(bool initialize) {
    ready_ = false;
    try {
        // Incluso un diario ilegible puede corresponder a una solicitud ya enviada.
        if (std::filesystem::exists(journal_)) outcome_ = WriteOutcome::incierto;
        const auto pending = intent();
        const auto observed = read();
        if (!pending.is_null()) {
            if (!matches(observed, pending.at("esperado")))
                fail(WriteOutcome::incierto, "Escritura pendiente sin confirmación; no se reenvía. Recupere por GET.");
            clearIntent();
            outcome_ = WriteOutcome::confirmado;
        } else if (observed.is_null()) {
            if (!initialize) throw std::runtime_error("Biblioteca ausente: inicialización explícita requerida");
            current_ = nullptr;
            write({{"proyecto",collection_},{"idBiblioteca","principal"},{"versionEsquema",1},
                   {"estado",{{"revision",std::uint64_t{1}},{"libros",Json::array()},{"dominio",EstadoBiblioteca{}}}}});
            detail_ = "cargado; falta validar índices";
            return;
        }
        current_ = observed;
        detail_ = "cargado; falta validar índices";
    } catch (const std::exception& e) {
        block(e.what()); throw;
    }
}
std::vector<Libro> MongoLibraryRepository::load(const std::filesystem::path& source) const {
    if (source != workingPath()) throw std::invalid_argument("Origen distinto de la biblioteca Mongo");
    return current_.at("estado").at("libros").get<std::vector<Libro>>();
}
EstadoBiblioteca MongoLibraryRepository::load() const {
    return current_.at("estado").at("dominio").get<EstadoBiblioteca>();
}
void MongoLibraryRepository::write(const Json& next) const {
    const bool creation = current_.is_null();
    // El diario se guarda ANTES de la red. Un reinicio nunca reenvía este POST/PUT.
    try {
        durableWrite(journal_, Json{{"endpoint",endpoint_},{"coleccion",collection_},
                                   {"anterior",current_},{"esperado",next}}.dump());
    } catch (const std::exception& e) { fail(WriteOutcome::rechazado, e.what()); }
    Json payload{{"coleccion",collection_}};
    if (creation) payload["datos"] = next;
    else {
        payload["filtro"] = {{"_id",current_.at("_id").at("$oid")},{"proyecto",collection_},
                             {"idBiblioteca","principal"},{"estado.revision",current_.at("estado").at("revision")}};
        payload["datos"] = {{"estado",next.at("estado")}};
    }
    bool rejected = false;
    try {
        auto ack = mongo::responseJson(request_(creation ? "POST" : "PUT", endpoint_, payload.dump()));
        if (!creation && ack.contains("modificados") && ack.at("modificados").is_number_unsigned())
            rejected = ack.at("modificados") == 0;
    } catch (const mongo::ApiRejection&) { rejected = true; }
      catch (const std::exception&) { /* Timeout/HTML: solo la relectura puede confirmar. */ }
    Json observed;
    try { observed = read(); }
    catch (const std::exception& e) { fail(WriteOutcome::incierto, std::string("No se confirmó la escritura: ") + e.what()); }
    if (matches(observed, next)) {
        try { clearIntent(); } catch (const std::exception& e) { fail(WriteOutcome::incierto, e.what()); }
        current_ = observed; outcome_ = WriteOutcome::confirmado; detail_ = "validado";
        return;
    }
    if (rejected && observed == current_) {
        try { clearIntent(); } catch (const std::exception& e) { fail(WriteOutcome::incierto, e.what()); }
        outcome_ = WriteOutcome::rechazado;
        throw PersistenceError(outcome_, "Escritura rechazada; relectura conserva el estado anterior");
    }
    fail(WriteOutcome::incierto, "Escritura no confirmada; mutaciones bloqueadas. Recuperación explícita requerida.");
}
void MongoLibraryRepository::replaceState(Json state) const {
    if (!ready_) throw PersistenceError(outcome_, "Biblioteca bloqueada: " + detail_);
    try {
        if (!intent().is_null()) fail(WriteOutcome::incierto, "Hay una escritura pendiente; recupere primero");
        if (read() != current_) fail(WriteOutcome::rechazado, "Estado remoto cambió; recargue antes de escribir");
    } catch (const PersistenceError&) { throw; }
      catch (const std::exception& e) { fail(WriteOutcome::rechazado, std::string("No se envió la escritura: ") + e.what()); }
    const auto revision = current_.at("estado").at("revision").get<std::uint64_t>();
    if (revision == std::numeric_limits<std::uint64_t>::max()) {
        outcome_ = WriteOutcome::rechazado;
        throw PersistenceError(outcome_, "Revisión agotada");
    }
    state["revision"] = revision + 1;
    auto next = current_; next["estado"] = std::move(state);
    try { validateDocument(next); }
    catch (const std::exception& e) {
        outcome_ = WriteOutcome::rechazado;
        throw PersistenceError(outcome_, std::string("No se envió la instantánea inválida: ") + e.what());
    }
    write(next);
}
void MongoLibraryRepository::save(const std::vector<Libro>& books) const {
    auto state = current_.at("estado"); state["libros"] = books; replaceState(std::move(state));
}
void MongoLibraryRepository::migrateDomain() {
    if (!migration_) throw std::runtime_error("Migración no autorizada");
    if (current_.at("estado").at("dominio").at("version") == 2) return;
    auto migrated = migrarDominioV1(current_.at("estado").at("dominio"));
    CatalogService catalog(*this); catalog.load(workingPath());
    MemoryDomainRepository domain; domain.replace(migrated.get<EstadoBiblioteca>());
    LibraryService candidate(catalog, domain); candidate.validar();
    auto state = current_.at("estado"); state["dominio"] = migrated;
    ready_ = true;
    try { replaceState(std::move(state)); ready_ = false; }
    catch (...) { ready_ = false; throw; }
}
void MongoLibraryRepository::replace(const EstadoBiblioteca& domain) {
    auto state = current_.at("estado"); state["dominio"] = domain; replaceState(std::move(state));
}
} // namespace biblioteca
