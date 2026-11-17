#pragma once
#include "biblioteca/IndiceUsuarios.hpp"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace biblioteca {
using Usuario = UsuarioIndice;
struct Ejemplar { std::string idEjemplar, idLibro, estado = "disponible"; };
struct Auditoria { std::string fecha, motivo, responsable; };
struct Prestamo {
    std::string idPrestamo, idEjemplar, idUsuario, inicio, vencimiento, devolucion;
    Auditoria cancelacion;
};
struct Reserva {
    std::string idReserva, idLibro, idUsuario;
    std::uint64_t llegada = 0;
    std::string estado = "pendiente", idEjemplar, creada;
    Auditoria cancelacion;
};
struct Aviso {
    std::string idAviso, tipo, idUsuario, idReserva, idPrestamo, creado;
    std::string estado = "pendiente";
};
struct Cargo {
    std::string idPrestamo, idUsuario;
    std::int64_t importe = 0;
    std::string moneda = "CRC", estado = "pendiente";
};
struct Comprobante {
    std::string idComprobante, fecha, idUsuario, idPrestamo;
    std::int64_t importe = 0;
    std::string moneda = "CRC", etiqueta = "SIMULADO";
};
inline bool activo(const Prestamo& p) { return p.devolucion.empty() && p.cancelacion.fecha.empty(); }
inline std::string estadoPrestamo(const Prestamo& p) {
    return !p.cancelacion.fecha.empty() ? "cancelado" : (p.devolucion.empty() ? "activo" : "devuelto");
}
struct EstadoBiblioteca {
    std::vector<Usuario> usuarios;
    std::vector<Ejemplar> ejemplares;
    std::vector<Prestamo> prestamos;
    std::vector<Reserva> reservas;
    std::vector<Aviso> avisos;
    std::vector<Cargo> cargos;
    std::vector<Comprobante> comprobantes;
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
