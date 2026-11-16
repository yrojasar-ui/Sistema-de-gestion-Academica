#include "biblioteca/MongoApiProtocol.hpp"
#include <algorithm>
#include <cctype>
namespace biblioteca::mongo {
using Json = nlohmann::json;
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
bool validOid(const std::string& oid) {
    return oid.size() == 24 && oid.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos;
}
void validateConfig(const std::string& endpoint, const std::string& collection) {
    if (endpoint.compare(0, 8, "https://") != 0 || endpoint.size() <= 8 ||
        endpoint.find_first_of("?#@ \t\r\n") != std::string::npos)
        throw std::invalid_argument("El endpoint requiere una URL HTTPS sin consulta, fragmento ni credenciales");
    if (collection.empty() || collection.size() > 100 ||
        collection.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
        throw std::invalid_argument("Nombre de colección inválido (1..100 letras ASCII, dígitos, - o _)");
}
}
