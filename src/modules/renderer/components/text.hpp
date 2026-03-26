#pragma once

#include "glm/glm.hpp"
#include "guid.hpp"
#include <string>

namespace astralix::rendering {

struct TextSprite {
  std::string text;
  ResourceDescriptorID font_id;
  glm::vec2 position = glm::vec2(0.0f);
  float scale = 1.0f;
  glm::vec3 color = glm::vec3(1.0f);
};

} // namespace astralix::rendering
