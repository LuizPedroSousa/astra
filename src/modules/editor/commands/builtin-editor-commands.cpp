#include "commands/builtin-editor-commands.hpp"

#include "components/light.hpp"
#include "components/serialization/light.hpp"
#include "components/tags.hpp"
#include "components/transform.hpp"
#include "console.hpp"
#include "editor-selection-store.hpp"
#include "entities/serializers/scene-component-serialization.hpp"
#include "entities/serializers/scene-snapshot.hpp"
#include "glm/common.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "managers/scene-manager.hpp"
#include "tools/inspector/build.hpp"
#include "world.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace astralix::editor {
namespace {

using inspector_panel::ComponentDescriptor;

constexpr size_t k_ntityList_limit = 32u;

struct SelectedEntityContext {
  Scene *scene = nullptr;
  ecs::EntityRef entity;
};

struct SceneEntityInfo {
  EntityID id;
  std::string name;
  bool active = true;
};

ConsoleCommandResult success_result(std::vector<std::string> lines = {}) {
  ConsoleCommandResult result;
  result.success = true;
  result.lines = std::move(lines);
  return result;
}

ConsoleCommandResult error_result(std::string line) {
  ConsoleCommandResult result;
  result.success = false;
  result.lines.push_back(std::move(line));
  return result;
}

std::string lowercase_copy(std::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());

  for (const char character : value) {
    lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }

  return lowered;
}

std::string normalize_key(std::string_view value) {
  std::string normalized;
  normalized.reserve(value.size());

  for (const char character : value) {
    if (std::isalnum(static_cast<unsigned char>(character)) == 0) {
      continue;
    }

    normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }

  return normalized;
}

bool contains_case_insensitive(
    std::string_view haystack,
    std::string_view needle
) {
  if (needle.empty()) {
    return true;
  }

  const std::string lowered_haystack = lowercase_copy(haystack);
  const std::string lowered_needle = lowercase_copy(needle);
  return lowered_haystack.find(lowered_needle) != std::string::npos;
}

std::string join_strings(
    const std::vector<std::string> &items,
    std::string_view separator
) {
  if (items.empty()) {
    return {};
  }

  std::ostringstream stream;
  for (size_t index = 0u; index < items.size(); ++index) {
    if (index > 0u) {
      stream << separator;
    }
    stream << items[index];
  }

  return stream.str();
}

std::string join_arguments(
    const std::vector<std::string> &arguments,
    size_t start_index
) {
  if (start_index >= arguments.size()) {
    return {};
  }

  std::ostringstream stream;
  for (size_t index = start_index; index < arguments.size(); ++index) {
    if (index > start_index) {
      stream << ' ';
    }
    stream << arguments[index];
  }

  return stream.str();
}

std::string entity_label(EntityID entity_id, std::string_view name) {
  return std::string(name) + " (#" + static_cast<std::string>(entity_id) + ")";
}

float clean_float(float value) {
  return std::fabs(value) < 0.0001f ? 0.0f : value;
}

std::string format_float(float value) {
  return inspector_panel::format_value(clean_float(value));
}

std::string format_vec3(const glm::vec3 &value) {
  return format_float(value.x) + " " + format_float(value.y) + " " +
         format_float(value.z);
}

Scene *active_scene() {
  auto scene_manager = SceneManager::get();
  return scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
}

std::optional<SelectedEntityContext> current_selection_context() {
  Scene *scene = active_scene();
  if (scene == nullptr) {
    return std::nullopt;
  }

  const std::optional<EntityID> selected_entity_id =
      editor_selection_store()->selected_entity();
  if (!selected_entity_id.has_value()) {
    return std::nullopt;
  }

  if (!scene->world().contains(*selected_entity_id)) {
    editor_selection_store()->clear_selected_entity();
    return std::nullopt;
  }

  return SelectedEntityContext{
      .scene = scene,
      .entity = scene->world().entity(*selected_entity_id),
  };
}

