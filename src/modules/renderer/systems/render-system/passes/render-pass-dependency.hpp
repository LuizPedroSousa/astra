#pragma once

#include "base.hpp"
#include "guid.hpp"
#include <cstdint>
#include <string>
#include <variant>

namespace astralix {

class Shader;
class Texture2D;
class Texture3D;
class Material;
class Model;
class Font;
class Svg;
class AudioClip;
class TerrainRecipe;

enum class RenderPassDependencyType : uint8_t {
  Shader,
  Texture2D,
  Texture3D,
  Material,
  Model,
  Font,
  Svg,
  AudioClip,
  TerrainRecipe,
  Opaque,
};

struct RenderPassDependencyDeclaration {
  RenderPassDependencyType type = RenderPassDependencyType::Opaque;
  std::string slot;
  ResourceDescriptorID descriptor_id;
};

using RenderPassDependencyResource = std::variant<
    std::monostate,
    Ref<Shader>,
    Ref<Texture2D>,
    Ref<Texture3D>,
    Ref<Material>,
    Ref<Model>,
    Ref<Font>,
    Ref<Svg>,
    Ref<AudioClip>,
    Ref<TerrainRecipe>>;

struct ResolvedRenderPassDependency {
  RenderPassDependencyDeclaration declaration;
  RenderPassDependencyResource resource;
};

} // namespace astralix
