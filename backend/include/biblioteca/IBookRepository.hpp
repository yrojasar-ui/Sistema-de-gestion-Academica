#pragma once
#include "biblioteca/Libro.hpp"
#include <filesystem>

namespace biblioteca {
// Persistencia de snapshots; las consultas pertenecen al B+, no al adaptador.
class IBookRepository {
public:
    virtual ~IBookRepository() = default;
    virtual std::vector<Libro> load(const std::filesystem::path& source) const = 0;
    virtual void save(const std::vector<Libro>& books) const = 0;
    virtual std::filesystem::path workingPath() const = 0;
};
} // namespace biblioteca
