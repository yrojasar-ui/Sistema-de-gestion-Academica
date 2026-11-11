#include "biblioteca/CatalogService.hpp"
#include "biblioteca/MongoApiBookRepository.hpp"
#include <nlohmann/json.hpp>
#include <deque>
#include <fstream>
#include <iostream>

using namespace biblioteca;
using Json = nlohmann::json;
namespace {
void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}
template<class F> void rejects(F operation, const std::string& fragment) {
    try { operation(); }
    catch (const std::exception& e) {
        require(std::string(e.what()).find(fragment) != std::string::npos, e.what());
        return;
    }
    throw std::runtime_error("Faltó rechazo: " + fragment);
}
const std::string endpoint = "https://fixture.invalid/mongodb.php";
const std::string collection = "biblioteca_yoel_ed2_libros";
const Libro book{"E2B-FIXTURE", "Bitácora sintética E2B", "Autor sintético", {"prueba"}};
struct Step {
    std::string method, url;
    Json payload;
    HttpResponse reply;
};
class Exchange {
public:
    std::deque<Step> queue;
    bool mismatch = false;
    void get(const Json& reply, bool filtered = true) {
        queue.push_back({"GET", endpoint + "?coleccion=" + collection + (filtered ? "&idLibro=" + book.idLibro : ""),
                         nullptr, {200, "application/json; charset=UTF-8", reply.dump()}});
    }
    void write(const std::string& method, const Json& payload, const Json& reply, bool timeout = false) {
        queue.push_back({method, endpoint, payload, {timeout ? 0 : 200, "application/json", reply.dump()}});
    }
    HttpResponse operator()(const std::string& method, const std::string& url, const std::string& body) {
        if (queue.empty() || queue.front().method != method || queue.front().url != url ||
            (body.empty() ? !queue.front().payload.is_null() : queue.front().payload != Json::parse(body))) {
            mismatch = true;
            throw std::runtime_error("Solicitud HTTP inesperada: " + method + " " + url);
        }
        auto reply = queue.front().reply;
        queue.pop_front();
        if (reply.status == 0) throw std::runtime_error("timeout simulado");
        return reply;
    }
};
}
int main() {
    try {
        std::ifstream input(FIXTURE_PATH);
        const auto f = Json::parse(input);
        const auto empty = f.at("vacio"), original = f.at("libro"), updated = f.at("revisado");
        const auto doc = original.at("documentos").at(0);
        Libro revised = book;
        revised.titulo = updated.at("documentos").at(0).at("titulo");
        Json data = doc;
        data.erase("_id");
        const Json post{{"coleccion",collection},{"datos",data}};
        const Json filter{{"_id",doc.at("_id").at("$oid")},{"proyecto",collection},{"idLibro",book.idLibro}};
        const Json del{{"coleccion",collection},{"filtro",filter}};
        Json put = del;
        put["datos"] = {{"titulo",revised.titulo},{"autor",revised.autor},{"temas",revised.temas}};

        Exchange wire;
        MongoApiBookRepository repo(endpoint, collection, std::ref(wire));
        CatalogService catalog(repo);
        rejects([&]{ catalog.insert(book); }, "Recargue");
        wire.get(empty, false);
        require(catalog.load(collection) == 0, "Inicio vacío");
        wire.get(empty); wire.write("POST", post, f.at("insertado")); wire.get(original); wire.get(original, false);
        catalog.insert(book);
        require(catalog.byTitle("BITACORA SINTETICA E2B") == std::vector<Libro>{book}, "B+ tras alta");
        rejects([&]{ catalog.insert(book); }, "duplicado");
        wire.get(original); wire.write("PUT", put, f.at("actualizado")); wire.get(updated); wire.get(updated, false);
        catalog.update(revised);
        require(catalog.byTitle(book.titulo).empty() && catalog.byTitle(revised.titulo).size()==1, "Cambio de clave B+");
        wire.get(updated); wire.write("DELETE", del, f.at("eliminado")); wire.get(empty); wire.get(empty, false);
        require(catalog.erase(book.idLibro) && catalog.byPrefix("Bitácora").empty(), "B+ tras borrado");

        // Timeout después del POST. Solo hay un envío; el GET confirma o exige recarga.
        wire.get(empty); wire.write("POST", post, nullptr, true); wire.get(empty);
        rejects([&]{ catalog.insert(book); }, "incierto");
        rejects([&]{ catalog.byPrefix(""); }, "Recargue");
        rejects([&]{ catalog.insert(book); }, "Recargue");
        wire.get(empty, false); catalog.load(collection);
        wire.get(empty); wire.write("POST", post, nullptr, true); wire.get(original); wire.get(original, false);
        catalog.insert(book);

        Json other = original;
        other["documentos"][0]["_id"]["$oid"] = "aaaaaaaaaaaaaaaaaaaaaaaa";
        wire.get(other);
        rejects([&]{ catalog.erase(book.idLibro); }, "selección cambió");
        wire.get(original, false); catalog.load(collection);
        wire.get(original); wire.write("DELETE", del, {{"mensaje","Documento eliminado"},{"eliminados",0}});
        wire.get(original);
        rejects([&]{ catalog.erase(book.idLibro); }, "incierto");
        rejects([&]{ repo.eraseRecord(""); }, "Seleccione");
        rejects([&]{ repo.save({}); }, "no admite");
        rejects([&]{ repo.load("otro"); }, "Origen distinto");

        // Un GET fallido, parcial o fuera de ámbito nunca publica un catálogo vacío.
        wire.get(empty, false); wire.queue.back().reply = {200,"text/html","<html>API</html>"};
        rejects([&]{ catalog.load(collection); }, "Content-Type");
        wire.get({{"error","fallo simulado"}},false);
        rejects([&]{ catalog.load(collection); }, "error de aplicación");
        wire.get(empty,false); wire.queue.back().reply.status=503;
        rejects([&]{ catalog.load(collection); }, "503");
        wire.get(empty,false); wire.queue.back().reply.body="{";
        rejects([&]{ catalog.load(collection); }, "parse_error");
        for (const auto& field : {"proyecto","_id","total","idLibro"}) {
            auto bad=original;
            if (std::string(field)=="total") bad["total"]=2;
            else if (std::string(field)=="idLibro") {
                bad["documentos"].push_back(doc); bad["total"]=2;
            } else bad["documentos"][0][field]="ajeno";
            wire.get(bad,false);
            bool failed=false;
            try { catalog.load(collection); } catch (const std::exception&) { failed=true; }
            require(failed, std::string("Carga aceptada con campo inválido: ")+field);
            rejects([&]{ catalog.all(); }, "Recargue");
        }
        // La mutación se confirmó, pero falla la carga completa que alimenta al B+.
        wire.get(empty,false); catalog.load(collection);
        wire.get(empty); wire.write("POST",post,f.at("insertado")); wire.get(original);
        wire.get(empty,false); wire.queue.back().reply.status=503;
        rejects([&]{ catalog.insert(book); }, "Cambio confirmado");
        rejects([&]{ catalog.all(); }, "Recargue");
        wire.get(original,false); catalog.load(collection);
        std::string reason;
        require(catalog.validate(reason) && !catalog.dirty(), "Recuperación con GET");
        require(wire.queue.empty() && !wire.mismatch, "Intercambio incompleto o petición adicional");
        std::cout << "Fixtures Mongo OK: CRUD, identidad, reconciliación, fallos y reconstrucción del B+\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
