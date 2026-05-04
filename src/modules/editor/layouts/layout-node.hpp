#pragma once

#include "base.hpp"
#include "types.hpp"
#include <cstdint>

namespace astralix::editor {

enum class LayoutNodeKind : uint8_t {
  Split,
  Tabs,
  Leaf,
};

struct LayoutSplitBehavior {
  bool resizable = true;
  bool show_divider = true;
  ui::UILength first_extent = ui::UILength::auto_value();
};

struct LayoutNode {
  LayoutNodeKind kind = LayoutNodeKind::Leaf;

  ui::FlexDirection split_axis = ui::FlexDirection::Row;
  float split_ratio = 0.5f;
  LayoutSplitBehavior split_behavior;
  Scope<LayoutNode> first;
  Scope<LayoutNode> second;

  std::vector<std::string> tabs;
  std::string active_tab_id;

  std::string panel_instance_id;

  LayoutNode() = default;
  LayoutNode(const LayoutNode &other) { *this = other; }
  LayoutNode &operator=(const LayoutNode &other) {
    if (this == &other) {
      return *this;
    }

    kind = other.kind;
    split_axis = other.split_axis;
    split_ratio = other.split_ratio;
    split_behavior = other.split_behavior;
    first = other.first != nullptr
                ? create_scope<LayoutNode>(*other.first)
                : nullptr;
    second = other.second != nullptr
                 ? create_scope<LayoutNode>(*other.second)
                 : nullptr;
    tabs = other.tabs;
    active_tab_id = other.active_tab_id;
    panel_instance_id = other.panel_instance_id;
    return *this;
  }
  LayoutNode(LayoutNode &&) noexcept = default;
  LayoutNode &operator=(LayoutNode &&) noexcept = default;

  static LayoutNode leaf(std::string panel_instance_id) {
    LayoutNode node;
    node.kind = LayoutNodeKind::Leaf;
    node.panel_instance_id = std::move(panel_instance_id);
    return node;
  }

  static LayoutNode tabs_node(
      std::vector<std::string> tabs, std::string active_tab_id
  ) {
    LayoutNode node;
    node.kind = LayoutNodeKind::Tabs;
    node.tabs = std::move(tabs);
    node.active_tab_id = std::move(active_tab_id);
    return node;
  }

  static LayoutNode split(
      ui::FlexDirection axis,
      float ratio,
      LayoutNode first,
      LayoutNode second,
      LayoutSplitBehavior behavior = {}
  ) {
    LayoutNode node;
    node.kind = LayoutNodeKind::Split;
    node.split_axis = axis;
    node.split_ratio = ratio;
    node.split_behavior = behavior;
    node.first = create_scope<LayoutNode>(std::move(first));
    node.second = create_scope<LayoutNode>(std::move(second));
    return node;
  }
};

struct LayoutTemplate {
  std::string id;
  std::string title;
  LayoutNode root;
};

} // namespace astralix::editor