std::optional<SelectedEntityContext> require_selected_entity(
    ConsoleCommandResult &result
) {
  Scene *scene = active_scene();
  if (scene == nullptr) {
    result = error_result("no active scene");
    return std::nullopt;
  }

  auto selection = current_selection_context();
  if (!selection.has_value()) {
    result = error_result("no entity selected");
    return std::nullopt;
  }

  return selection;
}

std::vector<std::string> visible_component_names(ecs::EntityRef entity) {
  std::vector<std::string> names;
  const auto components = serialization::collect_entity_component_snapshots(entity);
  names.reserve(components.size());

  for (const auto &component : components) {
    const ComponentDescriptor *descriptor =
        inspector_panel::find_component_descriptor(component.name);
    if (descriptor != nullptr && !descriptor->visible) {
      continue;
    }

    names.push_back(component.name);
  }

  return names;
}

std::vector<std::string> describe_entity(ecs::EntityRef entity) {
  std::vector<std::string> lines;
  lines.push_back(entity_label(entity.id(), entity.name()));
  lines.push_back(std::string("active: ") + (entity.active() ? "true" : "false"));

  const auto component_names = visible_component_names(entity);
  lines.push_back(
      "components: " +
      (component_names.empty() ? std::string("none")
                               : join_strings(component_names, ", "))
  );
  return lines;
}

std::vector<SceneEntityInfo> collect_scene_entities(Scene *scene) {
  std::vector<SceneEntityInfo> entities;
  if (scene == nullptr) {
    return entities;
  }

  auto &world = scene->world();
  world.each<astralix::scene::SceneEntity>(
      [&](EntityID entity_id, const astralix::scene::SceneEntity &) {
        auto entity = world.entity(entity_id);
        entities.push_back(SceneEntityInfo{
            .id = entity_id,
            .name = std::string(entity.name()),
            .active = entity.active(),
        });
      }
  );

  std::sort(
      entities.begin(),
      entities.end(),
      [](const SceneEntityInfo &lhs, const SceneEntityInfo &rhs) {
        const std::string lhs_key = lowercase_copy(lhs.name);
        const std::string rhs_key = lowercase_copy(rhs.name);
        if (lhs_key != rhs_key) {
          return lhs_key < rhs_key;
        }

        return static_cast<uint64_t>(lhs.id) < static_cast<uint64_t>(rhs.id);
      }
  );

  return entities;
}

std::vector<SceneEntityInfo> filter_entities_by_name(
    const std::vector<SceneEntityInfo> &entities,
    std::string_view query
) {
  std::vector<SceneEntityInfo> matches;
  for (const auto &entity : entities) {
    if (contains_case_insensitive(entity.name, query)) {
      matches.push_back(entity);
    }
  }

  return matches;
}

void append_match_details(
    ConsoleCommandResult &result,
    const std::vector<SceneEntityInfo> &matches
) {
  const size_t preview_count = std::min<size_t>(matches.size(), 5u);
  for (size_t index = 0u; index < preview_count; ++index) {
    const auto &match = matches[index];
    result.lines.push_back(
        " - " + entity_label(match.id, match.name) +
        (match.active ? "" : " [inactive]")
    );
  }

  if (matches.size() > preview_count) {
    result.lines.push_back(
        " - +" + std::to_string(matches.size() - preview_count) +
        " more matches"
    );
  }
}

std::optional<EntityID> resolve_entity_id(std::string_view value) {
  if (value.empty()) {
    return std::nullopt;
  }

  char *end = nullptr;
  const uint64_t parsed =
      std::strtoull(std::string(value).c_str(), &end, 10);
  if (end == nullptr || *end != '\0') {
    return std::nullopt;
  }

  return EntityID(parsed);
}

std::optional<bool> parse_bool(std::string_view value) {
  const std::string normalized = normalize_key(value);
  if (normalized == "1" || normalized == "true" || normalized == "on" ||
      normalized == "yes") {
    return true;
  }

  if (normalized == "0" || normalized == "false" || normalized == "off" ||
      normalized == "no") {
    return false;
  }

  return std::nullopt;
}

