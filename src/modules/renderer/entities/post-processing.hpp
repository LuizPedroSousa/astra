#pragma once

#include "entities/entity.hpp"
#include "guid.hpp"
#include "targets/render-target.hpp"

namespace astralix {

class PostProcessing : public Entity<PostProcessing> {
public:
  PostProcessing(ENTITY_INIT_PARAMS, ResourceDescriptorID shader_id);
  ~PostProcessing() = default;

  void start(Ref<RenderTarget> render_target);
  void post_update(Ref<RenderTarget> render_target);

  void on_enable() override {};
  void on_disable() override {};

  ResourceDescriptorID shader_descriptor_id;
};
} // namespace astralix
