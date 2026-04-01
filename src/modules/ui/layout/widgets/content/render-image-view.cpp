#include "layout/widgets/content/render-image-view.hpp"

#include "layout/common.hpp"
#include "managers/system-manager.hpp"
#include "systems/render-system/render-system.hpp"

#include <utility>

namespace astralix::ui {

glm::vec2 measure_render_image_view_size(const UIDocument::UINode &node) {
  auto *render_system = SystemManager::get()->get_system<RenderSystem>();
  if (render_system == nullptr || !node.render_image_key.has_value()) {
    return glm::vec2(0.0f);
  }

  auto resolved = render_system->resolve_render_image(*node.render_image_key);
  if (!resolved.has_value() || !resolved->available) {
    return glm::vec2(0.0f);
  }

  return glm::vec2(
      static_cast<float>(resolved->width),
      static_cast<float>(resolved->height)
  );
}

void append_render_image_view_node_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIDocument::UINode &node,
    const UIResolvedStyle &resolved
) {
  const UIRect content_bounds = node.layout.content_bounds;
  if (!node.render_image_key.has_value() || content_bounds.width <= 0.0f ||
      content_bounds.height <= 0.0f) {
    return;
  }

  UIDrawCommand command;
  command.type = DrawCommandType::RenderImageView;
  command.node_id = node_id;
  command.rect = content_bounds;
  apply_content_clip(command, node);
  command.render_image_key = node.render_image_key;
  command.tint = resolved.tint * glm::vec4(1.0f, 1.0f, 1.0f, resolved.opacity);
  document.draw_list().commands.push_back(std::move(command));
}

} // namespace astralix::ui
