#include "tools/scene-hierachy/helpers.hpp"

#include <algorithm>
#include <cctype>

namespace astralix::editor::scene_hierarchy_panel {

bool same_entity(EntityID lhs, EntityID rhs) {
  return static_cast<uint64_t>(lhs) == static_cast<uint64_t>(rhs);
}

std::string lowercase_ascii(std::string_view value) {
  std::string lower(value);
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return lower;
}

std::string entity_count_label(size_t count) {
  return std::to_string(count) + (count == 1u ? " entity" : " entities");
}

std::string entity_count_label(size_t visible_count, size_t total_count) {
  if (visible_count == total_count) {
    return entity_count_label(total_count);
  }

  return std::to_string(visible_count) + " of " + std::to_string(total_count) +
         (total_count == 1u ? " entity" : " entities");
}

const char *light_type_label(rendering::LightType type) {
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

bool contains_case_insensitive(std::string_view haystack, std::string_view needle) {
  if (needle.empty()) {
    return true;
  }

  return lowercase_ascii(haystack).find(lowercase_ascii(needle)) !=
         std::string::npos;
}

} // namespace astralix::editor::scene_hierarchy_panel
