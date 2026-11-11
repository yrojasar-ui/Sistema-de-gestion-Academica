#pragma once
#include "biblioteca/IRecordBookRepository.hpp"
#include <functional>
#include <optional>
#include <unordered_map>

namespace biblioteca {
struct HttpResponse {
    long status;
    std::string contentType;
    std::string body;
};
using HttpTransport = std::function<HttpResponse(const std::string& method, const std::string& url,
                                                  const std::string& body)>;
HttpResponse curlRequest(const std::string& method, const std::string& url, const std::string& body);

class MongoApiBookRepository final : public IRecordBookRepository {
public:
    MongoApiBookRepository(std::string endpoint, std::string collection, HttpTransport transport = curlRequest);
    std::filesystem::path workingPath() const override { return collection_; }
    std::vector<Libro> load(const std::filesystem::path& source) const override;
    void insertRecord(const Libro& book) override;
    void updateRecord(const Libro& book) override;
    void eraseRecord(const std::string& id) override;
private:
    struct Record { Libro book; std::string oid; };
    std::string endpoint_, collection_;
    HttpTransport request_;
    // Identidad y versión leídas para comprobar la selección antes de PUT/DELETE.
    mutable std::unordered_map<std::string, Record> loaded_;
    std::vector<Record> read(const std::string& id = {}) const;
    Record selected(const std::string& id) const;
    void writeAndVerify(const std::string& method, const std::string& payload, const std::string& id,
                        const Libro* expected, const std::string& oid);
};
} // namespace biblioteca
