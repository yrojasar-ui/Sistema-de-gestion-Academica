#pragma once
#include "biblioteca/CatalogService.hpp"
#include "biblioteca/ColaReservas.hpp"
#include "biblioteca/GrafoTemas.hpp"
#include "biblioteca/IDomainRepository.hpp"
#include "biblioteca/PrestamosActivos.hpp"
#include <map>

namespace biblioteca {
class LibraryService {
public:
    LibraryService(CatalogService& catalogo, IDomainRepository& repositorio, bool deferLoad = false);
    void swapState(LibraryService& other) noexcept { actual_.swap(other.actual_); }
    const IDomainRepository& repository() const { return repositorio_; }
    const EstadoBiblioteca& estado() const { return actual_->datos; }
    Usuario usuario(const std::string& id) const;
    void crearUsuario(const Usuario& usuario);
    void editarUsuario(const Usuario& usuario, const std::string& responsable = "servicio");
    void borrarUsuario(const std::string& id);
    void crearEjemplar(const std::string& id, const std::string& libro);
    void estadoEjemplar(const std::string& id, const std::string& estado);
    void borrarEjemplar(const std::string& id);
    void prestar(const std::string& id, const std::string& ejemplar, const std::string& usuario, const std::string& vence);
    void devolver(const std::string& id);
    void renovar(const std::string& id, const std::string& vence);
    void reservar(const std::string& id, const std::string& libro, const std::string& usuario);
    void cancelar(const std::string& id, const std::string& motivo = "Cancelación solicitada", const std::string& responsable = "servicio");
    void anular(const std::string& id, const std::string& motivo, const std::string& responsable, bool copiaRecuperada);
    Comprobante pagar(const std::string& id);
    void configurarCargo(std::int64_t importe, const std::string& moneda);
    void crearLibro(const Libro& libro);
    void editarLibro(const Libro& libro);
    void borrarLibro(const std::string& id);
    std::vector<Prestamo> activos(const std::string& hasta = "") const;
    std::vector<Libro> relacionados(const std::string& id) const;
    void reconstruir(const EstadoBiblioteca& estado);
    void validar() const;
    std::string snapshots() const;
private:
    struct Runtime {
        EstadoBiblioteca datos;
        IndiceUsuarios usuarios;
        PrestamosActivos activos;
        GrafoTemas grafo;
        std::map<std::string, ColaReservas> colas;
    };
    std::int64_t importeDemo_ = 10000;
    std::string monedaDemo_ = "CRC";
    CatalogService& catalogo_;
    IDomainRepository& repositorio_;
    std::unique_ptr<Runtime> actual_;
    std::unique_ptr<Runtime> construir(const EstadoBiblioteca& estado) const;
    void cambiar(const std::function<void(Runtime&)>& operacion);
    void validar(const Runtime& estado) const;
    static Usuario usuario(const Runtime& estado, const std::string& id);
    static void asignar(Runtime& estado, const std::string& libro);
    static void aviso(Runtime& estado, const std::string& tipo, const std::string& usuario,
                      const std::string& reserva, const std::string& prestamo);
    static void liberar(Runtime& estado, Reserva& reserva);
};
} // namespace biblioteca
