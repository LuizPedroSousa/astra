#pragma once

#include <cstdint>

namespace astralix::physics {

enum class RigidBodyMode : uint8_t {
  Static = 0,
  Dynamic = 1,
};

struct RigidBody {
  RigidBodyMode mode = RigidBodyMode::Dynamic;
  float gravity = 0.5f;
  float velocity = 2.0f;
  float acceleration = 2.0f;
  float drag = 0.0f;
  float mass = 1.0f;
};

} // namespace astralix::physics
