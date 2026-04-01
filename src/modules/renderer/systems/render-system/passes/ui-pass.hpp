#pragma once

#include "render-pass.hpp"
#include "resources/mesh.hpp"
#include "resources/shader.hpp"
#include "types.hpp"
#include "vertex-array.hpp"
#include "vertex-buffer.hpp"

#include <cstddef>

namespace astralix {

  class UIPass : public RenderPass {
  public:
    UIPass() = default;
    ~UIPass() override = default;

    void setup(Ref<RenderTarget> render_target, const std::vector<const RenderGraphResource*>& resources) override;
    void begin(double dt) override;
    void execute(double dt) override;
    void end(double dt) override;
    void cleanup() override;

    std::string name() const override { return "UIPass"; }
    bool has_side_effects() const override { return true; }

  private:
    void ensure_shaders_loaded();
    void draw_rect_command(const ui::UIDrawCommand& command, glm::mat4 projection);
    void draw_image_command(const ui::UIDrawCommand& command, glm::mat4 projection);
    void draw_svg_image_command(const ui::UIDrawCommand& command, glm::mat4 projection);
    void draw_render_image_view_command(const ui::UIDrawCommand& command, glm::mat4 projection);
    void draw_text_command(const ui::UIDrawCommand& command, glm::mat4 projection);
    void draw_polyline_command(const ui::UIDrawCommand& command, glm::mat4 projection);
    void draw_color_triangles(
        const std::vector<ui::UIPolylineVertex>& triangle_vertices,
        glm::mat4 projection
    );
    void apply_clip(const ui::UIDrawCommand& command, uint32_t framebuffer_height);
    void ensure_polyline_resources(size_t required_vertices);

    Ref<Shader> m_solid_shader;
    Ref<Shader> m_image_shader;
    Ref<Shader> m_text_shader;
    Mesh m_quad = Mesh::quad(1.0f);

    Ref<Shader> m_polyline_shader;
    Ref<VertexArray> m_polyline_vertex_array = nullptr;
    Ref<VertexBuffer> m_polyline_vertex_buffer = nullptr;
    size_t m_polyline_vertex_capacity = 0u;
  };

} // namespace astralix
