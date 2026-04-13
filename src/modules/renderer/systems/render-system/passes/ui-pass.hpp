#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "render-pass.hpp"
#include "resources/shader.hpp"
#include "systems/render-system/render-frame.hpp"

namespace astralix {

namespace ui_pass_detail {

struct ScissorRect {
  bool enabled = false;
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

inline ScissorRect resolve_scissor_rect(
    const ui::UIRect &clip,
    float ui_width,
    float ui_height,
    const ImageExtent &target_extent
) {
  ScissorRect scissor{.enabled = true};
  if (clip.width <= 0.0f || clip.height <= 0.0f || ui_width <= 0.0f ||
      ui_height <= 0.0f || target_extent.width == 0 ||
      target_extent.height == 0) {
    return scissor;
  }

  const ui::UIRect clamped = ui::intersect_rect(
      clip,
      ui::UIRect{
          .x = 0.0f,
          .y = 0.0f,
          .width = ui_width,
          .height = ui_height,
      }
  );
  if (clamped.width <= 0.0f || clamped.height <= 0.0f) {
    return scissor;
  }

  const float scale_x = static_cast<float>(target_extent.width) / ui_width;
  const float scale_y = static_cast<float>(target_extent.height) / ui_height;
  const float max_x = static_cast<float>(target_extent.width);
  const float max_y = static_cast<float>(target_extent.height);

  const uint32_t left = static_cast<uint32_t>(std::clamp(
      std::floor(clamped.x * scale_x), 0.0f, max_x
  ));
  const uint32_t right = static_cast<uint32_t>(std::clamp(
      std::ceil((clamped.x + clamped.width) * scale_x), 0.0f, max_x
  ));
  const uint32_t top = static_cast<uint32_t>(std::clamp(
      std::floor(clamped.y * scale_y), 0.0f, max_y
  ));
  const uint32_t bottom = static_cast<uint32_t>(std::clamp(
      std::ceil((clamped.y + clamped.height) * scale_y), 0.0f, max_y
  ));

  if (right <= left || bottom <= top) {
    return scissor;
  }

  scissor.x = left;
  scissor.y = top;
  scissor.width = right - left;
  scissor.height = bottom - top;
  return scissor;
}

} // namespace ui_pass_detail

class UIPass : public FramePass {
public:
  explicit UIPass(rendering::ResolvedMeshDraw quad = {});
  ~UIPass() override = default;

  void setup(PassSetupContext &ctx) override;
  void record(PassRecordContext &ctx, PassRecorder &recorder) override;

  std::string name() const override { return "UIPass"; }
  bool has_side_effects() const override { return true; }

private:
  struct Shaders {
    Ref<Shader> solid;
    Ref<Shader> image;
    Ref<Shader> text;
    Ref<Shader> polyline;
  };

  Shaders m_shaders{};
  rendering::ResolvedMeshDraw m_quad{};
};

} // namespace astralix
