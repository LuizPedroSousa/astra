#pragma once

#include <cstdint>

namespace astralix::terrain {

enum class ExecutionTrigger : uint8_t {
  OneShot,
  PerFrame,
  OnDemand,
};

enum class QueueHint : uint8_t {
  Main,
  AsyncCompute,
  Background,
};

struct ExecutionPolicy {
  ExecutionTrigger trigger = ExecutionTrigger::OneShot;
  QueueHint queue_hint = QueueHint::Main;
};

} // namespace astralix::terrain
