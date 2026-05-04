#pragma once

#include "astralix/modules/renderer/entities/scene.hpp"
#include <glm/glm.hpp>
#include <guid.hpp>
#include <string>

using namespace astralix;

class RenderBenchmark : public Scene {
public:
  RenderBenchmark();

  void request_reset_scene() { m_should_reset_scene = true; }

  const std::string &active_model_id() const { return m_active_model_id; }
  void set_active_model_id(const std::string &model_id) { m_active_model_id = model_id; }

private:
  void setup() override;
  void after_preview_ready() override;
  void after_runtime_ready() override;
  void update_runtime() override;
  void build_source_world() override;
  void evaluate_build(SceneBuildContext &ctx) override;

  bool m_should_reset_scene = false;
  std::string m_active_model_id = "models::sponza_atrium";
};
