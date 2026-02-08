
#include "skybox.hpp"
#include "base.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "entities/ientity.hpp"

#include "glad/glad.h"
#include "guid.hpp"

namespace astralix {

Skybox::Skybox(ENTITY_INIT_PARAMS, const ResourceDescriptorID cubemap_id,
               const ResourceDescriptorID shader_id)
    : ENTITY_INIT(), cubemap_id(cubemap_id), shader_id(shader_id) {
  add_component<ResourceComponent>();
  add_component<MeshComponent>();
}
} // namespace astralix
