#pragma once
#include "entities/scene.hpp"
#include "systems/system.hpp"

namespace astralix {

  class SceneSystem : public System<SceneSystem> {

  public:
    void start() override;
    void fixed_update(double fixed_dt) override;
    void pre_update(double dt) override;
    void update(double dt) override;
  };

} // namespace astralix
