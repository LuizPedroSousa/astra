#include "entities/serializers/derived-override-serializer.hpp"

#include "entities/serializers/component-snapshot-context.hpp"
#include "context-proxy.hpp"
#include "serialization-context.hpp"

#include <string_view>

namespace astralix {

void serialize_derived_state(SerializationContext &ctx, const DerivedState &state) {
  for (size_t index = 0; index < state.overrides.size(); ++index) {
    const auto &override_record = state.overrides[index];
    auto override_ctx = ctx["derived_overrides"][static_cast<int>(index)];
    override_ctx["generator_id"] = override_record.key.generator_id;
    override_ctx["stable_key"] = override_record.key.stable_key;
    override_ctx["active"] = override_record.active;

    if (override_record.name.has_value()) {
      override_ctx["name"] = *override_record.name;
    }

    for (size_t component_index = 0;
         component_index < override_record.removed_components.size();
         ++component_index) {
      override_ctx["removed_components"][static_cast<int>(component_index)] =
          override_record.removed_components[component_index];
    }

    for (size_t component_index = 0;
         component_index < override_record.components.size();
         ++component_index) {
      const auto &component = override_record.components[component_index];
      auto component_ctx =
          override_ctx["components"][static_cast<int>(component_index)];
      component_ctx["type"] = component.name;
      for (const auto &field : component.fields) {
        write_nested_field(component_ctx["fields"], field.name, field.value);
      }
    }
  }

  for (size_t index = 0; index < state.suppressions.size(); ++index) {
    const auto &suppression = state.suppressions[index];
    auto suppression_ctx =
        ctx["derived_suppressions"][static_cast<int>(index)];
    suppression_ctx["generator_id"] = suppression.key.generator_id;
    suppression_ctx["stable_key"] = suppression.key.stable_key;
  }
}

DerivedState deserialize_derived_state(const Ref<SerializationContext> &ctx) {
  DerivedState state;
  if (ctx == nullptr) {
    return state;
  }

  const size_t override_count = (*ctx)["derived_overrides"].size();
  state.overrides.reserve(override_count);
  for (size_t index = 0; index < override_count; ++index) {
    auto override_ctx = (*ctx)["derived_overrides"][static_cast<int>(index)];
    if (override_ctx["generator_id"].kind() != SerializationTypeKind::String ||
        override_ctx["stable_key"].kind() != SerializationTypeKind::String) {
      continue;
    }

    DerivedOverrideRecord override_record{
        .key =
            DerivedEntityKey{
                .generator_id = override_ctx["generator_id"].as<std::string>(),
                .stable_key = override_ctx["stable_key"].as<std::string>(),
            },
        .active = override_ctx["active"].kind() == SerializationTypeKind::Bool
                      ? override_ctx["active"].as<bool>()
                      : true,
    };

    if (override_ctx["name"].kind() == SerializationTypeKind::String) {
      override_record.name = override_ctx["name"].as<std::string>();
    }

    const size_t removed_component_count =
        override_ctx["removed_components"].size();
    override_record.removed_components.reserve(removed_component_count);
    for (size_t component_index = 0; component_index < removed_component_count;
         ++component_index) {
      auto removed_component_ctx =
          override_ctx["removed_components"][static_cast<int>(component_index)];
      if (removed_component_ctx.kind() == SerializationTypeKind::String) {
        override_record.removed_components.push_back(
            removed_component_ctx.as<std::string>()
        );
      }
    }

    const size_t component_count = override_ctx["components"].size();
    override_record.components.reserve(component_count);
    for (size_t component_index = 0; component_index < component_count;
         ++component_index) {
      auto component = read_component_snapshot(
          override_ctx["components"][static_cast<int>(component_index)]
      );
      if (component.has_value()) {
        override_record.components.push_back(std::move(*component));
      }
    }

    state.overrides.push_back(std::move(override_record));
  }

  const size_t suppression_count = (*ctx)["derived_suppressions"].size();
  state.suppressions.reserve(suppression_count);
  for (size_t index = 0; index < suppression_count; ++index) {
    auto suppression_ctx =
        (*ctx)["derived_suppressions"][static_cast<int>(index)];
    if (suppression_ctx["generator_id"].kind() != SerializationTypeKind::String ||
        suppression_ctx["stable_key"].kind() != SerializationTypeKind::String) {
      continue;
    }

    state.suppressions.push_back(DerivedSuppressionRecord{
        .key =
            DerivedEntityKey{
                .generator_id =
                    suppression_ctx["generator_id"].as<std::string>(),
                .stable_key =
                    suppression_ctx["stable_key"].as<std::string>(),
            },
    });
  }

  return state;
}

} // namespace astralix
