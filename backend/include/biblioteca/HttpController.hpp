#pragma once
#include "biblioteca/LibraryService.hpp"
#include <httplib.h>

namespace biblioteca {
void registrarHttp(httplib::Server& servidor, CatalogService& catalogo, LibraryService& biblioteca,
                   const std::vector<std::string>& origenes, std::function<void()> recover = {});
} // namespace biblioteca
