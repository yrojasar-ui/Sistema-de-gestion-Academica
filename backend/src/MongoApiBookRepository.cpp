#include "biblioteca/MongoApiBookRepository.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace biblioteca {
namespace {
using Json = nlohmann::json;
struct ApiRejection : std::runtime_error { using std::runtime_error::runtime_error; };
Json responseJson(const HttpResponse& response) {
    if (response.status >= 400 && response.status < 500)
        throw ApiRejection("HTTP " + std::to_string(response.status));
    if (response.status < 200 || response.status >= 300)
        throw std::runtime_error("HTTP " + std::to_string(response.status));
    auto type = response.contentType.substr(0, response.contentType.find(';'));
    type.erase(type.find_last_not_of(" \t") + 1);
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (type != "application/json") throw std::runtime_error("Content-Type inesperado: " + type);
    auto json = Json::parse(response.body, [](int depth, Json::parse_event_t, Json&) {
        if (depth > 32) throw std::runtime_error("Respuesta JSON demasiado anidada");
        return true;
    });
    if (!json.is_object()) throw std::runtime_error("Se esperaba un objeto JSON");
    if (json.contains("error")) throw ApiRejection("La API devolvió un error de aplicación");
    return json;
}
Json fields(const Libro& b) { return {{"titulo", b.titulo}, {"autor", b.autor}, {"temas", b.temas}}; }
Json exactFilter(const std::string& collection, const std::string& id, const std::string& oid) {
    return {{"_id", oid}, {"idLibro", id}, {"proyecto", collection}};
}
bool validOid(const std::string& oid) {
    return oid.size() == 24 && oid.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
}
}

MongoApiBookRepository::MongoApiBookRepository(std::string endpoint, std::string collection, HttpTransport transport)
    : endpoint_(std::move(endpoint)), collection_(std::move(collection)), request_(std::move(transport)) {
    if (endpoint_.compare(0, 8, "https://") != 0 || endpoint_.size() <= 8 ||
        endpoint_.find_first_of("?#@ \t\r\n") != std::string::npos)
        throw std::invalid_argument("El endpoint requiere una URL HTTPS sin consulta, fragmento ni credenciales");
    if (collection_.empty() || collection_.size() > 100 ||
        collection_.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
        throw std::invalid_argument("Nombre de colección inválido (1..100 letras ASCII, dígitos, - o _)");
}

std::vector<MongoApiBookRepository::Record> MongoApiBookRepository::read(const std::string& id) const {
    // La colección y los idLibro validados solo contienen caracteres seguros de URL.
    auto url = endpoint_ + "?coleccion=" + collection_;
    if (!id.empty()) url += "&idLibro=" + id;
    const auto json = responseJson(request_("GET", url, ""));
    const auto& docs = json.at("documentos");
    if (json.at("coleccion") != collection_ || !docs.is_array() || !json.at("total").is_number_unsigned() ||
        json.at("total") != docs.size() || docs.size() > 10000)
        throw std::runtime_error("Colección, total o documentos inválidos en la lectura Mongo");
    std::vector<Record> records;
    std::unordered_set<std::string> ids, oids;
    for (const auto& d : docs) {
        if (d.at("proyecto") != collection_) throw std::runtime_error("Documento de otro proyecto en la colección");
        Libro book{d.at("idLibro").get<std::string>(), d.at("titulo").get<std::string>(),
                   d.at("autor").get<std::string>(), d.at("temas").get<std::vector<std::string>>()};
        validateBook(book);
        const auto oid = d.at("_id").at("$oid").get<std::string>();
        if (!validOid(oid) || (!id.empty() && book.idLibro != id) ||
            !ids.insert(book.idLibro).second || !oids.insert(oid).second)
            throw std::runtime_error("Identidad Mongo ambigua o filtro idLibro incumplido");
        records.push_back({std::move(book), oid});
    }
    return records;
}

std::vector<Libro> MongoApiBookRepository::load(const std::filesystem::path& source) const {
    if (source != workingPath()) throw std::invalid_argument("Origen distinto de la colección configurada");
    std::vector<Libro> books;
    decltype(loaded_) bindings;
    for (auto& record : read()) {
        books.push_back(record.book);
        bindings.emplace(books.back().idLibro, std::move(record));
    }
    loaded_.swap(bindings);
    return books;
}
MongoApiBookRepository::Record MongoApiBookRepository::selected(const std::string& id) const {
    const auto known = loaded_.find(id);
    if (known == loaded_.end()) throw std::runtime_error("Seleccione un idLibro cargado de esta colección");
    const auto current = read(id);
    if (current.size() != 1 || current.front().oid != known->second.oid || !(current.front().book == known->second.book))
        throw std::runtime_error("La selección cambió en la nube; vuelva a cargar antes de escribir");
    return current.front();
}

void MongoApiBookRepository::writeAndVerify(const std::string& method, const std::string& payload,
                                           const std::string& id, const Libro* expected, const std::string& oid) {
    std::string problem;
    bool rejected = false;
    try {
        const auto response = request_(method, endpoint_, payload);
        const auto json = responseJson(response);
        if (!json.contains("mensaje") || !json.at("mensaje").is_string())
            throw std::runtime_error("Acuse sin mensaje");
        if (method != "POST") {
            const auto& count = json.at(method == "PUT" ? "modificados" : "eliminados");
            rejected = count != 1 || !count.is_number_unsigned();
            if (rejected) problem = "el contador del acuse difiere de uno";
        }
    } catch (const ApiRejection& e) {
        rejected = true;
        problem = e.what();
    } catch (const std::exception& e) {
        // Incluso un timeout o un acuse ilegible puede llegar después del cambio.
        problem = e.what();
    }
    loaded_.erase(id);
    try {
        auto observed = read(id);
        if (!rejected) {
            if (!expected && observed.empty()) return;
            if (expected && observed.size() == 1 && observed.front().book == *expected &&
                (oid.empty() || observed.front().oid == oid)) {
                loaded_.emplace(id, std::move(observed.front()));
                return;
            }
        }
        throw std::runtime_error("la lectura y el acuse no confirman el cambio solicitado");
    } catch (const std::exception& e) {
        throw std::runtime_error(method + " con resultado incierto (" + id + "): " +
                                 (problem.empty() ? "" : problem + "; ") + e.what() +
                                 ". Recargue la colección antes de decidir otra escritura.");
    }
}
void MongoApiBookRepository::insertRecord(const Libro& book) {
    validateBook(book);
    if (!read(book.idLibro).empty()) throw std::invalid_argument("idLibro ya existe en la nube");
    auto data = fields(book);
    data["idLibro"] = book.idLibro;
    data["proyecto"] = collection_;
    const Json payload{{"coleccion", collection_}, {"datos", data}};
    writeAndVerify("POST", payload.dump(), book.idLibro, &book, "");
}
void MongoApiBookRepository::updateRecord(const Libro& book) {
    validateBook(book);
    const auto previous = selected(book.idLibro);
    if (previous.book == book) return;
    const Json payload{{"coleccion", collection_}, {"filtro", exactFilter(collection_, book.idLibro, previous.oid)},
                       {"datos", fields(book)}};
    writeAndVerify("PUT", payload.dump(), book.idLibro, &book, previous.oid);
}
void MongoApiBookRepository::eraseRecord(const std::string& id) {
    const auto previous = selected(id);
    const Json payload{{"coleccion", collection_}, {"filtro", exactFilter(collection_, id, previous.oid)}};
    writeAndVerify("DELETE", payload.dump(), id, nullptr, previous.oid);
}
} // namespace biblioteca
