#pragma once
#include "biblioteca/Libro.hpp"
#include <memory>
#include <optional>
#include <ostream>

namespace biblioteca {
class BPlusTree {
public:
    struct Stats {
        std::size_t leafSplits = 0, internalSplits = 0;
        std::size_t leafBorrowLeft = 0, leafBorrowRight = 0;
        std::size_t internalBorrowLeft = 0, internalBorrowRight = 0;
        std::size_t leafMerges = 0, internalMerges = 0, rootShrinks = 0;
    };
    // Orden m = máximo m hijos internos y m-1 entradas por hoja; 3 <= m <= 128.
    explicit BPlusTree(std::size_t order = 4);
    bool insert(const Libro& book);
    bool erase(const BookKey& key);
    std::optional<Libro> find(const BookKey& key) const;
    std::vector<Libro> byTitle(const std::string& title) const;
    std::vector<Libro> byPrefix(const std::string& prefix) const;
    std::vector<Libro> range(const BookKey& low, const BookKey& high) const;
    std::vector<Libro> all() const;
    bool validate(std::string& reason) const;
    void visualize(std::ostream& out) const;
    std::size_t size() const { return size_; }
    std::size_t order() const { return order_; }
    const Stats& stats() const { return stats_; }
    void swap(BPlusTree& other) noexcept;
private:
    struct Entry { BookKey key; Libro book; };
    struct Node {
        explicit Node(bool isLeaf) : leaf(isLeaf) {}
        bool leaf;
        Node* parent = nullptr;
        Node* next = nullptr;
        Node* prev = nullptr;
        std::vector<Entry> entries; // Solo hojas: registros reales, nunca tombstones.
        std::vector<BookKey> keys; // Solo internos: mínimo del hijo situado a la derecha.
        std::vector<std::unique_ptr<Node>> children;
    };
    std::size_t order_, size_ = 0;
    std::unique_ptr<Node> root_;
    Stats stats_;
    Node* leafFor(const BookKey& key) const;
    Node* firstLeaf() const;
    const BookKey& minimum(const Node* node) const;
    void refresh(Node* node);
    void refreshUp(Node* node);
    std::size_t childIndex(const Node* node) const;
    void attachRight(Node* left, std::unique_ptr<Node> right);
    void split(Node* node);
    void rebalance(Node* node);
    std::size_t occupancy(const Node* node) const;
    std::size_t minimumOccupancy(const Node* node) const;
};
} // namespace biblioteca
