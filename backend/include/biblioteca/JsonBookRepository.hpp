#pragma once
#include "biblioteca/IBookRepository.hpp"

namespace biblioteca {
class JsonBookRepository final : public IBookRepository {
public:
    JsonBookRepository(std::filesystem::path initial, std::filesystem::path working);
    std::vector<Libro> load(const std::filesystem::path& source) const override;
    void save(const std::vector<Libro>& books) const override;
    std::filesystem::path workingPath() const override { return working_; }
private:
    std::filesystem::path initial_, working_;
    void checkWorkingPath() const;
};
} // namespace biblioteca
