#pragma once

#include "components/light.hpp"
#include "guid.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace astralix::editor::scene_hierarchy_panel {

bool same_entity(EntityID lhs, EntityID rhs);
std::string lowercase_ascii(std::string_view value);
std::string entity_count_label(size_t count);
std::string entity_count_label(size_t visible_count, size_t total_count);
const char *light_type_label(rendering::LightType type);
bool contains_case_insensitive(std::string_view haystack, std::string_view needle);

} // namespace astralix::editor::scene_hierarchy_panel
