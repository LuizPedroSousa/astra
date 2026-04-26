#pragma once

#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"

#include <string>

namespace astralix {

class RenderBindingGroupBuilder {
public:
  RenderBindingGroupBuilder(CompiledFrame &frame, Ref<Shader> shader, RenderBindingGroupHandle handle)
      : m_frame(frame), m_shader(std::move(shader)), m_handle(handle) {}

  template <typename FieldTag, typename Value>
    requires std::same_as<std::remove_cvref_t<Value>, typename FieldTag::value_type>
  void set_value(FieldTag, Value &&value) {
    constexpr auto kind = shader_detail::value_kind<
        typename FieldTag::value_type>::kind;

    m_frame.add_value_binding(m_handle, FieldTag::binding_id, kind,
                              std::forward<Value>(value));
  }

  template <typename ResourceTag>
  void set_sampled_image(ResourceTag, ImageViewRef image_view, CompiledSampledImageTarget target = CompiledSampledImageTarget::Texture2D) {
    m_frame.add_sampled_image_binding(m_handle, ResourceTag::binding_id,
                                      image_view, target);
  }

  RenderBindingGroupHandle finish() { return m_handle; }

private:
  CompiledFrame &m_frame;
  Ref<Shader> m_shader;
  RenderBindingGroupHandle m_handle;
};

} // namespace astralix
