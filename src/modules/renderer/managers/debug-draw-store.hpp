#pragma once

#include "base-manager.hpp"

#include "glm/geometric.hpp"
#include "glm/glm.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astralix {

enum class DebugDrawDepthMode : uint8_t {
  DepthTest = 0,
  NoDepth = 1,
  XRay = 2,
};

enum class DebugDrawSizeSpace : uint8_t {
  Pixels = 0,
  World = 1,
};

struct DebugDrawStyle {
  glm::vec4 color = glm::vec4(1.0f);
  float duration_seconds = 0.0f;
  DebugDrawDepthMode depth_mode = DebugDrawDepthMode::DepthTest;
  float thickness = 2.0f;
  DebugDrawSizeSpace size_space = DebugDrawSizeSpace::Pixels;
  std::string category = "default";
  std::optional<glm::mat4> transform;
};

struct DebugDrawPointStyle {
  glm::vec4 color = glm::vec4(1.0f);
  float duration_seconds = 0.0f;
  DebugDrawDepthMode depth_mode = DebugDrawDepthMode::DepthTest;
  float size = 8.0f;
  DebugDrawSizeSpace size_space = DebugDrawSizeSpace::Pixels;
  std::string category = "default";
  std::optional<glm::mat4> transform;
};

struct DebugDrawLineCommand {
  glm::vec3 start = glm::vec3(0.0f);
  glm::vec3 end = glm::vec3(0.0f);
  DebugDrawStyle style{};
  bool arrow = false;
};

struct DebugDrawPointCommand {
  glm::vec3 position = glm::vec3(0.0f);
  DebugDrawPointStyle style{};
};

class DebugDrawStore : public BaseManager<DebugDrawStore> {
public:
  const std::vector<DebugDrawLineCommand> &lines() const { return m_lines; }
  const std::vector<DebugDrawPointCommand> &points() const { return m_points; }

  bool empty() const { return m_lines.empty() && m_points.empty(); }

  void line(
      const glm::vec3 &start,
      const glm::vec3 &end,
      const glm::vec4 &color = glm::vec4(1.0f),
      float duration_seconds = 0.0f
  ) {
    DebugDrawStyle style;
    style.color = color;
    style.duration_seconds = duration_seconds;
    line(start, end, std::move(style));
  }

  void line(
      const glm::vec3 &start,
      const glm::vec3 &end,
      DebugDrawStyle style
  ) {
    if (glm::dot(end - start, end - start) <= k_epsilon) {
      return;
    }

    m_lines.push_back(DebugDrawLineCommand{
        .start = start,
        .end = end,
        .style = sanitize_style(std::move(style)),
        .arrow = false,
    });
    ++m_revision;
  }

  void polyline(
      const std::vector<glm::vec3> &points,
      const glm::vec4 &color,
      float duration_seconds = 0.0f,
      bool closed = false
  ) {
    DebugDrawStyle style;
    style.color = color;
    style.duration_seconds = duration_seconds;
    polyline(points, std::move(style), closed);
  }

  void polyline(
      const std::vector<glm::vec3> &points,
      DebugDrawStyle style,
      bool closed = false
  ) {
    if (points.size() < 2u) {
      return;
    }

    style = sanitize_style(std::move(style));
    for (size_t index = 1u; index < points.size(); ++index) {
      const glm::vec3 &start = points[index - 1u];
      const glm::vec3 &end = points[index];
      if (glm::dot(end - start, end - start) <= k_epsilon) {
        continue;
      }

      m_lines.push_back(DebugDrawLineCommand{
          .start = start,
          .end = end,
          .style = style,
          .arrow = false,
      });
    }

    if (closed) {
      const glm::vec3 &start = points.back();
      const glm::vec3 &end = points.front();
      if (glm::dot(end - start, end - start) > k_epsilon) {
        m_lines.push_back(DebugDrawLineCommand{
            .start = start,
            .end = end,
            .style = style,
            .arrow = false,
        });
      }
    }

    if (!points.empty()) {
      ++m_revision;
    }
  }

