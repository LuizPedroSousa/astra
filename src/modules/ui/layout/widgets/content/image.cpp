#include "layout/widgets/content/image.hpp"

#include "layout/common.hpp"
#include "resources/texture.hpp"

#include <utility>

namespace astralix::ui {

glm::vec2 measure_image_size(const UIDocument::UINode &node) {
  auto texture = resource_manager()->get_by_descriptor_id<Texture2D>(
      node.texture_id
  );
  if (texture == nullptr) {
    return glm::vec2(0.0f);
  }

  return glm::vec2(
      static_cast<float>(texture->width()),
      static_cast<float>(texture->height())
  );
}

void append_image_node_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
) {
  const UIRect content_bounds = node.layout.content_bounds;
  if (node.texture_id.empty() || content_bounds.width <= 0.0f ||
      content_bounds.height <= 0.0f) {
    return;
  }

  UIDrawCommand command;
  command.type = DrawCommandType::Image;
  command.node_id = node_id;
  command.rect = content_bounds;
  apply_content_clip(command, node);
  command.texture_id = node.texture_id;
  command.tint = resolved.tint * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  document.draw_list().commands.push_back(std::move(command));
}

} // namespace astralix::ui
