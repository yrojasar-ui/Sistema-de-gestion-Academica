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
inline void to_json(Json& j, const Prestamo& v) {
    j = {{"idPrestamo",v.idPrestamo},{"idEjemplar",v.idEjemplar},{"idUsuario",v.idUsuario},
         {"inicio",v.inicio},{"vencimiento",v.vencimiento},{"devolucion",v.devolucion}};
}
inline void from_json(const Json& j, Prestamo& v) {
    campos(j, {"idPrestamo","idEjemplar","idUsuario","inicio","vencimiento","devolucion"});
    j.at("idPrestamo").get_to(v.idPrestamo); j.at("idEjemplar").get_to(v.idEjemplar); j.at("idUsuario").get_to(v.idUsuario);
    j.at("inicio").get_to(v.inicio); j.at("vencimiento").get_to(v.vencimiento); j.at("devolucion").get_to(v.devolucion);
}
inline void to_json(Json& j, const Reserva& v) {
    j = {{"idReserva",v.idReserva},{"idLibro",v.idLibro},{"idUsuario",v.idUsuario},{"llegada",v.llegada},
         {"estado",v.estado},{"idEjemplar",v.idEjemplar},{"creada",v.creada}};
}
inline void from_json(const Json& j, Reserva& v) {
    campos(j, {"idReserva","idLibro","idUsuario","llegada","estado","idEjemplar","creada"});
    if (!j.at("llegada").is_number_unsigned()) throw std::invalid_argument("Llegada debe ser un entero sin signo");
    j.at("idReserva").get_to(v.idReserva); j.at("idLibro").get_to(v.idLibro); j.at("idUsuario").get_to(v.idUsuario);
    j.at("llegada").get_to(v.llegada); j.at("estado").get_to(v.estado); j.at("idEjemplar").get_to(v.idEjemplar); j.at("creada").get_to(v.creada);
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
    j = {{"version",1},{"usuarios",v.usuarios},{"ejemplares",v.ejemplares},{"prestamos",v.prestamos},
         {"reservas",v.reservas},{"avisos",v.avisos},{"siguienteLlegada",v.siguienteLlegada}};
}
inline void from_json(const Json& j, EstadoBiblioteca& v) {
    campos(j, {"version","usuarios","ejemplares","prestamos","reservas","avisos","siguienteLlegada"});
    if (j.at("version") != 1 || !j.at("siguienteLlegada").is_number_unsigned()) throw std::invalid_argument("Versión o secuencia inválida");
    for (const auto* coleccion : {"usuarios","ejemplares","prestamos","reservas","avisos"})
        if (!j.at(coleccion).is_array() || j.at(coleccion).size() > 10000) throw std::invalid_argument("Colección inválida o excesiva");
    j.at("usuarios").get_to(v.usuarios); j.at("ejemplares").get_to(v.ejemplares); j.at("prestamos").get_to(v.prestamos);
    j.at("reservas").get_to(v.reservas); j.at("avisos").get_to(v.avisos); j.at("siguienteLlegada").get_to(v.siguienteLlegada);
}
} // namespace biblioteca
