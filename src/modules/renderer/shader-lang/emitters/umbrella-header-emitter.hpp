#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace astralix {

class UmbrellaHeaderEmitter {
public:
  std::string
  emit(const std::vector<std::pair<std::string, std::filesystem::path>>
           &headers);
};

} // namespace astralix
