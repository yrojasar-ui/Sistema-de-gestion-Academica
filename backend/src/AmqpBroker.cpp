#include "biblioteca/AmqpBroker.hpp"
#include "biblioteca/DomainJson.hpp"
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/framing.h>
#include <rabbitmq-c/tcp_socket.h>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

namespace biblioteca {
namespace {

std::string amqpBytesToString(amqp_bytes_t b) {
    if (!b.bytes || b.len == 0) return {};
    return std::string(static_cast<const char*>(b.bytes), b.len);
}

std::string methodCloseError(const amqp_method_t& method, const std::string& context) {
    if (method.id == AMQP_CONNECTION_CLOSE_METHOD && method.decoded != nullptr) {
        auto* m = static_cast<amqp_connection_close_t*>(method.decoded);
        return context + ": cierre de conexión AMQP (" + std::to_string(m->reply_code) + ") " +
               amqpBytesToString(m->reply_text);
    }
    if (method.id == AMQP_CHANNEL_CLOSE_METHOD && method.decoded != nullptr) {
        auto* m = static_cast<amqp_channel_close_t*>(method.decoded);
        return context + ": cierre de canal AMQP (" + std::to_string(m->reply_code) + ") " +
               amqpBytesToString(m->reply_text);
    }
    if (method.id == AMQP_BASIC_CANCEL_METHOD) {
        return context + ": consumidor cancelado por el broker (basic.cancel)";
    }
    return context + ": error de protocolo AMQP";
}

std::string replyError(const amqp_rpc_reply_t& r, const std::string& context) {
    if (r.reply_type == AMQP_RESPONSE_NORMAL) return "";
    if (r.reply_type == AMQP_RESPONSE_NONE) return context + ": sin respuesta RPC del broker";
    if (r.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION)
        return context + ": " + amqp_error_string2(r.library_error);
    if (r.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION)
        return methodCloseError(r.reply, context);
    return context + ": error de protocolo AMQP";
}

void requireReply(const amqp_rpc_reply_t& r, const std::string& context) {
    if (r.reply_type != AMQP_RESPONSE_NORMAL) throw std::runtime_error(replyError(r, context));
}

timeval makeTimeval(int timeoutMs) {
    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    return tv;
}

void declareTopology(amqp_connection_state_t conn, amqp_channel_t ch, const AmqpConfig& cfg) {
    amqp_exchange_declare(conn, ch, amqp_cstring_bytes(cfg.exchange.c_str()),
                          amqp_cstring_bytes("direct"), 0, /*durable=*/1, 0, 0, amqp_empty_table);
    requireReply(amqp_get_rpc_reply(conn), "exchange.declare principal");

    amqp_exchange_declare(conn, ch, amqp_cstring_bytes(cfg.dlxExchange.c_str()),
                          amqp_cstring_bytes("direct"), 0, /*durable=*/1, 0, 0, amqp_empty_table);
    requireReply(amqp_get_rpc_reply(conn), "exchange.declare DLX");

    if (!cfg.declareQueues) return;

    amqp_queue_declare(conn, ch, amqp_cstring_bytes(cfg.dlqQueue.c_str()),
                       0, /*durable=*/1, 0, 0, amqp_empty_table);
    requireReply(amqp_get_rpc_reply(conn), "queue.declare DLQ");

    amqp_queue_bind(conn, ch, amqp_cstring_bytes(cfg.dlqQueue.c_str()),
                    amqp_cstring_bytes(cfg.dlxExchange.c_str()),
                    amqp_cstring_bytes(cfg.dlqRoutingKey.c_str()), amqp_empty_table);
    requireReply(amqp_get_rpc_reply(conn), "queue.bind DLQ");

    amqp_queue_declare(conn, ch, amqp_cstring_bytes(cfg.failedReportQueue.c_str()),
                       0, /*durable=*/1, 0, 0, amqp_empty_table);
    requireReply(amqp_get_rpc_reply(conn), "queue.declare fallo_pendiente");

    amqp_queue_bind(conn, ch, amqp_cstring_bytes(cfg.failedReportQueue.c_str()),
                    amqp_cstring_bytes(cfg.dlxExchange.c_str()),
                    amqp_cstring_bytes(cfg.failedReportRoutingKey.c_str()), amqp_empty_table);
    requireReply(amqp_get_rpc_reply(conn), "queue.bind fallo_pendiente");

    amqp_table_entry_t argsEntries[2];
    argsEntries[0].key = amqp_cstring_bytes("x-dead-letter-exchange");
    argsEntries[0].value.kind = AMQP_FIELD_KIND_UTF8;
    argsEntries[0].value.value.bytes = amqp_cstring_bytes(cfg.dlxExchange.c_str());
    argsEntries[1].key = amqp_cstring_bytes("x-dead-letter-routing-key");
    argsEntries[1].value.kind = AMQP_FIELD_KIND_UTF8;
    argsEntries[1].value.value.bytes = amqp_cstring_bytes(cfg.dlqRoutingKey.c_str());
    amqp_table_t queueArgs{2, argsEntries};

    amqp_queue_declare(conn, ch, amqp_cstring_bytes(cfg.queue.c_str()),
                       0, /*durable=*/1, 0, 0, queueArgs);
    requireReply(amqp_get_rpc_reply(conn), "queue.declare principal");

    amqp_queue_bind(conn, ch, amqp_cstring_bytes(cfg.queue.c_str()),
                    amqp_cstring_bytes(cfg.exchange.c_str()),
                    amqp_cstring_bytes(cfg.routingKey.c_str()), amqp_empty_table);
    requireReply(amqp_get_rpc_reply(conn), "queue.bind principal");
}

void publishWithConfirm(amqp_connection_state_t conn, amqp_channel_t ch, const AmqpConfig& cfg,
                        const std::string& exchange, const std::string& routingKey,
                        const std::string& payload, const std::string& messageId,
                        const std::string& correlationId, std::uint32_t intentos) {
    amqp_basic_properties_t props{};
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG |
                   AMQP_BASIC_MESSAGE_ID_FLAG | AMQP_BASIC_CORRELATION_ID_FLAG |
                   AMQP_BASIC_HEADERS_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = AMQP_DELIVERY_PERSISTENT; // 2 = persistente
    props.message_id = amqp_cstring_bytes(messageId.c_str());
    props.correlation_id = amqp_cstring_bytes(correlationId.c_str());

    amqp_table_entry_t hdrEntries[1];
    hdrEntries[0].key = amqp_cstring_bytes("x-intentos");
    hdrEntries[0].value.kind = AMQP_FIELD_KIND_U32;
    hdrEntries[0].value.value.u32 = intentos;
    props.headers = amqp_table_t{1, hdrEntries};

    const int rc = amqp_basic_publish(conn, ch, amqp_cstring_bytes(exchange.c_str()),
                                      amqp_cstring_bytes(routingKey.c_str()),
                                      /*mandatory=*/1, /*immediate=*/0, &props,
                                      amqp_bytes_t{payload.size(), const_cast<char*>(payload.data())});
    if (rc != AMQP_STATUS_OK)
        throw std::runtime_error(std::string("basic.publish falló: ") + amqp_error_string2(rc));

    bool returned = false;
    std::string returnText;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(cfg.timeoutMs);
    while (true) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) throw std::runtime_error("Timeout esperando publisher confirm del broker");
        const auto remainingMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        timeval tv = makeTimeval(std::max(50, remainingMs));
        amqp_frame_t frame{};
        const int st = amqp_simple_wait_frame_noblock(conn, &frame, &tv);
        if (st == AMQP_STATUS_TIMEOUT) throw std::runtime_error("Timeout esperando publisher confirm del broker");
        if (st != AMQP_STATUS_OK)
            throw std::runtime_error(std::string("Error leyendo confirmación del broker: ") + amqp_error_string2(st));

