#pragma once

#include "editor-theme.hpp"
#include "inspector-panel-controller.hpp"
#include "world.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astralix::editor::inspector_panel {

inline constexpr std::string_view k_material_properties_component_name =
    "material_properties";

enum class FieldMode : uint8_t {
  ReadOnly,
  Text,
  Numeric,
  Toggle,
  Enum,
};

struct FieldGroup {
  std::string key;
  std::string label;
  std::vector<std::string> field_names;
  std::vector<std::string> axis_labels;
  std::vector<SerializableValue> values;
  FieldMode mode = FieldMode::ReadOnly;
  const std::vector<std::string> *options = nullptr;
};

struct ComponentDescriptor {
  const char *name = "";
  bool visible = true;
  bool removable = true;
  bool (*can_add)(ecs::EntityRef) = nullptr;
  void (*remove_component)(ecs::EntityRef) = nullptr;
  bool (*field_editable)(std::string_view) = nullptr;
  const std::vector<std::string> *(*enum_options)(std::string_view) = nullptr;
};

bool same_entity(EntityID lhs, EntityID rhs);
bool same_entity(
    const std::optional<EntityID> &lhs,
    const std::optional<EntityID> &rhs
);

const ComponentDescriptor *component_descriptors();
size_t component_descriptor_count();
const ComponentDescriptor *find_component_descriptor(std::string_view name);
bool is_material_properties_component(std::string_view name);

serialization::ComponentSnapshot *find_component_snapshot(
    std::vector<serialization::ComponentSnapshot> &components,
    std::string_view name
);
const serialization::ComponentSnapshot *find_component_snapshot(
    const std::vector<serialization::ComponentSnapshot> &components,
    std::string_view name
);
serialization::fields::Field *find_field(
    serialization::fields::FieldList &fields,
    std::string_view name
);

std::string format_value(const SerializableValue &value);
std::optional<int> parse_int(std::string_view value);
std::optional<float> parse_float(std::string_view value);

std::string humanize_token(std::string_view token);
std::string node_token(std::string_view value);
std::string component_count_label(size_t count);
std::string field_draft_key(
    std::string_view component_name,
    std::string_view field_name
);
std::string group_draft_key(
    std::string_view component_name,
    std::string_view field_name
);

std::vector<FieldGroup> build_field_groups(
    const serialization::ComponentSnapshot &component
);
size_t visible_component_count(
    const std::vector<serialization::ComponentSnapshot> &components
);
bool snapshots_equal(
    const std::vector<serialization::ComponentSnapshot> &lhs,
    const std::vector<serialization::ComponentSnapshot> &rhs
);
bool snapshots_equal(
    const InspectorPanelController::InspectedEntitySnapshot &lhs,
    const InspectorPanelController::InspectedEntitySnapshot &rhs
);

} // namespace astralix::editor::inspector_panel
