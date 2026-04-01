#pragma once

#include "components/light.hpp"
#include "guid.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace astralix::editor::scene_hierarchy_panel {

enum class ScopeBucket : uint8_t {
  SceneBacked = 0,
  WorldOnly,
};

enum class TypeBucket : uint8_t {
  Camera = 0,
  Light,
  Renderable,
  UI,
  Other,
};

inline constexpr std::array<ScopeBucket, 2u> scope_order{
    ScopeBucket::SceneBacked,
    ScopeBucket::WorldOnly,
};

inline constexpr std::array<TypeBucket, 5u> type_order{
    TypeBucket::Camera,
    TypeBucket::Light,
    TypeBucket::Renderable,
    TypeBucket::UI,
    TypeBucket::Other,
};

bool same_entity(EntityID lhs, EntityID rhs);
std::string lowercase_ascii(std::string_view value);
std::string entity_count_label(size_t count);
std::string entity_count_label(size_t visible_count, size_t total_count);
const char *light_type_label(rendering::LightType type);
bool contains_case_insensitive(std::string_view haystack, std::string_view needle);

ScopeBucket scope_bucket(bool scene_backed);
TypeBucket type_bucket(
    bool has_camera,
    bool has_light,
    bool has_renderable,
    bool has_ui_root
);

const char *scope_bucket_label(ScopeBucket bucket);
const char *type_bucket_label(TypeBucket bucket);
const char *type_bucket_icon(TypeBucket bucket);

std::string scope_group_key(ScopeBucket bucket);
std::string type_group_key(ScopeBucket scope, TypeBucket type);

bool default_group_open(std::string_view key);
void seed_default_group_open(std::unordered_map<std::string, bool> &group_open);

} // namespace astralix::editor::scene_hierarchy_panel
