#include "biblioteca/CatalogService.hpp"
#include <stdexcept>

namespace biblioteca {
CatalogService::CatalogService(IBookRepository& repository, std::size_t order) : repository_(repository),
    records_(dynamic_cast<IRecordBookRepository*>(&repository)), needsReload_(writesThrough()), tree_(order) {}
void CatalogService::requireLoaded() const {
    if (needsReload_) throw std::runtime_error("Recargue la colección para consultar o modificar el catálogo");
}
void CatalogService::reloadAfterWrite() {
    try {
        load(workingPath());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Cambio confirmado. Falló la recarga del B+: ") + e.what());
    }
}
std::size_t CatalogService::load(const std::filesystem::path& source) {
    needsReload_ = writesThrough();
    const auto books = repository_.load(source);
    if (books.size() > 10000) throw std::invalid_argument("Máximo 10000 libros en este prototipo");
    // Resolver también la procedencia antes del intercambio: una excepción del
    // adaptador o del sistema de archivos no debe publicar una carga a medias.
    const bool imported = !writesThrough() && std::filesystem::weakly_canonical(source) !=
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
    needsReload_ = false;
    dirty_ = imported;
    return books.size();
}
void CatalogService::insert(const Libro& book) {
    requireLoaded();
    validateBook(book);
    if (ids_.count(book.idLibro)) throw DuplicateBookId("ID duplicado: " + book.idLibro);
    if (tree_.size() >= 10000) throw std::invalid_argument("Máximo 10000 libros en este prototipo");
    if (records_) {
        needsReload_ = true;
        records_->insertRecord(book);
        reloadAfterWrite();
        return;
    }
    auto inserted = ids_.emplace(book.idLibro, keyOf(book));
    try {
        if (!tree_.insert(book)) throw std::logic_error("Clave duplicada en B+");
    } catch (...) { ids_.erase(inserted.first); throw; }
    dirty_ = true;
}
void CatalogService::update(const Libro& book) {
    requireLoaded();
    if (!records_) throw std::logic_error("La edición por documento requiere almacenamiento Mongo");
    validateBook(book);
    if (!ids_.count(book.idLibro)) throw std::invalid_argument("ID ausente");
    needsReload_ = true;
    records_->updateRecord(book);
    reloadAfterWrite();
}
bool CatalogService::erase(const std::string& id) {
    requireLoaded();
    const auto pos = ids_.find(id);
    if (pos == ids_.end()) return false;
    if (records_) {
        needsReload_ = true;
        records_->eraseRecord(id);
        reloadAfterWrite();
        return true;
    }
    if (!tree_.erase(pos->second)) throw std::logic_error("ID e índice desincronizados");
    ids_.erase(pos); dirty_ = true; return true;
}
void CatalogService::save() {
    repository_.save(tree_.all());
    dirty_ = false; // Si save lanza, el catálogo sigue modificado y se puede reintentar.
}
bool CatalogService::validate(std::string& reason) const {
    requireLoaded();
    if (!tree_.validate(reason)) return false;
    if (tree_.size() != ids_.size()) { reason = "Tamaños ID/B+ distintos"; return false; }
    for (const auto& book : tree_.all()) {
        const auto it = ids_.find(book.idLibro);
        if (it == ids_.end() || it->second != keyOf(book)) { reason = "ID/B+ no coincide"; return false; }
    }
    return true;
}
} // namespace biblioteca
