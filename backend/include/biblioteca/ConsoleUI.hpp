#pragma once
#include "biblioteca/CatalogService.hpp"
#include <istream>

namespace biblioteca {
class ConsoleUI {
public:
    ConsoleUI(CatalogService& service, std::filesystem::path initial, std::istream& input, std::ostream& output);
    int run();
private:
    CatalogService& service_;
    std::filesystem::path initial_;
    std::istream& input_;
    std::ostream& output_;
    std::string ask(const std::string& label);
    void print(const std::vector<Libro>& books);
};
} // namespace biblioteca