std::optional<glm::vec3> parse_vec3(
    const std::vector<std::string> &arguments,
    size_t start_index
) {
  if (arguments.size() < start_index + 3u) {
    return std::nullopt;
  }

  const auto x = inspector_panel::parse_float(arguments[start_index]);
  const auto y = inspector_panel::parse_float(arguments[start_index + 1u]);
  const auto z = inspector_panel::parse_float(arguments[start_index + 2u]);
  if (!x.has_value() || !y.has_value() || !z.has_value()) {
    return std::nullopt;
  }

  return glm::vec3(*x, *y, *z);
}

const ComponentDescriptor *resolve_component_descriptor(std::string_view value) {
  if (value.empty()) {
    return nullptr;
  }

  const std::string normalized_query = normalize_key(value);
  for (size_t index = 0u; index < inspector_panel::component_descriptor_count();
       ++index) {
    const ComponentDescriptor &descriptor =
        inspector_panel::component_descriptors()[index];
    if (normalize_key(descriptor.name) == normalized_query) {
      return &descriptor;
    }
  }

  return nullptr;
}

std::vector<std::string> known_component_names() {
  std::vector<std::string> names;
  names.reserve(inspector_panel::component_descriptor_count());

  for (size_t index = 0u; index < inspector_panel::component_descriptor_count();
       ++index) {
    names.push_back(inspector_panel::component_descriptors()[index].name);
  }

  return names;
}

std::optional<rendering::LightType> parse_light_type(std::string_view value) {
  const std::string normalized = normalize_key(value);
  if (normalized == "directional") {
    return rendering::LightType::Directional;
  }

  if (normalized == "point") {
    return rendering::LightType::Point;
  }

  if (normalized == "spot") {
    return rendering::LightType::Spot;
  }

  return std::nullopt;
}

std::string default_light_name(rendering::LightType type) {
  switch (type) {
    case rendering::LightType::Directional:
      return "Directional Light";
    case rendering::LightType::Point:
      return "Point Light";
    case rendering::LightType::Spot:
      return "Spot Light";
  }

  return "Light";
}

ConsoleCommandResult handle_selection_command(
    const ConsoleCommandInvocation &invocation
) {
  const auto &arguments = invocation.arguments;
  if (arguments.empty() || normalize_key(arguments.front()) == "get") {
    auto selection = current_selection_context();
    if (!selection.has_value()) {
      return success_result({"no entity selected"});
    }

    return success_result(describe_entity(selection->entity));
  }

  const std::string action = normalize_key(arguments.front());
  if (action == "clear") {
    editor_selection_store()->clear_selected_entity();
    return success_result({"selection cleared"});
  }

  Scene *scene = active_scene();
  if (scene == nullptr) {
    return error_result("no active scene");
  }

  if (action == "id") {
    if (arguments.size() < 2u) {
      return error_result("usage: selection id <entity_id>");
    }

    const auto entity_id = resolve_entity_id(arguments[1u]);
    if (!entity_id.has_value()) {
      return error_result("invalid entity id: " + arguments[1u]);
    }

    if (!scene->world().contains(*entity_id)) {
      return error_result(
          "entity not found in active scene: #" +
          static_cast<std::string>(*entity_id)
      );
    }

    editor_selection_store()->set_selected_entity(*entity_id);
    return success_result(describe_entity(scene->world().entity(*entity_id)));
  }

  if (action == "name") {
    const std::string query = join_arguments(arguments, 1u);
    if (query.empty()) {
      return error_result("usage: selection name <entity_name>");
    }

    const auto entities = collect_scene_entities(scene);
    std::vector<SceneEntityInfo> exact_matches;
    std::vector<SceneEntityInfo> fuzzy_matches;

    for (const auto &entity : entities) {
      if (lowercase_copy(entity.name) == lowercase_copy(query)) {
        exact_matches.push_back(entity);
      } else if (contains_case_insensitive(entity.name, query)) {
        fuzzy_matches.push_back(entity);
      }
    }

    const auto &matches = exact_matches.empty() ? fuzzy_matches : exact_matches;
    if (matches.empty()) {
      return error_result("no entity matches: " + query);
    }

    if (matches.size() > 1u) {
      ConsoleCommandResult result =
          error_result("selection is ambiguous for: " + query);
      append_match_details(result, matches);
      return result;
    }

    editor_selection_store()->set_selected_entity(matches.front().id);
    return success_result(
        describe_entity(scene->world().entity(matches.front().id))
    );
  }

  return error_result(
      "unknown selection subcommand: " + arguments.front()
  );
}

