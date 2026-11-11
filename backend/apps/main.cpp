#include "biblioteca/ConsoleUI.hpp"
#include "biblioteca/JsonBookRepository.hpp"
#include <charconv>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        std::filesystem::path initial = "data/libros_iniciales.json";
        std::filesystem::path working = "data/libros_trabajo.json";
        std::size_t order = 4;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "Uso: biblioteca [--initial archivo] [--working archivo] [--order 3..128]\n";
                return 0;
            }
            if (++i == argc) throw std::invalid_argument("Falta valor de " + arg);
            const std::string value = argv[i];
            if (arg == "--initial") initial = value;
            else if (arg == "--working") working = value;
            else if (arg == "--order") {
                const auto result = std::from_chars(value.data(), value.data() + value.size(), order);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) throw std::invalid_argument("Orden inválido");
            } else throw std::invalid_argument("Argumento desconocido: " + arg);
        }
        biblioteca::JsonBookRepository repository(initial, working);
        biblioteca::CatalogService service(repository, order);
        biblioteca::ConsoleUI ui(service, initial, std::cin, std::cout);
        return ui.run();
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
