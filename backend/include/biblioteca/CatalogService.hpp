#pragma once
#include "biblioteca/BPlusTree.hpp"
#include "biblioteca/IRecordBookRepository.hpp"
#include <stdexcept>
#include <unordered_map>

namespace biblioteca {
class DuplicateBookId : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class CatalogService {
public:
    explicit CatalogService(IBookRepository& repository, std::size_t order = 4);
    std::size_t load(const std::filesystem::path& source);
    void insert(const Libro& book);
    void update(const Libro& book);
    bool erase(const std::string& id);
    void save();
    std::vector<Libro> byTitle(const std::string& title) const { requireLoaded(); return tree_.byTitle(title); }
    std::vector<Libro> byPrefix(const std::string& title) const { requireLoaded(); return tree_.byPrefix(title); }
    std::vector<Libro> all() const { requireLoaded(); return tree_.all(); }
    bool validate(std::string& reason) const;
    void visualize(std::ostream& out) const { requireLoaded(); tree_.visualize(out); }
    bool writesThrough() const { return records_ != nullptr; }
    bool dirty() const { return dirty_; }
    std::filesystem::path workingPath() const { return repository_.workingPath(); }
private:
    IBookRepository& repository_;
    IRecordBookRepository* records_;
    bool needsReload_;
    void requireLoaded() const;
    void reloadAfterWrite();
    BPlusTree tree_;
    // Auxiliar de unicidad global y localización de la clave por ID. Nunca responde
    // consultas por título, prefijo, rango ni listado: todas recorren el B+.
    std::unordered_map<std::string, BookKey> ids_;
    bool dirty_ = false;
};
} // namespace biblioteca