ConsoleCommandResult handle_entity_command(
    const ConsoleCommandInvocation &invocation
) {
  const auto &arguments = invocation.arguments;
  const std::string action =
      arguments.empty() ? std::string("get") : normalize_key(arguments.front());

  if (action == "list" || action == "ls") {
    Scene *scene = active_scene();
    if (scene == nullptr) {
      return error_result("no active scene");
    }

    const std::string query = join_arguments(arguments, 1u);
    auto entities = collect_scene_entities(scene);
    if (!query.empty()) {
      entities = filter_entities_by_name(entities, query);
    }

    if (entities.empty()) {
      return success_result(
          {query.empty() ? "no entities in active scene"
                         : "no entities match: " + query}
      );
    }

    std::vector<std::string> lines;
    lines.reserve(std::min(entities.size(), k_ntityList_limit) + 1u);

    const std::optional<EntityID> selected_entity_id =
        editor_selection_store()->selected_entity();
    const size_t visible_count = std::min(entities.size(), k_ntityList_limit);
    for (size_t index = 0u; index < visible_count; ++index) {
      const auto &entity = entities[index];
      const bool selected =
          selected_entity_id.has_value() &&
          static_cast<uint64_t>(*selected_entity_id) ==
              static_cast<uint64_t>(entity.id);

      lines.push_back(
          std::string(selected ? "* " : "  ") +
          entity_label(entity.id, entity.name) +
          (entity.active ? "" : " [inactive]")
      );
    }

    if (entities.size() > visible_count) {
      lines.push_back(
          "showing " + std::to_string(visible_count) + " of " +
          std::to_string(entities.size()) + " entities"
      );
    }

    return success_result(std::move(lines));
  }

  if (action == "new") {
    Scene *scene = active_scene();
    if (scene == nullptr) {
      return error_result("no active scene");
    }

    if (arguments.size() < 2u) {
      return error_result(
          "usage: entity new <empty|light> [options]"
      );
    }

    const std::string kind = normalize_key(arguments[1u]);
    if (kind == "empty") {
      const std::string name =
          join_arguments(arguments, 2u).empty() ? "GameObject"
                                                : join_arguments(arguments, 2u);

      auto entity = scene->spawn_entity(name);
      entity.emplace<astralix::scene::SceneEntity>();
      entity.emplace<astralix::scene::Transform>();
      editor_selection_store()->set_selected_entity(entity.id());
      return success_result({"created " + entity_label(entity.id(), entity.name())});
    }

    if (kind == "light") {
      rendering::LightType type = rendering::LightType::Directional;
      size_t name_index = 2u;

      if (arguments.size() >= 3u) {
        const auto parsed_type = parse_light_type(arguments[2u]);
        if (parsed_type.has_value()) {
          type = *parsed_type;
          name_index = 3u;
        } else if (arguments.size() > 3u) {
          return error_result(
              "usage: entity new light [directional|point|spot] [name]"
          );
        }
      }

      const std::string name =
          join_arguments(arguments, name_index).empty()
              ? default_light_name(type)
              : join_arguments(arguments, name_index);

      auto entity = scene->spawn_entity(name);
      entity.emplace<astralix::scene::SceneEntity>();
      entity.emplace<astralix::scene::Transform>();
      entity.emplace<rendering::Light>(rendering::Light{.type = type});
      if (type == rendering::LightType::Point) {
        entity.emplace<rendering::PointLightAttenuation>();
      } else if (type == rendering::LightType::Spot) {
        entity.emplace<rendering::SpotLightCone>();
      }

      editor_selection_store()->set_selected_entity(entity.id());
      return success_result({"created " + entity_label(entity.id(), entity.name())});
    }

    return error_result("unknown entity type: " + arguments[1u]);
  }

  ConsoleCommandResult result;
  auto selection = require_selected_entity(result);
  if (!selection.has_value()) {
    return result;
  }

  auto entity = selection->entity;

  if (action == "get") {
    return success_result(describe_entity(entity));
  }

  if (action == "rename") {
    const std::string name = join_arguments(arguments, 1u);
    if (name.empty()) {
      return error_result("usage: entity rename <name>");
    }

    entity.set_name(name);
    return success_result({"renamed to " + entity_label(entity.id(), entity.name())});
  }

  if (action == "active") {
    if (arguments.size() < 2u) {
      return error_result("usage: entity active <on|off>");
    }

    const auto active = parse_bool(arguments[1u]);
    if (!active.has_value()) {
      return error_result("invalid active state: " + arguments[1u]);
    }

    entity.set_active(*active);
    return success_result(
        {entity_label(entity.id(), entity.name()) +
         (*active ? " activated" : " deactivated")}
    );
  }

  if (action == "delete") {
    const std::string label = entity_label(entity.id(), entity.name());
    selection->scene->world().destroy(entity.id());
    editor_selection_store()->clear_selected_entity();
    return success_result({"deleted " + label});
  }

  return error_result("unknown entity subcommand: " + arguments.front());
}

