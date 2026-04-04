#pragma once

#include <string>

namespace astralix::scene {

struct SceneEntity {};
struct EditorOnly {};
struct GeneratorSpec {};
struct DerivedEntity {};

struct MetaEntityOwner {
  std::string generator_id;
  std::string stable_key;
};

} // namespace astralix::scene

namespace astralix::rendering {

struct Renderable {};
struct MainCamera {};
struct ShadowCaster {};

} // namespace astralix::rendering
