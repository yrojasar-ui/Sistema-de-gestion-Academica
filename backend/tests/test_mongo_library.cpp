#include "biblioteca/MongoLibraryRepository.hpp"
#include "biblioteca/HttpController.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <unistd.h>

using namespace biblioteca;
namespace {
void check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
template<class F> void fails(F operation) {
    bool failed = false;
    try { operation(); } catch (const std::exception&) { failed = true; }
    check(failed, "Se esperaba un rechazo");
}
struct Fixture {
    Json document = nullptr;
    Json delayed = nullptr;
    std::string fault;
    int writes = 0, posts = 0;
    bool failNextRead = false, duplicate = false, incomplete = false;
    HttpResponse call(const std::string& method, const std::string& url, const std::string& body) {
        check(url.rfind("https://fixture.invalid/api", 0) == 0, "Red externa prohibida en fixture");
        if (method == "GET") {
            if (fault == "read" || failNextRead) { failNextRead = false; throw std::runtime_error("Lectura simulada fallida"); }
            auto docs = Json::array();
            if (!document.is_null()) docs.push_back(document);
            if (duplicate && !document.is_null()) docs.push_back(document);
            return {200, "application/json", Json{{"coleccion","prueba"},{"total",docs.size() + (incomplete ? 1U : 0U)},{"documentos",docs}}.dump()};
        }
        ++writes;
        auto p = Json::parse(body);
        check(p.at("coleccion") == "prueba", "Ámbito");
        if (fault == "reject") { fault.clear(); return {409,"application/json","{}"}; }
        Json next;
        if (method == "POST") {
            ++posts; check(document.is_null(), "POST duplicado");
            next = p.at("datos"); next["_id"] = {{"$oid","0123456789abcdef01234567"}};
        } else {
            check(method == "PUT", "Método de escritura");
            check(p.at("filtro").at("_id") == document.at("_id").at("$oid") &&
                  p.at("filtro").at("estado.revision") == document.at("estado").at("revision") &&
                  p.at("filtro").at("proyecto") == "prueba" && p.at("filtro").at("idBiblioteca") == "principal", "Filtro condicionado exacto");
            check(p.at("datos").size() == 1 && p.at("datos").contains("estado"), "Una sola unidad libros/dominio");
            next = document; next["estado"] = p.at("datos").at("estado");
        }
        const auto mode = fault; fault.clear();
        if (mode == "delayed") {
            delayed = next;
            throw std::runtime_error("Timeout previo a la aplicación tardía");
        }
        document = next;
        if (mode == "partial") {
            delayed = next;
            // Servidor defectuoso simulado: pierde parte del dominio, conserva revisión/libros.
            document["estado"]["dominio"]["usuarios"] = Json::array();
            throw std::runtime_error("Escritura parcial simulada");
        }
        if (mode == "unreadable") failNextRead = true;
        if (mode == "timeout") throw std::runtime_error("Timeout después de persistir");
        if (mode == "html") return {200, "text/html", "<html>Ambiguo</html>"};
        return {200, "application/json", R"({"mensaje":"OK","modificados":1})"};
    }
};
struct Harness {
    MongoLibraryRepository repo;
    CatalogService catalog;
    LibraryService library;
    Harness(Fixture& f, const std::filesystem::path& journal)
        : repo("https://fixture.invalid/api", "prueba", journal,
               [&f](const auto& m, const auto& u, const auto& b) { return f.call(m,u,b); }),
          catalog(repo), library(catalog, repo, true) {}
    void recover(bool init = false) {
        try {
            repo.refresh(init);
            CatalogService next(repo); next.load(repo.workingPath());
            LibraryService nextLibrary(next, repo); nextLibrary.validar();
            catalog.swapState(next); library.swapState(nextLibrary); repo.accept();
        } catch (const std::exception& e) { repo.block(e.what()); throw; }
    }
};
}
int main() {
    auto pattern = (std::filesystem::temp_directory_path() / "biblioteca-mongo-XXXXXX").string();
    std::vector<char> temp(pattern.begin(), pattern.end()); temp.push_back('\0');
    const auto* created = ::mkdtemp(temp.data());
    if (!created) return 1;
    const std::filesystem::path root(created);
    try {
        Fixture f; const auto journal = root / "mongo.pending.json";
        Harness h(f,journal);
        fails([&] { h.recover(); });
        check(!h.repo.ready() && f.writes == 0, "Ausente no inicializa automáticamente");
        h.recover(true);
        check(f.posts == 1 && h.repo.ready() && h.repo.outcome() == "confirmado", "Inicialización explícita única");
        const Libro book{"L","Atlas del campus","Autor sintético",{"campus"}};
        h.library.crearLibro(book);
        for (const auto* name : {"Ana","Bruno","Carla"}) h.library.crearUsuario({name,name,true});
        h.library.crearEjemplar("E","L");
        h.library.prestar("PA","E","Ana","2099-10-01");
        h.library.reservar("RB","L","Bruno"); h.library.reservar("RC","L","Carla");
        auto before = Json(h.library.estado());
        const auto writes = f.writes;
        fails([&] { h.library.prestar("PD","E","Bruno","2099-10-01"); });
        check(f.writes == writes && before == Json(h.library.estado()), "Rechazo de dominio no escribe");
        h.library.renovar("PA","2099-11-01");
        auto edited = book; edited.titulo = "Geografía"; edited.temas = {"geografia"};
        h.library.editarLibro(edited);
        {
            Harness restart(f,journal); restart.recover();
            check(restart.library.activos().size() == 1 && restart.library.activos()[0].vencimiento == "2099-11-01", "Reinicio conserva RN renovado");
            check(restart.catalog.byTitle("Atlas del campus").empty() && restart.catalog.byPrefix("geografia").size() == 1, "B+ editado reconstruido");
            check(restart.library.estado().reservas[0].llegada == 1 && restart.library.estado().reservas[1].llegada == 2, "Secuencia recuperada");
            restart.library.devolver("PA"); restart.library.validar();
        }
        h.recover();
        check(h.library.estado().ejemplares[0].estado == "apartado" && h.library.estado().reservas[0].estado == "apartada", "Apartado Bruno");
        const auto notices = Json(h.library.estado().avisos);
        h.recover();
        check(Json(h.library.estado().avisos) == notices && !notices.empty(), "Avisos estables tras recarga");
        h.library.cancelar("RB");
        check(h.library.estado().reservas[1].estado == "apartada", "Apartado Carla");
        h.library.prestar("PC","E","Carla","2099-10-01"); h.library.devolver("PC"); h.recover();
        check(h.library.activos().empty() && h.library.estado().prestamos.size() == 2 &&
              h.library.estado().ejemplares[0].estado == "disponible", "Historial conservado");
        h.library.reservar("R3","L","Ana");
        check(h.library.estado().reservas.back().llegada == 3, "Secuencia no reiniciada");
        std::cout << "Historia y reinicios: activos, renovación, B+, grafo, FIFO, avisos, historial y secuencia OK.\n";

        f.fault = "timeout"; h.library.crearUsuario({"Timeout","Sintético",true});
        check(h.library.usuario("Timeout").activo && h.repo.outcome() == "confirmado", "Timeout aplicado confirmado por GET");
        f.fault = "html"; h.library.crearUsuario({"Html","Sintético",true});
        check(h.library.usuario("Html").activo, "HTTP 200 HTML reconciliado");
        before = Json(h.library.estado());
        f.fault = "reject"; fails([&] { h.library.crearUsuario({"Rechazo","Sintético",true}); });
        check(h.repo.ready() && h.repo.outcome() == "rechazado" && Json(h.library.estado()) == before, "Rechazo confirmado conserva memoria");
        const auto beforeRead = f.writes;
        f.fault = "read"; fails([&] { h.library.crearUsuario({"Lectura","Sintético",true}); });
        check(!h.repo.ready() && f.writes == beforeRead && Json(h.library.estado()) == before, "Lectura fallida no envía cambios");
        f.fault.clear(); h.recover();

        f.fault = "unreadable"; fails([&] { h.library.crearUsuario({"Pendiente","Sintético",true}); });
        check(!h.repo.ready() && h.repo.outcome() == "incierto" && Json(h.library.estado()) == before &&
              std::filesystem::exists(journal), "Acuse no basta; bloquea y preserva memoria");
        auto count = f.writes;
        fails([&] { h.library.crearUsuario({"Bloqueado","Sintético",true}); });
        check(f.writes == count, "No envía mutaciones bloqueadas");
        {
            Harness restart(f,journal); restart.recover();
            check(restart.library.usuario("Pendiente").activo && f.writes == count, "Reinicio reconcilia sin repetir PUT");
        }
        h.recover();

        f.fault = "delayed"; fails([&] { h.library.crearUsuario({"Tardio","Sintético",true}); });
        count = f.writes; fails([&] { h.recover(); });
        check(!h.repo.ready() && f.writes == count, "GET anterior tras timeout NO equivale a rechazo");
        f.document = f.delayed; h.recover(); check(h.library.usuario("Tardio").activo, "Aplicación tardía recuperable");

        before = Json(h.library.estado()); f.fault = "partial";
        fails([&] { h.library.crearUsuario({"Parcial","Sintético",true}); });
        fails([&] { h.recover(); });
        check(!h.repo.ready() && Json(h.library.estado()) == before, "Parcial no se publica ni se mezcla");
        // Intervención del fixture: completar exactamente el estado esperado; recuperación solo GET.
        f.document = f.delayed; count = f.writes; h.recover();
        check(h.library.usuario("Parcial").activo && f.writes == count, "Parcial resuelto sin repetir escritura");
        std::cout << "Fixtures: timeout aplicado/tardío, HTML, rechazo, lectura fallida, diario y parcial OK.\n";

        const auto good = f.document; before = Json(h.library.estado());
        for (int mode = 0; mode < 5; ++mode) {
            if (mode == 0) f.document["versionEsquema"] = 2;
            if (mode == 1) f.document["estado"]["dominio"]["usuarios"] = Json::array();
            if (mode == 2) f.document["estado"]["libros"].push_back(f.document["estado"]["libros"][0]);
            if (mode == 3) f.duplicate = true;
            if (mode == 4) f.incomplete = true;
            fails([&] { h.recover(); });
            check(!h.repo.ready() && Json(h.library.estado()) == before, "Carga incompatible conserva memoria sin habilitarla");
            f.document = good; f.duplicate = false; f.incomplete = false; h.recover();
        }
        f.document["estado"]["revision"] = f.document["estado"]["revision"].get<std::uint64_t>() + 1;
        f.document["estado"]["libros"][0]["autor"] = "Edición externa sintética";
        count = f.writes;
        fails([&] { h.library.crearUsuario({"Externo","Sintético",true}); });
        check(f.writes == count && !h.repo.ready(), "Cambio externo detectado antes de escribir");
        h.recover();

        // Contrato HTTP real sobre loopback, con transporte Mongo inyectado, sin nube.
        httplib::Server server;
        server.new_task_queue = [] { return new httplib::ThreadPool(1); };
        registrarHttp(server, h.catalog, h.library, {}, [&] { h.recover(); });
        const auto port = server.bind_to_any_port("127.0.0.1");
        check(port > 0, "Puerto fixture");
        std::thread worker([&] { server.listen_after_bind(); });
        try {
            server.wait_until_ready();
            httplib::Client client("127.0.0.1", port);
            auto ready = client.Get("/readyz");
            check(ready && ready->status == 200 && Json::parse(ready->body)["dominio"] == "mongo", "Ready Mongo");
            f.fault = "unreadable";
            auto result = client.Post("/api/usuarios", R"({"idUsuario":"HTTP","nombre":"Sintético","activo":true})", "application/json");
            check(result && result->status == 503 && Json::parse(result->body)["resultado"] == "incierto", "HTTP resultado incierto");
            check(client.Get("/healthz")->status == 200 && client.Get("/readyz")->status == 503 &&
                  client.Get("/api/libros")->status == 503, "Proceso vivo, biblioteca no lista; no publica catálogo vacío");
            result = client.Post("/api/recuperacion", "{}", "application/json");
            check(result && result->status == 200 && client.Get("/api/usuarios/HTTP")->status == 200, "Recuperación HTTP");
        } catch (...) { server.stop(); worker.join(); throw; }
        server.stop(); worker.join();
        std::cout << "HTTP local: health/ready, bloqueo y recuperación explícita OK.\n";

        Fixture corruptIntent;
        const auto corruptPath = root / "corrupto.pending.json";
        { std::ofstream out(corruptPath); out << "{diario truncado"; }
        Harness corrupt(corruptIntent, corruptPath);
        fails([&] { corrupt.recover(true); });
        check(corruptIntent.writes == 0 && !corrupt.repo.ready() && corrupt.repo.outcome() == "incierto",
              "Diario ilegible no se presenta como rechazo ni permite POST");
        Fixture invalidJournal;
        Harness missingDirectory(invalidJournal, root / "ausente" / "diario.json");
        fails([&] { missingDirectory.recover(true); });
        check(invalidJournal.writes == 0 && !missingDirectory.repo.ready(), "Fallo de diario impide enviar POST");
        Fixture newLibrary; newLibrary.fault = "delayed";
        const auto secondJournal = root / "post.pending.json";
        { Harness pending(newLibrary,secondJournal); fails([&] { pending.recover(true); }); }
        const auto postCount = newLibrary.posts;
        {
            Harness restart(newLibrary,secondJournal);
            fails([&] { restart.recover(true); });
            check(newLibrary.posts == postCount && !restart.repo.ready(), "No repetir POST incierto al reiniciar");
            newLibrary.document = newLibrary.delayed;
            restart.recover();
            check(restart.repo.ready() && newLibrary.posts == postCount, "POST tardío reconciliado por GET");
        }
        std::cout << "POST incierto y reinicio: un solo envío, recuperación por GET OK.\n";
        std::filesystem::remove_all(root);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FALLO: " << e.what() << "\n"; std::filesystem::remove_all(root); return 1;
    }
}
