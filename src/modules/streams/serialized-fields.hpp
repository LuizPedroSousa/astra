#pragma once

#include "guid.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace astralix::serialization::fields {

struct Field {
  std::string name;
  SerializableValue value;
};

using FieldList = std::vector<Field>;

inline const SerializableValue *find(const FieldList &fields,
                                     std::string_view name) {
  for (const auto &field : fields) {
    if (field.name == name) {
      return &field.value;
    }
  }

  return nullptr;
}

inline std::optional<std::string> read_string(const FieldList &fields,
                                              std::string_view name) {
  const auto *value = find(fields, name);
  if (value == nullptr || !std::holds_alternative<std::string>(*value)) {
    return std::nullopt;
  }

  return std::get<std::string>(*value);
}

inline std::optional<bool> read_bool(const FieldList &fields,
                                     std::string_view name) {
  const auto *value = find(fields, name);
  if (value == nullptr || !std::holds_alternative<bool>(*value)) {
    return std::nullopt;
  }

  return std::get<bool>(*value);
}

inline std::optional<int> read_int(const FieldList &fields,
                                   std::string_view name) {
  const auto *value = find(fields, name);
  if (value == nullptr) {
    return std::nullopt;
  }

  if (std::holds_alternative<int>(*value)) {
    return std::get<int>(*value);
  }

  if (std::holds_alternative<float>(*value)) {
    return static_cast<int>(std::get<float>(*value));
  }

  return std::nullopt;
}

inline std::optional<float> read_float(const FieldList &fields,
                                       std::string_view name) {
  const auto *value = find(fields, name);
  if (value == nullptr) {
    return std::nullopt;
  }

  if (std::holds_alternative<float>(*value)) {
    return std::get<float>(*value);
  }

  if (std::holds_alternative<int>(*value)) {
    return static_cast<float>(std::get<int>(*value));
  }

  return std::nullopt;
}

inline std::optional<EntityID> read_entity_id(const FieldList &fields,
                                              std::string_view name) {
  auto value = read_string(fields, name);
  if (!value.has_value() || value->empty()) {
    return std::nullopt;
  }

  return EntityID(std::stoull(*value));
}

inline glm::vec2 read_vec2(const FieldList &fields, const std::string &prefix,
                            const glm::vec2 &fallback = glm::vec2(0.0f)) {
  return glm::vec2(
      read_float(fields, prefix + ".x").value_or(fallback.x),
      read_float(fields, prefix + ".y").value_or(fallback.y));
}

inline glm::vec3 read_vec3(const FieldList &fields, const std::string &prefix,
                            const glm::vec3 &fallback = glm::vec3(0.0f)) {
  return glm::vec3(
      read_float(fields, prefix + ".x").value_or(fallback.x),
      read_float(fields, prefix + ".y").value_or(fallback.y),
      read_float(fields, prefix + ".z").value_or(fallback.z));
}

inline glm::quat
read_quat(const FieldList &fields, const std::string &prefix,
           const glm::quat &fallback = glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
  return glm::quat(
      read_float(fields, prefix + ".w").value_or(fallback.w),
      read_float(fields, prefix + ".x").value_or(fallback.x),
      read_float(fields, prefix + ".y").value_or(fallback.y),
      read_float(fields, prefix + ".z").value_or(fallback.z));
}

inline void append_vec2(FieldList &fields, const std::string &prefix,
                         const glm::vec2 &value) {
  fields.push_back({prefix + ".x", value.x});
  fields.push_back({prefix + ".y", value.y});
}

inline void append_vec3(FieldList &fields, const std::string &prefix,
                         const glm::vec3 &value) {
  fields.push_back({prefix + ".x", value.x});
  fields.push_back({prefix + ".y", value.y});
  fields.push_back({prefix + ".z", value.z});
}

inline void append_quat(FieldList &fields, const std::string &prefix,
                         const glm::quat &value) {
  fields.push_back({prefix + ".w", value.w});
  fields.push_back({prefix + ".x", value.x});
  fields.push_back({prefix + ".y", value.y});
  fields.push_back({prefix + ".z", value.z});
}

inline std::vector<std::string>
read_string_series(const FieldList &fields, const std::string &field_prefix) {
  std::vector<std::string> values;

  for (size_t index = 0;; ++index) {
    auto value = read_string(fields, field_prefix + std::to_string(index));
    if (!value.has_value()) {
      break;
    }

    values.push_back(*value);
  }

  return values;
}

} // namespace astralix::serialization::fields