        if (frame.frame_type == AMQP_FRAME_METHOD && frame.payload.method.id == AMQP_CONNECTION_CLOSE_METHOD) {
            throw std::runtime_error(methodCloseError(frame.payload.method, "basic.publish"));
        }
        if (frame.frame_type == AMQP_FRAME_METHOD && frame.channel == ch) {
            if (frame.payload.method.id == AMQP_BASIC_RETURN_METHOD) {
                auto* ret = static_cast<amqp_basic_return_t*>(frame.payload.method.decoded);
                returnText = ret ? amqpBytesToString(ret->reply_text) : std::string{};
                returned = true;
                // Drenar cabecera y cuerpo del mensaje devuelto (basic.return).
                amqp_frame_t hdr{};
                timeval tv2 = makeTimeval(cfg.timeoutMs);
                if (amqp_simple_wait_frame_noblock(conn, &hdr, &tv2) == AMQP_STATUS_OK &&
                    hdr.frame_type == AMQP_FRAME_HEADER) {
                    std::uint64_t remaining = hdr.payload.properties.body_size;
                    while (remaining > 0) {
                        amqp_frame_t bodyFrame{};
                        timeval tv3 = makeTimeval(cfg.timeoutMs);
                        if (amqp_simple_wait_frame_noblock(conn, &bodyFrame, &tv3) != AMQP_STATUS_OK) break;
                        if (bodyFrame.frame_type == AMQP_FRAME_BODY) {
                            const auto chunk = bodyFrame.payload.body_fragment.len;
                            remaining = chunk >= remaining ? 0 : remaining - chunk;
                        }
                    }
                }
                continue;
            }
            if (frame.payload.method.id == AMQP_BASIC_ACK_METHOD) {
                amqp_maybe_release_buffers_on_channel(conn, ch);
                if (returned)
                    throw std::runtime_error("Mensaje sin ruta en broker (basic.return): " + returnText);
                return;
            }
            if (frame.payload.method.id == AMQP_BASIC_NACK_METHOD) {
                amqp_maybe_release_buffers_on_channel(conn, ch);
                throw std::runtime_error("Broker rechazó la publicación (basic.nack)");
            }
            if (frame.payload.method.id == AMQP_CHANNEL_CLOSE_METHOD) {
                throw std::runtime_error(methodCloseError(frame.payload.method, "basic.publish"));
            }
        }
    }
}