ConsoleCommandResult handle_transform_command(
    const ConsoleCommandInvocation &invocation
) {
  ConsoleCommandResult result;
  auto selection = require_selected_entity(result);
  if (!selection.has_value()) {
    return result;
  }

  auto entity = selection->entity;
  auto *transform = entity.get<astralix::scene::Transform>();
  if (transform == nullptr) {
    return error_result("selected entity has no Transform component");
  }

  const auto &arguments = invocation.arguments;
  const std::string action =
      arguments.empty() ? std::string("get") : normalize_key(arguments.front());

  if (action == "get") {
    const glm::vec3 euler_degrees =
        glm::degrees(glm::eulerAngles(transform->rotation));
    return success_result({
        "position: " + format_vec3(transform->position),
        "rotation: " + format_vec3(euler_degrees),
        "scale: " + format_vec3(transform->scale),
    });
  }

  if (action == "reset") {
    *transform = astralix::scene::Transform{};
    selection->scene->world().touch();
    return success_result({"transform reset"});
  }

  if (action == "pos" || action == "position") {
    const auto position = parse_vec3(arguments, 1u);
    if (!position.has_value()) {
      return error_result("usage: transform pos <x> <y> <z>");
    }

    transform->position = *position;
    transform->dirty = true;
    selection->scene->world().touch();
    return success_result({"position: " + format_vec3(transform->position)});
  }

  if (action == "move" || action == "translate") {
    const auto delta = parse_vec3(arguments, 1u);
    if (!delta.has_value()) {
      return error_result("usage: transform move <x> <y> <z>");
    }

    transform->position += *delta;
    transform->dirty = true;
    selection->scene->world().touch();
    return success_result({"position: " + format_vec3(transform->position)});
  }

  if (action == "scale") {
    glm::vec3 scale(1.0f);
    if (arguments.size() == 2u) {
      const auto uniform = inspector_panel::parse_float(arguments[1u]);
      if (!uniform.has_value()) {
        return error_result("usage: transform scale <s> | <x> <y> <z>");
      }

      scale = glm::vec3(*uniform);
    } else {
      const auto parsed_scale = parse_vec3(arguments, 1u);
      if (!parsed_scale.has_value()) {
        return error_result("usage: transform scale <s> | <x> <y> <z>");
      }

      scale = *parsed_scale;
    }

    transform->scale = scale;
    transform->dirty = true;
    selection->scene->world().touch();
    return success_result({"scale: " + format_vec3(transform->scale)});
  }

  if (action == "rotate" || action == "rot" || action == "roteuler") {
    const auto euler_degrees = parse_vec3(arguments, 1u);
    if (!euler_degrees.has_value()) {
      return error_result("usage: transform rotate <x> <y> <z>");
    }

    transform->rotation = glm::quat(glm::radians(*euler_degrees));
    transform->dirty = true;
    selection->scene->world().touch();
    return success_result({"rotation: " + format_vec3(*euler_degrees)});
  }

  return error_result("unknown transform subcommand: " + arguments.front());
}

