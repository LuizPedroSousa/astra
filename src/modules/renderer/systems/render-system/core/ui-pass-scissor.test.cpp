#include "systems/render-system/passes/ui-pass.hpp"

#include <gtest/gtest.h>

namespace astralix {

TEST(UIPassScissorTest, ScalesUiClipRectIntoRenderTargetPixels) {
  const auto scissor = ui_pass_detail::resolve_scissor_rect(
      ui::UIRect{
          .x = 100.0f,
          .y = 50.0f,
          .width = 200.0f,
          .height = 100.0f,
      },
      800.0f,
      600.0f,
      ImageExtent{
          .width = 1600,
          .height = 1200,
          .depth = 1,
      }
  );

  EXPECT_TRUE(scissor.enabled);
  EXPECT_EQ(scissor.x, 200u);
  EXPECT_EQ(scissor.y, 100u);
  EXPECT_EQ(scissor.width, 400u);
  EXPECT_EQ(scissor.height, 200u);
}

TEST(UIPassScissorTest, ProducesZeroAreaScissorForFullyClippedRects) {
  const auto scissor = ui_pass_detail::resolve_scissor_rect(
      ui::UIRect{
          .x = 900.0f,
          .y = 50.0f,
          .width = 120.0f,
          .height = 100.0f,
      },
      800.0f,
      600.0f,
      ImageExtent{
          .width = 1600,
          .height = 1200,
          .depth = 1,
      }
  );

  EXPECT_TRUE(scissor.enabled);
  EXPECT_EQ(scissor.x, 0u);
  EXPECT_EQ(scissor.y, 0u);
  EXPECT_EQ(scissor.width, 0u);
  EXPECT_EQ(scissor.height, 0u);
}

TEST(UIPassScissorTest, RenderImageViewFlipDependsOnBackend) {
  EXPECT_FLOAT_EQ(
      ui_pass_detail::render_image_sample_flip_y(RendererBackend::OpenGL),
      1.0f
  );
  EXPECT_FLOAT_EQ(
      ui_pass_detail::render_image_sample_flip_y(RendererBackend::Vulkan),
      0.0f
  );
  EXPECT_FLOAT_EQ(
      ui_pass_detail::render_image_sample_flip_y(RendererBackend::None),
      0.0f
  );
}

} // namespace astralix