amqp_connection_state_t openAmqpConnection(const AmqpConfig& cfg) {
    if (cfg.host.empty() || cfg.port < 1 || cfg.port > 65535)
        throw std::invalid_argument("Configuración AMQP inválida");
    amqp_connection_state_t conn = amqp_new_connection();
    if (!conn) throw std::runtime_error("No se pudo asignar conexión AMQP");
    try {
        amqp_socket_t* socket = amqp_tcp_socket_new(conn);
        if (!socket) throw std::runtime_error("No se pudo crear socket TCP AMQP");
        timeval tv = makeTimeval(cfg.timeoutMs);
        const int st = amqp_socket_open_noblock(socket, cfg.host.c_str(), cfg.port, &tv);
        if (st != AMQP_STATUS_OK)
            throw std::runtime_error(std::string("No se pudo conectar a RabbitMQ: ") + amqp_error_string2(st));
        amqp_table_entry_t capEntry{};
        capEntry.key = amqp_cstring_bytes("consumer_cancel_notify");
        capEntry.value.kind = AMQP_FIELD_KIND_BOOLEAN;
        capEntry.value.value.boolean = 1;
        amqp_table_t capabilities{1, &capEntry};

        amqp_table_entry_t propEntry{};
        propEntry.key = amqp_cstring_bytes("capabilities");
        propEntry.value.kind = AMQP_FIELD_KIND_TABLE;
        propEntry.value.value.table = capabilities;
        amqp_table_t clientProps{1, &propEntry};

        const auto loginReply = amqp_login_with_properties(
            conn, cfg.vhost.c_str(), 0, 131072, cfg.heartbeatSec, &clientProps,
            AMQP_SASL_METHOD_PLAIN, cfg.user.c_str(), cfg.password.c_str());
        requireReply(loginReply, "amqp_login");
        return conn;
    } catch (...) {
        amqp_destroy_connection(conn);
        throw;
    }
}

