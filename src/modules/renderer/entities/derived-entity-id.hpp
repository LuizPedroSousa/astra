#pragma once

#include "fnv1a.hpp"
#include "world.hpp"

#include <cstdint>
#include <string_view>

namespace astralix {

[[nodiscard]] inline EntityID deterministic_derived_entity_id(
    std::string_view generator_id,
    std::string_view stable_key,
    const ecs::World &world
) {
  constexpr uint64_t k_derived_entity_offset_basis = 1469598103934665603ull;

  uint64_t hash = k_derived_entity_offset_basis;
  hash = fnv1a64_append_string(generator_id, hash);
  hash ^= 0xffu;
  hash *= k_fnv1a64_prime;
  hash = fnv1a64_append_string(stable_key, hash);

  while (hash == 0u || world.contains(EntityID(hash))) {
    hash ^= 0x9e3779b97f4a7c15ull;
    hash *= k_fnv1a64_prime;
  }

  return EntityID(hash);
}

} // namespace astralix
