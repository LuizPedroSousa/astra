#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_icon_button(
    ResourceDescriptorID texture_id,
    const std::function<void()> &on_click
) {
  UINodeId button_id = create_pressable();
  UINodeId icon_id = create_image(std::move(texture_id));
  append_child(button_id, icon_id);
  set_on_click(button_id, on_click);

  mutate_style(button_id, [](UIStyle &style) {
    style.padding = UIEdges::all(8.0f);
    style.width = UILength::pixels(34.0f);
    style.height = UILength::pixels(34.0f);
    style.border_radius = 10.0f;
    style.background_color = glm::vec4(0.08f, 0.13f, 0.2f, 0.88f);
    style.border_color = glm::vec4(0.34f, 0.47f, 0.63f, 0.35f);
    style.border_width = 1.0f;
    style.align_items = AlignItems::Center;
    style.justify_content = JustifyContent::Center;
    style.flex_shrink = 0.0f;
    style.hovered_style.background_color =
        glm::vec4(0.12f, 0.19f, 0.29f, 0.94f);
    style.pressed_style.background_color =
        glm::vec4(0.07f, 0.12f, 0.19f, 0.98f);
    style.focused_style.border_color = glm::vec4(0.8f, 0.88f, 1.0f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.55f;
  });

  mutate_style(icon_id, [](UIStyle &style) {
    style.width = UILength::pixels(16.0f);
    style.height = UILength::pixels(16.0f);
    style.tint = glm::vec4(0.9f, 0.96f, 1.0f, 1.0f);
  });

  return button_id;
}

} // namespace astralix::ui