void closeAmqpConnection(amqp_connection_state_t& conn, std::initializer_list<amqp_channel_t> channels) noexcept {
    if (!conn) return;
    for (auto ch : channels) (void)amqp_channel_close(conn, ch, AMQP_REPLY_SUCCESS);
    (void)amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    (void)amqp_destroy_connection(conn);
    conn = nullptr;
}

std::uint32_t extractAttemptsHeader(const amqp_basic_properties_t& props) {
    if (!(props._flags & AMQP_BASIC_HEADERS_FLAG)) return 0;
    for (int i = 0; i < props.headers.num_entries; ++i) {
        const auto& e = props.headers.entries[i];
        if (amqpBytesToString(e.key) == "x-intentos") {
            switch (e.value.kind) {
            case AMQP_FIELD_KIND_U32: return e.value.value.u32;
            case AMQP_FIELD_KIND_I32: return e.value.value.i32 > 0 ? static_cast<std::uint32_t>(e.value.value.i32) : 0U;
            case AMQP_FIELD_KIND_U64: return static_cast<std::uint32_t>(std::min<std::uint64_t>(e.value.value.u64, 1000U));
            case AMQP_FIELD_KIND_I64: return e.value.value.i64 > 0 ? static_cast<std::uint32_t>(std::min<std::int64_t>(e.value.value.i64, 1000)) : 0U;
            default: return 0;
            }
        }
    }
    return 0;
}

} // namespace

std::string sanitizeErrorText(const std::string& raw, std::size_t maxLen) {
    std::string out;
    out.reserve(std::min(raw.size(), maxLen));
    for (char c : raw) {
        const auto uc = static_cast<unsigned char>(c);
        if (uc == '\n' || uc == '\r' || uc == '\t') out.push_back(' ');
        else if (uc >= 32) out.push_back(c);
        if (out.size() >= maxLen) break;
    }
    // Redactar cualquier posible esquema con credenciales amqp://user:pass@host
    const auto scheme = out.find("://");
    if (scheme != std::string::npos) {
        const auto at = out.find('@', scheme + 3);
        if (at != std::string::npos) out.replace(scheme + 3, at - (scheme + 3), "***");
    }
    if (out.empty()) out = "Error de mensajería";
    return out;
}

void loadAmqpCredentialsFile(const std::filesystem::path& file, AmqpConfig& cfg) {
    const auto path = std::filesystem::absolute(file).lexically_normal();
    struct stat info{};
    if (::lstat(path.parent_path().c_str(), &info) || info.st_uid != ::geteuid() || (info.st_mode & 0077) || !S_ISDIR(info.st_mode))
        throw std::runtime_error("Directorio de credenciales AMQP requiere permisos 0700 y propietario actual");
    if (::lstat(path.c_str(), &info) || info.st_uid != ::geteuid() || (info.st_mode & 0077) || !S_ISREG(info.st_mode))
        throw std::runtime_error("Archivo de credenciales AMQP requiere permisos 0600 y propietario actual");
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const auto k = line.substr(0, eq);
        const auto v = line.substr(eq + 1);
        if (k == "user") cfg.user = v;
        else if (k == "password") cfg.password = v;
        else if (k == "vhost") cfg.vhost = v;
    }
}

struct RabbitEventPublisher::Impl {
    amqp_connection_state_t conn = nullptr;
    static constexpr amqp_channel_t channel = 1;
};

RabbitEventPublisher::RabbitEventPublisher(AmqpConfig cfg)
    : cfg_(std::move(cfg)), impl_(std::make_unique<Impl>()) {}

RabbitEventPublisher::~RabbitEventPublisher() { close(); }

void RabbitEventPublisher::close() noexcept {
    if (impl_) closeAmqpConnection(impl_->conn, {Impl::channel});
}

