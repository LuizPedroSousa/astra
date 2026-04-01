#include "layout/widgets/content/render-image-view.hpp"
#include "resources/font.hpp"

#include <optional>

namespace astralix {

std::optional<CharacterGlyph> Font::glyph(char, uint32_t) const {
  return std::nullopt;
}

glm::vec2 Font::measure_text(const std::string &text, float pixel_size) const {
  return glm::vec2(
      static_cast<float>(text.size()) * pixel_size * 0.5f,
      pixel_size
  );
}

float Font::line_height(float pixel_size) const { return pixel_size; }

} // namespace astralix

namespace astralix::ui {

glm::vec2 measure_render_image_view_size(const UIDocument::UINode &) {
  return glm::vec2(0.0f);
}

void append_render_image_view_node_commands(
    UIDocument &,
    UINodeId,
    const UIDocument::UINode &,
    const UIResolvedStyle &
) {}

} // namespace astralix::ui
