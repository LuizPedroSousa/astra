#pragma once

#include "document/document.hpp"
#include <string>

namespace astralix::ui {

struct UIDisclosureNodes {
  UINodeId root = k_invalid_node_id;
  UINodeId header_button = k_invalid_node_id;
  UINodeId header_content = k_invalid_node_id;
  UINodeId body = k_invalid_node_id;
};

struct UIDisclosureOptions {
  std::string name;
  bool open = false;
};

inline UIDisclosureNodes create_disclosure(UIDocument &document, UINodeId parent_id, UIDisclosureOptions options = {}) {
  const std::string name_prefix =
      options.name.empty() ? "ui_disclosure" : std::move(options.name);

  UIDisclosureNodes nodes;
  nodes.root = document.create_view(name_prefix);
  nodes.header_button = document.create_pressable(name_prefix + "_header");
  nodes.header_content = document.create_view(name_prefix + "_header_content");
  nodes.body = document.create_view(name_prefix + "_body");

  document.append_child(parent_id, nodes.root);
  document.append_child(nodes.root, nodes.header_button);
  document.append_child(nodes.header_button, nodes.header_content);
  document.append_child(nodes.root, nodes.body);

  document.mutate_style(nodes.root, [](UIStyle &style) {
    style.flex_direction = FlexDirection::Column;
    style.align_items = AlignItems::Stretch;
    style.justify_content = JustifyContent::Start;
  });

  document.mutate_style(nodes.header_button, [](UIStyle &style) {
    style.flex_direction = FlexDirection::Row;
    style.align_items = AlignItems::Center;
    style.justify_content = JustifyContent::Start;
    style.width = UILength::percent(1.0f);
    style.padding = UIEdges::all(0.0f);
    style.border_width = 0.0f;
    style.background_color = glm::vec4(0.0f);
  });

  document.mutate_style(nodes.header_content, [](UIStyle &style) {
    style.flex_direction = FlexDirection::Row;
    style.align_items = AlignItems::Center;
    style.justify_content = JustifyContent::Start;
    style.width = UILength::percent(1.0f);
  });

  document.mutate_style(nodes.body, [](UIStyle &style) {
    style.flex_direction = FlexDirection::Column;
    style.align_items = AlignItems::Start;
    style.justify_content = JustifyContent::Start;
    style.width = UILength::percent(1.0f);
  });

  document.set_visible(nodes.body, options.open);
  return nodes;
}

inline bool disclosure_open(const UIDocument &document, const UIDisclosureNodes &nodes) {
  const auto *body = document.node(nodes.body);
  return body != nullptr && body->visible;
}

inline void set_disclosure_open(UIDocument &document, const UIDisclosureNodes &nodes, bool open) {
  document.set_visible(nodes.body, open);
}

} // namespace astralix::ui
