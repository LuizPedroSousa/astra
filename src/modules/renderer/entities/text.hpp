#include "entities/entity.hpp"
#include "guid.hpp"
#include "resources/mesh.hpp"
#include "targets/render-target.hpp"
#include <imgui.h>

namespace astralix {
class Text : public Entity<Text> {
public:
  Text(ENTITY_INIT_PARAMS, std::string text, ResourceDescriptorID font_id,
       glm::vec2 position, float scale, glm::vec3 color);

  void start(Ref<RenderTarget> render_target);
  void pre_update();
  void update();
  void post_update();

  void on_enable() override {};
  void on_disable() override {};

private:
  ResourceHandle m_cubemap_id;
  ResourceHandle m_shader_id;
  std::string m_text;

  glm::vec2 m_position;
  float m_scale;
  glm::vec3 m_color;

  ResourceDescriptorID m_font_id;

  Mesh m_mesh;
};

} // namespace astralix
