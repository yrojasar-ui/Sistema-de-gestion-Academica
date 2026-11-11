#pragma once
#include <string>
#include <utility>
#include <vector>

namespace biblioteca {
struct Libro {
    std::string idLibro;
    std::string titulo;
    std::string autor;
    std::vector<std::string> temas;
};
using BookKey = std::pair<std::string, std::string>;
// UTF-8 estricto; política española limitada y determinista, no colación lingüística.
std::string normalizeTitle(const std::string& text);
void validateBook(const Libro& book);
BookKey keyOf(const Libro& book);
bool operator==(const Libro& a, const Libro& b);
} // namespace biblioteca
