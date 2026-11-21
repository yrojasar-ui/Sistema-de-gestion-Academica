#pragma once
#include "biblioteca/IDomainRepository.hpp"
#include "biblioteca/LibraryService.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace biblioteca {

struct AmqpConfig {
    std::string host;
    int port = 5672;
    std::string vhost = "/";
    std::string user = "guest";
    std::string password;
    std::string exchange = "biblioteca.eventos";
    std::string queue = "biblioteca.avisos";
    std::string dlxExchange = "biblioteca.dlx";
    std::string dlqQueue = "biblioteca.avisos.fallidos";
    std::string failedReportQueue = "biblioteca.avisos.fallo_pendiente";
    std::string routingKey = "aviso.creado";
    std::string dlqRoutingKey = "aviso.fallido";
    std::string failedReportRoutingKey = "aviso.fallo_pendiente";
    int heartbeatSec = 15;
    int timeoutMs = 3000;
    bool declareQueues = true;
    bool enabled() const { return !host.empty(); }
};

// Carga credenciales AMQP desde archivo 0600 (líneas user=... y password=...) sin registrarlas.
void loadAmqpCredentialsFile(const std::filesystem::path& file, AmqpConfig& cfg);

// Sanea errores para garantizar que nunca se registren contraseñas ni credenciales.
std::string sanitizeErrorText(const std::string& raw, std::size_t maxLen = 512);

struct BrokerStatus {
    bool configurado = false;
    bool conectado = false;
    std::string ultimoError;
};

// Publicador AMQP 0-9-1 real sobre rabbitmq-c con colas durables, mensajes persistentes,
// publisher confirms y detección explícita de mensajes sin ruta (basic.return).
class RabbitEventPublisher final : public IEventPublisher {
public:
    explicit RabbitEventPublisher(AmqpConfig cfg);
    ~RabbitEventPublisher() override;
    RabbitEventPublisher(const RabbitEventPublisher&) = delete;
    RabbitEventPublisher& operator=(const RabbitEventPublisher&) = delete;

    void publish(const Aviso& aviso) override;
    void publishRaw(const std::string& routingKey, const std::string& payload,
                    const std::string& messageId, const std::string& correlationId,
                    std::uint32_t intentos = 0);
    void ensureConnected();
    void close() noexcept;
private:
    struct Impl;
    AmqpConfig cfg_;
    std::unique_ptr<Impl> impl_;
};

// Hilo de fondo desacoplado (patrón outbox) que publica avisos pendientes ya confirmados
// sin bloquear peticiones HTTP ni mantener el mutex durante I/O de red.
class AvisoOutboxRelay {
public:
    AvisoOutboxRelay(std::mutex& stateMutex, LibraryService& library, IEventPublisher& publisher,
                     int pollIntervalMs = 1500);
    ~AvisoOutboxRelay();
    AvisoOutboxRelay(const AvisoOutboxRelay&) = delete;
    AvisoOutboxRelay& operator=(const AvisoOutboxRelay&) = delete;

    void start();
    void stop();
    void notify();
    BrokerStatus status() const;
private:
    std::mutex& stateMutex_;
    LibraryService& library_;
    IEventPublisher& publisher_;
    int pollIntervalMs_;
    std::atomic<bool> stop_{false};
    std::mutex wakeMutex_;
    std::condition_variable wakeCv_;
    bool wakeRequested_ = false;
    mutable std::mutex statusMutex_;
    BrokerStatus status_;
    std::thread thread_;
    void loop();
};

struct AmqpMessage {
    std::uint64_t deliveryTag = 0;
    bool redelivered = false;
    std::uint32_t intentosHeader = 0;
    std::string messageId;
    std::string correlationId;
    std::string body;
};

// Consumidor AMQP 0-9-1 real con ACK manual, prefetch=1, republicación confirmada para reintentos
// y rechazo hacia DLX/DLQ durable.
class RabbitEventConsumer {
public:
    explicit RabbitEventConsumer(AmqpConfig cfg);
    ~RabbitEventConsumer();
    RabbitEventConsumer(const RabbitEventConsumer&) = delete;
    RabbitEventConsumer& operator=(const RabbitEventConsumer&) = delete;

    void connect();
    void close() noexcept;
    bool connected() const;
    std::optional<AmqpMessage> consumeNext(int timeoutMs);
    void ack(std::uint64_t deliveryTag);
    void rejectToDlq(std::uint64_t deliveryTag);
    void republishForRetry(const AmqpMessage& msg, std::uint32_t nextAttempt);
    void publishPendingFailureReport(const std::string& payload, const std::string& messageId,
                                     const std::string& correlationId);
    std::optional<AmqpMessage> pollPendingFailureReport();
    void ackPendingFailureReport(std::uint64_t deliveryTag);
    void requeuePendingFailureReport(std::uint64_t deliveryTag);
private:
    struct Impl;
    AmqpConfig cfg_;
    std::unique_ptr<Impl> impl_;
};

} // namespace biblioteca
