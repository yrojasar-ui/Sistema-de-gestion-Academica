#pragma once
#include "biblioteca/DomainJson.hpp"
namespace biblioteca {
// Solo se llama desde migración explícita. No inventa cargos retroactivos.
inline Json migrarDominioV1(Json j) {
    campos(j, {"version","usuarios","ejemplares","prestamos","reservas","avisos","siguienteLlegada"});
    if (j.at("version") != 1) throw std::invalid_argument("Migración requiere dominio versión 1");
    for (auto& p : j.at("prestamos")) {
        campos(p, {"idPrestamo","idEjemplar","idUsuario","inicio","vencimiento","devolucion"});
        p["estado"] = p.at("devolucion") == "" ? "activo" : "devuelto";
        p["cancelacion"] = Auditoria{};
    }
    for (auto& r : j.at("reservas")) {
        campos(r, {"idReserva","idLibro","idUsuario","llegada","estado","idEjemplar","creada"});
        r["cancelacion"] = r.at("estado") == "cancelada" ?
            Auditoria{r.at("creada"), "Migración v1: fecha y responsable originales no registrados", "migracion"} : Auditoria{};
    }
    for (auto& a : j.at("avisos")) {
        campos(a, {"idAviso","tipo","idUsuario","idReserva","idPrestamo","creado","estado"});
        a["idEvento"] = "EV-" + a.at("idAviso").get<std::string>();
        a["publicadoEn"] = "";
        a["procesadoEn"] = "";
        a["intentos"] = std::uint32_t{0};
        a["ultimoError"] = "";
    }
    j["version"] = 2; j["notificaciones"] = Json::array(); j["cargos"] = Json::array(); j["comprobantes"] = Json::array();
    (void)j.get<EstadoBiblioteca>();
    return j;
}
}
