#pragma once

#include "assert.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/core/compiled-frame.hpp"
#include <cstring>
#include <vector>

namespace astralix::rendering {

inline size_t shader_value_kind_size(ShaderValueKind kind) {
  switch (kind) {
    case ShaderValueKind::Bool:
      return sizeof(bool);
    case ShaderValueKind::Int:
      return sizeof(int);
    case ShaderValueKind::Float:
      return sizeof(float);
    case ShaderValueKind::Vec2:
      return sizeof(glm::vec2);
    case ShaderValueKind::Vec3:
      return sizeof(glm::vec3);
    case ShaderValueKind::Vec4:
      return sizeof(glm::vec4);
    case ShaderValueKind::Mat3:
      return sizeof(glm::mat3);
    case ShaderValueKind::Mat4:
      return sizeof(glm::mat4);
    default:
      ASTRA_EXCEPTION("Unsupported shader value kind");
  }
}

class ShaderParamRecorder final : public Shader {
public:
  struct RecordedUpload {
    uint64_t binding_id = 0;
    ShaderValueKind kind = ShaderValueKind::Float;
    std::vector<uint8_t> bytes;
  };

  ShaderParamRecorder()
      : Shader(ResourceHandle{0, 0}, "recorded-shader-params") {}

  void bind() const override {}
  void unbind() const override {}
  void attach() const override {}
  uint32_t renderer_id() const override { return 0; }

  const std::vector<RecordedUpload> &uploads() const noexcept {
    return m_uploads;
  }

protected:
  void set_typed_value(uint64_t binding_id, ShaderValueKind kind, const void *value) const override {
    const auto value_size = shader_value_kind_size(kind);
    RecordedUpload upload{
        .binding_id = binding_id,
        .kind = kind,
        .bytes = std::vector<uint8_t>(value_size),
    };
    std::memcpy(upload.bytes.data(), value, value_size);
    m_uploads.push_back(std::move(upload));
  }

private:
  mutable std::vector<RecordedUpload> m_uploads;
};

template <typename Params>
inline void record_shader_params(CompiledFrame &frame, RenderBindingGroupHandle binding_group, const Params &params) {
  ShaderParamRecorder recorder;
  recorder.set_all(params);

  for (const auto &upload : recorder.uploads()) {
    frame.add_value_binding_bytes(binding_group, upload.binding_id, upload.kind,
                                  upload.bytes);
  }
}

} // namespace astralix::rendering
