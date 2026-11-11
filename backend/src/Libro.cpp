#include "biblioteca/Libro.hpp"
#include <cstdint>
#include <stdexcept>

namespace biblioteca {
namespace {
std::vector<std::uint32_t> decode(const std::string& s) {
    std::vector<std::uint32_t> out;
    for (std::size_t i = 0; i < s.size();) {
        const auto lead = static_cast<unsigned char>(s[i++]);
        std::uint32_t cp = 0, minimum = 0;
        unsigned rest = 0;
        if (lead < 0x80) cp = lead;
        else if (lead >= 0xC2 && lead <= 0xDF) { cp = lead & 0x1F; rest = 1; minimum = 0x80; }
        else if (lead >= 0xE0 && lead <= 0xEF) { cp = lead & 0x0F; rest = 2; minimum = 0x800; }
        else if (lead >= 0xF0 && lead <= 0xF4) { cp = lead & 7; rest = 3; minimum = 0x10000; }
        else throw std::invalid_argument("UTF-8 inválido");
        while (rest--) {
            if (i == s.size()) throw std::invalid_argument("UTF-8 truncado");
            const auto c = static_cast<unsigned char>(s[i++]);
            if ((c & 0xC0) != 0x80) throw std::invalid_argument("UTF-8 inválido");
            cp = (cp << 6) | (c & 0x3F);
        }
        if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            throw std::invalid_argument("Punto Unicode inválido");
        if ((cp < 0x20 && cp != 9 && cp != 10 && cp != 13) || cp == 0x7F)
            throw std::invalid_argument("Carácter de control no admitido");
        out.push_back(cp);
    }
    return out;
}
void encode(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 63));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 63));
        out += static_cast<char>(0x80 | (cp & 63));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 63));
        out += static_cast<char>(0x80 | ((cp >> 6) & 63));
        out += static_cast<char>(0x80 | (cp & 63));
    }
}
}
std::string normalizeTitle(const std::string& text) {
    auto cps = decode(text);
    std::vector<std::uint32_t> normalized;
    for (auto cp : cps) {
        if (cp >= 'A' && cp <= 'Z') cp += 32;
        switch (cp) {
            case 0xC1: case 0xE1: cp = 'a'; break;
            case 0xC9: case 0xE9: cp = 'e'; break;
            case 0xCD: case 0xED: cp = 'i'; break;
            case 0xD3: case 0xF3: cp = 'o'; break;
            case 0xDA: case 0xFA: case 0xDC: case 0xFC: cp = 'u'; break;
            case 0xD1: cp = 0xF1; break;
            default: break;
        }
        // Equivalencias españolas descompuestas: vocal+agudo, u+diéresis, n+tilde.
        if (!normalized.empty()) {
            auto& previous = normalized.back();
            if (cp == 0x301 && (previous == 'a' || previous == 'e' || previous == 'i' || previous == 'o' || previous == 'u')) continue;
            if (cp == 0x308 && previous == 'u') continue;
            if (cp == 0x303 && previous == 'n') { previous = 0xF1; continue; }
        }
        if (cp == ' ' || cp == '\t' || cp == '\r' || cp == '\n' || cp == 0xA0) {
            if (!normalized.empty() && normalized.back() != ' ') normalized.push_back(' ');
        } else normalized.push_back(cp);
    }
    if (!normalized.empty() && normalized.back() == ' ') normalized.pop_back();
    std::string out;
    for (auto cp : normalized) encode(out, cp);
    return out;
}
void validateBook(const Libro& book) {
    if (book.idLibro.empty() || book.idLibro.size() > 64) throw std::invalid_argument("ID: entre 1 y 64 caracteres ASCII");
    for (unsigned char c : book.idLibro)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            throw std::invalid_argument("ID solo admite letras ASCII, números, - y _");
    if (book.titulo.size() > 512 || normalizeTitle(book.titulo).empty()) throw std::invalid_argument("Título vacío o mayor de 512 bytes");
    if (book.autor.size() > 256 || normalizeTitle(book.autor).empty()) throw std::invalid_argument("Autor vacío o mayor de 256 bytes");
    if (book.temas.size() > 32) throw std::invalid_argument("Máximo 32 temas");
    for (const auto& tema : book.temas)
        if (tema.size() > 128 || normalizeTitle(tema).empty()) throw std::invalid_argument("Tema vacío o mayor de 128 bytes");
}
BookKey keyOf(const Libro& book) { return {normalizeTitle(book.titulo), book.idLibro}; }
bool operator==(const Libro& a, const Libro& b) {
    return a.idLibro == b.idLibro && a.titulo == b.titulo && a.autor == b.autor && a.temas == b.temas;
}
} // namespace biblioteca
