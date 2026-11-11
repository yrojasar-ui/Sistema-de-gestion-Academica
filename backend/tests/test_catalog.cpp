#include "biblioteca/ConsoleUI.hpp"
#include "biblioteca/JsonBookRepository.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>

using namespace biblioteca;
namespace fs = std::filesystem;
namespace {
std::size_t checks = 0;
void require(bool ok, const std::string& why) { ++checks; if (!ok) throw std::runtime_error(why); }
template<class F> void rejects(F function, const std::string& why) {
    bool failed = false;
    try { function(); } catch (const std::exception&) { failed = true; }
    require(failed, why);
}
struct TempDirectory {
    fs::path path;
    TempDirectory() {
        std::string pattern = (fs::temp_directory_path() / "biblioteca-tests-XXXXXX").string();
        std::vector<char> buffer(pattern.begin(), pattern.end()); buffer.push_back(0);
        if (!::mkdtemp(buffer.data())) throw std::runtime_error("mkdtemp falló");
        path = buffer.data();
    }
    ~TempDirectory() { std::error_code ec; fs::remove_all(path, ec); }
};
void write(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary); out << text;
    if (!out) throw std::runtime_error("No se pudo preparar fixture de prueba");
}
std::string read(const fs::path& path) { std::ifstream in(path, std::ios::binary); return {std::istreambuf_iterator<char>(in), {}}; }
void valid(const CatalogService& service) { std::string why; require(service.validate(why), why); }
class FakeRepository final : public IBookRepository {
public:
    mutable unsigned loads = 0, saves = 0;
    bool fail = false;
    bool failPath = false;
    std::vector<Libro> books;
    fs::path target;
    explicit FakeRepository(fs::path p) : target(std::move(p)) {}
    std::vector<Libro> load(const fs::path&) const override { ++loads; return books; }
    void save(const std::vector<Libro>&) const override { ++saves; if (fail) throw std::runtime_error("Fallo inyectado"); }
    fs::path workingPath() const override {
        if (failPath) throw std::runtime_error("Fallo de ruta inyectado");
        return target;
    }
};
}
int main() {
    try {
        TempDirectory temp;
        const fs::path initial = temp.path / "initial.json", working = temp.path / "working.json";
        fs::copy_file(fs::path(SOURCE_DIR) / "data/libros_iniciales.json", initial);
        const auto initialBytes = read(initial);
        JsonBookRepository repository(initial, working);
        CatalogService service(repository, 3);
        require(service.all().empty(), "Servicio inicialmente vacío"); valid(service);
        require(service.load(initial) == 16, "Cantidad del fixture"); valid(service);
        require(service.byTitle("PROGRAMACION MODULAR").size() == 2, "Mismo título e IDs distintos");
        const auto baseline = service.all();
        rejects([&] { service.insert({"L001", "Otro título", "Autor", {}}); }, "ID duplicado global");
        require(service.all() == baseline, "Duplicado no debe cambiar catálogo"); valid(service);
        rejects([&] { service.load(temp.path / "missing.json"); }, "Archivo ausente");
        rejects([&] { service.load(temp.path); }, "Directorio no es JSON");
        const std::string one = R"({"idLibro":"X","titulo":"Título","autor":"Autor","temas":[]})";
        const std::vector<std::string> malformed{
            "", "{}", "null", "[", "[" + one + ",]", "[" + one + "] basura", "[" + one + "," + one + "]",
            "[" + one + ",{}]", R"([{"idLibro":"A","titulo":3,"autor":"B","temas":[]}])",
            R"([{"idLibro":"A","titulo":" ","autor":"B","temas":[]}])",
            R"([{"idLibro":"A","titulo":"T","autor":"B","temas":[2]}])",
            R"([{"idLibro":"A","titulo":"T","autor":"B","temas":[],"extra":1}])",
            R"([{"idLibro":"A","titulo":"T","titulo":"U","autor":"B","temas":[]}])",
            R"([{"idLibro":"A","titulo":"T","autor":"","temas":[]}])",
            R"([{"idLibro":"a b","titulo":"T","autor":"B","temas":[]}])",
            R"([{"idLibro":"A","titulo":"T","autor":"B","temas":[""]}])",
            std::string(40, '[') + std::string(40, ']'),
            "[{\"idLibro\":\"A\",\"titulo\":\"" + std::string("\xC0\xAF") + "\",\"autor\":\"B\",\"temas\":[]}]"
        };
        for (const auto& text : malformed) {
            write(temp.path / "bad.json", text);
            rejects([&] { service.load(temp.path / "bad.json"); }, "JSON/campos inválidos aceptados");
            require(service.all() == baseline, "Carga rechazada debe conservar estado completo"); valid(service);
        }
        write(temp.path / "large.json", std::string(16 * 1024 * 1024 + 1, ' '));
        rejects([&] { service.load(temp.path / "large.json"); }, "Archivo demasiado grande");
        service.insert({"NUEVO", "Árbol nuevo", "Autor", {"prueba"}}); valid(service);
        require(service.byTitle("ARBOL NUEVO").size() == 1, "Buscar inserción");
        require(service.erase("L011"), "Borrar ID seleccionado"); valid(service);
        require(service.byTitle("Programación modular").front().idLibro == "L012", "Conservar homónimo");
        require(!service.erase("no-existe"), "Borrado ausente"); valid(service);
        service.save(); require(!service.dirty(), "Guardado limpia bandera");
        const auto saved = service.all();
        const auto savedBytes = read(working);
        rejects([&] { repository.save({saved.front(), saved.front()}); }, "Guardado rechaza IDs duplicados");
        require(read(working) == savedBytes, "Guardado inválido conserva archivo anterior");
        JsonBookRepository repo2(initial, working); CatalogService restarted(repo2, 5);
        require(restarted.load(working) == saved.size(), "Recarga cantidad");
        require(restarted.all() == saved && !restarted.dirty(), "Reinicio conserva cambios"); valid(restarted);
        require(read(initial) == initialBytes, "Fixture inicial intacto");
        rejects([&] { JsonBookRepository wrong(initial, initial); }, "Protección fixture mismo path");
        fs::create_symlink(initial, temp.path / "link.json");
        rejects([&] { JsonBookRepository wrong(initial, temp.path / "link.json"); }, "Protección enlace simbólico");
        fs::create_hard_link(initial, temp.path / "hard.json");
        rejects([&] { JsonBookRepository wrong(initial, temp.path / "hard.json"); }, "Protección enlace duro");
        JsonBookRepository noParent(initial, temp.path / "missing" / "work.json");
        CatalogService failing(noParent); failing.insert({"A", "Título", "Autor", {}});
        rejects([&] { failing.save(); }, "Guardado debe informar directorio ausente");
        require(failing.dirty() && failing.all().size() == 1, "Fallo conserva cambios en memoria"); valid(failing);
        fs::create_directory(temp.path / "missing"); failing.save(); require(!failing.dirty(), "Reintento de guardado");
        fs::create_directory(temp.path / "dest-directory"); write(temp.path / "dest-directory" / "keep", "conservar");
        JsonBookRepository renameFailure(initial, temp.path / "dest-directory");
        rejects([&] { renameFailure.save(saved); }, "Fallo rename esperado");
        require(read(temp.path / "dest-directory" / "keep") == "conservar", "No tocar destino ajeno");
        for (const auto& entry : fs::directory_iterator(temp.path))
            require(entry.path().filename().string().find(".tmp.") == std::string::npos, "Temporal no limpiado");
        // Consultas/borrado trabajan con el B+ y no piden al repositorio buscar.
        FakeRepository fake(temp.path / "fake.json"); fake.books = saved;
        CatalogService isolated(fake); isolated.load(initial); valid(isolated);
        isolated.byTitle("Árbol nuevo"); isolated.byPrefix("Pro"); isolated.all(); isolated.erase("NUEVO"); valid(isolated);
        require(fake.loads == 1 && fake.saves == 0, "Consulta no debe acceder al repositorio");
        fake.fail = true; rejects([&] { isolated.save(); }, "Fallo inyectado"); require(isolated.dirty(), "Dirty tras fallo inyectado");
        fake.books.push_back(fake.books.front());
        const auto beforeBadAdapter = isolated.all();
        rejects([&] { isolated.load(initial); }, "Servicio también valida IDs de adaptador");
        require(isolated.all() == beforeBadAdapter, "Carga transaccional del servicio"); valid(isolated);
        fake.books = saved; fake.failPath = true;
        rejects([&] { isolated.load(initial); }, "Fallo de procedencia del adaptador");
        require(isolated.all() == beforeBadAdapter && isolated.dirty(), "Fallo de ruta conserva estado y dirty"); valid(isolated);
        fake.failPath = false; fake.books.resize(10001, saved.front());
        rejects([&] { isolated.load(initial); }, "Servicio limita cantidad del adaptador");
        require(isolated.all() == beforeBadAdapter, "Exceso de registros no cambia catálogo"); valid(isolated);
        // Borrado inequívoco: el ID de otro título no puede usarse en la selección.
        std::istringstream input("4\nProgramación modular\nL001\n4\nProgramación modular\nL012\nSI\n3\nProgramación modular\n7\n8\n0\n");
        std::ostringstream output; ConsoleUI ui(restarted, initial, input, output);
        require(ui.run() == 0, "Consola termina correctamente");
        require(output.str().find("ID no pertenece") != std::string::npos, "Selección debe rechazar otro ID");
        require(output.str().find("Borrado: L012") != std::string::npos, "Selección correcta");
        require(output.str().find("Resultados: 0") != std::string::npos, "Buscar tras borrar"); valid(restarted);
        // EOF nunca afirma guardado si quedan cambios pendientes.
        std::istringstream eofInput("2\nEOF\nLibro sin guardar\nAutor\n\n"); std::ostringstream eofOutput;
        ConsoleUI eof(restarted, initial, eofInput, eofOutput);
        require(eof.run() == 2 && restarted.dirty(), "EOF con cambios se informa"); valid(restarted);
        write(temp.path / "empty.json", "[]");
        require(service.load(temp.path / "empty.json") == 0 && service.all().empty(), "Carga vacía válida"); valid(service);
        service.insert({"RE", "Reinsertar", "Autor", {}}); valid(service); service.save();
        require(read(initial) == initialBytes, "Fixture intacto al final");
        std::cout << "OK catálogo/JSON/consola; casos JSON inválido=" << malformed.size() << "; comprobaciones=" << checks << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << "FALLO: " << e.what() << '\n'; return 1; }
}
