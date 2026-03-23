#include "shader-lang/emitters/umbrella-header-emitter.hpp"

#include <sstream>

namespace astralix {

std::string UmbrellaHeaderEmitter::emit(
    const std::vector<std::pair<std::string, std::filesystem::path>> &headers) {
  std::ostringstream out;
  out << "#pragma once\n\n";

  for (const auto &[canonical_id, relative_header] : headers) {
    (void)canonical_id;
    out << "#include \"" << relative_header.generic_string() << "\"\n";
  }

  if (!headers.empty()) {
    out << '\n';
  }

  return out.str();
}

} // namespace astralix
