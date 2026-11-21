#pragma once
#include <filesystem>
#include <string>

namespace biblioteca {
// Identidad técnica mínima separada de las cuentas humanas (ADMIN/USUARIO).
// Solo autoriza rutas /api/interno/avisos/*. Conserva únicamente el hash BLAKE2b en RAM.
class ServiceIdentity {
public:
    ServiceIdentity() = default;
    explicit ServiceIdentity(const std::filesystem::path& tokenFile);
    bool configured() const { return configured_; }
    bool verifyAuthorization(const std::string& authorization) const;
    static void generateTokenFile(const std::filesystem::path& tokenFile);
    static std::string readSecretFile(const std::filesystem::path& tokenFile);
private:
    bool configured_ = false;
    std::string digestHex_;
};
} // namespace biblioteca
