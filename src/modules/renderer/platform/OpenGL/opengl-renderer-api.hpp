#include "renderer-api.hpp"

namespace astralix {

class OpenGLRendererAPI : public RendererAPI {
public:
  OpenGLRendererAPI() { m_backend = RendererBackend::OpenGL; }

  void init() override;
  void set_viewport(uint32_t x, uint32_t y, uint32_t width,
                    uint32_t height) override;
  void clear_color(glm::vec4 color) override;
  void enable_buffer_testing() override;
  void disable_buffer_testing() override;
  void enable_depth_test() override;
  void disable_depth_test() override;
  void enable_depth_write() override;
  void disable_depth_write() override;
  void enable_blend() override;
  void disable_blend() override;
  void set_blend_func(BlendFactor src, BlendFactor dst) override;
  void enable_scissor() override;
  void disable_scissor() override;
  void set_scissor_rect(uint32_t x, uint32_t y, uint32_t width,
                        uint32_t height) override;
  void bind_texture_2d(uint32_t texture_id, uint32_t slot) override;
  void bind_texture_cube(uint32_t texture_id, uint32_t slot) override;

  void clear_buffers(ClearBufferType type = ClearBufferType::All) override;

  void cull_face(CullFaceMode mode) override;
  void depth(DepthMode mode) override;

  void draw_indexed(const Ref<VertexArray> &vertex_array,
                    DrawPrimitive primitive_type = DrawPrimitive::TRIANGLES,
                    uint32_t index_count = -1) override;

  void draw_instanced_indexed(DrawPrimitive primitive_type,
                              uint32_t index_count,
                              uint32_t instance_count) override;

  void draw_lines(const Ref<VertexArray> &vertex_array,
                  uint32_t vertex_count) override;

private:
  uint32_t map_draw_primitive_type(DrawPrimitive primitive_type);
  uint32_t map_cull_face_mode(CullFaceMode mode);
  uint32_t map_depth_mode(DepthMode mode);
  uint32_t map_blend_factor(BlendFactor factor);
};

} // namespace astralix