ConsoleCommandResult handle_component_command(
    const ConsoleCommandInvocation &invocation
) {
  ConsoleCommandResult result;
  auto selection = require_selected_entity(result);
  if (!selection.has_value()) {
    return result;
  }

  auto entity = selection->entity;
  const auto &arguments = invocation.arguments;
  const std::string action =
      arguments.empty() ? std::string("list") : normalize_key(arguments.front());

  if (action == "list" || action == "ls") {
    const auto names = visible_component_names(entity);
    return success_result(
        {"components: " +
         (names.empty() ? std::string("none") : join_strings(names, ", "))}
    );
  }

  const std::string component_query = join_arguments(arguments, 1u);
  if (component_query.empty()) {
    return error_result(
        "usage: component <has|add|remove> <ComponentName>"
    );
  }

  const ComponentDescriptor *descriptor =
      resolve_component_descriptor(component_query);
  if (descriptor == nullptr) {
    return error_result(
        "unknown component: " + component_query + " (known: " +
        join_strings(known_component_names(), ", ") + ")"
    );
  }

  const auto components = serialization::collect_entity_component_snapshots(entity);
  const bool present =
      inspector_panel::find_component_snapshot(components, descriptor->name) !=
      nullptr;

  if (action == "has") {
    return success_result(
        {std::string(descriptor->name) + ": " + (present ? "yes" : "no")}
    );
  }

  if (action == "add") {
    if (descriptor->can_add == nullptr || !descriptor->can_add(entity)) {
      return error_result(
          "cannot add component to selected entity: " +
          std::string(descriptor->name)
      );
    }

    serialization::apply_component_snapshot(
        entity, serialization::ComponentSnapshot{.name = descriptor->name}
    );
    return success_result({"added component: " + std::string(descriptor->name)});
  }

  if (action == "remove" || action == "rm") {
    if (!present) {
      return error_result(
          "selected entity does not have component: " +
          std::string(descriptor->name)
      );
    }

    if (!descriptor->removable || descriptor->remove_component == nullptr) {
      return error_result(
          "component cannot be removed: " + std::string(descriptor->name)
      );
    }

    descriptor->remove_component(entity);
    return success_result(
        {"removed component: " + std::string(descriptor->name)}
    );
  }

  return error_result("unknown component subcommand: " + arguments.front());
}

} // namespace

void register_builtin_editor_commands() {
  auto &console = console_manager();

  console.register_command(
      "selection",
      "Inspect or change the current entity selection.",
      &handle_selection_command,
      {"sel"}
  );

  console.register_command(
      "entity",
      "Create, inspect, and mutate scene entities.",
      &handle_entity_command
  );

  console.register_command(
      "transform",
      "Inspect or edit the selected entity transform.",
      &handle_transform_command,
      {"trfs"}
  );

  console.register_command(
      "component",
      "Inspect, add, and remove components on the selected entity.",
      &handle_component_command,
      {"comp"}
  );
}

} // namespace astralix::editor
