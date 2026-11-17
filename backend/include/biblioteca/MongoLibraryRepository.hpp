#pragma once
#include "biblioteca/DomainJson.hpp"
#include "biblioteca/IDomainRepository.hpp"
#include "biblioteca/MongoApiProtocol.hpp"
namespace biblioteca {
// Una unidad remota para libros y dominio. El diario no es una segunda fuente de datos.
class MongoLibraryRepository final : public IBookRepository, public IDomainRepository {
public:
    MongoLibraryRepository(std::string endpoint, std::string collection, std::filesystem::path journal,
                           HttpTransport transport = curlRequest);
    std::filesystem::path workingPath() const override { return collection_; }
    std::vector<Libro> load(const std::filesystem::path& source) const override;
    EstadoBiblioteca load() const override;
    void save(const std::vector<Libro>& books) const override;
    void replace(const EstadoBiblioteca& state) override;
    bool ready() const override { return ready_; }
    std::string storage() const override { return "mongo"; }
    std::string recovery() const override { return detail_; }
    std::string outcome() const override { return outcomeName(outcome_); }
    // refresh solo lee/reconcilia; accept se llama después de reconstruir y validar TODOS los índices.
    void refresh(bool initialize = false);
    void allowMigration(bool allow) { migration_ = allow; }
    void migrateDomain();
    void accept() { ready_ = true; detail_ = "validado"; outcome_ = WriteOutcome::confirmado; }
    void block(const std::string& detail) const { ready_ = false; detail_ = detail; }
private:
    std::string endpoint_, collection_;
    std::filesystem::path journal_;
    HttpTransport request_;
    mutable Json current_;
    bool migration_ = false;
    mutable bool ready_ = false;
    mutable std::string detail_ = "sin cargar";
    mutable WriteOutcome outcome_ = WriteOutcome::rechazado;
    Json read() const;
    void validateDocument(const Json& doc) const;
    Json intent() const;
    void clearIntent() const;
    void write(const Json& next) const;
    void replaceState(Json state) const;
    void fail(WriteOutcome result, const std::string& message) const;
};
} // namespace biblioteca
