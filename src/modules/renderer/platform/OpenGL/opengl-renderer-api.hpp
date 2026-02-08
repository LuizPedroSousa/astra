#include "renderer-api.hpp"

namespace astralix {

class OpenGLRendererAPI : public RendererAPI {
public:
  OpenGLRendererAPI() { m_backend = RendererBackend::OpenGL; }

  void init() override;
  void set_viewport(uint32_t x, uint32_t y, uint32_t width,
                    uint32_t height) override;
  void clear_color() override;
  void enable_buffer_testing() override;
  void disable_buffer_testing() override;

  void clear_buffers() override;

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
};

} // namespace astralix
