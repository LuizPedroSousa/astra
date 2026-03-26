#pragma once

#include "entities/text.hpp"
#include "framebuffer.hpp"
#include "managers/entity-manager.hpp"
#include "render-pass.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"

namespace astralix {

class TextPass : public RenderPass {
public:
  TextPass() = default;
  ~TextPass() override = default;

  void setup(Ref<RenderTarget> render_target, const std::vector<const RenderGraphResource*>& resources) override {
    m_render_target = render_target;

    for (auto resource : resources) {
      if (resource->desc.type == RenderGraphResourceType::Framebuffer &&
          resource->desc.name == "scene_color") {
        m_scene_color = resource->get_framebuffer();
      }
    }

    if (m_scene_color == nullptr) {
      set_enabled(false);
      return;
    }

    auto entity_manager = EntityManager::get();
    entity_manager->for_each<Text>(
        [&](Text *text) { text->start(m_render_target); });
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    m_scene_color->bind();

    auto entity_manager = EntityManager::get();
    entity_manager->for_each<Text>([](Text *text) { text->update(); });

    m_scene_color->unbind();
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "TextPass"; }

private:
  Framebuffer *m_scene_color = nullptr;
};

} // namespace astralix
