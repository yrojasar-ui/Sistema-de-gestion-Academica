#include "PruebaEstructura.hpp"
#include "biblioteca/LibraryService.hpp"
#include "biblioteca/DomainJson.hpp"

using namespace biblioteca;
using namespace prueba;
namespace {
class Books : public IBookRepository {
public:
    mutable std::vector<Libro> registros{{"L1","Atlas","Autor",{"mapas"}}};
    bool fallar = false;
    std::vector<Libro> load(const std::filesystem::path&) const override { return registros; }
    void save(const std::vector<Libro>& filas) const override {
        if (fallar) throw std::runtime_error("Fallo de escritura inyectado");
        auto nuevos = filas;
        registros.swap(nuevos);
    }
    std::filesystem::path workingPath() const override { return "memoria-libros"; }
};
class Domain : public IDomainRepository {
public:
    bool fallar = false;
    MemoryDomainRepository memoria;
    EstadoBiblioteca load() const override { return memoria.load(); }
    void replace(const EstadoBiblioteca& e) override {
        if (fallar) throw std::runtime_error("Fallo de confirmación inyectado");
        memoria.replace(e);
    }
};
void probar(Caso& c) {
    Books books;
    CatalogService catalogo(books);
    catalogo.load(books.workingPath());
    Domain repositorio;
    LibraryService servicio(catalogo, repositorio);
    auto verificar = [&] {
        servicio.validar();
        c.exigir(Json(servicio.estado()) == Json(repositorio.load()), "Repositorio y servicio coinciden");
    };
    auto operar = [&](const auto& accion) {
        accion();
        ++c.operaciones;
        verificar();
    };
    auto rechazar = [&](const auto& accion) {
        const auto antes = Json(servicio.estado());
        const auto estructuras = servicio.snapshots();
        const auto registros = books.registros;
        bool rechazo = false;
        try { accion(); } catch (const std::exception&) { rechazo = true; }
        ++c.operaciones;
        c.exigir(rechazo, "Operación debe fallar");
        c.exigir(Json(servicio.estado()) == antes && servicio.snapshots() == estructuras && books.registros == registros,
                 "Rechazo conserva datos y topología de índices");
        verificar();
    };
    for (const auto& id : {"U1","U2","U3"}) operar([&] { servicio.crearUsuario({id, id, true}); });
    operar([&] { servicio.crearEjemplar("E1","L1"); });
    // Elegir año siguiente para que la prueba siga siendo válida al cambiar de día.
    const auto ano = std::stoi(ahoraUTC().substr(0,4)) + 1;
    const auto vence = std::to_string(ano) + "-12-01";
    operar([&] { servicio.prestar("P1","E1","U1",vence); });
    operar([&] { servicio.reservar("R1","L1","U2"); });
    operar([&] { servicio.reservar("R2","L1","U3"); });
    repositorio.fallar = true;
    rechazar([&] { servicio.devolver("P1"); });
    repositorio.fallar = false;
    operar([&] { servicio.devolver("P1"); });
    repositorio.fallar = true;
    rechazar([&] { servicio.cancelar("R1"); });
    repositorio.fallar = false;
    operar([&] { servicio.cancelar("R1"); });
    operar([&] { servicio.prestar("P2","E1","U3",vence); });
    repositorio.fallar = true;
    rechazar([&] { servicio.renovar("P2", std::to_string(ano+1)+"-01-01"); });
    repositorio.fallar = false;
    operar([&] { servicio.renovar("P2", std::to_string(ano+1)+"-01-01"); });
    books.fallar = true;
    rechazar([&] { servicio.editarLibro({"L1","Título nuevo","Autor",{"otro"}}); });
    rechazar([&] { servicio.crearLibro({"L2","Otro","Autor",{"mapas"}}); });
    books.fallar = false;
    operar([&] { servicio.editarLibro({"L1","Título nuevo","Autor",{"otro"}}); });
    c.exigir(catalogo.byTitle("Atlas").empty() && catalogo.byTitle("Título nuevo").size() == 1, "Edición cambia B+");
    auto invalido = servicio.estado();
    invalido.reservas[0].llegada = invalido.reservas[1].llegada;
    rechazar([&] { servicio.reconstruir(invalido); });
    invalido = servicio.estado();
    invalido.ejemplares[0].idLibro = "ausente";
    rechazar([&] { servicio.reconstruir(invalido); });
    invalido = servicio.estado();
    invalido.usuarios[2].activo = false;
    rechazar([&] { servicio.reconstruir(invalido); });
    // Carga de un repositorio nuevo reconstruye las mismas consultas y turnos.
    LibraryService recargado(catalogo, repositorio);
    recargado.validar();
    c.exigir(Json(recargado.estado()) == Json(servicio.estado()), "Recarga de repositorio");
    c.exigir(Json(recargado.activos()) == Json(servicio.activos()), "Índice tras recarga");
    for (const auto& fecha : {"2024-02-29","2000-02-29","2026-11-24"}) {
        validarFecha(fecha);
        ++c.operaciones;
    }
    for (const auto& fecha : {"1900-02-29","2026-02-29","2026-13-01","0000-01-01","2026-01-00","2026-1-01"})
        rechazar([&] { validarFecha(fecha); });
    rechazar([&] { validarInstante("2026-11-24T24:00:00Z"); });
    rechazar([&] { validarId("../archivo"); });
    std::cout << "Commit de dominio y catálogo, fallos de repositorio, reindexación, reconstrucción y calendario OK\n";
}
}
int main() { return ejecutar(20261114, probar); }
