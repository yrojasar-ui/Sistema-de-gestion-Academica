#include "biblioteca/AuthService.hpp"
#include <sodium.h>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

namespace biblioteca {
namespace {
void secure(const std::filesystem::path& path, bool directory) {
    struct stat info{};
    if (::lstat(path.c_str(), &info) || info.st_uid != ::geteuid() || (info.st_mode & 0077) ||
        (directory ? !S_ISDIR(info.st_mode) : !S_ISREG(info.st_mode)))
        throw std::runtime_error("Almacén privado requiere propietario actual y permisos 0700/0600, sin enlaces");
}
std::string randomToken() {
    unsigned char bytes[32]; char text[65];
    randombytes_buf(bytes, sizeof bytes);
    sodium_bin2hex(text, sizeof text, bytes, sizeof bytes);
    sodium_memzero(bytes, sizeof bytes);
    return text;
}
void roleValid(const std::string& role) {
    if (role != "ADMIN" && role != "USUARIO") throw ErrorDominio(400, "Rol ADMIN o USUARIO requerido");
}
}
AuthService::AuthService(std::filesystem::path file, int duration) : path_(std::move(file)), seconds_(duration) {
    if (sodium_init() < 0) throw std::runtime_error("No se pudo iniciar libsodium");
    if (seconds_ < 1 || seconds_ > 86400) throw std::invalid_argument("Sesión entre 1 y 86400 segundos");
    path_ = std::filesystem::absolute(path_).lexically_normal();
    const auto parent = path_.parent_path();
    if (!std::filesystem::exists(parent) && ::mkdir(parent.c_str(), 0700))
        throw std::runtime_error("No se pudo crear directorio privado");
    secure(parent, true);
    lock_ = std::make_unique<FileLock>(path_);
    if (std::filesystem::exists(path_)) {
        secure(path_, false);
        if (std::filesystem::file_size(path_) > 4*1024*1024) throw std::runtime_error("Almacén privado demasiado grande");
        std::ifstream fileIn(path_);
        Json j = Json::parse(fileIn);
        campos(j, {"version","cuentas"});
        if (j.at("version") != 1 || !j.at("cuentas").is_array() || j.at("cuentas").size() > 10000)
            throw std::runtime_error("Almacén privado incompatible");
        for (const auto& row : j.at("cuentas")) {
            campos(row, {"idUsuario","rol","hash","activa"});
            Account a{row.at("idUsuario"),row.at("rol"),row.at("hash"),row.at("activa")};
            validarId(a.id); roleValid(a.role);
            if (a.hash.size() >= crypto_pwhash_STRBYTES || a.hash.rfind("$argon2id$",0) != 0 ||
                crypto_pwhash_str_needs_rehash(a.hash.c_str(),crypto_pwhash_OPSLIMIT_INTERACTIVE,crypto_pwhash_MEMLIMIT_INTERACTIVE) < 0 ||
                std::any_of(accounts_.begin(),accounts_.end(),[&](const auto& x){return x.id == a.id;}))
                throw std::runtime_error("Cuenta privada incompatible");
            accounts_.push_back(std::move(a));
        }
    }
    dummy_ = hashPassword(randomToken());
}
std::string AuthService::hashPassword(const std::string& password) {
    if (password.size() < 12 || password.size() > 256) throw ErrorDominio(400, "Contraseña de 12 a 256 bytes requerida");
    char result[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str_alg(result,password.data(),password.size(),crypto_pwhash_OPSLIMIT_INTERACTIVE,
                             crypto_pwhash_MEMLIMIT_INTERACTIVE,crypto_pwhash_ALG_ARGON2ID13))
        throw std::runtime_error("No se pudo proteger la contraseña");
    return result;
}
void AuthService::persist(const std::vector<Account>& candidate) {
    if (blocked_) throw ErrorDominio(503,"Almacén privado bloqueado; reinicie para recuperar");
    Json rows = Json::array();
    for (const auto& a : candidate) rows.push_back({{"idUsuario",a.id},{"rol",a.role},{"hash",a.hash},{"activa",a.active}});
    const auto text = Json{{"version",1},{"cuentas",rows}}.dump();
    secure(path_.parent_path(), true);
    if (std::filesystem::exists(path_)) secure(path_,false);
    std::string pattern = path_.string()+".tmp.XXXXXX";
    std::vector<char> name(pattern.begin(),pattern.end()); name.push_back('\0');
    int fd = ::mkstemp(name.data());
    if (fd < 0) throw ErrorDominio(503,"No se guardó la cuenta");
    bool renamed = false;
    try {
        std::size_t done = 0;
        while (done < text.size()) {
            auto n = ::write(fd,text.data()+done,text.size()-done);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) throw std::runtime_error("write");
            done += static_cast<std::size_t>(n);
        }
        if (::fsync(fd)) throw std::runtime_error("fsync");
        const int closed = ::close(fd); fd = -1;
        if (closed) throw std::runtime_error("close");
        std::filesystem::rename(name.data(),path_); renamed = true;
        const int directory = ::open(path_.parent_path().c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        if (directory < 0) throw std::runtime_error("directory");
        const int synced = ::fsync(directory); ::close(directory);
        if (synced) throw std::runtime_error("fsync directory");
    } catch (...) {
        if (fd >= 0) ::close(fd);
        std::error_code ignored; std::filesystem::remove(name.data(),ignored);
        blocked_ = renamed;
        if (blocked_) sessions_.clear();
        throw ErrorDominio(503,renamed ? "Guardado privado incierto; reinicie antes de continuar" : "No se guardó la cuenta");
    }
}
void AuthService::create(const std::string& id, const std::string& role, const std::string& password) {
    validarId(id); roleValid(role);
    if (accounts_.size() >= 10000) throw ErrorDominio(409,"Límite de cuentas");
    for (const auto& a : accounts_) if (a.id == id) throw ErrorDominio(409,"Cuenta ya existente");
    auto next = accounts_; next.push_back({id,role,hashPassword(password),true});
    persist(next); accounts_.swap(next);
}
void AuthService::protectAdmin(const std::string& id) const {
    if (admin(id) && std::count_if(accounts_.begin(), accounts_.end(), [](const auto& a){return a.active && a.role == "ADMIN";}) <= 1)
        throw ErrorDominio(409,"No se puede desactivar el último administrador");
}
void AuthService::validateAdministrators(const EstadoBiblioteca& state) const {
    for (const auto& a : accounts_) if (a.active && a.role == "ADMIN") {
        if (std::none_of(state.usuarios.begin(),state.usuarios.end(),[&](const auto& u){return u.idUsuario == a.id && u.activo;}))
            throw ErrorDominio(409,"La importación debe conservar los administradores activos");
    }
}
void AuthService::update(const std::string& id, const std::string& role, bool active, const std::string& password) {
    roleValid(role);
    if (!active || role != "ADMIN") protectAdmin(id);
    auto next = accounts_;
    auto found = std::find_if(next.begin(),next.end(),[&](const auto& a){return a.id == id;});
    if (found == next.end()) throw ErrorDominio(404,"Cuenta ausente");
    found->role = role; found->active = active;
    if (!password.empty()) found->hash = hashPassword(password);
    persist(next); accounts_.swap(next); revoke(id);
}
Json AuthService::accounts() const {
    auto rows = Json::array();
    for (const auto& a : accounts_) rows.push_back({{"idUsuario",a.id},{"rol",a.role},{"activa",a.active}});
    return rows;
}
bool AuthService::admin(const std::string& id) const {
    return std::any_of(accounts_.begin(),accounts_.end(),[&](const auto& a){return a.id == id && a.active && a.role == "ADMIN";});
}
void AuthService::revoke(const std::string& id) {
    for (auto i=sessions_.begin();i!=sessions_.end();) {
        if (i->second.id == id) i=sessions_.erase(i); else ++i;
    }
}
std::string AuthService::tokenKey(const std::string& token) {
    if (token.size()!=64 || !std::all_of(token.begin(),token.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))
        throw ErrorDominio(401,"Sesión requerida o vencida");
    unsigned char digest[32]; char hex[65];
    crypto_generichash(digest,sizeof digest,reinterpret_cast<const unsigned char*>(token.data()),token.size(),nullptr,0);
    sodium_bin2hex(hex,sizeof hex,digest,sizeof digest);
    return hex;
}
Json AuthService::login(const std::string& id, const std::string& password, const std::string& ip,
                        const std::function<bool(const Identidad&)>& permitted) {
    if (blocked_) throw ErrorDominio(503,"Almacén privado bloqueado");
    const auto now = std::chrono::steady_clock::now();
    for (auto i=attempts_.begin();i!=attempts_.end();) { if (i->second.until <= now) i=attempts_.erase(i); else ++i; }
    if (id.size()>64 || password.size()>256) throw ErrorDominio(401,"Credenciales inválidas");
    const std::vector<std::string> keys{"ip:"+ip,"id:"+id};
    for (const auto& key:keys) {
        auto i=attempts_.find(key);
        if ((i!=attempts_.end() && i->second.count>=5) || (i==attempts_.end() && attempts_.size()>=1024))
            throw ErrorDominio(429,"Demasiados intentos; espere un minuto");
    }
    auto found=std::find_if(accounts_.begin(),accounts_.end(),[&](const auto& a){return a.id==id;});
    const auto& hash=found==accounts_.end()?dummy_:found->hash;
    const bool valid=crypto_pwhash_str_verify(hash.c_str(),password.data(),password.size())==0;
    if (!valid || found==accounts_.end() || !found->active || !permitted({found->id,found->role})) {
        for (const auto& key:keys) {
            auto& attempt=attempts_[key]; if (attempt.count==0) attempt.until=now+std::chrono::seconds(60); ++attempt.count;
        }
        throw ErrorDominio(401,"Credenciales inválidas");
    }
    attempts_.erase("id:"+id);
    for (auto i=sessions_.begin();i!=sessions_.end();) { if (i->second.expires<=now) i=sessions_.erase(i); else ++i; }
    revoke(id);
    if (sessions_.size()>=1024) throw ErrorDominio(503,"Límite temporal de sesiones");
    auto token=randomToken(); sessions_[tokenKey(token)]={id,now+std::chrono::seconds(seconds_)};
    return {{"token",token},{"idUsuario",id},{"rol",found->role},{"expiraEn",seconds_}};
}
Identidad AuthService::authenticate(const std::string& authorization) {
    if (blocked_) throw ErrorDominio(503,"Almacén privado bloqueado");
    if (authorization.rfind("Bearer ",0)!=0) throw ErrorDominio(401,"Sesión requerida o vencida");
    const auto key=tokenKey(authorization.substr(7));
    const auto session=sessions_.find(key);
    if (session==sessions_.end()) throw ErrorDominio(401,"Sesión requerida o vencida");
    if (session->second.expires<=std::chrono::steady_clock::now()) {
        sessions_.erase(session); throw ErrorDominio(401,"Sesión requerida o vencida");
    }
    for (const auto& a:accounts_) if (a.id==session->second.id && a.active) return {a.id,a.role};
    throw ErrorDominio(401,"Sesión requerida o vencida");
}
void AuthService::logout(const std::string& authorization) {
    authenticate(authorization); sessions_.erase(tokenKey(authorization.substr(7)));
}
}
