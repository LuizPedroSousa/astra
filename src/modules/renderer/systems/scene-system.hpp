#pragma once
#include "entities/scene.hpp"
#include "systems/system.hpp"

namespace astralix {

  inline bool scene_camera_input_enabled(
      bool console_captures_input,
      bool cursor_captured
  ) {
    return !console_captures_input && cursor_captured;
  }

  class SceneSystem : public System<SceneSystem> {

  public:
    void start() override;
    void fixed_update(double fixed_dt) override;
    void pre_update(double dt) override;
    void update(double dt) override;
  };

} // namespace astralix
