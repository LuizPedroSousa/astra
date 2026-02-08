#pragma once

#include "entities/text.hpp"
#include "managers/entity-manager.hpp"
#include "render-pass.hpp"

namespace astralix {

class TextPass : public RenderPass {
public:
  TextPass() = default;
  ~TextPass() override = default;

  void setup(Ref<RenderTarget> render_target, const std::vector<const RenderGraphResource*>& resources) override {
    m_render_target = render_target;

    auto entity_manager = EntityManager::get();
    entity_manager->for_each<Text>(
        [&](Text *text) { text->start(m_render_target); });
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    auto entity_manager = EntityManager::get();
    entity_manager->for_each<Text>([](Text *text) { text->update(); });
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "TextPass"; }
};

} // namespace astralix