void RabbitEventPublisher::ensureConnected() {
    if (impl_->conn) return;
    amqp_connection_state_t conn = openAmqpConnection(cfg_);
    try {
        amqp_channel_open(conn, Impl::channel);
        requireReply(amqp_get_rpc_reply(conn), "channel.open publicador");
        declareTopology(conn, Impl::channel, cfg_);
        amqp_confirm_select(conn, Impl::channel);
        requireReply(amqp_get_rpc_reply(conn), "confirm.select");
        impl_->conn = conn;
    } catch (...) {
        closeAmqpConnection(conn, {Impl::channel});
        throw;
    }
}

void RabbitEventPublisher::publishRaw(const std::string& routingKey, const std::string& payload,
                                      const std::string& messageId, const std::string& correlationId,
                                      std::uint32_t intentos) {
    try {
        ensureConnected();
        publishWithConfirm(impl_->conn, Impl::channel, cfg_, cfg_.exchange, routingKey, payload, messageId, correlationId, intentos);
    } catch (...) {
        close();
        throw;
    }
}

void RabbitEventPublisher::publish(const Aviso& aviso) {
    const Json payload = {
        {"idEvento", aviso.idEvento},
        {"idAviso", aviso.idAviso},
        {"tipo", aviso.tipo},
        {"idUsuario", aviso.idUsuario},
        {"idReserva", aviso.idReserva},
        {"idPrestamo", aviso.idPrestamo},
        {"creado", aviso.creado}
    };
    publishRaw(cfg_.routingKey, payload.dump(), aviso.idEvento, aviso.idAviso, aviso.intentos);
}

AvisoOutboxRelay::AvisoOutboxRelay(std::mutex& stateMutex, LibraryService& library,
                                   IEventPublisher& publisher, int pollIntervalMs)
    : stateMutex_(stateMutex), library_(library), publisher_(publisher),
      pollIntervalMs_(std::max(100, pollIntervalMs)) {
    status_.configurado = true;
}

AvisoOutboxRelay::~AvisoOutboxRelay() { stop(); }

void AvisoOutboxRelay::start() {
    stop_ = false;
    thread_ = std::thread([this] { loop(); });
}

