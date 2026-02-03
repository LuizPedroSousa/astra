#include "entities/entity.hpp"
#include "guid.hpp"
#include "targets/render-target.hpp"
#include <imgui.h>

namespace astralix {
class Skybox : public Entity<Skybox> {
public:
  Skybox(ENTITY_INIT_PARAMS, const ResourceDescriptorID cubemap_id,
         const ResourceDescriptorID shader_id);
  Skybox() {};

  void start(Ref<RenderTarget> render_target);
  void pre_update();
  void update(Ref<RenderTarget> render_target);
  void post_update();

  void on_enable() override {};
  void on_disable() override {};

private:
  ResourceDescriptorID m_cubemap_id;
  ResourceDescriptorID m_shader_id;
};

} // namespace astralix
