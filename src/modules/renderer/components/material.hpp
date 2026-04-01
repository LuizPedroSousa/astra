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

inline constexpr int k_default_bloom_render_layer = 0;

struct BloomSettings {
  bool enabled = true;
  int render_layer = k_default_bloom_render_layer;
};

} // namespace astralix::rendering