  void ray(
      const glm::vec3 &origin,
      const glm::vec3 &direction,
      float length,
      const glm::vec4 &color = glm::vec4(1.0f),
      float duration_seconds = 0.0f
  ) {
    DebugDrawStyle style;
    style.color = color;
    style.duration_seconds = duration_seconds;
    ray(origin, direction, length, std::move(style));
  }

  void ray(
      const glm::vec3 &origin,
      const glm::vec3 &direction,
      float length,
      DebugDrawStyle style
  ) {
    if (length <= 0.0f || glm::dot(direction, direction) <= k_epsilon) {
      return;
    }

    line(
        origin,
        origin + glm::normalize(direction) * length,
        std::move(style)
    );
  }

  void arrow(
      const glm::vec3 &start,
      const glm::vec3 &end,
      const glm::vec4 &color = glm::vec4(1.0f),
      float duration_seconds = 0.0f
  ) {
    DebugDrawStyle style;
    style.color = color;
    style.duration_seconds = duration_seconds;
    arrow(start, end, std::move(style));
  }

  void arrow(
      const glm::vec3 &start,
      const glm::vec3 &end,
      DebugDrawStyle style
  ) {
    if (glm::dot(end - start, end - start) <= k_epsilon) {
      return;
    }

    m_lines.push_back(DebugDrawLineCommand{
        .start = start,
        .end = end,
        .style = sanitize_style(std::move(style)),
        .arrow = true,
    });
    ++m_revision;
  }

  void point(
      const glm::vec3 &position,
      float size = 8.0f,
      const glm::vec4 &color = glm::vec4(1.0f),
      float duration_seconds = 0.0f
  ) {
    DebugDrawPointStyle style;
    style.color = color;
    style.duration_seconds = duration_seconds;
    style.size = size;
    point(position, std::move(style));
  }

  void point(const glm::vec3 &position, DebugDrawPointStyle style) {
    style = sanitize_point_style(std::move(style));
    if (style.size <= 0.0f) {
      return;
    }

    m_points.push_back(DebugDrawPointCommand{
        .position = position,
        .style = std::move(style),
    });
    ++m_revision;
  }

  void basis(
      const glm::mat4 &transform,
      float scale = 1.0f,
      float duration_seconds = 0.0f
  ) {
    DebugDrawStyle style;
    style.duration_seconds = duration_seconds;
    basis(transform, scale, std::move(style));
  }

  void basis(const glm::mat4 &transform, float scale, DebugDrawStyle style) {
    if (scale <= 0.0f) {
      return;
    }

    style = sanitize_style(std::move(style));
    const glm::vec3 origin(transform[3]);
    const glm::vec3 axes[] = {
        normalized_axis(transform[0], glm::vec3(1.0f, 0.0f, 0.0f)),
        normalized_axis(transform[1], glm::vec3(0.0f, 1.0f, 0.0f)),
        normalized_axis(transform[2], glm::vec3(0.0f, 0.0f, 1.0f)),
    };
    const glm::vec4 colors[] = {
        glm::vec4(0.95f, 0.34f, 0.34f, style.color.a),
        glm::vec4(0.41f, 0.86f, 0.46f, style.color.a),
        glm::vec4(0.37f, 0.58f, 0.96f, style.color.a),
    };

    for (size_t index = 0u; index < 3u; ++index) {
      DebugDrawStyle axis_style = style;
      axis_style.color = colors[index];
      arrow(origin, origin + axes[index] * scale, std::move(axis_style));
    }
  }

  void aabb(
      const glm::vec3 &min_corner,
      const glm::vec3 &max_corner,
      const glm::vec4 &color = glm::vec4(1.0f),
      float duration_seconds = 0.0f
  ) {
    DebugDrawStyle style;
    style.color = color;
    style.duration_seconds = duration_seconds;
    aabb(min_corner, max_corner, std::move(style));
  }

