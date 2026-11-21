#include "biblioteca/AmqpBroker.hpp"
#include "biblioteca/DomainJson.hpp"
#include "biblioteca/ServiceIdentity.hpp"
#include <httplib.h>
#include <sodium.h>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using namespace biblioteca;

namespace {
std::atomic<bool> g_stop{false};
void handleSignal(int) { g_stop = true; }

void sleepInterruptible(int ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (!g_stop && std::chrono::steady_clock::now() < end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

struct WorkerConfig {
    std::string apiUrl = "http://127.0.0.1:8080";
    std::filesystem::path serviceTokenFile;
    std::filesystem::path amqpCredentialsFile;
    AmqpConfig amqp;
    std::uint32_t maxRetries = 3;
    int backoffMs = 500;
    std::string testFailBeforeAck;
};

std::string envOr(const char* name, const std::string& fallback = "") {
    const char* v = std::getenv(name);
    return v ? std::string(v) : fallback;
}

WorkerConfig parseArgs(int argc, char** argv) {
    WorkerConfig cfg;
    cfg.apiUrl = envOr("API_URL", cfg.apiUrl);
    cfg.amqp.host = envOr("AMQP_HOST", "127.0.0.1");
    if (auto p = envOr("AMQP_PORT"); !p.empty()) cfg.amqp.port = std::stoi(p);
    cfg.amqp.vhost = envOr("AMQP_VHOST", "/");
    cfg.amqp.user = envOr("AMQP_USER", "guest");
    cfg.amqp.password = envOr("AMQP_PASSWORD", "guest");
    if (auto f = envOr("SERVICE_TOKEN_FILE"); !f.empty()) cfg.serviceTokenFile = f;
    if (auto f = envOr("AMQP_CREDENTIALS_FILE"); !f.empty()) cfg.amqpCredentialsFile = f;

    for (int i = 1; i < argc; ++i) {
        std::string flag = argv[i];
        if (++i == argc) throw std::invalid_argument("Falta valor para " + flag);
        std::string val = argv[i];
        if (flag == "--api-url") cfg.apiUrl = val;
        else if (flag == "--service-token-file") cfg.serviceTokenFile = val;
        else if (flag == "--amqp-host") cfg.amqp.host = val;
        else if (flag == "--amqp-vhost") cfg.amqp.vhost = val;
        else if (flag == "--amqp-credentials-file") cfg.amqpCredentialsFile = val;
        else if (flag == "--test-fail-before-ack") cfg.testFailBeforeAck = val;
        else if (flag == "--amqp-port" || flag == "--max-retries" || flag == "--backoff-ms") {
            int n = 0;
            auto r = std::from_chars(val.data(), val.data() + val.size(), n);
            if (r.ec != std::errc{} || r.ptr != val.data() + val.size() || n < 1 || n > 65535)
                throw std::invalid_argument("Valor numérico inválido para " + flag);
            if (flag == "--amqp-port") cfg.amqp.port = n;
            else if (flag == "--max-retries") cfg.maxRetries = static_cast<std::uint32_t>(n);
            else cfg.backoffMs = n;
        } else {
            throw std::invalid_argument("Opción desconocida: " + flag);
        }
    }
    if (!cfg.amqpCredentialsFile.empty()) loadAmqpCredentialsFile(cfg.amqpCredentialsFile, cfg.amqp);
    if (cfg.serviceTokenFile.empty()) throw std::invalid_argument("Se requiere --service-token-file");
    if (cfg.amqp.host.empty()) throw std::invalid_argument("Se requiere --amqp-host");
    return cfg;
}

std::pair<std::string, std::string> redactarNotificacion(const Json& ev) {
    const auto tipo = ev.at("tipo").get<std::string>();
    const auto idReserva = ev.at("idReserva").get<std::string>();
    const auto idPrestamo = ev.at("idPrestamo").get<std::string>();
    if (tipo == "reserva_apartada")
        return {"Reserva disponible para retirar",
                "Tu solicitud " + idReserva + " tiene un ejemplar apartado listo para retiro en mostrador."};
    if (tipo == "devolucion")
        return {"Devolución registrada",
                "Se confirmó la devolución física del préstamo " + idPrestamo + " y se conservó en tu historial."};
    if (tipo == "prestamo_registrado")
        return {"Préstamo registrado",
                "Se registró el préstamo " + idPrestamo + ". Consultá su fecha de vencimiento en Mi biblioteca."};
    if (tipo == "prestamo_renovado")
        return {"Préstamo renovado",
                "Se actualizó la fecha de vencimiento del préstamo " + idPrestamo + "."};
    if (tipo == "prestamo_anulado")
        return {"Préstamo anulado",
                "El préstamo " + idPrestamo + " fue anulado administrativamente. Revisá el motivo en tu historial."};
    if (tipo == "reserva_cancelada")
        return {"Solicitud cancelada",
                "Tu solicitud " + idReserva + " fue cancelada y conservada en tu historial."};
    if (tipo == "pago_simulado")
        return {"Comprobante simulado emitido",
                "Se generó el comprobante SIMULADO para el préstamo " + idPrestamo + " (demostración sin cobro real)."};
    throw std::invalid_argument("Tipo de evento desconocido: " + tipo);
}

struct ApiCallResult {
    int status = 0; // 0 = error de transporte/timeout
    std::string body;
    std::string error;
};

ApiCallResult postInternal(const std::string& apiUrl, const std::string& token,
                           const std::string& path, const Json& payload) {
    httplib::Client client(apiUrl);
    client.set_connection_timeout(3, 0);
    client.set_read_timeout(5, 0);
    client.set_write_timeout(5, 0);
    httplib::Headers headers{{"Authorization", "Bearer " + token}};
    auto res = client.Post(path, headers, payload.dump(), "application/json");
    if (!res) return {0, "", "No se pudo contactar la API escritora (" + httplib::to_string(res.error()) + ")"};
    return {res->status, res->body, ""};
}

// Registra el fallo definitivo en el escritor único; si el escritor devuelve error transitorio
// (p. ej. 503 o caída), persiste la reconciliación en la cola durable de fallos pendientes
// (con publisher confirm) antes de rechazar el mensaje original hacia la DLQ.
bool reportarFalloDefinitivoDurable(const WorkerConfig& cfg, const std::string& serviceToken,
                                    RabbitEventConsumer& consumer, const std::string& idAviso,
                                    const std::string& idEvento, std::uint32_t intentos,
                                    const std::string& errorText) {
    const Json payload = {
        {"idEvento", idEvento},
        {"intentos", intentos},
        {"error", sanitizeErrorText(errorText)},
        {"definitivo", true}
    };
    const auto res = postInternal(cfg.apiUrl, serviceToken, "/api/interno/avisos/" + idAviso + "/fallo", payload);
    if (res.status == 200 || res.status == 400 || res.status == 404) return true;
    Json rec = payload;
    rec["idAviso"] = idAviso;
    consumer.publishPendingFailureReport(rec.dump(), idEvento, idAviso);
    std::cerr << "[worker] Escritor no pudo registrar fallo final de " << idAviso
              << " (HTTP " << res.status << "); encolada reconciliación durable en "
              << cfg.amqp.failedReportQueue << std::endl;
    return false;
}

void reconciliarFallosPendientes(const WorkerConfig& cfg, const std::string& serviceToken,
                                 RabbitEventConsumer& consumer,
                                 std::chrono::steady_clock::time_point& nextReconcileAt,
                                 int& reconcileBackoffMs) {
    const auto now = std::chrono::steady_clock::now();
    if (now < nextReconcileAt) return;
    for (int drained = 0; drained < 16 && !g_stop; ++drained) {
        auto maybeRec = consumer.pollPendingFailureReport();
        if (!maybeRec) {
            reconcileBackoffMs = cfg.backoffMs;
            nextReconcileAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(cfg.backoffMs);
            return;
        }
        const auto& recMsg = *maybeRec;
        try {
            const auto rec = Json::parse(recMsg.body);
            const auto idAviso = rec.at("idAviso").get<std::string>();
            const auto idEvento = rec.at("idEvento").get<std::string>();
            const auto intentos = rec.at("intentos").get<std::uint32_t>();
            const auto errorText = rec.at("error").get<std::string>();
            const Json payload = {
                {"idEvento", idEvento},
                {"intentos", intentos},
                {"error", sanitizeErrorText(errorText)},
                {"definitivo", true}
            };
            const auto res = postInternal(cfg.apiUrl, serviceToken, "/api/interno/avisos/" + idAviso + "/fallo", payload);
            if (res.status == 200 || res.status == 400 || res.status == 404) {
                consumer.ackPendingFailureReport(recMsg.deliveryTag);
                std::cout << "[worker] Fallo definitivo reconciliado en el escritor para " << idAviso
                          << " (HTTP " << res.status << ")" << std::endl;
                reconcileBackoffMs = cfg.backoffMs;
            } else {
                consumer.requeuePendingFailureReport(recMsg.deliveryTag);
                nextReconcileAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(reconcileBackoffMs);
                reconcileBackoffMs = std::min(reconcileBackoffMs * 2, 5000);
                return;
            }
        } catch (const std::exception& e) {
            std::cerr << "[worker] Registro corrupto en " << cfg.amqp.failedReportQueue
                      << " descartado: " << sanitizeErrorText(e.what()) << std::endl;
            consumer.ackPendingFailureReport(recMsg.deliveryTag);
        }
    }
    nextReconcileAt = std::chrono::steady_clock::now();
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
    std::string serviceToken;
    try {
        if (sodium_init() < 0) throw std::runtime_error("No se pudo iniciar libsodium");
        const auto cfg = parseArgs(argc, argv);
        serviceToken = ServiceIdentity::readSecretFile(cfg.serviceTokenFile);
        RabbitEventConsumer consumer(cfg.amqp);
        std::cout << "Worker de avisos iniciado · broker " << cfg.amqp.host << ":" << cfg.amqp.port
                  << " · cola " << cfg.amqp.queue << " · reintentos máximos " << cfg.maxRetries << std::endl;

        int reconnectDelayMs = cfg.backoffMs;
        int reconcileBackoffMs = cfg.backoffMs;
        auto nextReconcileAt = std::chrono::steady_clock::now();
        while (!g_stop) {
            try {
                if (!consumer.connected()) {
                    consumer.connect();
                    reconnectDelayMs = cfg.backoffMs;
                    reconcileBackoffMs = cfg.backoffMs;
                    nextReconcileAt = std::chrono::steady_clock::now();
                    std::cout << "[worker] Conectado a RabbitMQ y consumiendo con ACK manual." << std::endl;
                }
                reconciliarFallosPendientes(cfg, serviceToken, consumer, nextReconcileAt, reconcileBackoffMs);
                auto maybeMsg = consumer.consumeNext(500);
                if (!maybeMsg) continue;
                const auto& msg = *maybeMsg;
                const std::uint32_t intentoActual = msg.intentosHeader + 1U;

                Json ev;
                bool malformed = false;
                std::string parseError;
                try {
                    ev = Json::parse(msg.body);
                    campos(ev, {"idEvento","idAviso","tipo","idUsuario","idReserva","idPrestamo","creado"});
                    validarId(ev.at("idAviso").get<std::string>());
                    validarId(ev.at("idEvento").get<std::string>());
                    validarId(ev.at("idUsuario").get<std::string>());
                    (void)redactarNotificacion(ev);
                } catch (const std::exception& e) {
                    malformed = true;
                    parseError = sanitizeErrorText(e.what());
                }

                if (malformed) {
                    std::cerr << "[worker] Mensaje inválido enviado a DLQ: " << parseError << std::endl;
                    if (ev.is_object() && ev.contains("idAviso") && ev.at("idAviso").is_string() &&
                        ev.contains("idEvento") && ev.at("idEvento").is_string()) {
                        if (!reportarFalloDefinitivoDurable(cfg, serviceToken, consumer,
                                                            ev.at("idAviso").get<std::string>(),
                                                            ev.at("idEvento").get<std::string>(),
                                                            intentoActual, "Mensaje inválido: " + parseError)) {
                            nextReconcileAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(reconcileBackoffMs);
                        }
                    }
                    consumer.rejectToDlq(msg.deliveryTag);
                    continue;
                }

                const auto idAviso = ev.at("idAviso").get<std::string>();
                const auto idEvento = ev.at("idEvento").get<std::string>();
                const auto [titulo, mensaje] = redactarNotificacion(ev);
                const auto res = postInternal(cfg.apiUrl, serviceToken,
                                              "/api/interno/avisos/" + idAviso + "/procesar",
                                              {{"idEvento", idEvento}, {"titulo", titulo},
                                               {"mensaje", mensaje}, {"intentos", intentoActual}});

                if (res.status == 200) {
                    if (!cfg.testFailBeforeAck.empty() && cfg.testFailBeforeAck == idAviso) {
                        std::cerr << "[worker] Caída controlada antes de ACK tras persistir " << idAviso << std::endl;
                        consumer.close();
                        sodium_memzero(serviceToken.data(), serviceToken.size());
                        return 42;
                    }
                    consumer.ack(msg.deliveryTag);
                    bool duplicado = false;
                    try { duplicado = Json::parse(res.body).value("duplicado", false); } catch (...) {}
                    std::cout << "[worker] " << idAviso << " (" << idEvento << ") confirmado y ACK enviado"
                              << (duplicado ? " [duplicado idempotente]" : "") << std::endl;
                    continue;
                }

                if (res.status == 400 || res.status == 404) {
                    const auto detalle = sanitizeErrorText("Rechazo definitivo HTTP " + std::to_string(res.status));
                    std::cerr << "[worker] " << idAviso << " rechazado definitivamente -> DLQ: " << detalle << std::endl;
                    if (!reportarFalloDefinitivoDurable(cfg, serviceToken, consumer, idAviso, idEvento, intentoActual, detalle)) {
                        nextReconcileAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(reconcileBackoffMs);
                    }
                    consumer.rejectToDlq(msg.deliveryTag);
                    continue;
                }

                // Error transitorio (API caída, 503 bloqueo/incertidumbre, timeout, etc.)
                const auto detalle = sanitizeErrorText(res.status == 0 ? res.error : ("HTTP " + std::to_string(res.status) + " del escritor"));
                std::cerr << "[worker] Fallo transitorio en " << idAviso << " (intento " << intentoActual
                          << "/" << cfg.maxRetries << "): " << detalle << std::endl;

                if (intentoActual < cfg.maxRetries) {
                    const int waitMs = std::min(cfg.backoffMs * (1 << std::min<std::uint32_t>(intentoActual - 1U, 4U)), 10000);
                    sleepInterruptible(waitMs);
                    if (g_stop) break;
                    // Republicar con x-intentos incrementado y publisher confirm ANTES de hacer ACK del original.
                    consumer.republishForRetry(msg, intentoActual);
                    consumer.ack(msg.deliveryTag);
                } else {
                    std::cerr << "[worker] Reintentos agotados para " << idAviso << " -> DLQ." << std::endl;
                    if (!reportarFalloDefinitivoDurable(cfg, serviceToken, consumer, idAviso, idEvento,
                                                        intentoActual, "Reintentos agotados: " + detalle)) {
                        nextReconcileAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(reconcileBackoffMs);
                    }
                    consumer.rejectToDlq(msg.deliveryTag);
                }
            } catch (const std::exception& e) {
                std::cerr << "[worker] Error de conexión/protocolo AMQP: " << sanitizeErrorText(e.what()) << std::endl;
                consumer.close();
                if (!g_stop) {
                    sleepInterruptible(reconnectDelayMs);
                    reconnectDelayMs = std::min(reconnectDelayMs * 2, 5000);
                }
            }
        }
        consumer.close();
        std::cout << "[worker] Cierre ordenado completado." << std::endl;
        sodium_memzero(serviceToken.data(), serviceToken.size());
        return 0;
    } catch (const std::exception& e) {
        if (!serviceToken.empty()) sodium_memzero(serviceToken.data(), serviceToken.size());
        std::cerr << "Error en worker: " << sanitizeErrorText(e.what()) << '\n';
        return 1;
    }
}
