#pragma once
#include <filesystem>
#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace biblioteca {
class FileLock {
public:
    explicit FileLock(const std::filesystem::path& path) {
        fd_ = ::open((path.string()+".lock").c_str(), O_CREAT|O_RDWR|O_CLOEXEC|O_NOFOLLOW,0600);
        if (fd_<0) throw std::runtime_error("No se pudo abrir el bloqueo");
        if (::flock(fd_,LOCK_EX|LOCK_NB)) {::close(fd_);throw std::runtime_error("Almacén ya en uso");}
    }
    ~FileLock() {::close(fd_);}
    FileLock(const FileLock&)=delete;
    FileLock& operator=(const FileLock&)=delete;
private: int fd_ = -1;
};
}
