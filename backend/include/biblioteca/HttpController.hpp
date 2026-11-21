#pragma once
#include "biblioteca/AmqpBroker.hpp"
#include "biblioteca/AuthService.hpp"
#include "biblioteca/LibraryService.hpp"
#include "biblioteca/ServiceIdentity.hpp"
#include <httplib.h>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace biblioteca {

struct HttpOptions {
    std::vector<std::string> origenes;
    std::function<void()> recover;
    std::mutex* stateMutex = nullptr;
    const ServiceIdentity* serviceIdentity = nullptr;
    std::function<BrokerStatus()> brokerStatus;
    std::function<void()> wakeOutbox;
    std::vector<std::string> trustedProxies;
};

void registrarHttp(httplib::Server& servidor, CatalogService& catalogo, LibraryService& biblioteca,
                   AuthService& auth, const HttpOptions& opciones);

inline void registrarHttp(httplib::Server& servidor, CatalogService& catalogo, LibraryService& biblioteca,
                          AuthService& auth, const std::vector<std::string>& origenes,
                          std::function<void()> recover = {}) {
    HttpOptions opts;
    opts.origenes = origenes;
    opts.recover = std::move(recover);
    registrarHttp(servidor, catalogo, biblioteca, auth, opts);
}

} // namespace biblioteca
