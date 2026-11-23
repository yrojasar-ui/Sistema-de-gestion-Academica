#include "biblioteca/LocalLibraryRepository.hpp"
#include "biblioteca/DomainMigration.hpp"
#include "biblioteca/LibraryService.hpp"
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
namespace biblioteca {
namespace {
Json read(const std::filesystem::path& p) {
    if (std::filesystem::is_symlink(p) || !std::filesystem::is_regular_file(p) || std::filesystem::file_size(p) > 1024*1024)
        throw std::runtime_error("Archivo de biblioteca inválido");
    std::ifstream f(p);
    return Json::parse(f, [](int depth, Json::parse_event_t, Json&) {
        if (depth > 32) throw std::runtime_error("Biblioteca demasiado anidada");
        return true;
    });
}
}
LocalLibraryRepository::LocalLibraryRepository(std::filesystem::path initial, std::filesystem::path working, bool migrate)
    : path_(std::filesystem::absolute(working).lexically_normal()) {
    if (path_ == std::filesystem::absolute(initial).lexically_normal() ||
        (std::filesystem::exists(path_) && std::filesystem::exists(initial) && std::filesystem::equivalent(path_, initial)))
        throw std::invalid_argument("El catálogo inicial no puede ser archivo de trabajo");
    bool changed = !std::filesystem::exists(path_);
    Json value = changed ? read(initial) : read(path_);
    if (value.is_array()) {
        if (!changed && !migrate) throw std::runtime_error("Catálogo anterior: respaldo y --migrate requeridos");
        value = {{"version",2},{"libros",value},{"dominio",EstadoBiblioteca{}}};
        changed = true;
    } else {
        campos(value, {"version","libros","dominio"});
        if (value.at("version") != 2) throw std::runtime_error("Versión local incompatible");
        if (value.at("dominio").at("version") == 1 && migrate) { value["dominio"] = migrarDominioV1(value.at("dominio")); changed = true; }
    }
    if (!value.at("libros").is_array() || value.at("libros").size() > 10000) throw std::runtime_error("Catálogo inválido");
    current_ = value;
    CatalogService catalog(*this); catalog.load(path_);
    MemoryDomainRepository temporary; temporary.replace(load());
    LibraryService library(catalog, temporary); library.validar();
    if (changed) commit(value, false);
}
std::vector<Libro> LocalLibraryRepository::load(const std::filesystem::path& source) const {
    if (std::filesystem::absolute(source).lexically_normal() != path_) throw std::invalid_argument("Origen local incorrecto");
    return current_.at("libros").get<std::vector<Libro>>();
}
EstadoBiblioteca LocalLibraryRepository::load() const { return current_.at("dominio").get<EstadoBiblioteca>(); }
void LocalLibraryRepository::save(const std::vector<Libro>& books) const {
    auto next = current_; next["libros"] = books; commit(std::move(next));
}
void LocalLibraryRepository::replace(const EstadoBiblioteca& domain) {
    auto next = current_; next["dominio"] = domain; commit(std::move(next));
}
void LocalLibraryRepository::commit(Json next, bool checkExternal) const {
    if (blocked_) throw PersistenceError(WriteOutcome::incierto, "Almacenamiento bloqueado; reinicie para reconciliar");
    const auto text = next.dump();
    if (text.size() > 1024*1024) throw PersistenceError(WriteOutcome::rechazado, "Biblioteca supera 1 MiB");
    if (checkExternal && std::filesystem::exists(path_)) {
        Json onDisk;
        try {
            onDisk = read(path_);
        } catch (const std::exception& e) {
            throw PersistenceError(WriteOutcome::rechazado,
                std::string("Archivo de biblioteca ilegible antes de guardar: ") + e.what());
        }
        if (onDisk != current_)
            throw PersistenceError(WriteOutcome::rechazado, "Archivo cambiado externamente; reinicie antes de escribir");
    }
    auto pattern = path_.string() + ".tmp.XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
    int fd = ::mkstemp(name.data());
    if (fd < 0) throw PersistenceError(WriteOutcome::rechazado, "No se pudo preparar el guardado");
    bool renamed = false;
    try {
        std::size_t done = 0;
        while (done < text.size()) {
            const auto n = ::write(fd, text.data()+done, text.size()-done);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) throw std::runtime_error("Guardado incompleto");
            done += static_cast<std::size_t>(n);
        }
        if (::fsync(fd)) throw std::runtime_error("No se pudo sincronizar archivo");
        const int closed = ::close(fd); fd = -1;
        if (closed) throw std::runtime_error("Error cerrando archivo");
        std::filesystem::rename(name.data(), path_); renamed = true;
        const int dir = ::open(path_.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dir < 0) throw std::runtime_error("No se pudo abrir directorio");
        const auto result = ::fsync(dir); ::close(dir);
        if (result) throw std::runtime_error("No se pudo sincronizar directorio");
        current_.swap(next);
    } catch (const std::exception& e) {
        if (fd >= 0) ::close(fd);
        std::error_code ignored; std::filesystem::remove(name.data(), ignored);
        blocked_ = renamed;
        throw PersistenceError(renamed ? WriteOutcome::incierto : WriteOutcome::rechazado, e.what());
    }
}
}
