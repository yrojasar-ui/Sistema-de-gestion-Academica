#pragma once
#include "biblioteca/LibraryService.hpp"
#include <httplib.h>
#include "biblioteca/AuthService.hpp"

namespace biblioteca {
void registrarHttp(httplib::Server& servidor, CatalogService& catalogo, LibraryService& biblioteca,
                   AuthService& auth, const std::vector<std::string>& origenes, std::function<void()> recover = {});
} // namespace biblioteca
