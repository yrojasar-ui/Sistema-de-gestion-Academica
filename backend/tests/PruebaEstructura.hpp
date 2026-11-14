#pragma once
#include <cstddef>
#include <exception>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>

namespace prueba {
using nlohmann::json;
struct Caso {
    explicit Caso(unsigned semilla) : azar(semilla) { std::cout << "Semilla=" << semilla << '\n'; }
    std::mt19937 azar;
    std::size_t operaciones = 0, comprobaciones = 0;
    void exigir(bool ok, const std::string& criterio) {
        ++comprobaciones;
        if (!ok) throw std::runtime_error(criterio + " (operación " + std::to_string(operaciones) + ")");
    }
};
inline int ejecutar(unsigned semilla, void (*probar)(Caso&)) {
    try {
        Caso caso(semilla);
        probar(caso);
        std::cout << "OK operaciones=" << caso.operaciones << " comprobaciones=" << caso.comprobaciones << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FALLO: " << e.what() << '\n';
        return 1;
    }
}
} // namespace prueba
