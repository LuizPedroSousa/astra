#pragma once

#include "render-pass.hpp"
#include "resources/mesh.hpp"
#include "resources/shader.hpp"
#include "types.hpp"

namespace astralix {

class UiPass : public RenderPass {
public:
  UiPass() = default;
  ~UiPass() override = default;

  void setup(Ref<RenderTarget> render_target,
             const std::vector<const RenderGraphResource *> &resources) override;
  void begin(double dt) override;
  void execute(double dt) override;
  void end(double dt) override;
  void cleanup() override;

  std::string name() const override { return "UiPass"; }
  bool has_side_effects() const override { return true; }

private:
  void ensure_shaders_loaded();
  void draw_rect_command(const ui::UiDrawCommand &command, glm::mat4 projection);
  void draw_image_command(const ui::UiDrawCommand &command,
                          glm::mat4 projection);
  void draw_text_command(const ui::UiDrawCommand &command, glm::mat4 projection);
  void apply_clip(const ui::UiDrawCommand &command, uint32_t framebuffer_height);

  Ref<Shader> m_solid_shader;
  Ref<Shader> m_image_shader;
  Ref<Shader> m_text_shader;
  Mesh m_quad = Mesh::quad(1.0f);
};

} // namespace astralix
