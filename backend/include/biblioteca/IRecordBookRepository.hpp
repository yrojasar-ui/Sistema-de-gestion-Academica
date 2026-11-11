#pragma once
#include "biblioteca/IBookRepository.hpp"
#include <stdexcept>

namespace biblioteca {
// El servicio distingue los cambios por documento del guardado de un snapshot JSON.
class IRecordBookRepository : public IBookRepository {
public:
    virtual void insertRecord(const Libro& book) = 0;
    virtual void updateRecord(const Libro& book) = 0;
    virtual void eraseRecord(const std::string& id) = 0;
    void save(const std::vector<Libro>&) const final {
        throw std::logic_error("El repositorio por documento no admite reemplazar la colección");
    }
};
} // namespace biblioteca
