#include "biblioteca/ServiceIdentity.hpp"
#include <sodium.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace biblioteca {
namespace {
void verificarPermisos(const std::filesystem::path& path, bool dir) {
    struct stat info{};
    if (::lstat(path.c_str(), &info) || info.st_uid != ::geteuid() || (info.st_mode & 0077) ||
        (dir ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode)))
        throw std::runtime_error("Credencial de servicio requiere propietario actual y permisos 0700/0600, sin enlaces");
}
std::string hashToken(const std::string& token) {
    unsigned char digest[32];
    char hex[65];
    crypto_generichash(digest, sizeof digest,
                       reinterpret_cast<const unsigned char*>(token.data()), token.size(), nullptr, 0);
    sodium_bin2hex(hex, sizeof hex, digest, sizeof digest);
    sodium_memzero(digest, sizeof digest);
    return hex;
}
std::string recortar(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}
} // namespace

std::string ServiceIdentity::readSecretFile(const std::filesystem::path& tokenFile) {
    const auto path = std::filesystem::absolute(tokenFile).lexically_normal();
    verificarPermisos(path.parent_path(), true);
    verificarPermisos(path, false);
    if (std::filesystem::file_size(path) > 4096) throw std::runtime_error("Archivo de token de servicio demasiado grande");
    std::ifstream in(path);
    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto clean = recortar(raw);
    sodium_memzero(raw.data(), raw.size());
    if (clean.size() < 32 || clean.size() > 256 ||
        std::any_of(clean.begin(), clean.end(), [](unsigned char c) { return c < 33 || c > 126; })) {
        sodium_memzero(clean.data(), clean.size());
        throw std::runtime_error("Token de servicio inválido (requiere 32 a 256 caracteres ASCII imprimibles)");
    }
    return clean;
}

ServiceIdentity::ServiceIdentity(const std::filesystem::path& tokenFile) {
    if (sodium_init() < 0) throw std::runtime_error("No se pudo iniciar libsodium");
    if (tokenFile.empty()) return;
    auto token = readSecretFile(tokenFile);
    digestHex_ = hashToken(token);
    sodium_memzero(token.data(), token.size());
    configured_ = true;
}

bool ServiceIdentity::verifyAuthorization(const std::string& authorization) const {
    if (!configured_ || authorization.rfind("Bearer ", 0) != 0) return false;
    const auto candidate = authorization.substr(7);
    if (candidate.size() < 32 || candidate.size() > 256) return false;
    const auto candidateHash = hashToken(candidate);
    if (candidateHash.size() != digestHex_.size()) return false;
    return sodium_memcmp(candidateHash.data(), digestHex_.data(), digestHex_.size()) == 0;
}

void ServiceIdentity::generateTokenFile(const std::filesystem::path& tokenFile) {
    if (sodium_init() < 0) throw std::runtime_error("No se pudo iniciar libsodium");
    const auto path = std::filesystem::absolute(tokenFile).lexically_normal();
    const auto parent = path.parent_path();
    if (!std::filesystem::exists(parent) && ::mkdir(parent.c_str(), 0700))
        throw std::runtime_error("No se pudo crear directorio privado del token de servicio");
    verificarPermisos(parent, true);
    if (std::filesystem::exists(path))
        throw std::runtime_error("El archivo de token de servicio ya existe; no se sobrescribe");
    unsigned char bytes[32];
    char hex[65];
    randombytes_buf(bytes, sizeof bytes);
    sodium_bin2hex(hex, sizeof hex, bytes, sizeof bytes);
    sodium_memzero(bytes, sizeof bytes);
    std::string content(hex);
    content.push_back('\n');
    sodium_memzero(hex, sizeof hex);

    std::string pattern = path.string() + ".tmp.XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    int fd = ::mkstemp(name.data());
    if (fd < 0) {
        sodium_memzero(content.data(), content.size());
        throw std::runtime_error("No se pudo crear archivo temporal de token");
    }
    try {
        std::size_t written = 0;
        while (written < content.size()) {
            const auto n = ::write(fd, content.data() + written, content.size() - written);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) throw std::runtime_error("Error escribiendo token de servicio");
            written += static_cast<std::size_t>(n);
        }
        if (::fsync(fd) != 0) throw std::runtime_error("Error sincronizando token de servicio");
        if (::close(fd) != 0) { fd = -1; throw std::runtime_error("Error cerrando token de servicio"); }
        fd = -1;
        std::filesystem::rename(name.data(), path);
        const int dir = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dir >= 0) { (void)::fsync(dir); ::close(dir); }
    } catch (...) {
        sodium_memzero(content.data(), content.size());
        if (fd >= 0) ::close(fd);
        std::error_code ignored;
        std::filesystem::remove(name.data(), ignored);
        throw;
    }
    sodium_memzero(content.data(), content.size());
}
} // namespace biblioteca
