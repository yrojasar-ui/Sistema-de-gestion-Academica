#include "biblioteca/Dominio.hpp"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace biblioteca {
void validarId(const std::string& id) {
    const bool valido = !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
    });
    if (!valido) throw std::invalid_argument("ID de 1 a 64 caracteres ASCII, letras, números, guion o guion bajo");
}
void validarFecha(const std::string& fecha) {
    if (fecha.size() != 10 || fecha[4] != '-' || fecha[7] != '-') throw std::invalid_argument("Fecha AAAA-MM-DD requerida");
    for (std::size_t i = 0; i < fecha.size(); ++i)
        if (i != 4 && i != 7 && (fecha[i] < '0' || fecha[i] > '9')) throw std::invalid_argument("Fecha inválida");
    const auto ano = std::stoi(fecha.substr(0,4)), mes = std::stoi(fecha.substr(5,2)), dia = std::stoi(fecha.substr(8,2));
    if (ano < 1 || mes < 1 || mes > 12) throw std::invalid_argument("Fecha fuera de calendario");
    int dias[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (ano%400 == 0 || (ano%4 == 0 && ano%100 != 0)) dias[1] = 29;
    if (dia < 1 || dia > dias[mes-1]) throw std::invalid_argument("Día fuera de calendario");
}
void validarInstante(const std::string& instante) {
    if (instante.size() != 20 || instante[10] != 'T' || instante[13] != ':' || instante[16] != ':' || instante[19] != 'Z')
        throw std::invalid_argument("Instante UTC AAAA-MM-DDTHH:MM:SSZ requerido");
    validarFecha(instante.substr(0,10));
    for (auto i : {11,12,14,15,17,18})
        if (instante[static_cast<std::size_t>(i)] < '0' || instante[static_cast<std::size_t>(i)] > '9')
            throw std::invalid_argument("Hora inválida");
    if (std::stoi(instante.substr(11,2)) > 23 || std::stoi(instante.substr(14,2)) > 59 || std::stoi(instante.substr(17,2)) > 59)
        throw std::invalid_argument("Hora fuera de rango");
}
std::string ahoraUTC() {
    const auto tiempo = std::time(nullptr);
    std::tm utc{};
    gmtime_r(&tiempo, &utc);
    std::ostringstream salida;
    salida << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return salida.str();
}
bool pendiente(const Reserva& reserva) { return reserva.estado == "pendiente" || reserva.estado == "apartada"; }
} // namespace biblioteca
