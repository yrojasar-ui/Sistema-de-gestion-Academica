#include "biblioteca/JsonBookRepository.hpp"
#include <nlohmann/json.hpp>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>
#include <unistd.h>

namespace biblioteca {
namespace fs = std::filesystem;
using Json = nlohmann::json;
namespace {
constexpr std::size_t maxBytes = 16 * 1024 * 1024;
constexpr std::size_t maxBooks = 10000;
void validateBooks(const std::vector<Libro>& books) {
    if (books.size() > maxBooks) throw std::invalid_argument("Máximo 10000 libros por archivo");
    std::set<std::string> ids;
    for (const auto& book : books) {
        validateBook(book);
        if (!ids.insert(book.idLibro).second) throw std::invalid_argument("ID duplicado: " + book.idLibro);
    }
}
// El temporal se crea de forma exclusiva junto al destino; su destructor limpia
// únicamente ese archivo propio si la escritura/cierre/rename falla.
struct TemporaryFile {
    int fd = -1;
    std::string path;
    explicit TemporaryFile(const fs::path& target) {
        std::string pattern = target.string() + ".tmp.XXXXXX";
        std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
        fd = ::mkstemp(name.data());
        if (fd == -1) throw std::runtime_error("No se puede crear temporal: " + std::string(std::strerror(errno)));
        path = name.data();
    }
    ~TemporaryFile() {
        if (fd != -1) ::close(fd);
        if (!path.empty()) ::unlink(path.c_str());
    }
    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
};
}
JsonBookRepository::JsonBookRepository(fs::path initial, fs::path working)
    : initial_(fs::absolute(std::move(initial))), working_(fs::absolute(std::move(working))) { checkWorkingPath(); }
void JsonBookRepository::checkWorkingPath() const {
    if (fs::weakly_canonical(initial_) == fs::weakly_canonical(working_) ||
        (fs::exists(initial_) && fs::exists(working_) && fs::equivalent(initial_, working_)))
        throw std::invalid_argument("El archivo de trabajo debe ser distinto al fixture inicial");
    if (fs::is_symlink(fs::symlink_status(working_))) throw std::invalid_argument("Archivo de trabajo no puede ser enlace simbólico");
}
std::vector<Libro> JsonBookRepository::load(const fs::path& source) const {
    try {
        if (!fs::is_regular_file(source)) throw std::runtime_error("No es un archivo regular accesible");
        if (fs::file_size(source) > maxBytes) throw std::runtime_error("Archivo supera 16 MiB");
        std::ifstream input(source, std::ios::binary);
        if (!input) throw std::runtime_error("No se puede abrir archivo");
        // Lectura acotada incluso si otro proceso aumenta el archivo después de file_size.
        std::string contents;
        char buffer[4096];
        while (input.read(buffer, sizeof(buffer)) || input.gcount()) {
            contents.append(buffer, static_cast<std::size_t>(input.gcount()));
            if (contents.size() > maxBytes) throw std::runtime_error("Archivo supera 16 MiB");
        }
        if (input.bad()) throw std::runtime_error("Error al leer archivo");
        // Rechaza nombres repetidos en objetos: el parser normal conservaría el último.
        std::vector<std::set<std::string>> objectKeys;
        auto callback = [&](int depth, Json::parse_event_t event, Json& parsed) {
            if (depth > 32) throw std::runtime_error("JSON demasiado anidado");
            if (event == Json::parse_event_t::object_start) objectKeys.emplace_back();
            if (event == Json::parse_event_t::key && !objectKeys.back().insert(parsed.get<std::string>()).second)
                throw std::runtime_error("Campo JSON repetido: " + parsed.get<std::string>());
            if (event == Json::parse_event_t::object_end) objectKeys.pop_back();
            return true;
        };
        const auto json = Json::parse(contents, callback);
        if (!json.is_array() || json.size() > maxBooks) throw std::runtime_error("Se esperaba un array de hasta 10000 libros");
        std::vector<Libro> books;
        for (std::size_t i = 0; i < json.size(); ++i) {
            const auto& item = json[i];
            try {
                if (!item.is_object() || item.size() != 4 || !item.contains("idLibro") || !item.contains("titulo") || !item.contains("autor") || !item.contains("temas"))
                    throw std::invalid_argument("Campos exactos requeridos: idLibro, titulo, autor, temas");
                if (!item.at("idLibro").is_string() || !item.at("titulo").is_string() || !item.at("autor").is_string() || !item.at("temas").is_array())
                    throw std::invalid_argument("Tipos de campos incorrectos");
                books.push_back({item.at("idLibro").get<std::string>(), item.at("titulo").get<std::string>(), item.at("autor").get<std::string>(), item.at("temas").get<std::vector<std::string>>()});
                validateBook(books.back());
            } catch (const std::exception& e) { throw std::runtime_error("Registro " + std::to_string(i + 1) + ": " + e.what()); }
        }
        validateBooks(books);
        return books;
    } catch (const std::exception& e) { throw std::runtime_error("Carga rechazada [" + source.string() + "]: " + e.what()); }
}
void JsonBookRepository::save(const std::vector<Libro>& books) const {
    checkWorkingPath(); validateBooks(books);
    Json json = Json::array();
    for (const auto& b : books) json.push_back({{"idLibro", b.idLibro}, {"titulo", b.titulo}, {"autor", b.autor}, {"temas", b.temas}});
    const auto contents = json.dump(2) + '\n';
    if (contents.size() > maxBytes) throw std::runtime_error("Snapshot supera 16 MiB; cambios siguen en memoria");
    TemporaryFile temp(working_);
    std::size_t written = 0;
    while (written < contents.size()) {
        const auto count = ::write(temp.fd, contents.data() + written, contents.size() - written);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) throw std::runtime_error("Error escribiendo archivo temporal");
        written += static_cast<std::size_t>(count);
    }
    const int fd = temp.fd; temp.fd = -1;
    if (::close(fd) != 0) throw std::runtime_error("Error cerrando archivo temporal");
    checkWorkingPath();
    // rename en el mismo directorio/sistema de archivos (Linux). No fsync:
    // no se promete durabilidad ante cortes eléctricos ni coordinación multiusuario.
    fs::rename(temp.path, working_); temp.path.clear();
}
} // namespace biblioteca
