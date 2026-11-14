#pragma once
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace biblioteca {
// Fecha ISO AAAA-MM-DD e ID. El dominio conserva el registro histórico.
using ClavePrestamo = std::pair<std::string, std::string>;

class PrestamosActivos {
public:
    bool insertar(const ClavePrestamo& clave);
    bool eliminar(const ClavePrestamo& clave);
    bool cambiarVencimiento(const ClavePrestamo& anterior, const std::string& fechaNueva);
    bool contiene(const ClavePrestamo& clave, std::vector<ClavePrestamo>* traza = nullptr) const;
    std::vector<ClavePrestamo> enOrden() const;
    bool validar() const;
    std::string snapshot() const;
private:
    struct Nodo {
        ClavePrestamo clave;
        bool rojo;
        Nodo* padre;
        std::unique_ptr<Nodo> izq, der;
    };
    using Rama = std::unique_ptr<Nodo>;
    Rama raiz_;
    std::size_t cantidad_ = 0;
    static bool rojo(const Rama& rama);
    static void rotarIzq(Rama& rama);
    static void rotarDer(Rama& rama);
    static void invertirColores(Nodo& nodo);
    static void equilibrar(Rama& rama);
    static void moverRojoIzq(Rama& rama);
    static void moverRojoDer(Rama& rama);
    static bool insertarEn(Rama& rama, Nodo* padre, const ClavePrestamo& clave);
    static void quitarMinimo(Rama& rama);
    static void quitar(Rama& rama, const ClavePrestamo& clave);
    static int alturaNegra(const Nodo* nodo, const Nodo* padre, const ClavePrestamo* menor,
                           const ClavePrestamo* mayor, std::size_t& cantidad);
};
} // namespace biblioteca
