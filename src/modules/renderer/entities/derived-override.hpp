#pragma once

#include "entities/serializers/scene-snapshot-types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace astralix {

struct DerivedEntityKey {
  std::string generator_id;
  std::string stable_key;

  bool matches(std::string_view generator, std::string_view key) const {
    return generator_id == generator && stable_key == key;
  }
};

struct DerivedSuppressionRecord {
  DerivedEntityKey key;
};

struct DerivedOverrideRecord {
  DerivedEntityKey key;
  bool active = true;
  std::optional<std::string> name;
  std::vector<std::string> removed_components;
  std::vector<serialization::ComponentSnapshot> components;
};

struct DerivedState {
  std::vector<DerivedOverrideRecord> overrides;
  std::vector<DerivedSuppressionRecord> suppressions;
};

const DerivedOverrideRecord *find_derived_override(
    const DerivedState &state,
    std::string_view generator_id,
    std::string_view stable_key
);

bool is_derived_suppressed(
    const DerivedState &state,
    std::string_view generator_id,
    std::string_view stable_key
);

void apply_derived_override(
    serialization::EntitySnapshot &snapshot,
    const DerivedOverrideRecord &override_record
);

} // namespace astralix
