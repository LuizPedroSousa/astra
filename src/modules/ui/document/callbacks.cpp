#include "document.hpp"

#include <utility>

namespace astralix::ui {

void UIDocument::set_on_hover(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_hover = std::move(callback);
  }
}

void UIDocument::set_on_press(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_press = std::move(callback);
  }
}

void UIDocument::set_on_release(
    UINodeId node_id,
    std::function<void()> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_release = std::move(callback);
  }
}

void UIDocument::set_on_click(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_click = std::move(callback);
  }
}

void UIDocument::set_on_secondary_click(
    UINodeId node_id,
    std::function<void(const UIPointerButtonEvent &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_secondary_click = std::move(callback);
  }
}

void UIDocument::set_on_focus(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_focus = std::move(callback);
  }
}

void UIDocument::set_on_blur(UINodeId node_id, std::function<void()> callback) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_blur = std::move(callback);
  }
}

void UIDocument::set_on_key_input(
    UINodeId node_id,
    std::function<void(const UIKeyInputEvent &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_key_input = std::move(callback);
  }
}

void UIDocument::set_on_character_input(
    UINodeId node_id,
    std::function<void(const UICharacterInputEvent &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_character_input = std::move(callback);
  }
}

void UIDocument::set_on_mouse_wheel(
    UINodeId node_id,
    std::function<void(const UIMouseWheelInputEvent &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_mouse_wheel = std::move(callback);
  }
}

void UIDocument::set_on_change(
    UINodeId node_id,
    std::function<void(const std::string &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_change = std::move(callback);
  }
}

void UIDocument::set_on_submit(
    UINodeId node_id,
    std::function<void(const std::string &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_submit = std::move(callback);
  }
}

void UIDocument::set_on_toggle(
    UINodeId node_id,
    std::function<void(bool)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_toggle = std::move(callback);
  }
}

void UIDocument::set_on_value_change(
    UINodeId node_id,
    std::function<void(float)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_value_change = std::move(callback);
  }
}

void UIDocument::set_on_select(
    UINodeId node_id,
    std::function<void(size_t, const std::string &)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_select = std::move(callback);
  }
}

void UIDocument::set_on_chip_toggle(
    UINodeId node_id,
    std::function<void(size_t, const std::string &, bool)> callback
) {
  UINode *target = node(node_id);
  if (target != nullptr) {
    target->on_chip_toggle = std::move(callback);
  }
}

void UIDocument::queue_callback(const std::function<void()> &callback) {
  if (callback) {
    m_callback_queue.push_back(callback);
  }
}

void UIDocument::flush_callbacks() {
  if (m_callback_queue.empty()) {
    return;
  }

  auto callbacks = std::move(m_callback_queue);
  m_callback_queue.clear();

  for (auto &callback : callbacks) {
    if (callback) {
      callback();
    }
  }
}

} // namespace astralix::ui
