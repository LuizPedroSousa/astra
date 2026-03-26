#pragma once

#include "guid.hpp"
#include <string>
#include <vector>

namespace astralix::rendering {

struct MaterialSlots {
  std::vector<ResourceDescriptorID> materials;
};

struct ShaderBinding {
  ResourceDescriptorID shader;
};

struct TextureBinding {
  ResourceDescriptorID id;
  std::string name;
  bool cubemap = false;
};

struct TextureBindings {
  std::vector<TextureBinding> bindings;
};

} // namespace astralix::rendering
