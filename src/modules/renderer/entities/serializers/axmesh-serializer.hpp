#pragma once

#include "resources/mesh.hpp"

#include <filesystem>
#include <vector>

namespace astralix {

class AxMeshSerializer {
public:
  static constexpr int k_version = 1;

  static void write(const std::filesystem::path &path, const std::vector<Mesh> &meshes);
  static std::vector<Mesh> read(const std::filesystem::path &path);
};

} // namespace astralix