void AvisoOutboxRelay::stop() {
    stop_ = true;
    {
        std::lock_guard<std::mutex> lk(wakeMutex_);
        wakeRequested_ = true;
    }
    wakeCv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void AvisoOutboxRelay::notify() {
    {
        std::lock_guard<std::mutex> lk(wakeMutex_);
        wakeRequested_ = true;
    }
    wakeCv_.notify_one();
}

BrokerStatus AvisoOutboxRelay::status() const {
    std::lock_guard<std::mutex> lk(statusMutex_);
    return status_;
}

void AvisoOutboxRelay::loop() {
    int backoffMs = pollIntervalMs_;
    while (!stop_) {
        std::vector<Aviso> pendientes;
        {
            std::lock_guard<std::mutex> lk(stateMutex_);
            if (library_.repository().ready()) {
                for (const auto& a : library_.estado().avisos)
                    if (a.estado == "pendiente") pendientes.push_back(a);
            }
        }
        bool huboFallo = false;
        for (const auto& aviso : pendientes) {
            if (stop_) break;
            try {
                // Publicar FUERA de stateMutex_ para no bloquear solicitudes HTTP.
                publisher_.publish(aviso);
                {
                    std::lock_guard<std::mutex> lk(statusMutex_);
                    status_.conectado = true;
                    status_.ultimoError.clear();
                }
                std::lock_guard<std::mutex> lk(stateMutex_);
                if (library_.repository().ready()) {
                    try { library_.marcarAvisoPublicado(aviso.idAviso); }
                    catch (const std::exception& e) {
                        std::cerr << "[outbox] Aviso publicado en broker pero persistencia falló ("
                                  << aviso.idAviso << "): " << sanitizeErrorText(e.what()) << '\n';
                        huboFallo = true;
                        break;
                    }
                }
                backoffMs = pollIntervalMs_;
            } catch (const std::exception& e) {
                const auto msg = sanitizeErrorText(e.what());
                {
                    std::lock_guard<std::mutex> lk(statusMutex_);
                    status_.conectado = false;
                    status_.ultimoError = msg;
                }
                std::cerr << "[outbox] RabbitMQ no disponible o sin confirmación para "
                          << aviso.idAviso << ": " << msg << '\n';
                huboFallo = true;
                break;
            }
        }
        const int waitMs = huboFallo ? backoffMs : pollIntervalMs_;
        if (huboFallo) backoffMs = std::min(backoffMs * 2, 5000);
        std::unique_lock<std::mutex> lk(wakeMutex_);
        wakeCv_.wait_for(lk, std::chrono::milliseconds(waitMs), [this] { return stop_.load() || wakeRequested_; });
        wakeRequested_ = false;
    }
}

struct RabbitEventConsumer::Impl {
    amqp_connection_state_t conn = nullptr;
    amqp_connection_state_t reconcileConn = nullptr;
    static constexpr amqp_channel_t consumeChannel = 1;
    static constexpr amqp_channel_t retryChannel = 2;
    static constexpr amqp_channel_t reconcileChannel = 1;
};

RabbitEventConsumer::RabbitEventConsumer(AmqpConfig cfg)
    : cfg_(std::move(cfg)), impl_(std::make_unique<Impl>()) {}

RabbitEventConsumer::~RabbitEventConsumer() { close(); }

bool RabbitEventConsumer::connected() const { return impl_ && impl_->conn != nullptr && impl_->reconcileConn != nullptr; }

void RabbitEventConsumer::close() noexcept {
    if (impl_) {
        closeAmqpConnection(impl_->conn, {Impl::consumeChannel, Impl::retryChannel});
        closeAmqpConnection(impl_->reconcileConn, {Impl::reconcileChannel});
    }
}

void RabbitEventConsumer::connect() {
    close();
    amqp_connection_state_t conn = openAmqpConnection(cfg_);
    amqp_connection_state_t rconn = nullptr;
    try {
        amqp_channel_open(conn, Impl::consumeChannel);
        requireReply(amqp_get_rpc_reply(conn), "channel.open consumidor");
        declareTopology(conn, Impl::consumeChannel, cfg_);
        amqp_basic_qos(conn, Impl::consumeChannel, 0, /*prefetch_count=*/1, 0);
        requireReply(amqp_get_rpc_reply(conn), "basic.qos");
        amqp_basic_consume(conn, Impl::consumeChannel, amqp_cstring_bytes(cfg_.queue.c_str()),
                           amqp_empty_bytes, /*no_local=*/0, /*no_ack=*/0, /*exclusive=*/0, amqp_empty_table);
        requireReply(amqp_get_rpc_reply(conn), "basic.consume");

        amqp_channel_open(conn, Impl::retryChannel);
        requireReply(amqp_get_rpc_reply(conn), "channel.open reintentos");
        amqp_confirm_select(conn, Impl::retryChannel);
        requireReply(amqp_get_rpc_reply(conn), "confirm.select reintentos");

        rconn = openAmqpConnection(cfg_);
        amqp_channel_open(rconn, Impl::reconcileChannel);
        requireReply(amqp_get_rpc_reply(rconn), "channel.open reconciliación");
        amqp_confirm_select(rconn, Impl::reconcileChannel);
        requireReply(amqp_get_rpc_reply(rconn), "confirm.select reconciliación");

        impl_->conn = conn;
        impl_->reconcileConn = rconn;
    } catch (...) {
        closeAmqpConnection(rconn, {Impl::reconcileChannel});
        closeAmqpConnection(conn, {Impl::consumeChannel, Impl::retryChannel});
        throw;
    }
}

std::optional<AmqpMessage> RabbitEventConsumer::consumeNext(int timeoutMs) {
    if (!impl_->conn) throw std::runtime_error("Consumidor no conectado");
    amqp_maybe_release_buffers(impl_->conn);
    amqp_envelope_t envelope{};
    timeval tv = makeTimeval(timeoutMs);
    const auto res = amqp_consume_message(impl_->conn, &envelope, &tv, 0);
    if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION) {
        if (res.library_error == AMQP_STATUS_TIMEOUT) return std::nullopt;
        if (res.library_error == AMQP_STATUS_UNEXPECTED_STATE) {
            amqp_frame_t frame{};
            timeval tv2 = makeTimeval(200);
            const int st = amqp_simple_wait_frame_noblock(impl_->conn, &frame, &tv2);
            if (st == AMQP_STATUS_TIMEOUT) return std::nullopt;
            if (st != AMQP_STATUS_OK) {
                const auto err = std::string("Conexión AMQP interrumpida: ") + amqp_error_string2(st);
                close();
                throw std::runtime_error(err);
            }
            if (frame.frame_type == AMQP_FRAME_METHOD &&
                (frame.payload.method.id == AMQP_CONNECTION_CLOSE_METHOD ||
                 frame.payload.method.id == AMQP_CHANNEL_CLOSE_METHOD ||
                 frame.payload.method.id == AMQP_BASIC_CANCEL_METHOD)) {
                // Construir el mensaje ANTES de close(), ya que close() destruye el pool del frame decodificado.
                const auto err = methodCloseError(frame.payload.method, "basic.consume");
                close();
                throw std::runtime_error(err);
            }
            return std::nullopt;
        }
        const auto err = std::string("Error consumiendo mensaje AMQP: ") + amqp_error_string2(res.library_error);
        close();
        throw std::runtime_error(err);
    }
    if (res.reply_type != AMQP_RESPONSE_NORMAL) {
        // Construir el texto de replyError ANTES de close() para no leer memoria liberada.
        const auto err = replyError(res, "basic.consume");
        close();
        throw std::runtime_error(err);
    }
    AmqpMessage msg;
    msg.deliveryTag = envelope.delivery_tag;
    msg.redelivered = envelope.redelivered != 0;
    msg.intentosHeader = extractAttemptsHeader(envelope.message.properties);
    if (envelope.message.properties._flags & AMQP_BASIC_MESSAGE_ID_FLAG)
        msg.messageId = amqpBytesToString(envelope.message.properties.message_id);
    if (envelope.message.properties._flags & AMQP_BASIC_CORRELATION_ID_FLAG)
        msg.correlationId = amqpBytesToString(envelope.message.properties.correlation_id);
    msg.body = amqpBytesToString(envelope.message.body);
    amqp_destroy_envelope(&envelope);
    return msg;
}

