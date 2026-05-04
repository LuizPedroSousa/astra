#include "vector/path-builder.hpp"
#include "vector/path-tessellator.hpp"

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

namespace astralix::ui {
namespace {

struct Bounds {
  glm::vec2 min = glm::vec2(std::numeric_limits<float>::infinity());
  glm::vec2 max = glm::vec2(-std::numeric_limits<float>::infinity());
};

Bounds compute_bounds(const std::vector<UIPolylineVertex> &vertices) {
  Bounds bounds;
  for (const auto &vertex : vertices) {
    bounds.min.x = std::min(bounds.min.x, vertex.position.x);
    bounds.min.y = std::min(bounds.min.y, vertex.position.y);
    bounds.max.x = std::max(bounds.max.x, vertex.position.x);
    bounds.max.y = std::max(bounds.max.y, vertex.position.y);
  }
  return bounds;
}

void expect_finite_vertices(const std::vector<UIPolylineVertex> &vertices) {
  for (const auto &vertex : vertices) {
    EXPECT_TRUE(std::isfinite(vertex.position.x));
    EXPECT_TRUE(std::isfinite(vertex.position.y));
    EXPECT_TRUE(std::isfinite(vertex.color.r));
    EXPECT_TRUE(std::isfinite(vertex.color.g));
    EXPECT_TRUE(std::isfinite(vertex.color.b));
    EXPECT_TRUE(std::isfinite(vertex.color.a));
  }
}

bool same_color(const glm::vec4 &lhs, const glm::vec4 &rhs) {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

TEST(UIPathBuilderTest, BuildsClosedCircleCommands) {
  UIPathStyle style;
  style.fill = true;
  style.stroke = true;
  style.stroke_width = 4.0f;

  const auto command =
      UIPathBuilder(style).append_circle(glm::vec2(24.0f, 18.0f), 10.0f).build();

  ASSERT_EQ(command.elements.size(), 6u);
  EXPECT_EQ(command.elements.front().verb, UIPathVerb::MoveTo);
  EXPECT_EQ(command.elements.back().verb, UIPathVerb::Close);
  EXPECT_EQ(command.elements.front().p0, glm::vec2(34.0f, 18.0f));
  EXPECT_TRUE(command.style.fill);
  EXPECT_TRUE(command.style.stroke);
}

TEST(UIPathTessellatorTest, TessellatesCircleDeterministically) {
  UIPathStyle style;
  style.fill = true;
  style.fill_color = glm::vec4(0.2f, 0.4f, 0.8f, 0.6f);
  style.stroke = true;
  style.stroke_color = glm::vec4(0.9f, 0.7f, 0.2f, 1.0f);
  style.stroke_width = 3.0f;

  const auto command =
      UIPathBuilder(style).append_circle(glm::vec2(40.0f, 32.0f), 12.0f).build();

  const auto first = tessellate_path(command);
  const auto second = tessellate_path(command);

  ASSERT_FALSE(first.empty());
  ASSERT_EQ(
      first.triangle_vertices.size(), second.triangle_vertices.size()
  );
  ASSERT_GT(first.triangle_vertices.size(), 30u);
  expect_finite_vertices(first.triangle_vertices);

  bool saw_fill = false;
  bool saw_stroke = false;
  for (size_t index = 0u; index < first.triangle_vertices.size(); ++index) {
    const auto &lhs = first.triangle_vertices[index];
    const auto &rhs = second.triangle_vertices[index];
    EXPECT_NEAR(lhs.position.x, rhs.position.x, 1.0e-4f);
    EXPECT_NEAR(lhs.position.y, rhs.position.y, 1.0e-4f);
    EXPECT_FLOAT_EQ(lhs.color.r, rhs.color.r);
    EXPECT_FLOAT_EQ(lhs.color.g, rhs.color.g);
    EXPECT_FLOAT_EQ(lhs.color.b, rhs.color.b);
    EXPECT_FLOAT_EQ(lhs.color.a, rhs.color.a);

    saw_fill = saw_fill || same_color(lhs.color, style.fill_color);
    saw_stroke = saw_stroke || same_color(lhs.color, style.stroke_color);
  }

  EXPECT_TRUE(saw_fill);
  EXPECT_TRUE(saw_stroke);
}

TEST(UIPathTessellatorTest, SquareCapsExtendStrokeBounds) {
  UIPathStyle style;
  style.fill = false;
  style.stroke = true;
  style.stroke_color = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f);
  style.stroke_width = 10.0f;
  style.line_cap = UIStrokeCap::Square;
  style.line_join = UIStrokeJoin::Miter;

  const auto command = UIPathBuilder(style)
                           .move_to(glm::vec2(10.0f, 20.0f))
                           .line_to(glm::vec2(50.0f, 20.0f))
                           .build();

  const auto tessellated = tessellate_path(command);
  ASSERT_FALSE(tessellated.empty());
  expect_finite_vertices(tessellated.triangle_vertices);

  const Bounds bounds = compute_bounds(tessellated.triangle_vertices);
  EXPECT_NEAR(bounds.min.x, 5.0f, 1.0e-3f);
  EXPECT_NEAR(bounds.max.x, 55.0f, 1.0e-3f);
  EXPECT_NEAR(bounds.min.y, 15.0f, 1.0e-3f);
  EXPECT_NEAR(bounds.max.y, 25.0f, 1.0e-3f);
}

TEST(UIPathTessellatorTest, RoundJoinAddsMoreGeometryThanBevelJoin) {
  UIPathStyle round_style;
  round_style.fill = false;
  round_style.stroke = true;
  round_style.stroke_color = glm::vec4(1.0f);
  round_style.stroke_width = 8.0f;
  round_style.line_join = UIStrokeJoin::Round;

  UIPathStyle bevel_style = round_style;
  bevel_style.line_join = UIStrokeJoin::Bevel;

  const auto round_command = UIPathBuilder(round_style)
                                 .move_to(glm::vec2(10.0f, 10.0f))
                                 .line_to(glm::vec2(40.0f, 10.0f))
                                 .line_to(glm::vec2(40.0f, 40.0f))
                                 .build();
  const auto bevel_command = UIPathBuilder(bevel_style)
                                 .move_to(glm::vec2(10.0f, 10.0f))
                                 .line_to(glm::vec2(40.0f, 10.0f))
                                 .line_to(glm::vec2(40.0f, 40.0f))
                                 .build();

  const auto round_tessellation = tessellate_path(round_command);
  const auto bevel_tessellation = tessellate_path(bevel_command);

  ASSERT_FALSE(round_tessellation.empty());
  ASSERT_FALSE(bevel_tessellation.empty());
  EXPECT_GT(
      round_tessellation.triangle_vertices.size(),
      bevel_tessellation.triangle_vertices.size()
  );
}

} // namespace
} // namespace astralix::ui
