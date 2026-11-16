#pragma once
#include "biblioteca/Dominio.hpp"
#include <memory>
#include "biblioteca/Persistence.hpp"

namespace biblioteca {
// replace retorna solo con persistencia confirmada. El servicio publica después.
// PersistenceError distingue rechazo de resultado incierto; este último bloquea
// nuevas escrituras hasta reconciliar, sin afirmar que el remoto quedó intacto.
class IDomainRepository {
public:
    virtual ~IDomainRepository() = default;
    virtual bool ready() const { return true; }
    virtual std::string storage() const { return "memoria"; }
    virtual std::string recovery() const { return "validado"; }
    virtual std::string outcome() const { return "confirmado"; }
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
