#include "biblioteca/ColaReservas.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <tuple>

namespace biblioteca {
ColaReservas::ColaReservas(std::string idLibro) : libro_(std::move(idLibro)) {
    if (libro_.empty()) throw std::invalid_argument("La cola necesita un libro");
}
ColaReservas::~ColaReservas() {
    // Liberación iterativa incluso si quedan muchos turnos pendientes.
    while (inicio_) {
        auto anterior = std::move(inicio_);
        inicio_ = std::move(anterior->siguiente);
    }
}
bool ColaReservas::encolar(std::string idReserva, std::uint64_t llegada) {
    if (idReserva.empty()) throw std::invalid_argument("idReserva vacío");
    // La llegada es la secuencia asignada al recibir la reserva. El ID desempata.
    if (ids_.count(idReserva)) return false;
    auto nuevo = std::make_unique<Nodo>(Nodo{std::move(idReserva), llegada, {}});
    const auto turno = std::tie(nuevo->llegada, nuevo->id);
    auto insercion = &inicio_;
    if (fin_ && std::tie(fin_->llegada, fin_->id) < turno) {
        insercion = &fin_->siguiente;
    } else {
        while (*insercion && std::tie((*insercion)->llegada, (*insercion)->id) < turno)
            insercion = &(*insercion)->siguiente;
    }
    ids_.insert(nuevo->id);
    if (!*insercion) fin_ = nuevo.get();
    nuevo->siguiente = std::move(*insercion);
    *insercion = std::move(nuevo);
    return true;
}
std::optional<std::string> ColaReservas::frente() const {
    if (!inicio_) return std::nullopt;
    return inicio_->id;
}
std::optional<std::string> ColaReservas::desencolar() {
    if (!inicio_) return std::nullopt;
    auto anterior = std::move(inicio_);
    inicio_ = std::move(anterior->siguiente);
    if (!inicio_) fin_ = nullptr;
    ids_.erase(anterior->id);
    return std::move(anterior->id);
}
std::optional<std::string> ColaReservas::atender(const Cancelada& cancelada) {
    while (inicio_ && cancelada(inicio_->id)) desencolar();
    return desencolar();
}
std::vector<std::string> ColaReservas::recorrer() const {
    std::vector<std::string> ids;
    for (auto n = inicio_.get(); n; n = n->siguiente.get()) ids.push_back(n->id);
    return ids;
}
std::string ColaReservas::snapshot() const {
    using nlohmann::json;
    auto id = [](const Nodo* n) -> json { return n ? json(n->id) : json(nullptr); };
    auto nodos = json::array();
    for (auto n = inicio_.get(); n; n = n->siguiente.get())
        nodos.push_back({{"id", n->id}, {"llegada", n->llegada}, {"siguiente", id(n->siguiente.get())}});
    return json{{"idLibro", libro_}, {"inicio", id(inicio_.get())}, {"fin", id(fin_)},
        {"cantidad", ids_.size()}, {"nodos", std::move(nodos)}}.dump();
}
} // namespace biblioteca
