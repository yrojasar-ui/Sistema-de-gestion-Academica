#include "biblioteca/ConsoleUI.hpp"
#include "biblioteca/JsonBookRepository.hpp"
#ifdef BIBLIOTECA_MONGO
#include "biblioteca/MongoApiBookRepository.hpp"
#endif
#include <charconv>
#include <memory>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        std::filesystem::path initial = "data/libros_iniciales.json";
        std::filesystem::path working = "data/libros_trabajo.json";
        std::string storage = "json";
        std::string endpoint = "https://paginas-web-cr.com/Api/apis/mongodb.php";
        std::string collection = "biblioteca_yoel_ed2_libros";
        bool fileOptions = false, mongoOptions = false;
        std::size_t order = 4;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "Uso: biblioteca [--storage=json|--storage=mongo] [--order 3..128]\n"
                             "JSON (predeterminado): [--initial archivo] [--working archivo]\n"
                             "Mongo: [--endpoint URL_HTTPS] [--collection nombre]\n";
                return 0;
            }
            if (arg.rfind("--storage=", 0) == 0) {
                storage = arg.substr(10);
                if (storage != "json" && storage != "mongo") throw std::invalid_argument("Almacenamiento desconocido");
                continue;
            }
            if (++i == argc) throw std::invalid_argument("Falta valor de " + arg);
            const std::string value = argv[i];
            if (arg == "--initial") { initial = value; fileOptions = true; }
            else if (arg == "--working") { working = value; fileOptions = true; }
            else if (arg == "--endpoint") { endpoint = value; mongoOptions = true; }
            else if (arg == "--collection") { collection = value; mongoOptions = true; }
            else if (arg == "--order") {
                const auto result = std::from_chars(value.data(), value.data() + value.size(), order);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) throw std::invalid_argument("Orden inválido");
            } else throw std::invalid_argument("Argumento desconocido: " + arg);
        }
        std::unique_ptr<biblioteca::IBookRepository> repository;
        if (storage == "mongo") {
            if (fileOptions) throw std::invalid_argument("--working y --initial requieren --storage=json");
#ifdef BIBLIOTECA_MONGO
            repository = std::make_unique<biblioteca::MongoApiBookRepository>(endpoint, collection);
#else
            throw std::runtime_error("Este binario se compiló sin Mongo. Configure -DENABLE_MONGO=ON con libcurl.");
#endif
        } else {
            if (mongoOptions) throw std::invalid_argument("--endpoint y --collection requieren --storage=mongo");
            repository = std::make_unique<biblioteca::JsonBookRepository>(initial, working);
        }
        biblioteca::CatalogService service(*repository, order);
        if (service.writesThrough()) service.load(repository->workingPath());
        biblioteca::ConsoleUI ui(service, initial, std::cin, std::cout);
        return ui.run();
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
