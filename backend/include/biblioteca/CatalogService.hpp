#pragma once
#include "biblioteca/BPlusTree.hpp"
#include "biblioteca/IBookRepository.hpp"
#include <unordered_map>

namespace biblioteca {
class CatalogService {
public:
    explicit CatalogService(IBookRepository& repository, std::size_t order = 4);
    std::size_t load(const std::filesystem::path& source);
    void insert(const Libro& book);
    bool erase(const std::string& id);
    void save();
    std::vector<Libro> byTitle(const std::string& title) const { return tree_.byTitle(title); }
    std::vector<Libro> byPrefix(const std::string& title) const { return tree_.byPrefix(title); }
    std::vector<Libro> all() const { return tree_.all(); }
    bool validate(std::string& reason) const;
    void visualize(std::ostream& out) const { tree_.visualize(out); }
    bool dirty() const { return dirty_; }
    std::filesystem::path workingPath() const { return repository_.workingPath(); }
private:
    IBookRepository& repository_;
    BPlusTree tree_;
    // Auxiliar de unicidad global y localización de la clave por ID. Nunca responde
    // consultas por título, prefijo, rango ni listado: todas recorren el B+.
    std::unordered_map<std::string, BookKey> ids_;
    bool dirty_ = false;
};
} // namespace biblioteca
