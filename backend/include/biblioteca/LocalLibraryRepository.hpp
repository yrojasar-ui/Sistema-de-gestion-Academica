#pragma once
#include "biblioteca/IBookRepository.hpp"
#include "biblioteca/IDomainRepository.hpp"
#include "biblioteca/DomainJson.hpp"
namespace biblioteca {
// Un archivo agregado: nunca se publica la mitad libros/dominio.
class LocalLibraryRepository final : public IBookRepository, public IDomainRepository {
public:
    LocalLibraryRepository(std::filesystem::path initial, std::filesystem::path working, bool migrate = false);
    std::vector<Libro> load(const std::filesystem::path&) const override;
    EstadoBiblioteca load() const override;
    void save(const std::vector<Libro>&) const override;
    void replace(const EstadoBiblioteca&) override;
    std::filesystem::path workingPath() const override { return path_; }
    std::string storage() const override { return "json"; }
    bool ready() const override { return !blocked_; }
    std::string recovery() const override { return blocked_ ? "reinicio requerido" : "validado"; }
    std::string outcome() const override { return blocked_ ? "incierto" : "confirmado"; }
private:
    std::filesystem::path path_;
    mutable Json current_;
    mutable bool blocked_ = false;
    void commit(Json next, bool checkExternal = true) const;
};
}