void RabbitEventConsumer::ack(std::uint64_t deliveryTag) {
    if (!impl_->conn) throw std::runtime_error("Consumidor sin conexión para ACK");
    const int rc = amqp_basic_ack(impl_->conn, Impl::consumeChannel, deliveryTag, 0);
    if (rc != AMQP_STATUS_OK) {
        close();
        throw std::runtime_error(std::string("basic.ack falló: ") + amqp_error_string2(rc));
    }
}

void RabbitEventConsumer::rejectToDlq(std::uint64_t deliveryTag) {
    if (!impl_->conn) throw std::runtime_error("Consumidor sin conexión para reject");
    const int rc = amqp_basic_reject(impl_->conn, Impl::consumeChannel, deliveryTag, /*requeue=*/0);
    if (rc != AMQP_STATUS_OK) {
        close();
        throw std::runtime_error(std::string("basic.reject falló: ") + amqp_error_string2(rc));
    }
}

void RabbitEventConsumer::republishForRetry(const AmqpMessage& msg, std::uint32_t nextAttempt) {
    if (!impl_->conn) throw std::runtime_error("Consumidor sin conexión para republicar reintento");
    try {
        publishWithConfirm(impl_->conn, Impl::retryChannel, cfg_, cfg_.exchange, cfg_.routingKey, msg.body,
                           msg.messageId, msg.correlationId, nextAttempt);
    } catch (...) {
        close();
        throw;
    }
}

