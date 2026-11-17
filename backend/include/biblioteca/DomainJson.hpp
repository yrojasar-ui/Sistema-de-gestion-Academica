#pragma once
#include "biblioteca/Dominio.hpp"
#include "biblioteca/Libro.hpp"
#include <nlohmann/json.hpp>

namespace biblioteca {
using Json = nlohmann::json;
inline void campos(const Json& j, std::initializer_list<const char*> nombres) {
    if (!j.is_object() || j.size() != nombres.size()) throw std::invalid_argument("Campos inesperados o ausentes");
    for (const auto* n : nombres) if (!j.contains(n)) throw std::invalid_argument("Campo requerido ausente");
}
inline void to_json(Json& j, const Libro& v) {
    j = {{"idLibro",v.idLibro},{"titulo",v.titulo},{"autor",v.autor},{"temas",v.temas}};
}
inline void from_json(const Json& j, Libro& v) {
    campos(j, {"idLibro","titulo","autor","temas"});
    j.at("idLibro").get_to(v.idLibro); j.at("titulo").get_to(v.titulo);
    j.at("autor").get_to(v.autor); j.at("temas").get_to(v.temas);
}
inline void to_json(Json& j, const Usuario& v) {
    j = {{"idUsuario",v.idUsuario},{"nombre",v.nombre},{"activo",v.activo}};
}
inline void from_json(const Json& j, Usuario& v) {
    campos(j, {"idUsuario","nombre","activo"});
    j.at("idUsuario").get_to(v.idUsuario); j.at("nombre").get_to(v.nombre); j.at("activo").get_to(v.activo);
}
inline void to_json(Json& j, const Ejemplar& v) {
    j = {{"idEjemplar",v.idEjemplar},{"idLibro",v.idLibro},{"estado",v.estado}};
}
inline void from_json(const Json& j, Ejemplar& v) {
    campos(j, {"idEjemplar","idLibro","estado"});
    j.at("idEjemplar").get_to(v.idEjemplar); j.at("idLibro").get_to(v.idLibro); j.at("estado").get_to(v.estado);
}
inline void to_json(Json& j, const Auditoria& v) {
    j = {{"fecha",v.fecha},{"motivo",v.motivo},{"responsable",v.responsable}};
}
inline void from_json(const Json& j, Auditoria& v) {
    campos(j, {"fecha","motivo","responsable"});
    j.at("fecha").get_to(v.fecha); j.at("motivo").get_to(v.motivo); j.at("responsable").get_to(v.responsable);
}
inline void to_json(Json& j, const Cargo& v) {
    j = {{"idPrestamo",v.idPrestamo},{"idUsuario",v.idUsuario},{"importe",v.importe},{"moneda",v.moneda},{"estado",v.estado}};
}
inline void from_json(const Json& j, Cargo& v) {
    campos(j, {"idPrestamo","idUsuario","importe","moneda","estado"});
    if (!j.at("importe").is_number_integer()) throw std::invalid_argument("Importe entero requerido");
    j.at("idPrestamo").get_to(v.idPrestamo); j.at("idUsuario").get_to(v.idUsuario); j.at("importe").get_to(v.importe);
    j.at("moneda").get_to(v.moneda); j.at("estado").get_to(v.estado);
}
inline void to_json(Json& j, const Comprobante& v) {
    j = {{"idComprobante",v.idComprobante},{"fecha",v.fecha},{"idUsuario",v.idUsuario},{"idPrestamo",v.idPrestamo},
         {"importe",v.importe},{"moneda",v.moneda},{"etiqueta",v.etiqueta}};
}
inline void from_json(const Json& j, Comprobante& v) {
    campos(j, {"idComprobante","fecha","idUsuario","idPrestamo","importe","moneda","etiqueta"});
    if (!j.at("importe").is_number_integer()) throw std::invalid_argument("Importe entero requerido");
    j.at("idComprobante").get_to(v.idComprobante); j.at("fecha").get_to(v.fecha); j.at("idUsuario").get_to(v.idUsuario);
    j.at("idPrestamo").get_to(v.idPrestamo); j.at("importe").get_to(v.importe); j.at("moneda").get_to(v.moneda);
    j.at("etiqueta").get_to(v.etiqueta);
}
inline void to_json(Json& j, const Prestamo& v) {
    j = {{"idPrestamo",v.idPrestamo},{"idEjemplar",v.idEjemplar},{"idUsuario",v.idUsuario},
         {"inicio",v.inicio},{"vencimiento",v.vencimiento},{"devolucion",v.devolucion},{"estado",estadoPrestamo(v)},{"cancelacion",v.cancelacion}};
}
inline void from_json(const Json& j, Prestamo& v) {
    campos(j, {"idPrestamo","idEjemplar","idUsuario","inicio","vencimiento","devolucion","estado","cancelacion"});
    j.at("idPrestamo").get_to(v.idPrestamo); j.at("idEjemplar").get_to(v.idEjemplar); j.at("idUsuario").get_to(v.idUsuario);
    j.at("inicio").get_to(v.inicio); j.at("vencimiento").get_to(v.vencimiento); j.at("devolucion").get_to(v.devolucion); j.at("cancelacion").get_to(v.cancelacion);
    if (j.at("estado") != estadoPrestamo(v)) throw std::invalid_argument("Estado de préstamo incompatible");
}
inline void to_json(Json& j, const Reserva& v) {
    j = {{"idReserva",v.idReserva},{"idLibro",v.idLibro},{"idUsuario",v.idUsuario},{"llegada",v.llegada},
         {"estado",v.estado},{"idEjemplar",v.idEjemplar},{"creada",v.creada},{"cancelacion",v.cancelacion}};
}
inline void from_json(const Json& j, Reserva& v) {
    campos(j, {"idReserva","idLibro","idUsuario","llegada","estado","idEjemplar","creada","cancelacion"});
    if (!j.at("llegada").is_number_unsigned()) throw std::invalid_argument("Llegada debe ser un entero sin signo");
    j.at("idReserva").get_to(v.idReserva); j.at("idLibro").get_to(v.idLibro); j.at("idUsuario").get_to(v.idUsuario);
    j.at("llegada").get_to(v.llegada); j.at("estado").get_to(v.estado); j.at("idEjemplar").get_to(v.idEjemplar); j.at("creada").get_to(v.creada); j.at("cancelacion").get_to(v.cancelacion);
}
inline void to_json(Json& j, const Aviso& v) {
    j = {{"idAviso",v.idAviso},{"tipo",v.tipo},{"idUsuario",v.idUsuario},{"idReserva",v.idReserva},
         {"idPrestamo",v.idPrestamo},{"creado",v.creado},{"estado",v.estado}};
}
inline void from_json(const Json& j, Aviso& v) {
    campos(j, {"idAviso","tipo","idUsuario","idReserva","idPrestamo","creado","estado"});
    j.at("idAviso").get_to(v.idAviso); j.at("tipo").get_to(v.tipo); j.at("idUsuario").get_to(v.idUsuario);
    j.at("idReserva").get_to(v.idReserva); j.at("idPrestamo").get_to(v.idPrestamo); j.at("creado").get_to(v.creado); j.at("estado").get_to(v.estado);
}
inline void to_json(Json& j, const EstadoBiblioteca& v) {
    j = {{"version",2},{"usuarios",v.usuarios},{"ejemplares",v.ejemplares},{"prestamos",v.prestamos},
         {"reservas",v.reservas},{"avisos",v.avisos},{"siguienteLlegada",v.siguienteLlegada},{"cargos",v.cargos},{"comprobantes",v.comprobantes}};
}
inline void from_json(const Json& j, EstadoBiblioteca& v) {
    campos(j, {"version","usuarios","ejemplares","prestamos","reservas","avisos","siguienteLlegada","cargos","comprobantes"});
    if (j.at("version") != 2 || !j.at("siguienteLlegada").is_number_unsigned()) throw std::invalid_argument("Versión o secuencia inválida");
    for (const auto* coleccion : {"usuarios","ejemplares","prestamos","reservas","avisos","cargos","comprobantes"})
        if (!j.at(coleccion).is_array() || j.at(coleccion).size() > 10000) throw std::invalid_argument("Colección inválida o excesiva");
    j.at("usuarios").get_to(v.usuarios); j.at("ejemplares").get_to(v.ejemplares); j.at("prestamos").get_to(v.prestamos);
    j.at("reservas").get_to(v.reservas); j.at("avisos").get_to(v.avisos); j.at("siguienteLlegada").get_to(v.siguienteLlegada);
    j.at("cargos").get_to(v.cargos); j.at("comprobantes").get_to(v.comprobantes);
}
} // namespace biblioteca
