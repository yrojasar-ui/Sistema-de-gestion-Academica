#include "biblioteca/PrestamosActivos.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace biblioteca {
bool PrestamosActivos::rojo(const Rama& rama) { return rama && rama->rojo; }

void PrestamosActivos::rotarIzq(Rama& rama) {
    auto cima = std::move(rama->der);
    rama->der = std::move(cima->izq);
    if (rama->der) rama->der->padre = rama.get();
    cima->padre = rama->padre;
    rama->padre = cima.get();
    cima->rojo = rama->rojo;
    rama->rojo = true;
    cima->izq = std::move(rama);
    rama = std::move(cima);
}
void PrestamosActivos::rotarDer(Rama& rama) {
    auto cima = std::move(rama->izq);
    rama->izq = std::move(cima->der);
    if (rama->izq) rama->izq->padre = rama.get();
    cima->padre = rama->padre;
    rama->padre = cima.get();
    cima->rojo = rama->rojo;
    rama->rojo = true;
    cima->der = std::move(rama);
    rama = std::move(cima);
}
void PrestamosActivos::invertirColores(Nodo& nodo) {
    nodo.rojo = !nodo.rojo;
    nodo.izq->rojo = !nodo.izq->rojo;
    nodo.der->rojo = !nodo.der->rojo;
}
// Variante con enlaces rojos inclinados a la izquierda (LLRB).
void PrestamosActivos::equilibrar(Rama& rama) {
    if (rojo(rama->der) && !rojo(rama->izq)) rotarIzq(rama);
    if (rojo(rama->izq) && rojo(rama->izq->izq)) rotarDer(rama);
    if (rojo(rama->izq) && rojo(rama->der)) invertirColores(*rama);
}
bool PrestamosActivos::insertarEn(Rama& rama, Nodo* padre, const ClavePrestamo& clave) {
    if (!rama) {
        rama = std::make_unique<Nodo>(Nodo{clave, true, padre, {}, {}});
        return true;
    }
    if (clave == rama->clave) return false;
    auto& hija = clave < rama->clave ? rama->izq : rama->der;
    if (!insertarEn(hija, rama.get(), clave)) return false;
    equilibrar(rama);
    return true;
}
bool PrestamosActivos::insertar(const ClavePrestamo& clave) {
    if (clave.first.empty() || clave.second.empty()) throw std::invalid_argument("Clave de préstamo vacía");
    if (!insertarEn(raiz_, nullptr, clave)) return false;
    raiz_->rojo = false;
    ++cantidad_;
    return true;
}
bool PrestamosActivos::contiene(const ClavePrestamo& clave, std::vector<ClavePrestamo>* traza) const {
    if (traza) traza->clear();
    auto nodo = raiz_.get();
    while (nodo) {
        if (traza) traza->push_back(nodo->clave);
        if (clave == nodo->clave) return true;
        nodo = (clave < nodo->clave ? nodo->izq : nodo->der).get();
    }
    return false;
}
// Antes de bajar, prestar o fusionar para que la rama que pierde una clave tenga un rojo.
void PrestamosActivos::moverRojoIzq(Rama& rama) {
    invertirColores(*rama);
    if (rojo(rama->der->izq)) {
        rotarDer(rama->der);
        rotarIzq(rama);
        invertirColores(*rama);
    }
}
void PrestamosActivos::moverRojoDer(Rama& rama) {
    invertirColores(*rama);
    if (rojo(rama->izq->izq)) {
        rotarDer(rama);
        invertirColores(*rama);
    }
}
void PrestamosActivos::quitarMinimo(Rama& rama) {
    if (!rama->izq) { rama.reset(); return; }
    if (!rojo(rama->izq) && !rojo(rama->izq->izq)) moverRojoIzq(rama);
    quitarMinimo(rama->izq);
    equilibrar(rama);
}
void PrestamosActivos::quitar(Rama& rama, const ClavePrestamo& clave) {
    if (clave < rama->clave) {
        if (!rojo(rama->izq) && !rojo(rama->izq->izq)) moverRojoIzq(rama);
        quitar(rama->izq, clave);
    } else {
        if (rojo(rama->izq)) rotarDer(rama);
        if (clave == rama->clave && !rama->der) { rama.reset(); return; }
        if (!rojo(rama->der) && !rojo(rama->der->izq)) moverRojoDer(rama);
        if (clave == rama->clave) {
            auto sucesor = rama->der.get();
            while (sucesor->izq) sucesor = sucesor->izq.get();
            rama->clave.swap(sucesor->clave);
            quitarMinimo(rama->der);
        } else {
            quitar(rama->der, clave);
        }
    }
    equilibrar(rama);
}
bool PrestamosActivos::eliminar(const ClavePrestamo& clave) {
    if (!contiene(clave)) return false;
    if (!rojo(raiz_->izq) && !rojo(raiz_->der)) raiz_->rojo = true;
    quitar(raiz_, clave);
    if (raiz_) raiz_->rojo = false;
    --cantidad_;
    return true;
}
bool PrestamosActivos::cambiarVencimiento(const ClavePrestamo& anterior, const std::string& fechaNueva) {
    if (!contiene(anterior)) return false;
    if (anterior.first == fechaNueva) return true;
    // Reservar el nodo nuevo antes de retirar el anterior conserva la entrada ante un fallo.
    if (!insertar({fechaNueva, anterior.second})) return false;
    return eliminar(anterior);
}
int PrestamosActivos::alturaNegra(const Nodo* nodo, const Nodo* padre, const ClavePrestamo* menor,
                                  const ClavePrestamo* mayor, std::size_t& cantidad) {
    if (!nodo) return 1; // La hoja nula es negra.
    ++cantidad;
    if (nodo->padre != padre || (menor && *menor >= nodo->clave) || (mayor && nodo->clave >= *mayor)
        || rojo(nodo->der) || (nodo->rojo && rojo(nodo->izq))) return -1;
    int izq = alturaNegra(nodo->izq.get(), nodo, menor, &nodo->clave, cantidad);
    int der = alturaNegra(nodo->der.get(), nodo, &nodo->clave, mayor, cantidad);
    if (izq < 0 || izq != der) return -1;
    return izq + (nodo->rojo ? 0 : 1);
}
bool PrestamosActivos::validar() const {
    std::size_t cantidad = 0;
    return !rojo(raiz_) && alturaNegra(raiz_.get(), nullptr, nullptr, nullptr, cantidad) > 0 && cantidad == cantidad_;
}
std::vector<ClavePrestamo> PrestamosActivos::enOrden() const {
    std::vector<ClavePrestamo> claves;
    auto visitar = [&](auto& recorrer, const Nodo* nodo) -> void {
        if (!nodo) return;
        recorrer(recorrer, nodo->izq.get());
        claves.push_back(nodo->clave);
        recorrer(recorrer, nodo->der.get());
    };
    visitar(visitar, raiz_.get());
    return claves;
}
std::string PrestamosActivos::snapshot() const {
    using nlohmann::json;
    auto id = [](const Nodo* n) -> json { return n ? json(n->clave) : json(nullptr); };
    auto nodos = json::array();
    auto visitar = [&](auto& recorrer, const Nodo* n) -> void {
        if (!n) return;
        nodos.push_back({{"clave", n->clave}, {"color", n->rojo ? "rojo" : "negro"},
            {"padre", id(n->padre)}, {"izq", id(n->izq.get())}, {"der", id(n->der.get())}});
        recorrer(recorrer, n->izq.get());
        recorrer(recorrer, n->der.get());
    };
    visitar(visitar, raiz_.get());
    std::size_t cantidad = 0;
    return json{{"raiz", id(raiz_.get())}, {"cantidad", cantidad_}, {"nodos", std::move(nodos)},
        {"alturaNegra", alturaNegra(raiz_.get(), nullptr, nullptr, nullptr, cantidad)}}.dump();
}
} // namespace biblioteca
