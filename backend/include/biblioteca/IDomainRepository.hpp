#pragma once
#include "biblioteca/Dominio.hpp"
#include <memory>

namespace biblioteca {
// load entrega una copia. replace debe confirmar todo o lanzar conservando el estado.
// Un futuro adaptador remoto debe declarar y resolver escrituras de resultado incierto.
class IDomainRepository {
public:
    virtual ~IDomainRepository() = default;
    virtual EstadoBiblioteca load() const = 0;
    virtual void replace(const EstadoBiblioteca& estado) = 0;
};
class MemoryDomainRepository final : public IDomainRepository {
public:
    EstadoBiblioteca load() const override { return *estado_; }
    void replace(const EstadoBiblioteca& estado) override {
        auto nuevo = std::make_unique<EstadoBiblioteca>(estado);
        estado_.swap(nuevo);
    }
private:
    std::unique_ptr<EstadoBiblioteca> estado_ = std::make_unique<EstadoBiblioteca>();
};
// El dominio prepara avisos pendientes. RabbitMQ implementará esta frontera.
class IEventPublisher {
public:
    virtual ~IEventPublisher() = default;
    virtual void publish(const Aviso& aviso) = 0;
};
} // namespace biblioteca
