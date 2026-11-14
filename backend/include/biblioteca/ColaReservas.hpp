#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <unordered_set>

namespace biblioteca {
class ColaReservas {
public:
    explicit ColaReservas(std::string idLibro);
    ~ColaReservas();
    bool encolar(std::string idReserva, std::uint64_t llegada);
    std::optional<std::string> frente() const;
    std::optional<std::string> desencolar();
    using Cancelada = std::function<bool(const std::string&)>;
    std::optional<std::string> atender(const Cancelada& cancelada);
    std::vector<std::string> recorrer() const;
    std::string snapshot() const;
private:
    struct Nodo {
        std::string id;
        std::uint64_t llegada;
        std::unique_ptr<Nodo> siguiente;
    };
    std::string libro_;
    std::unique_ptr<Nodo> inicio_;
    Nodo* fin_ = nullptr;
    std::unordered_set<std::string> ids_; // Solo unicidad. Los turnos viven en la lista.
};
} // namespace biblioteca
