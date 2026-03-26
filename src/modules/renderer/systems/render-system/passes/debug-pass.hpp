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
  void update(Ref<RenderTarget> render_target, Framebuffer *shadow_map);

  void toggle_fullscreen() { m_fullscreen = !m_fullscreen; }

  void on_enable() override {};
  void on_disable() override {};

private:
  bool m_fullscreen = false;
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

class DebugGBuffer : public Entity<DebugGBuffer> {
public:
  DebugGBuffer(ENTITY_INIT_PARAMS);
  ~DebugGBuffer() = default;

  void start(Ref<RenderTarget> render_target);
  void update(Ref<RenderTarget> render_target, Framebuffer *g_buffer);

  void on_enable() override {};
  void on_disable() override {};

private:
  int m_attachment_index = 0;
};

class DebugGBufferPass : public RenderPass {
public:
  DebugGBufferPass() = default;
  ~DebugGBufferPass() override { delete m_entity_manager; };

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;
    m_entity_manager = new EntityManager();

    for (auto resource : resources) {
      switch (resource->desc.type) {
        case RenderGraphResourceType::Framebuffer: {
          if (resource->desc.name == "scene_color") {
            m_scene_color = resource->get_framebuffer();
          }

          if (resource->desc.name == "g_buffer") {
            m_g_buffer = resource->get_framebuffer();
          }
          break;
        }

        default:
          break;
      }
    }

    if (m_scene_color == nullptr || m_g_buffer == nullptr) {
      set_enabled(false);
      return;
    }

    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        m_render_target->renderer_api()->get_backend(), {"shaders::debug_g_buffer"});

    m_entity_manager->add_entity<DebugGBuffer>("g_buffer")->start(m_render_target);
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    auto g_buffer = m_entity_manager->get_entity<DebugGBuffer>();

    if (input::IS_KEY_RELEASED(input::KeyCode::F4) && g_buffer != nullptr) {
      g_buffer->set_active(!g_buffer->is_active());
    }

    if (g_buffer != nullptr && m_g_buffer != nullptr) {
      m_scene_color->bind();
      g_buffer->update(m_render_target, m_g_buffer);
      m_scene_color->unbind();
    }
  }

  void end(double dt) override {}

  void cleanup() override {
    delete m_entity_manager;
    m_entity_manager = nullptr;
  }

  std::string name() const override { return "DebugGBufferPass"; }

private:
  EntityManager *m_entity_manager = nullptr;
  Framebuffer *m_scene_color = nullptr;
  Framebuffer *m_g_buffer = nullptr;
};

class DebugOverlayPass : public RenderPass {
public:
  DebugOverlayPass() = default;
  ~DebugOverlayPass() override { delete m_entity_manager; };

  void
  setup(Ref<RenderTarget> render_target,
        const std::vector<const RenderGraphResource *> &resources) override {
    m_render_target = render_target;
    m_entity_manager = new EntityManager();

    for (auto resource : resources) {
      switch (resource->desc.type) {
        case RenderGraphResourceType::Framebuffer: {
          if (resource->desc.name == "shadow_map") {
            m_shadow_map = resource->get_framebuffer();
          }

          if (resource->desc.name == "scene_color") {
            m_scene_color = resource->get_framebuffer();
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

    if (m_scene_color == nullptr) {
      set_enabled(false);
      return;
    }

    Shader::create("debug_normal", "shaders/fragment/debug_normal.glsl"_engine,
                   "shaders/vertex/debug_normal.glsl"_engine,
                   "shaders/geometry/debug_normal.glsl"_engine);

    resource_manager()->load_from_descriptors_by_ids<ShaderDescriptor>(
        m_render_target->renderer_api()->get_backend(),
        {"debug_normal", "shaders::debug_depth"});

    m_entity_manager->add_entity<DebugNormal>("normal");

    if (m_shadow_map != nullptr) {
      m_entity_manager->add_entity<DebugDepth>("depth")->start(m_render_target);
    }
  }

  void begin(double dt) override {}

  void execute(double dt) override {
    auto depth = m_entity_manager->get_entity<DebugDepth>();
    auto normal = m_entity_manager->get_entity<DebugNormal>();

    m_scene_color->bind();

    if (input::IS_KEY_RELEASED(input::KeyCode::F2) && depth != nullptr) {
      if (input::IS_KEY_DOWN(input::KeyCode::LeftShift)) {
        depth->toggle_fullscreen();
      } else {
        depth->set_active(!depth->is_active());
      }
    }

    if (input::IS_KEY_RELEASED(input::KeyCode::F3)) {
      normal->set_active(!normal->is_active());
    }

    if (depth != nullptr && m_shadow_map != nullptr) {
      depth->update(m_render_target, m_shadow_map);
    }

    normal->update(m_render_target, m_mesh_collector, m_mesh_collector_storage,
                   m_mesh_context);

    m_scene_color->unbind();
  }

  void end(double dt) override {}

  void cleanup() override {
    delete m_entity_manager;
    m_entity_manager = nullptr;
  }

  std::string name() const override { return "DebugOverlayPass"; }

private:
  EntityManager *m_entity_manager = nullptr;
  Framebuffer *m_scene_color = nullptr;
  Framebuffer *m_shadow_map = nullptr;

  MeshCollector m_mesh_collector;
  MeshContext *m_mesh_context = nullptr;
  StorageBuffer *m_mesh_collector_storage = nullptr;
};

} // namespace astralix
