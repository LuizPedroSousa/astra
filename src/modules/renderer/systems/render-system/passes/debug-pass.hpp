#pragma once

#include "entities/entity.hpp"
#include "events/key-codes.hpp"
#include "managers/entity-manager.hpp"
#include "managers/resource-manager.hpp"
#include "managers/window-manager.hpp"
#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/collectors/mesh-collector.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "targets/render-target.hpp"

namespace astralix {

class DebugDepth : public Entity<DebugDepth> {
public:
  DebugDepth(ENTITY_INIT_PARAMS);
  ~DebugDepth() = default;

  void start(Ref<RenderTarget> render_target);
  void update(Ref<RenderTarget> render_target,
              Framebuffer *shadow_mapping_framebuffer);

  void on_enable() override {};
  void on_disable() override {};
};

class DebugNormal : public Entity<DebugNormal> {
public:
  DebugNormal(ENTITY_INIT_PARAMS);
  ~DebugNormal() = default;

  void start();
  void update(Ref<RenderTarget> render_target, MeshCollector &mesh_collector,
              StorageBuffer *mesh_collector_storage, MeshContext *mesh_ctx);

  void on_enable() override {};
  void on_disable() override {};
};

class DebugPass : public RenderPass {
public:
  DebugPass() = default;
  ~DebugPass() override { delete m_entity_manager; };

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;
    m_entity_manager = new EntityManager();

    for (auto resource : resources) {
      switch (resource->desc.type) {
      case RenderGraphResourceType::Framebuffer: {
        if (resource->desc.name == "shadow_map") {
          m_shadow_mapping_framebuffer = resource->get_framebuffer();
        }
        break;
      }

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

    Shader::create("debug_normal", "shaders/fragment/debug_normal.glsl"_engine,
                   "shaders/vertex/debug_normal.glsl"_engine,
                   "shaders/geometry/debug_normal.glsl"_engine);

    if (m_shadow_mapping_framebuffer != nullptr) {
      Shader::create("debug_depth", "shaders/fragment/debug_depth.glsl"_engine,
                     "shaders/vertex/debug_depth.glsl"_engine);
    }

    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        m_render_target->renderer_api()->get_backend(),
        {"debug_normal", "debug_depth"});

    auto event_dispatcher = EventDispatcher::get();

    m_entity_manager->add_entity<DebugNormal>("normal");

    if (m_shadow_mapping_framebuffer != nullptr) {
      m_entity_manager->add_entity<DebugDepth>("depth")->start(m_render_target);
    }
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    auto depth = m_entity_manager->get_entity<DebugDepth>();
    auto normal = m_entity_manager->get_entity<DebugNormal>();

    if (input::IS_KEY_RELEASED(input::KeyCode::F2) && depth != nullptr) {
      depth->set_active(!depth->is_active());
    }

    if (input::IS_KEY_RELEASED(input::KeyCode::F3)) {
      normal->set_active(!normal->is_active());
    }

    if (depth != nullptr && m_shadow_mapping_framebuffer != nullptr) {
      depth->update(m_render_target, m_shadow_mapping_framebuffer);
    }

    normal->update(m_render_target, m_mesh_collector, m_mesh_collector_storage,
                   m_mesh_context);
  }

  void end(double dt) override {}

  void cleanup() override { delete m_entity_manager; }

  std::string name() const override { return "DebugPass"; }

private:
  EntityManager *m_entity_manager = nullptr;
  Framebuffer *m_shadow_mapping_framebuffer = nullptr;

  MeshCollector m_mesh_collector;
  MeshContext *m_mesh_context = nullptr;
  StorageBuffer *m_mesh_collector_storage = nullptr;
};

} // namespace astralix
