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

ScopeBucket scope_bucket(bool scene_backed) {
  return scene_backed ? ScopeBucket::SceneBacked : ScopeBucket::WorldOnly;
}

TypeBucket type_bucket(
    bool has_camera,
    bool has_light,
    bool has_renderable,
    bool has_ui_root
) {
  if (has_camera) {
    return TypeBucket::Camera;
  }
  if (has_light) {
    return TypeBucket::Light;
  }
  if (has_renderable) {
    return TypeBucket::Renderable;
  }
  if (has_ui_root) {
    return TypeBucket::UI;
  }

  return TypeBucket::Other;
}

const char *scope_bucket_label(ScopeBucket bucket) {
  switch (bucket) {
    case ScopeBucket::SceneBacked:
      return "Scene-backed";
    case ScopeBucket::WorldOnly:
      return "World-only";
  }

  return "Unknown";
}

const char *type_bucket_label(TypeBucket bucket) {
  switch (bucket) {
    case TypeBucket::Camera:
      return "Cameras";
    case TypeBucket::Light:
      return "Lights";
    case TypeBucket::Renderable:
      return "Renderables";
    case TypeBucket::UI:
      return "UI";
    case TypeBucket::Other:
      return "Other";
  }

  return "Other";
}

const char *type_bucket_icon(TypeBucket bucket) {
  switch (bucket) {
    case TypeBucket::Camera:
      return "icons::camera";
    case TypeBucket::Light:
      return "icons::light";
    case TypeBucket::Renderable:
      return "icons::mesh";
    case TypeBucket::UI:
    case TypeBucket::Other:
      return "icons::cube";
  }

  return "icons::cube";
}

std::string scope_group_key(ScopeBucket bucket) {
  switch (bucket) {
    case ScopeBucket::SceneBacked:
      return "scope:scene_backed";
    case ScopeBucket::WorldOnly:
      return "scope:world_only";
  }

  return "scope:unknown";
}

std::string type_group_key(ScopeBucket scope, TypeBucket type) {
  std::string key = "type:";
  key += scope == ScopeBucket::SceneBacked ? "scene_backed:" : "world_only:";

  switch (type) {
    case TypeBucket::Camera:
      key += "camera";
      break;
    case TypeBucket::Light:
      key += "light";
      break;
    case TypeBucket::Renderable:
      key += "renderable";
      break;
    case TypeBucket::UI:
      key += "ui";
      break;
    case TypeBucket::Other:
      key += "other";
      break;
  }

  return key;
}

bool default_group_open(std::string_view key) {
  return key.rfind("scope:", 0u) == 0u;
}

void seed_default_group_open(std::unordered_map<std::string, bool> &group_open) {
  for (ScopeBucket scope : scope_order) {
    const std::string scope_key = scope_group_key(scope);
    group_open.emplace(scope_key, default_group_open(scope_key));

    for (TypeBucket type : type_order) {
      const std::string type_key = type_group_key(scope, type);
      group_open.emplace(type_key, default_group_open(type_key));
    }
  }
}

} // namespace astralix::editor::scene_hierarchy_panel
