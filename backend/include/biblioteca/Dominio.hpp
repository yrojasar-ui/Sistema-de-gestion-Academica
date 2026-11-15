#pragma once
#include "biblioteca/IndiceUsuarios.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace biblioteca {
using Usuario = UsuarioIndice;
struct Ejemplar { std::string idEjemplar, idLibro, estado = "disponible"; };
struct Prestamo {
    std::string idPrestamo, idEjemplar, idUsuario, inicio, vencimiento, devolucion;
};
struct Reserva {
    std::string idReserva, idLibro, idUsuario;
    std::uint64_t llegada = 0;
    std::string estado = "pendiente", idEjemplar, creada;
};
struct Aviso {
    std::string idAviso, tipo, idUsuario, idReserva, idPrestamo, creado;
    std::string estado = "pendiente";
};
struct EstadoBiblioteca {
    std::vector<Usuario> usuarios;
    std::vector<Ejemplar> ejemplares;
    std::vector<Prestamo> prestamos;
    std::vector<Reserva> reservas;
    std::vector<Aviso> avisos;
    std::uint64_t siguienteLlegada = 1;
};
class ErrorDominio : public std::runtime_error {
public:
    ErrorDominio(int codigo, const std::string& mensaje) : std::runtime_error(mensaje), status(codigo) {}
    int status;
};
void validarId(const std::string& id);
void validarFecha(const std::string& fecha);
void validarInstante(const std::string& instante);
std::string ahoraUTC();
bool pendiente(const Reserva& reserva);
} // namespace biblioteca
