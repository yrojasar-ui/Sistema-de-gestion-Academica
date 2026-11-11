#include "biblioteca/ConsoleUI.hpp"
#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace biblioteca {
ConsoleUI::ConsoleUI(CatalogService& service, std::filesystem::path initial, std::istream& input, std::ostream& output)
    : service_(service), initial_(std::move(initial)), input_(input), output_(output) {}
std::string ConsoleUI::ask(const std::string& label) {
    output_ << label << ": ";
    std::string line;
    if (!std::getline(input_, line)) throw std::runtime_error("Fin de entrada");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}
void ConsoleUI::print(const std::vector<Libro>& books) {
    output_ << "Resultados: " << books.size() << '\n';
    for (const auto& b : books) {
        output_ << "[" << b.idLibro << "] " << b.titulo << " | " << b.autor << " | temas:";
        for (const auto& tema : b.temas) output_ << " " << tema;
        output_ << '\n';
    }
}
int ConsoleUI::run() {
    output_ << "Biblioteca académica — E2-A (JSON local, guardado explícito)\n";
    while (input_) {
        try {
            output_ << "\n1 Cargar  2 Insertar  3 Buscar título  4 Borrar selección\n"
                       "5 Listar  6 Visualizar B+  7 Validar  8 Guardar  9 Prefijo  0 Salir\n";
            const auto option = ask("Opción");
            if (option == "1") {
                const auto path = ask("Origen: 1 inicial, 2 trabajo, o ruta JSON");
                if (service_.dirty() && ask("Reemplazar cambios sin guardar (SI)") != "SI") continue;
                const auto source = path == "1" ? initial_ : path == "2" ? service_.workingPath() : std::filesystem::path(path);
                const auto count = service_.load(source);
                output_ << "Cargados: " << count << " | origen: " << source.string() << '\n';
            } else if (option == "2") {
                Libro book;
                book.idLibro = ask("ID"); book.titulo = ask("Título"); book.autor = ask("Autor");
                std::istringstream themes(ask("Temas separados por ; (vacío permitido)"));
                std::string theme;
                while (std::getline(themes, theme, ';')) book.temas.push_back(theme);
                service_.insert(book); output_ << "Insertado: " << book.idLibro << " (sin guardar)\n";
            } else if (option == "3") {
                print(service_.byTitle(ask("Título exacto")));
            } else if (option == "4") {
                const auto found = service_.byTitle(ask("Título de la selección")); print(found);
                if (found.empty()) continue;
                const auto id = ask("ID exacto a borrar (vacío cancela)");
                if (id.empty()) continue;
                if (std::none_of(found.begin(), found.end(), [&](const Libro& b) { return b.idLibro == id; }))
                    throw std::invalid_argument("ID no pertenece a la selección mostrada");
                if (ask("Confirmar borrado de " + id + " (SI)") != "SI") continue;
                if (!service_.erase(id)) throw std::logic_error("Selección ya no existe");
                output_ << "Borrado: " << id << " (sin guardar)\n";
            } else if (option == "5") print(service_.all());
            else if (option == "6") service_.visualize(output_);
            else if (option == "7") {
                std::string reason;
                const bool valid = service_.validate(reason);
                output_ << "Invariantes " << (valid ? "OK: " : "FALLO: ") << reason << '\n';
                if (!valid) return 2;
            } else if (option == "8") {
                service_.save(); output_ << "Guardado: " << service_.workingPath().string() << '\n';
            } else if (option == "9") print(service_.byPrefix(ask("Prefijo")));
            else if (option == "0") {
                if (service_.dirty()) {
                    const auto answer = ask("Cambios pendientes: g guardar, d descartar, otro cancelar");
                    if (answer == "g") { service_.save(); output_ << "Guardado: " << service_.workingPath().string() << '\n'; }
                    else if (answer != "d") continue;
                }
                return 0;
            } else output_ << "Opción inválida\n";
        } catch (const std::exception& e) {
            output_ << "Error: " << e.what() << '\n';
            if (!input_) break;
        }
    }
    if (service_.dirty()) { output_ << "Fin de entrada con cambios SIN GUARDAR\n"; return 2; }
    return 0;
}
} // namespace biblioteca
