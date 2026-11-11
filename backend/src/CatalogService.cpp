#include "biblioteca/CatalogService.hpp"
#include <stdexcept>

namespace biblioteca {
CatalogService::CatalogService(IBookRepository& repository, std::size_t order) : repository_(repository), tree_(order) {}
std::size_t CatalogService::load(const std::filesystem::path& source) {
    const auto books = repository_.load(source);
    if (books.size() > 10000) throw std::invalid_argument("Máximo 10000 libros en este prototipo");
    // Resolver también la procedencia antes del intercambio: una excepción del
    // adaptador o del sistema de archivos no debe publicar una carga a medias.
    const bool imported = std::filesystem::weakly_canonical(source) !=
                          std::filesystem::weakly_canonical(repository_.workingPath());
    BPlusTree candidate(tree_.order());
    std::unordered_map<std::string, BookKey> ids;
    for (const auto& book : books) {
        validateBook(book);
        if (!ids.emplace(book.idLibro, keyOf(book)).second) throw std::invalid_argument("ID duplicado: " + book.idLibro);
        if (!candidate.insert(book)) throw std::logic_error("Clave duplicada en carga");
    }
    std::string reason;
    if (!candidate.validate(reason)) throw std::logic_error("Carga rechazada por invariantes: " + reason);
    // Solo publicar cuando el snapshot completo es válido. La reconstrucción aquí
    // corresponde a carga/reinicio, nunca sustituye el algoritmo de borrado.
    tree_.swap(candidate); ids_.swap(ids);
    dirty_ = imported;
    return books.size();
}
void CatalogService::insert(const Libro& book) {
    validateBook(book);
    if (ids_.count(book.idLibro)) throw std::invalid_argument("ID duplicado: " + book.idLibro);
    if (tree_.size() >= 10000) throw std::invalid_argument("Máximo 10000 libros en este prototipo");
    auto inserted = ids_.emplace(book.idLibro, keyOf(book));
    try {
        if (!tree_.insert(book)) throw std::logic_error("Clave duplicada en B+");
    } catch (...) { ids_.erase(inserted.first); throw; }
    dirty_ = true;
}
bool CatalogService::erase(const std::string& id) {
    const auto pos = ids_.find(id);
    if (pos == ids_.end()) return false;
    if (!tree_.erase(pos->second)) throw std::logic_error("ID e índice desincronizados");
    ids_.erase(pos); dirty_ = true; return true;
}
void CatalogService::save() {
    repository_.save(tree_.all());
    dirty_ = false; // Si save lanza, el catálogo sigue modificado y se puede reintentar.
}
bool CatalogService::validate(std::string& reason) const {
    if (!tree_.validate(reason)) return false;
    if (tree_.size() != ids_.size()) { reason = "Tamaños ID/B+ distintos"; return false; }
    for (const auto& book : tree_.all()) {
        const auto it = ids_.find(book.idLibro);
        if (it == ids_.end() || it->second != keyOf(book)) { reason = "ID/B+ no coincide"; return false; }
    }
    return true;
}
} // namespace biblioteca
