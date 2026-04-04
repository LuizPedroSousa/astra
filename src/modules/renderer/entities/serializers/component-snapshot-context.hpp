#pragma once

#include "assert.hpp"
#include "context-proxy.hpp"
#include "scene-snapshot-types.hpp"
#include "serialization-context.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace astralix {

inline void write_nested_field(
    ContextProxy ctx, std::string_view path, const SerializableValue &value
) {
  const size_t separator = path.find('.');
  if (separator == std::string_view::npos) {
    ctx[std::string(path)] = value;
    return;
  }

  write_nested_field(
      ctx[std::string(path.substr(0, separator))],
      path.substr(separator + 1),
      value
  );
}

inline std::optional<SerializableValue> read_serializable_value(ContextProxy &ctx) {
  switch (ctx.kind()) {
    case SerializationTypeKind::String:
      return SerializableValue(ctx.as<std::string>());
    case SerializationTypeKind::Int:
      return SerializableValue(ctx.as<int>());
    case SerializationTypeKind::Float:
      return SerializableValue(ctx.as<float>());
    case SerializationTypeKind::Bool:
      return SerializableValue(ctx.as<bool>());
    default:
      return std::nullopt;
  }
}

inline void append_nested_fields(
    ContextProxy &field_ctx,
    std::string_view prefix,
    serialization::fields::FieldList &fields
) {
  if (auto value = read_serializable_value(field_ctx); value.has_value()) {
    if (!prefix.empty()) {
      fields.push_back(serialization::fields::Field{
          .name = std::string(prefix),
          .value = std::move(*value),
      });
    }
    return;
  }

  if (field_ctx.kind() != SerializationTypeKind::Object) {
    return;
  }

  for (const auto &key : field_ctx.object_keys()) {
    const std::string field_name =
        prefix.empty() ? key : std::string(prefix) + "." + key;
    auto child_ctx = field_ctx[key];
    append_nested_fields(child_ctx, field_name, fields);
  }
}

inline std::optional<serialization::ComponentSnapshot>
read_component_snapshot(ContextProxy component_ctx) {
  if (component_ctx["type"].kind() != SerializationTypeKind::String) {
    return std::nullopt;
  }

  serialization::ComponentSnapshot component{
      .name = component_ctx["type"].as<std::string>(),
  };

  auto fields_ctx = component_ctx["fields"];
  component.fields.reserve(fields_ctx.size());

  append_nested_fields(fields_ctx, "", component.fields);

  return component;
}

} // namespace astralix
