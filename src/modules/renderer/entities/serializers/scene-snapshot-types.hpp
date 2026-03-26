#pragma once

#include "guid.hpp"
#include "serialized-fields.hpp"
#include <string>
#include <vector>

namespace astralix::serialization {

struct ComponentSnapshot {
  std::string name;
  serialization::fields::FieldList fields;
};

struct EntitySnapshot {
  EntityID id;
  std::string name;
  bool active = true;
  std::vector<ComponentSnapshot> components;
};

} // namespace astralix::serialization
