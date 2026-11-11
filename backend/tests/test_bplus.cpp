#include "biblioteca/BPlusTree.hpp"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>

using namespace biblioteca;
namespace {
std::size_t operations = 0, checks = 0;
BPlusTree::Stats totals;
void require(bool ok, const std::string& why) {
    ++checks;
    if (!ok) throw std::runtime_error(why + " operación=" + std::to_string(operations));
}
std::string number(unsigned n) { std::ostringstream out; out << std::setw(4) << std::setfill('0') << n; return out.str(); }
Libro book(unsigned id) { return {"ID" + number(id), "titulo " + number(id % 23), "Autor de prueba", {"sintético"}}; }
BookKey oracleKey(unsigned id) { return {"titulo " + number(id % 23), "ID" + number(id)}; }
void verify(const BPlusTree& tree, const std::map<BookKey, Libro>& oracle) {
    std::string reason;
    require(tree.validate(reason), reason);
    require(tree.size() == oracle.size(), "Tamaño distinto al oráculo");
    std::vector<Libro> expected;
    for (const auto& [key, value] : oracle) {
        expected.push_back(value);
        require(tree.find(key) == std::optional<Libro>(value), "Búsqueda difiere del oráculo");
    }
    require(tree.all() == expected, "Listado/enlaces difiere de std::map");
    require(!tree.find({"ausente", "no"}), "Clave ausente encontrada");
    std::vector<Libro> title, prefix, range;
    const BookKey low{"titulo 0005", ""}, high{"titulo 0011", "ID9999"};
    for (const auto& [key, value] : oracle) {
        if (key.first == "titulo 0007") title.push_back(value);
        if (key.first.compare(0, 10, "titulo 000") == 0) prefix.push_back(value);
        if (key >= low && key <= high) range.push_back(value);
    }
    require(tree.byTitle("TÍTULO 0007") == title, "Título duplicado distribuido entre hojas");
    require(tree.byPrefix("titulo 000") == prefix, "Prefijo difiere del oráculo");
    require(tree.range(low, high) == range, "Rango inclusivo difiere del oráculo");
    require(tree.range(high, low).empty(), "Rango invertido no vacío");
}
void insert(BPlusTree& tree, std::map<BookKey, Libro>& oracle, unsigned id) {
    ++operations;
    const bool expected = oracle.emplace(oracleKey(id), book(id)).second;
    require(tree.insert(book(id)) == expected, "Resultado de inserción");
    verify(tree, oracle);
}
void erase(BPlusTree& tree, std::map<BookKey, Libro>& oracle, unsigned id) {
    ++operations;
    const bool expected = oracle.erase(oracleKey(id)) != 0;
    require(tree.erase(oracleKey(id)) == expected, "Resultado de borrado");
    verify(tree, oracle);
}
void collect(const BPlusTree& tree) {
    const auto& s = tree.stats();
    totals.leafSplits += s.leafSplits; totals.internalSplits += s.internalSplits;
    totals.leafBorrowLeft += s.leafBorrowLeft; totals.leafBorrowRight += s.leafBorrowRight;
    totals.internalBorrowLeft += s.internalBorrowLeft; totals.internalBorrowRight += s.internalBorrowRight;
    totals.leafMerges += s.leafMerges; totals.internalMerges += s.internalMerges; totals.rootShrinks += s.rootShrinks;
}
void sequences(std::size_t order, std::mt19937& rng) {
    for (unsigned mode = 0; mode < 3; ++mode) {
        BPlusTree tree(order); std::map<BookKey, Libro> oracle;
        verify(tree, oracle); erase(tree, oracle, 999);
        std::vector<unsigned> ids(120); std::iota(ids.begin(), ids.end(), 0);
        std::sort(ids.begin(), ids.end(), [](unsigned a, unsigned b) { return oracleKey(a) < oracleKey(b); });
        if (mode == 1) std::reverse(ids.begin(), ids.end());
        if (mode == 2) std::shuffle(ids.begin(), ids.end(), rng);
        for (auto id : ids) insert(tree, oracle, id);
        insert(tree, oracle, ids.front()); // misma clave exacta rechazada
        if (mode == 2) std::shuffle(ids.begin(), ids.end(), rng);
        for (auto id : ids) erase(tree, oracle, id);
        erase(tree, oracle, ids.front());
        insert(tree, oracle, 901); erase(tree, oracle, 901);
        collect(tree);
    }
    BPlusTree tree(order); std::map<BookKey, Libro> oracle;
    for (unsigned i = 0; i < 1800; ++i) {
        const auto id = static_cast<unsigned>(rng() % 240);
        const auto choice = rng() % 3;
        if (choice == 0) insert(tree, oracle, id);
        else if (choice == 1) erase(tree, oracle, id);
        else {
            ++operations;
            const auto it = oracle.find(oracleKey(id));
            const auto expected = it == oracle.end() ? std::optional<Libro>{} : std::optional<Libro>{it->second};
            require(tree.find(oracleKey(id)) == expected, "Consulta mixta"); verify(tree, oracle);
        }
    }
    std::vector<unsigned> ids(240); std::iota(ids.begin(), ids.end(), 0); std::shuffle(ids.begin(), ids.end(), rng);
    for (auto id : ids) erase(tree, oracle, id);
    insert(tree, oracle, 500); erase(tree, oracle, 500); collect(tree);
    std::cout << "orden=" << order << " ascendente/descendente/mezclada/aleatoria OK\n";
}
void normalization() {
    require(normalizeTitle("  ÁÉÍÓÚ Ü Ñ  ") == "aeiou u ñ", "Acentos y mayúsculas");
    require(normalizeTitle(u8"A\u0301E\u0301I\u0301O\u0301U\u0301 U\u0308 N\u0303") == "aeiou u ñ", "Español descompuesto");
    require(normalizeTitle("NIÑO") != normalizeTitle("nino"), "Preservar ñ");
    require(normalizeTitle("  Uno\t DOS\n tres\r ") == "uno dos tres", "Espacios");
    require(normalizeTitle(u8"日本 📚") == u8"日本 📚", "Conservar otros Unicode");
    for (const auto& invalid : {std::string("\xC0\xAF"), std::string("\xED\xA0\x80"), std::string("\xF4\x90\x80\x80"), std::string("\xC3"), std::string("a\0b", 3), std::string("\x80")}) {
        bool rejected = false;
        try { normalizeTitle(invalid); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Rechazo de UTF-8/control inválido");
    }
}
}
int main() {
    try {
        constexpr unsigned seed = 20261109;
        std::cout << "Semilla=" << seed << "; oráculo=std::map<BookKey,Libro>\n";
        normalization();
        for (std::size_t invalid : {0U, 1U, 2U, 129U}) {
            bool rejected = false;
            try { BPlusTree tree(invalid); } catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "Orden inválido aceptado");
        }
        std::mt19937 rng(seed);
        for (std::size_t order : {3U, 4U, 5U, 6U, 7U, 8U, 16U, 32U}) sequences(order, rng);
        // Todas las permutaciones de cinco claves, con borrado inverso de inserción.
        std::vector<unsigned> permutation{0, 1, 2, 3, 4};
        do {
            BPlusTree tree(3); std::map<BookKey, Libro> oracle;
            for (auto id : permutation) insert(tree, oracle, id);
            for (auto it = permutation.rbegin(); it != permutation.rend(); ++it) erase(tree, oracle, *it);
            collect(tree);
        } while (std::next_permutation(permutation.begin(), permutation.end()));
        require(totals.leafSplits && totals.internalSplits, "No se ejercitaron ambos splits");
        require(totals.leafBorrowLeft && totals.leafBorrowRight && totals.internalBorrowLeft && totals.internalBorrowRight, "Falta una rama de redistribución");
        require(totals.leafMerges && totals.internalMerges && totals.rootShrinks, "Falta fusión/contracción");
        std::cout << "splits hoja/interno=" << totals.leafSplits << '/' << totals.internalSplits
                  << " préstamo hoja izq/der=" << totals.leafBorrowLeft << '/' << totals.leafBorrowRight
                  << " préstamo interno izq/der=" << totals.internalBorrowLeft << '/' << totals.internalBorrowRight
                  << " fusiones hoja/interno=" << totals.leafMerges << '/' << totals.internalMerges
                  << " contracciones=" << totals.rootShrinks << '\n';
        std::cout << "OK operaciones=" << operations << " comprobaciones=" << checks << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << "FALLO: " << e.what() << '\n'; return 1; }
}
