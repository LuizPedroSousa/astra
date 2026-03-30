#pragma once

#include "components/ui.hpp"
#include "events/key-event.hpp"
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace astralix::ui_system_core {

struct Target {
  EntityID entity_id;
  Ref<ui::UIDocument> document;
  ui::UINodeId node_id = ui::k_invalid_node_id;
};

struct RootEntry {
  EntityID entity_id;
  rendering::UIRoot *root = nullptr;
  Ref<ui::UIDocument> document = nullptr;
};

struct PointerHit {
  Target target;
  ui::UIHitPart part = ui::UIHitPart::Body;
  std::optional<size_t> item_index;
};

struct ScrollDispatch {
  Target target;
  glm::vec2 delta = glm::vec2(0.0f);
};

bool same_target(const std::optional<Target> &lhs, const std::optional<Target> &rhs);
ui::UILayoutContext make_context(const RootEntry &entry, const glm::vec2 &viewport_size);
bool target_uses_root(const RootEntry &entry, const Target &target);
const RootEntry *find_root_entry(const std::vector<RootEntry> &roots, const Target &target);
bool target_available(const std::vector<RootEntry> &roots, const std::optional<Target> &target, bool require_input_enabled);
std::optional<Target> target_from_node(const RootEntry &entry, ui::UINodeId node_id);
std::optional<PointerHit> target_from_hit(const RootEntry &entry, const ui::UIHitResult &hit);
std::optional<Target> map_to_ancestor_target(
    const RootEntry &entry, ui::UINodeId node_id,
    const std::function<std::optional<ui::UINodeId>(const ui::UIDocument &, ui::UINodeId)> &mapper
);
std::optional<Target> find_hoverable_target(const RootEntry &entry, ui::UINodeId node_id);
std::optional<Target> find_drag_handle_target(const RootEntry &entry, ui::UINodeId node_id);
std::optional<Target> find_draggable_panel_target(const RootEntry &entry, ui::UINodeId node_id);
std::optional<ui::UILayoutContext>
context_for_target(const std::vector<RootEntry> &roots, const Target &target, const glm::vec2 &viewport_size);
std::vector<Target> collect_focus_order(const std::vector<RootEntry> &roots);

bool should_reset_caret_for_key(const ui::UIKeyInputEvent &event);
bool shift_pressed();

float resolve_length(const ui::UILength &value, float basis, float rem_basis, float auto_value = 0.0f);
float resolve_min_length(const ui::UILength &value, float basis, float rem_basis, float content_value);
float resolve_max_length(const ui::UILength &value, float basis, float rem_basis, float content_value);

ui::UIRect parent_content_bounds(const ui::UIDocument &document, ui::UINodeId node_id);
void write_absolute_bounds_to_style(const Target &target, const ui::UIRect &parent_bounds, const ui::UIRect &bounds);
void canonicalize_absolute_bounds(const Target &target);
std::pair<float, float>
resolved_main_axis_limits(const ui::UIDocument::UINode &node, float basis, ui::FlexDirection direction, float rem_basis);

} // namespace astralix::ui_system_core