void RabbitEventConsumer::publishPendingFailureReport(const std::string& payload, const std::string& messageId,
                                                      const std::string& correlationId) {
    if (!impl_->reconcileConn) throw std::runtime_error("Sin conexión AMQP para publicar fallo pendiente");
    try {
        publishWithConfirm(impl_->reconcileConn, Impl::reconcileChannel, cfg_, cfg_.dlxExchange,
                           cfg_.failedReportRoutingKey, payload, messageId, correlationId, 0U);
    } catch (...) {
        close();
        throw;
    }
}

std::optional<AmqpMessage> RabbitEventConsumer::pollPendingFailureReport() {
    if (!impl_->reconcileConn) throw std::runtime_error("Sin conexión AMQP para consultar fallos pendientes");
    amqp_maybe_release_buffers(impl_->reconcileConn);
    const auto r = amqp_basic_get(impl_->reconcileConn, Impl::reconcileChannel,
                                  amqp_cstring_bytes(cfg_.failedReportQueue.c_str()), /*no_ack=*/0);
    if (r.reply_type != AMQP_RESPONSE_NORMAL) {
        // Construir replyError ANTES de close() para no leer la respuesta decodificada tras liberar la conexión.
        const auto err = replyError(r, "basic.get fallo_pendiente");
        close();
        throw std::runtime_error(err);
    }
    if (r.reply.id == AMQP_BASIC_GET_EMPTY_METHOD) return std::nullopt;
    if (r.reply.id != AMQP_BASIC_GET_OK_METHOD || r.reply.decoded == nullptr) {
        close();
        throw std::runtime_error("Respuesta inesperada en basic.get fallo_pendiente");
    }
    auto* ok = static_cast<amqp_basic_get_ok_t*>(r.reply.decoded);
    const std::uint64_t deliveryTag = ok->delivery_tag;
    const bool redelivered = ok->redelivered != 0;

    amqp_message_t message{};
    const auto readReply = amqp_read_message(impl_->reconcileConn, Impl::reconcileChannel, &message, 0);
    if (readReply.reply_type != AMQP_RESPONSE_NORMAL) {
        const auto err = replyError(readReply, "read_message fallo_pendiente");
        close();
        throw std::runtime_error(err);
    }
    AmqpMessage msg;
    msg.deliveryTag = deliveryTag;
    msg.redelivered = redelivered;
    if (message.properties._flags & AMQP_BASIC_MESSAGE_ID_FLAG)
        msg.messageId = amqpBytesToString(message.properties.message_id);
    if (message.properties._flags & AMQP_BASIC_CORRELATION_ID_FLAG)
        msg.correlationId = amqpBytesToString(message.properties.correlation_id);
    msg.body = amqpBytesToString(message.body);
    amqp_destroy_message(&message);
    amqp_maybe_release_buffers_on_channel(impl_->reconcileConn, Impl::reconcileChannel);
    return msg;
}

void RabbitEventConsumer::ackPendingFailureReport(std::uint64_t deliveryTag) {
    if (!impl_->reconcileConn) throw std::runtime_error("Sin conexión AMQP para ACK de fallo pendiente");
    const int rc = amqp_basic_ack(impl_->reconcileConn, Impl::reconcileChannel, deliveryTag, 0);
    if (rc != AMQP_STATUS_OK) {
        close();
        throw std::runtime_error(std::string("basic.ack fallo_pendiente falló: ") + amqp_error_string2(rc));
    }
}

void RabbitEventConsumer::requeuePendingFailureReport(std::uint64_t deliveryTag) {
    if (!impl_->reconcileConn) throw std::runtime_error("Sin conexión AMQP para requeue de fallo pendiente");
    const int rc = amqp_basic_nack(impl_->reconcileConn, Impl::reconcileChannel, deliveryTag, /*multiple=*/0, /*requeue=*/1);
    if (rc != AMQP_STATUS_OK) {
        close();
        throw std::runtime_error(std::string("basic.nack fallo_pendiente falló: ") + amqp_error_string2(rc));
    }
}

} // namespace biblioteca
