#pragma once
#include "biblioteca/MongoApiBookRepository.hpp"
#include <nlohmann/json.hpp>
namespace biblioteca::mongo {
struct ApiRejection : std::runtime_error { using std::runtime_error::runtime_error; };
nlohmann::json responseJson(const HttpResponse& response);
bool validOid(const std::string& oid);
void validateConfig(const std::string& endpoint, const std::string& collection);
} // namespace biblioteca::mongo
