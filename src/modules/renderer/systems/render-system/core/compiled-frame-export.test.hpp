#include "systems/render-system/core/compiled-frame.hpp"

#include <gtest/gtest.h>

namespace astralix {
namespace {

TEST(CompiledFrameExportTest, FindExportPrefersLatestMatchingKey) {
  CompiledFrame frame;

  const auto key =
      make_render_image_export_key(RenderImageResource::SceneColor);
  const ImageHandle first_image{1};
  const ImageHandle second_image{2};

  frame.export_entries.push_back(CompiledExportEntry{
      .key = key,
      .image = first_image,
      .extent = ImageExtent{.width = 640, .height = 360, .depth = 1},
  });
  frame.export_entries.push_back(CompiledExportEntry{
      .key = key,
      .image = second_image,
      .extent = ImageExtent{.width = 1280, .height = 720, .depth = 1},
  });

  const auto *export_entry = frame.find_export(key);
  ASSERT_NE(export_entry, nullptr);
  EXPECT_EQ(export_entry->image, second_image);
  EXPECT_EQ(export_entry->extent.width, 1280u);
  EXPECT_EQ(export_entry->extent.height, 720u);
}

} // namespace
} // namespace astralix
