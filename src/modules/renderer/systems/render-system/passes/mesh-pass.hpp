#pragma once

#include "managers/entity-manager.hpp"
#include "render-pass.hpp"
#include "storage-buffer.hpp"
#include "systems/render-system/collectors/mesh-collector.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"

namespace astralix {

class MeshPass : public RenderPass {
public:
  MeshPass() = default;
  ~MeshPass() override = default;

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;

    for (auto resource : resources) {

      switch (resource->desc.type) {

      case RenderGraphResourceType::LogicalBuffer: {
        if (resource->desc.name == "mesh_context") {
          m_mesh_context =
              static_cast<MeshContext *>(resource->get_logical_buffer());
        }
      }

      case RenderGraphResourceType::StorageBuffer: {
        if (resource->desc.name == "mesh_collector_storage") {
          m_mesh_collector_storage = resource->get_storage_buffer();
        }
      }

      default:
        break;
      }
    }

    if (m_mesh_context == nullptr || m_mesh_collector_storage == nullptr) {
      set_enabled(false);
      return;
    }
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    auto entity_manager = EntityManager::get();
    auto objects = entity_manager->get_entities<Object>();

    m_mesh_collector.draw(objects, m_render_target->renderer_api(),
                          m_mesh_collector_storage, m_mesh_context);
  }

  void end(double dt) override {}

  void cleanup() override {}

  std::string name() const override { return "MeshPass"; }

private:
  MeshCollector m_mesh_collector;
  MeshContext *m_mesh_context = nullptr;
  StorageBuffer *m_mesh_collector_storage = nullptr;
};

} // namespace astralix
