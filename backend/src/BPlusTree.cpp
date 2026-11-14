#include "biblioteca/BPlusTree.hpp"
#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace biblioteca {
BPlusTree::BPlusTree(std::size_t order) : order_(order), root_(std::make_unique<Node>(true)) {
    if (order < 3 || order > 128) throw std::invalid_argument("Orden B+ debe estar entre 3 y 128");
}
void BPlusTree::swap(BPlusTree& other) noexcept {
    using std::swap;
    swap(order_, other.order_); swap(size_, other.size_);
    swap(root_, other.root_); swap(stats_, other.stats_);
}
BPlusTree::Node* BPlusTree::leafFor(const BookKey& key, std::vector<const Node*>* camino) const {
    Node* node = root_.get();
    if (camino) camino->push_back(node);
    while (!node->leaf) {
        // Igualdad con un separador pertenece al hijo de la derecha.
        const auto i = static_cast<std::size_t>(std::upper_bound(node->keys.begin(), node->keys.end(), key) - node->keys.begin());
        node = node->children[i].get();
        if (camino) camino->push_back(node);
    }
    return node;
}
BPlusTree::Node* BPlusTree::firstLeaf() const {
    Node* node = root_.get();
    while (!node->leaf) node = node->children.front().get();
    return node;
}
const BookKey& BPlusTree::minimum(const Node* node) const {
    while (!node->leaf) node = node->children.front().get();
    return node->entries.front().key;
}
void BPlusTree::refresh(Node* node) {
    if (node->leaf) return;
    node->keys.clear();
    for (std::size_t i = 1; i < node->children.size(); ++i) node->keys.push_back(minimum(node->children[i].get()));
}
void BPlusTree::refreshUp(Node* node) {
    for (; node; node = node->parent) refresh(node);
}
std::size_t BPlusTree::childIndex(const Node* node) const {
    const auto& children = node->parent->children;
    for (std::size_t i = 0; i < children.size(); ++i) if (children[i].get() == node) return i;
    throw std::logic_error("Padre no contiene al hijo");
}
void BPlusTree::attachRight(Node* left, std::unique_ptr<Node> right) {
    if (!left->parent) {
        auto newRoot = std::make_unique<Node>(false);
        root_->parent = newRoot.get(); right->parent = newRoot.get();
        newRoot->children.push_back(std::move(root_));
        newRoot->children.push_back(std::move(right));
        refresh(newRoot.get()); root_ = std::move(newRoot);
        return;
    }
    Node* parent = left->parent;
    const auto index = childIndex(left);
    right->parent = parent;
    parent->children.insert(parent->children.begin() + static_cast<std::ptrdiff_t>(index + 1), std::move(right));
    refresh(parent);
    if (parent->children.size() > order_) split(parent);
    else refreshUp(parent->parent);
}
void BPlusTree::split(Node* node) {
    auto right = std::make_unique<Node>(node->leaf);
    if (node->leaf) {
        const auto middle = (node->entries.size() + 1) / 2;
        auto pivot = node->entries.begin() + static_cast<std::ptrdiff_t>(middle);
        right->entries.assign(std::make_move_iterator(pivot), std::make_move_iterator(node->entries.end()));
        node->entries.erase(pivot, node->entries.end());
        right->next = node->next; right->prev = node;
        if (right->next) right->next->prev = right.get();
        node->next = right.get(); ++stats_.leafSplits;
    } else {
        const auto middle = (node->children.size() + 1) / 2;
        auto pivot = node->children.begin() + static_cast<std::ptrdiff_t>(middle);
        right->children.assign(std::make_move_iterator(pivot), std::make_move_iterator(node->children.end()));
        node->children.erase(pivot, node->children.end());
        for (auto& child : right->children) child->parent = right.get();
        refresh(node); refresh(right.get()); ++stats_.internalSplits;
    }
    attachRight(node, std::move(right));
}
bool BPlusTree::insert(const Libro& book) {
    validateBook(book);
    const auto key = keyOf(book);
    Node* node = leafFor(key);
    auto pos = std::lower_bound(node->entries.begin(), node->entries.end(), key,
                               [](const Entry& e, const BookKey& k) { return e.key < k; });
    if (pos != node->entries.end() && pos->key == key) return false;
    node->entries.insert(pos, Entry{key, book}); ++size_;
    if (node->entries.size() >= order_) split(node);
    else refreshUp(node->parent);
    return true;
}
std::size_t BPlusTree::occupancy(const Node* node) const {
    return node->leaf ? node->entries.size() : node->children.size();
}
std::size_t BPlusTree::minimumOccupancy(const Node* node) const {
    return node->leaf ? order_ / 2 : (order_ + 1) / 2;
}
void BPlusTree::rebalance(Node* node) {
    // Reparación local: mover una entrada/hijo; si nadie presta, fusionar hermanos.
    // Solo se propaga la pérdida de un hijo hacia los ancestros, sin reconstruir árbol.
    while (node != root_.get()) {
        if (occupancy(node) >= minimumOccupancy(node)) { refreshUp(node); return; }
        Node* parent = node->parent;
        const auto index = childIndex(node);
        Node* left = index ? parent->children[index - 1].get() : nullptr;
        Node* right = index + 1 < parent->children.size() ? parent->children[index + 1].get() : nullptr;
        if (left && occupancy(left) > minimumOccupancy(left)) {
            if (node->leaf) {
                node->entries.insert(node->entries.begin(), std::move(left->entries.back()));
                left->entries.pop_back(); ++stats_.leafBorrowLeft;
            } else {
                auto child = std::move(left->children.back()); left->children.pop_back();
                child->parent = node; node->children.insert(node->children.begin(), std::move(child));
                ++stats_.internalBorrowLeft;
            }
            refresh(left); refreshUp(node); return;
        }
        if (right && occupancy(right) > minimumOccupancy(right)) {
            if (node->leaf) {
                node->entries.push_back(std::move(right->entries.front()));
                right->entries.erase(right->entries.begin()); ++stats_.leafBorrowRight;
            } else {
                auto child = std::move(right->children.front()); right->children.erase(right->children.begin());
                child->parent = node; node->children.push_back(std::move(child));
                ++stats_.internalBorrowRight;
            }
            refresh(right); refreshUp(node); return;
        }
        Node* destination = left ? left : node;
        Node* source = left ? node : right;
        const auto removedIndex = left ? index : index + 1;
        if (node->leaf) {
            destination->entries.insert(destination->entries.end(), std::make_move_iterator(source->entries.begin()), std::make_move_iterator(source->entries.end()));
            destination->next = source->next;
            if (source->next) source->next->prev = destination;
            ++stats_.leafMerges;
        } else {
            for (auto& child : source->children) { child->parent = destination; destination->children.push_back(std::move(child)); }
            ++stats_.internalMerges;
        }
        parent->children.erase(parent->children.begin() + static_cast<std::ptrdiff_t>(removedIndex));
        refresh(destination); refresh(parent); node = parent;
    }
    if (!root_->leaf && root_->children.size() == 1) {
        auto oldRoot = std::move(root_);
        root_ = std::move(oldRoot->children.front()); root_->parent = nullptr;
        ++stats_.rootShrinks;
    }
    refresh(root_.get());
}
bool BPlusTree::erase(const BookKey& key) {
    Node* node = leafFor(key);
    auto pos = std::lower_bound(node->entries.begin(), node->entries.end(), key,
                               [](const Entry& e, const BookKey& k) { return e.key < k; });
    if (pos == node->entries.end() || pos->key != key) return false;
    node->entries.erase(pos); --size_; rebalance(node); return true;
}
std::optional<Libro> BPlusTree::find(const BookKey& key) const {
    const Node* node = leafFor(key);
    auto pos = std::lower_bound(node->entries.begin(), node->entries.end(), key,
                               [](const Entry& e, const BookKey& k) { return e.key < k; });
    if (pos == node->entries.end() || pos->key != key) return std::nullopt;
    return pos->book;
}
std::vector<Libro> BPlusTree::range(const BookKey& low, const BookKey& high) const {
    std::vector<Libro> result;
    if (high < low) return result;
    for (const Node* node = leafFor(low); node; node = node->next)
        for (const auto& e : node->entries) {
            if (high < e.key) return result;
            if (!(e.key < low)) result.push_back(e.book);
        }
    return result;
}
std::vector<Libro> BPlusTree::byTitle(const std::string& title) const {
    const auto normalized = normalizeTitle(title);
    std::vector<Libro> result;
    for (const Node* node = leafFor({normalized, ""}); node; node = node->next)
        for (const auto& e : node->entries) {
            if (e.key.first > normalized) return result;
            if (e.key.first == normalized) result.push_back(e.book);
        }
    return result;
}
std::vector<Libro> BPlusTree::byPrefix(const std::string& prefix) const {
    const auto normalized = normalizeTitle(prefix);
    std::vector<Libro> result;
    for (const Node* node = leafFor({normalized, ""}); node; node = node->next)
        for (const auto& e : node->entries) {
            if (e.key.first < normalized) continue;
            if (e.key.first.compare(0, normalized.size(), normalized) != 0) return result;
            result.push_back(e.book);
        }
    return result;
}
std::vector<Libro> BPlusTree::all() const {
    std::vector<Libro> result;
    for (const Node* node = firstLeaf(); node; node = node->next)
        for (const auto& e : node->entries) result.push_back(e.book);
    return result;
}
bool BPlusTree::validate(std::string& reason) const {
    std::vector<const Node*> leaves;
    std::optional<std::size_t> leafDepth;
    std::size_t count = 0;
    struct Bounds { BookKey first, last; };
    try {
        auto require = [](bool condition, const char* why) { if (!condition) throw std::logic_error(why); };
        std::function<Bounds(const Node*, const Node*, std::size_t)> visit;
        visit = [&](const Node* n, const Node* parent, std::size_t depth) -> Bounds {
            require(n != nullptr && n->parent == parent, "Padre inconsistente");
            const auto maximum = n->leaf ? order_ - 1 : order_;
            require(occupancy(n) <= maximum, "Sobreocupación");
            if (parent) require(occupancy(n) >= minimumOccupancy(n), "Subocupación");
            if (n->leaf) {
                require(n->children.empty() && n->keys.empty(), "Hoja contiene hijos/separadores");
                if (!leafDepth) leafDepth = depth;
                require(*leafDepth == depth, "Hojas a diferente profundidad");
                leaves.push_back(n); count += n->entries.size();
                for (std::size_t i = 0; i < n->entries.size(); ++i) {
                    validateBook(n->entries[i].book);
                    require(n->entries[i].key == keyOf(n->entries[i].book), "Clave no coincide con Libro");
                    if (i) require(n->entries[i - 1].key < n->entries[i].key, "Hoja desordenada/duplicada");
                }
                if (n->entries.empty()) { require(!parent && size_ == 0, "Hoja vacía no raíz"); return {}; }
                return {n->entries.front().key, n->entries.back().key};
            }
            require(n->entries.empty() && !n->next && !n->prev, "Interno contiene registros/enlaces");
            require(n->children.size() >= 2 && n->keys.size() + 1 == n->children.size(), "Aridad inválida");
            Bounds whole = visit(n->children.front().get(), n, depth + 1);
            for (std::size_t i = 1; i < n->children.size(); ++i) {
                const auto child = visit(n->children[i].get(), n, depth + 1);
                require(whole.last < child.first, "Subárboles solapados");
                require(n->keys[i - 1] == child.first, "Separador distinto al mínimo derecho");
                whole.last = child.last;
            }
            return whole;
        };
        visit(root_.get(), nullptr, 0);
        require(count == size_, "Tamaño incorrecto");
        for (std::size_t i = 0; i < leaves.size(); ++i) {
            require(leaves[i]->prev == (i ? leaves[i - 1] : nullptr), "Enlace anterior incorrecto");
            require(leaves[i]->next == (i + 1 < leaves.size() ? leaves[i + 1] : nullptr), "Enlace siguiente incorrecto");
        }
        reason = "OK: orden, ocupación, separadores, padres, profundidad y hojas enlazadas";
        return true;
    } catch (const std::exception& e) { reason = e.what(); return false; }
}
std::vector<const BPlusTree::Node*> BPlusTree::enumerarNodos(std::map<const Node*, std::size_t>& ids) const {
    std::vector<const Node*> nodos{root_.get()};
    ids.emplace(root_.get(), 0);
    for (std::size_t i = 0; i < nodos.size(); ++i) {
        for (const auto& hijo : nodos[i]->children) {
            ids.emplace(hijo.get(), nodos.size());
            nodos.push_back(hijo.get());
        }
    }
    return nodos;
}
std::string BPlusTree::snapshot(std::optional<BookKey> buscar) const {
    using nlohmann::json;
    std::map<const Node*, std::size_t> ids;
    auto nodos = enumerarNodos(ids);
    auto id = [&](const Node* n) -> json { return n ? json(ids.at(n)) : json(nullptr); };
    auto datos = json::array();
    for (const auto* nodo : nodos) {
        auto claves = json(nodo->keys);
        for (const auto& entrada : nodo->entries) claves.push_back(json(entrada.key));
        auto hijos = json::array();
        for (const auto& hijo : nodo->children) hijos.push_back(id(hijo.get()));
        datos.push_back({{"id", id(nodo)}, {"hoja", nodo->leaf}, {"claves", std::move(claves)},
            {"hijos", std::move(hijos)}, {"anterior", id(nodo->prev)}, {"siguiente", id(nodo->next)}});
    }
    json salida{{"raiz", 0}, {"orden", order_}, {"cantidad", size_}, {"nodos", std::move(datos)}};
    if (buscar) {
        auto traza = json::array();
        std::vector<const Node*> camino;
        leafFor(*buscar, &camino);
        for (const auto* nodo : camino) traza.push_back(id(nodo));
        salida["traza"] = std::move(traza);
    }
    return salida.dump();
}
void BPlusTree::visualize(std::ostream& out) const {
    // El mapa solo asigna etiquetas de presentación a nodos reales, nunca indexa libros.
    std::map<const Node*, std::size_t> ids;
    auto queue = enumerarNodos(ids);
    out << "B+ orden=" << order_ << " libros=" << size_ << '\n';
    for (const auto* node : queue) {
        out << 'N' << ids.at(node) << (node->leaf ? " hoja [" : " interno [");
        if (node->leaf) for (const auto& e : node->entries) out << '(' << e.key.first << ',' << e.key.second << ") ";
        else for (const auto& key : node->keys) out << '(' << key.first << ',' << key.second << ") ";
        out << ']';
        if (!node->leaf) { out << " hijos:"; for (const auto& child : node->children) out << " N" << ids.at(child.get()); }
        out << '\n';
    }
    out << "Hojas: ";
    for (const Node* node = firstLeaf(); node; node = node->next) out << 'N' << ids.at(node) << " -> ";
    out << "fin\n";
}
} // namespace biblioteca
