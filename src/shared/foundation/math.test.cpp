#include "math.hpp"

#include <gtest/gtest.h>

namespace astralix {
namespace {

TEST(MathTest, ClampBoundsValuesToTheProvidedRange) {
  EXPECT_FLOAT_EQ(clamp(2.0f, 0.0f, 1.0f), 1.0f);
  EXPECT_FLOAT_EQ(clamp(-1.0f, 0.0f, 1.0f), 0.0f);
  EXPECT_FLOAT_EQ(clamp(0.5f, 0.0f, 1.0f), 0.5f);
}

TEST(MathTest, SaturateBoundsValuesToUnitRange) {
  EXPECT_FLOAT_EQ(saturate(1.5f), 1.0f);
  EXPECT_FLOAT_EQ(saturate(-0.25f), 0.0f);
  EXPECT_FLOAT_EQ(saturate(0.25f), 0.25f);
}

TEST(MathTest, LerpUsesASaturatedInterpolationFactor) {
  EXPECT_FLOAT_EQ(lerp(12.0f, 16.0f, -1.0f), 12.0f);
  EXPECT_FLOAT_EQ(lerp(12.0f, 16.0f, 0.5f), 14.0f);
  EXPECT_FLOAT_EQ(lerp(12.0f, 16.0f, 2.0f), 16.0f);
}

} // namespace
} // namespace astralix
