#pragma once

#include "framebuffer.hpp"
#include "glm/glm.hpp"
#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/passes/render-graph-resource.hpp"
#include "targets/render-target.hpp"
#include "vertex-array.hpp"
#include "vertex-buffer.hpp"

#include <cstddef>
#include <vector>

namespace astralix {

class EditorGizmoPass : public RenderPass {
public:
  EditorGizmoPass() = default;
  ~EditorGizmoPass() override = default;

  void setup(
      Ref<RenderTarget> render_target,
      const std::vector<const RenderGraphResource *> &resources
  ) override;
  void begin(double dt) override;
  void execute(double dt) override;
  void end(double dt) override;
  void cleanup() override;

  std::string name() const override { return "EditorGizmoPass"; }

private:
  void ensure_mesh_resources(size_t required_vertices);

  Framebuffer *m_scene_color = nullptr;
  Ref<Shader> m_shader = nullptr;
  Ref<VertexArray> m_vertex_array = nullptr;
  Ref<VertexBuffer> m_vertex_buffer = nullptr;
  size_t m_vertex_capacity = 0u;
};

} // namespace astralix
