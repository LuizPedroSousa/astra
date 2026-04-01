#include "scene-system.hpp"

#include <gtest/gtest.h>

namespace astralix {
namespace {

TEST(SceneSystemTest, CameraInputRequiresCapturedCursorAndFreeConsole) {
  EXPECT_TRUE(scene_camera_input_enabled(false, true));
  EXPECT_FALSE(scene_camera_input_enabled(true, true));
  EXPECT_FALSE(scene_camera_input_enabled(false, false));
  EXPECT_FALSE(scene_camera_input_enabled(true, false));
}

} // namespace
} // namespace astralix
