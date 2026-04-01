#include "document/document.hpp"

#include <utility>

namespace astralix::ui {

UINodeId UIDocument::create_button(
    const std::string &label,
    const std::function<void()> &on_click
) {
  UINodeId button_id = create_pressable();
  UINodeId label_id = create_text(label);
  append_child(button_id, label_id);
  set_on_click(button_id, on_click);

  mutate_style(button_id, [](UIStyle &style) {
    style.padding = UIEdges::symmetric(14.0f, 10.0f);
    style.border_radius = 0.0f;
    style.background_color = glm::vec4(0.16f, 0.24f, 0.37f, 0.92f);
    style.border_color = glm::vec4(0.62f, 0.76f, 0.94f, 0.35f);
    style.border_width = 1.0f;
    style.align_items = AlignItems::Center;
    style.justify_content = JustifyContent::Center;
    style.cursor = CursorStyle::Pointer;
    style.hovered_style.background_color =
        glm::vec4(0.2f, 0.31f, 0.46f, 0.96f);
    style.pressed_style.background_color =
        glm::vec4(0.11f, 0.18f, 0.29f, 0.96f);
    style.focused_style.border_color = glm::vec4(0.8f, 0.88f, 1.0f, 0.95f);
    style.focused_style.border_width = 2.0f;
    style.disabled_style.opacity = 0.55f;
  });

  mutate_style(label_id, [](UIStyle &style) {
    style.text_color = glm::vec4(0.93f, 0.97f, 1.0f, 1.0f);
    style.font_size = 16.0f;
  });

  return button_id;
}

} // namespace astralix::ui
