#include "entities/derived-override.hpp"

#include <algorithm>

namespace astralix {

const DerivedOverrideRecord *find_derived_override(
    const DerivedState &state,
    std::string_view generator_id,
    std::string_view stable_key
) {
  for (const auto &override_record : state.overrides) {
    if (override_record.key.matches(generator_id, stable_key)) {
      return &override_record;
    }
  }

  return nullptr;
}

bool is_derived_suppressed(
    const DerivedState &state,
    std::string_view generator_id,
    std::string_view stable_key
) {
  return std::any_of(
      state.suppressions.begin(),
      state.suppressions.end(),
      [&](const auto &suppression) {
        return suppression.key.matches(generator_id, stable_key);
      }
  );
}

void apply_derived_override(
    serialization::EntitySnapshot &snapshot,
    const DerivedOverrideRecord &override_record
) {
  snapshot.active = override_record.active;
  if (override_record.name.has_value()) {
    snapshot.name = *override_record.name;
  }

  snapshot.components.erase(
      std::remove_if(
          snapshot.components.begin(),
          snapshot.components.end(),
          [&](const auto &component) {
            return std::find(
                       override_record.removed_components.begin(),
                       override_record.removed_components.end(),
                       component.name
                   ) != override_record.removed_components.end();
          }
      ),
      snapshot.components.end()
  );

  for (const auto &override_component : override_record.components) {
    auto existing = std::find_if(
        snapshot.components.begin(),
        snapshot.components.end(),
        [&](const auto &component) {
          return component.name == override_component.name;
        }
    );

    if (existing != snapshot.components.end()) {
      *existing = override_component;
    } else {
      snapshot.components.push_back(override_component);
    }
  }
}

} // namespace astralix
