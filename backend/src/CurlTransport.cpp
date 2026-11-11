#include "biblioteca/MongoApiBookRepository.hpp"
#include <curl/curl.h>
#include <memory>

namespace biblioteca {
namespace {
struct CurlRuntime {
    CurlRuntime() {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
            throw std::runtime_error("No se pudo inicializar libcurl");
    }
    ~CurlRuntime() { curl_global_cleanup(); }
};
size_t append(char* data, size_t size, size_t count, void* output) noexcept {
    auto& buffer = *static_cast<std::string*>(output);
    constexpr size_t limit = 16 * 1024 * 1024;
    if (size && count > (limit - buffer.size()) / size) return 0;
    try { buffer.append(data, size * count); }
    catch (...) { return 0; }
    return size * count;
}
void check(CURLcode code) {
    if (code != CURLE_OK) throw std::runtime_error(std::string("libcurl: ") + curl_easy_strerror(code));
}
}
HttpResponse curlRequest(const std::string& method, const std::string& url, const std::string& body) {
    static const CurlRuntime runtime;
    using Handle = std::unique_ptr<CURL, decltype(&curl_easy_cleanup)>;
    Handle curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) throw std::runtime_error("No se pudo abrir la conexión");
    using Headers = std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)>;
    Headers headers(curl_slist_append(nullptr, "Content-Type: application/json"), curl_slist_free_all);
    if (!headers) throw std::bad_alloc();
    HttpResponse result{};
    check(curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str()));
    check(curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https"));
    check(curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L));
    check(curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 2L));
    check(curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 6L));
    check(curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 20L));
    check(curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L));
    check(curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, append));
    check(curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &result.body));
    if (method != "GET") {
        check(curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get()));
        check(curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, body.c_str()));
        check(curl_easy_setopt(curl.get(), CURLOPT_CUSTOMREQUEST, method.c_str()));
    }
    check(curl_easy_perform(curl.get()));
    char* type = nullptr;
    check(curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &result.status));
    check(curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_TYPE, &type));
    if (type) result.contentType = type;
    return result;
}
} // namespace biblioteca
