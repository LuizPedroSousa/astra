#include "entities/entity.hpp"
#include "guid.hpp"
#include <imgui.h>

namespace astralix {
class Skybox : public Entity<Skybox> {
public:
  Skybox(ENTITY_INIT_PARAMS, const ResourceDescriptorID cubemap_id,
         const ResourceDescriptorID shader_id);
  Skybox() {};

  void on_enable() override {};
  void on_disable() override {};

  ResourceDescriptorID cubemap_id;
  ResourceDescriptorID shader_id;
};

} // namespace astralix
