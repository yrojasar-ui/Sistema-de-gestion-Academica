#pragma once
#include "biblioteca/DomainJson.hpp"
#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include "biblioteca/FileLock.hpp"
namespace biblioteca {
struct Identidad { std::string idUsuario, rol; };
class AuthService {
public:
    explicit AuthService(std::filesystem::path archivo, int duracion = 1800);
    void invalidate(const std::string& id) { revoke(id); }
    bool empty() const { return accounts_.empty(); }
    void create(const std::string& id, const std::string& rol, const std::string& password);
    void update(const std::string& id, const std::string& rol, bool activa, const std::string& password = "");
    Json accounts() const;
    Json login(const std::string& id, const std::string& password, const std::string& ip,
               const std::function<bool(const Identidad&)>& permitted);
    Identidad authenticate(const std::string& authorization);
    void logout(const std::string& authorization);
    bool admin(const std::string& id) const;
    void protectAdmin(const std::string& id) const;
    void validateAdministrators(const EstadoBiblioteca& state) const;
private:
    struct Account { std::string id, role, hash; bool active = true; };
    struct Session { std::string id; std::chrono::steady_clock::time_point expires; };
    struct Attempts { int count = 0; std::chrono::steady_clock::time_point until; };
    std::filesystem::path path_;
    std::unique_ptr<FileLock> lock_;
    std::vector<Account> accounts_;
    std::map<std::string, Session> sessions_; // Índice de seguridad, no sustituye estructuras del negocio.
    std::map<std::string, Attempts> attempts_;
    std::string dummy_;
    int seconds_;
    bool blocked_ = false;
    void persist(const std::vector<Account>&);
    static std::string hashPassword(const std::string&);
    static std::string tokenKey(const std::string&);
    void revoke(const std::string&);
};
}
