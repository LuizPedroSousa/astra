#pragma once

#include "components/material.hpp"
#include "guid.hpp"
#include <vector>

namespace astralix::rendering {

struct RenderResourceRequest {
  std::vector<ResourceDescriptorID> model_ids;
  ResourceDescriptorID shader_id;
  std::vector<ResourceDescriptorID> material_ids;
  std::vector<TextureBinding> textures;
  bool renderable = true;
};

} // namespace astralix::rendering
