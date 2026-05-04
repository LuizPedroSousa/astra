#pragma once

#include "resources/mesh.hpp"

#include <filesystem>
#include <vector>

namespace astralix {

struct ModelImportSettings {
  bool triangulate = true;
  bool flip_uvs = true;
  bool generate_normals = true;
  bool pre_transform_vertices = false;
};

struct ImportedModelData {
  std::vector<Mesh> meshes;
  std::vector<uint32_t> mesh_material_slots;
  size_t material_slot_count = 0;
};

ImportedModelData import_model_file(
    const std::filesystem::path &path,
    const ModelImportSettings &settings
);

} // namespace astralix