  void aabb(
      const glm::vec3 &min_corner,
      const glm::vec3 &max_corner,
      DebugDrawStyle style
  ) {
    style = sanitize_style(std::move(style));
    const glm::vec3 lo = glm::min(min_corner, max_corner);
    const glm::vec3 hi = glm::max(min_corner, max_corner);

    const glm::vec3 corners[] = {
        {lo.x, lo.y, lo.z},
        {hi.x, lo.y, lo.z},
        {hi.x, hi.y, lo.z},
        {lo.x, hi.y, lo.z},
        {lo.x, lo.y, hi.z},
        {hi.x, lo.y, hi.z},
        {hi.x, hi.y, hi.z},
        {lo.x, hi.y, hi.z},
    };
    const std::pair<size_t, size_t> edges[] = {
        {0u, 1u}, {1u, 2u}, {2u, 3u}, {3u, 0u},
        {4u, 5u}, {5u, 6u}, {6u, 7u}, {7u, 4u},
        {0u, 4u}, {1u, 5u}, {2u, 6u}, {3u, 7u},
    };

    for (const auto &[start_index, end_index] : edges) {
      line(corners[start_index], corners[end_index], style);
    }
  }

  void set_category_enabled(std::string category, bool enabled) {
    if (category.empty()) {
      category = "default";
    }

    const auto it = m_category_enabled.find(category);
    const bool current_enabled =
        it == m_category_enabled.end() ? true : it->second;
    if (current_enabled == enabled) {
      return;
    }

    if (enabled) {
      m_category_enabled.erase(category);
    } else {
      m_category_enabled[std::move(category)] = false;
    }
    ++m_revision;
  }

  bool category_enabled(std::string_view category) const {
    if (category.empty()) {
      category = "default";
    }

    const auto it = m_category_enabled.find(std::string(category));
    return it == m_category_enabled.end() || it->second;
  }

  void clear() {
    if (m_lines.empty() && m_points.empty()) {
      return;
    }

    m_lines.clear();
    m_points.clear();
    ++m_revision;
  }

  void advance(double dt) {
    const float clamped_dt = static_cast<float>(std::max(dt, 0.0));
    const size_t line_count = m_lines.size();
    const size_t point_count = m_points.size();
    advance_commands(m_lines, clamped_dt);
    advance_commands(m_points, clamped_dt);

    if (m_lines.size() != line_count || m_points.size() != point_count) {
      ++m_revision;
    }
  }

  uint64_t revision() const { return m_revision; }

private:
  template <typename Command>
  static void advance_commands(std::vector<Command> &commands, float dt) {
    std::erase_if(commands, [dt](Command &command) {
      if (command.style.duration_seconds <= 0.0f) {
        return true;
      }

      command.style.duration_seconds =
          std::max(0.0f, command.style.duration_seconds - dt);
      return command.style.duration_seconds <= 0.0f;
    });
  }

  static DebugDrawStyle sanitize_style(DebugDrawStyle style) {
    style.duration_seconds = std::max(0.0f, style.duration_seconds);
    style.thickness =
        std::isfinite(style.thickness) ? std::max(0.0f, style.thickness) : 0.0f;
    if (style.category.empty()) {
      style.category = "default";
    }
    return style;
  }

  static DebugDrawPointStyle sanitize_point_style(DebugDrawPointStyle style) {
    style.duration_seconds = std::max(0.0f, style.duration_seconds);
    style.size = std::isfinite(style.size) ? std::max(0.0f, style.size) : 0.0f;
    if (style.category.empty()) {
      style.category = "default";
    }
    return style;
  }

  static glm::vec3 normalized_axis(
      const glm::vec4 &column,
      const glm::vec3 &fallback
  ) {
    const glm::vec3 axis(column);
    return glm::dot(axis, axis) > k_epsilon ? glm::normalize(axis) : fallback;
  }

  static constexpr float k_epsilon = 0.0001f;

  std::vector<DebugDrawLineCommand> m_lines;
  std::vector<DebugDrawPointCommand> m_points;
  std::unordered_map<std::string, bool> m_category_enabled;
  uint64_t m_revision = 1u;
};

inline Ref<DebugDrawStore> debug_draw() {
  if (DebugDrawStore::get() == nullptr) {
    DebugDrawStore::init();
  }

  return DebugDrawStore::get();
}

} // namespace astralix
