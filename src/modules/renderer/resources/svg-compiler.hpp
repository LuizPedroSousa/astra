#pragma once

#include "adapters/xml/xml-serialization-context.hpp"
#include "assert.hpp"
#include "glm/glm.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace astralix {

struct SvgColorVertex {
  glm::vec2 position = glm::vec2(0.0f);
  glm::vec4 color = glm::vec4(1.0f);
};

struct SvgTriangleBatch {
  std::vector<SvgColorVertex> vertices;
};

struct SvgDocumentData {
  float width = 0.0f;
  float height = 0.0f;
  std::vector<SvgTriangleBatch> batches;
};

namespace svg_detail {

using XmlAttribute = xml_detail::Attribute;
using XmlNode = xml_detail::Node;

inline const XmlAttribute *find_xml_attribute(
    const XmlNode &node,
    std::string_view name
) {
  return xml_detail::find_attribute(node, name);
}

inline bool has_xml_attribute(const XmlNode &node, std::string_view name) {
  return find_xml_attribute(node, name) != nullptr;
}

inline std::string_view xml_attribute_value(
    const XmlNode &node,
    std::string_view name
) {
  const auto *attribute = find_xml_attribute(node, name);
  return attribute == nullptr ? std::string_view{} : std::string_view(attribute->value);
}

constexpr float k_svg_epsilon = 1.0e-4f;
constexpr float k_curve_flatness = 0.25f;

enum class FillRule : uint8_t {
  NonZero,
  EvenOdd,
};

enum class StrokeLineCap : uint8_t {
  Butt,
  Round,
  Square,
};

enum class StrokeLineJoin : uint8_t {
  Miter,
  Round,
  Bevel,
};

struct SvgContour {
  std::vector<glm::vec2> points;
  bool closed = false;
};

struct SvgShape {
  std::vector<SvgContour> contours;
};

struct SvgStyleState {
  bool visible = true;
  bool fill_enabled = true;
  glm::vec4 fill = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  bool stroke_enabled = false;
  glm::vec4 stroke = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
  float stroke_width = 1.0f;
  float opacity = 1.0f;
  float fill_opacity = 1.0f;
  float stroke_opacity = 1.0f;
  float miter_limit = 4.0f;
  FillRule fill_rule = FillRule::NonZero;
  StrokeLineCap line_cap = StrokeLineCap::Butt;
  StrokeLineJoin line_join = StrokeLineJoin::Miter;
  glm::mat3 transform = glm::mat3(1.0f);
};

struct SvgDocumentMetrics {
  float width = 0.0f;
  float height = 0.0f;
  glm::mat3 root_transform = glm::mat3(1.0f);
};

inline std::string trim_copy(std::string_view value) {
  size_t start = 0u;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1u]))) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

inline std::string lower_copy(std::string_view value) {
  std::string lowered(value);
  std::transform(
      lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
      }
  );
  return lowered;
}

inline bool nearly_equal(float lhs, float rhs, float epsilon = k_svg_epsilon) {
  return std::abs(lhs - rhs) <= epsilon;
}

inline bool nearly_equal(
    const glm::vec2 &lhs,
    const glm::vec2 &rhs,
    float epsilon = k_svg_epsilon
) {
  return glm::length(lhs - rhs) <= epsilon;
}

inline glm::mat3 make_translation(float x, float y) {
  glm::mat3 matrix(1.0f);
  matrix[2][0] = x;
  matrix[2][1] = y;
  return matrix;
}

inline glm::mat3 make_scale(float x, float y) {
  glm::mat3 matrix(1.0f);
  matrix[0][0] = x;
  matrix[1][1] = y;
  return matrix;
}

inline glm::mat3 make_rotation(float radians) {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);

  glm::mat3 matrix(1.0f);
  matrix[0][0] = cosine;
  matrix[0][1] = sine;
  matrix[1][0] = -sine;
  matrix[1][1] = cosine;
  return matrix;
}

inline glm::mat3 make_skew_x(float radians) {
  glm::mat3 matrix(1.0f);
  matrix[1][0] = std::tan(radians);
  return matrix;
}

inline glm::mat3 make_skew_y(float radians) {
  glm::mat3 matrix(1.0f);
  matrix[0][1] = std::tan(radians);
  return matrix;
}

inline glm::vec2 transform_point(const glm::mat3 &matrix, const glm::vec2 &point) {
  const glm::vec3 transformed = matrix * glm::vec3(point, 1.0f);
  return glm::vec2(transformed.x, transformed.y);
}

inline float cross(const glm::vec2 &lhs, const glm::vec2 &rhs) {
  return lhs.x * rhs.y - lhs.y * rhs.x;
}

inline float signed_area(const std::vector<glm::vec2> &polygon) {
  if (polygon.size() < 3u) {
    return 0.0f;
  }

  float area = 0.0f;
  for (size_t index = 0u; index < polygon.size(); ++index) {
    const glm::vec2 &current = polygon[index];
    const glm::vec2 &next = polygon[(index + 1u) % polygon.size()];
    area += (current.x * next.y) - (next.x * current.y);
  }

  return area * 0.5f;
}

inline bool is_clockwise(const std::vector<glm::vec2> &polygon) {
  return signed_area(polygon) < 0.0f;
}

inline bool point_in_polygon(const glm::vec2 &point, const std::vector<glm::vec2> &polygon) {
  bool inside = false;

  for (size_t index = 0u, previous = polygon.size() - 1u; index < polygon.size();
       previous = index++) {
    const glm::vec2 &vertex = polygon[index];
    const glm::vec2 &prev = polygon[previous];

    const bool intersects =
        ((vertex.y > point.y) != (prev.y > point.y)) &&
        (point.x <
         (prev.x - vertex.x) * (point.y - vertex.y) / (prev.y - vertex.y + k_svg_epsilon) +
             vertex.x);
    if (intersects) {
      inside = !inside;
    }
  }

  return inside;
}

inline std::optional<float> parse_number_token(std::string_view token) {
  const std::string trimmed = trim_copy(token);
  if (trimmed.empty()) {
    return std::nullopt;
  }

  char *end = nullptr;
  const float value = std::strtof(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || *end != '\0') {
    return std::nullopt;
  }

  return value;
}

inline float parse_required_number(std::string_view token, std::string_view label) {
  const auto parsed = parse_number_token(token);
  ASTRA_ENSURE(!parsed.has_value(), "Invalid SVG numeric value for ", label, ": ", token);
  return *parsed;
}

inline float parse_svg_length(std::string_view token, std::string_view label) {
  const std::string trimmed = trim_copy(token);
  ASTRA_ENSURE(trimmed.empty(), "Missing SVG length for ", label);

  if (trimmed.ends_with('%')) {
    ASTRA_EXCEPTION("Unsupported SVG percentage length for ", label, ": ", trimmed);
  }

  if (trimmed.ends_with("px")) {
    return parse_required_number(trimmed.substr(0u, trimmed.size() - 2u), label);
  }

  for (const std::string_view suffix :
       {"em", "rem", "pt", "pc", "cm", "mm", "in", "vh", "vw"}) {
    if (trimmed.ends_with(suffix)) {
      ASTRA_EXCEPTION("Unsupported SVG length unit for ", label, ": ", trimmed);
    }
  }

  return parse_required_number(trimmed, label);
}

inline std::vector<float> parse_number_list(std::string_view input) {
  std::vector<float> values;
  std::string buffer(input);
  const char *cursor = buffer.c_str();
  char *end = nullptr;

  while (*cursor != '\0') {
    while (*cursor != '\0' &&
           (std::isspace(static_cast<unsigned char>(*cursor)) || *cursor == ',')) {
      ++cursor;
    }

    if (*cursor == '\0') {
      break;
    }

    const float value = std::strtof(cursor, &end);
    ASTRA_ENSURE(end == cursor, "Invalid SVG numeric list value: ", input);
    values.push_back(value);
    cursor = end;
  }

  return values;
}

inline std::vector<glm::vec2> parse_points(std::string_view input) {
  const std::vector<float> values = parse_number_list(input);
  ASTRA_ENSURE(values.size() % 2u != 0u, "Invalid SVG points list: ", input);

  std::vector<glm::vec2> points;
  points.reserve(values.size() / 2u);

  for (size_t index = 0u; index < values.size(); index += 2u) {
    points.emplace_back(values[index], values[index + 1u]);
  }

  return points;
}

inline glm::vec4 parse_hex_color(std::string_view input) {
  auto hex_to_float = [](std::string_view value) {
    const int component = std::stoi(std::string(value), nullptr, 16);
    return static_cast<float>(component) / 255.0f;
  };

  if (input.size() == 4u) {
    const float r = hex_to_float(std::string(2u, input[1u]).substr(0u, 2u));
    const float g = hex_to_float(std::string(2u, input[2u]).substr(0u, 2u));
    const float b = hex_to_float(std::string(2u, input[3u]).substr(0u, 2u));
    return glm::vec4(r, g, b, 1.0f);
  }

  if (input.size() == 7u) {
    return glm::vec4(
        hex_to_float(input.substr(1u, 2u)),
        hex_to_float(input.substr(3u, 2u)),
        hex_to_float(input.substr(5u, 2u)),
        1.0f
    );
  }

  if (input.size() == 9u) {
    return glm::vec4(
        hex_to_float(input.substr(1u, 2u)),
        hex_to_float(input.substr(3u, 2u)),
        hex_to_float(input.substr(5u, 2u)),
        hex_to_float(input.substr(7u, 2u))
    );
  }

  ASTRA_EXCEPTION("Unsupported SVG hex color: ", input);
}

inline glm::vec4 parse_color(std::string_view value) {
  static const std::unordered_map<std::string, glm::vec4> named_colors = {
      {"black", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)},
      {"white", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)},
      {"red", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)},
      {"green", glm::vec4(0.0f, 0.5f, 0.0f, 1.0f)},
      {"blue", glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)},
      {"gray", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)},
      {"grey", glm::vec4(0.5f, 0.5f, 0.5f, 1.0f)},
      {"yellow", glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)},
      {"cyan", glm::vec4(0.0f, 1.0f, 1.0f, 1.0f)},
      {"magenta", glm::vec4(1.0f, 0.0f, 1.0f, 1.0f)},
      {"transparent", glm::vec4(0.0f, 0.0f, 0.0f, 0.0f)},
  };

  const std::string normalized = lower_copy(trim_copy(value));
  ASTRA_ENSURE(normalized.empty(), "Missing SVG color value");
  ASTRA_ENSURE(normalized == "none", "SVG color parser cannot parse 'none'");

  if (normalized.starts_with('#')) {
    return parse_hex_color(normalized);
  }

  if (normalized.starts_with("rgb(") && normalized.ends_with(')')) {
    const auto components =
        parse_number_list(std::string_view(normalized).substr(4u, normalized.size() - 5u));
    ASTRA_ENSURE(components.size() != 3u, "Invalid SVG rgb() color: ", normalized);
    return glm::vec4(
        components[0u] / 255.0f,
        components[1u] / 255.0f,
        components[2u] / 255.0f,
        1.0f
    );
  }

  auto it = named_colors.find(normalized);
  ASTRA_ENSURE(it == named_colors.end(), "Unsupported SVG color value: ", normalized);
  return it->second;
}

inline void apply_style_property(
    SvgStyleState &state,
    const std::string &name,
    const std::string &value
) {
  const std::string property = lower_copy(name);
  const std::string parsed_value = trim_copy(value);

  if (property == "display") {
    if (lower_copy(parsed_value) == "none") {
      state.visible = false;
    }
    return;
  }

  if (property == "visibility") {
    const std::string lowered = lower_copy(parsed_value);
    if (lowered == "hidden" || lowered == "collapse") {
      state.visible = false;
    }
    return;
  }

  if (property == "fill") {
    if (lower_copy(parsed_value) == "none") {
      state.fill_enabled = false;
    } else {
      state.fill_enabled = true;
      state.fill = parse_color(parsed_value);
    }
    return;
  }

  if (property == "stroke") {
    if (lower_copy(parsed_value) == "none") {
      state.stroke_enabled = false;
    } else {
      state.stroke_enabled = true;
      state.stroke = parse_color(parsed_value);
    }
    return;
  }

  if (property == "stroke-width") {
    state.stroke_width = parse_svg_length(parsed_value, "stroke-width");
    return;
  }

  if (property == "opacity") {
    state.opacity = parse_required_number(parsed_value, "opacity");
    return;
  }

  if (property == "fill-opacity") {
    state.fill_opacity = parse_required_number(parsed_value, "fill-opacity");
    return;
  }

  if (property == "stroke-opacity") {
    state.stroke_opacity = parse_required_number(parsed_value, "stroke-opacity");
    return;
  }

  if (property == "fill-rule") {
    const std::string lowered = lower_copy(parsed_value);
    ASTRA_ENSURE(lowered != "evenodd" && lowered != "nonzero", "Unsupported SVG fill-rule: ", parsed_value);
    state.fill_rule =
        lowered == "evenodd" ? FillRule::EvenOdd : FillRule::NonZero;
    return;
  }

  if (property == "stroke-linecap") {
    const std::string lowered = lower_copy(parsed_value);
    if (lowered == "round") {
      state.line_cap = StrokeLineCap::Round;
      return;
    }

    if (lowered == "square") {
      state.line_cap = StrokeLineCap::Square;
      return;
    }

    ASTRA_ENSURE(lowered != "butt", "Unsupported SVG stroke-linecap: ", parsed_value);
    state.line_cap = StrokeLineCap::Butt;
    return;
  }

  if (property == "stroke-linejoin") {
    const std::string lowered = lower_copy(parsed_value);
    if (lowered == "round") {
      state.line_join = StrokeLineJoin::Round;
      return;
    }

    if (lowered == "bevel") {
      state.line_join = StrokeLineJoin::Bevel;
      return;
    }

    ASTRA_ENSURE(lowered != "miter", "Unsupported SVG stroke-linejoin: ", parsed_value);
    state.line_join = StrokeLineJoin::Miter;
    return;
  }

  if (property == "stroke-miterlimit") {
    state.miter_limit = parse_required_number(parsed_value, "stroke-miterlimit");
    return;
  }
}

inline void apply_style_attribute(SvgStyleState &state, std::string_view style_value) {
  std::stringstream stream(trim_copy(style_value));
  std::string pair;
  while (std::getline(stream, pair, ';')) {
    const size_t separator = pair.find(':');
    if (separator == std::string::npos) {
      continue;
    }

    apply_style_property(
        state,
        trim_copy(std::string_view(pair).substr(0u, separator)),
        trim_copy(std::string_view(pair).substr(separator + 1u))
    );
  }
}

inline glm::mat3 parse_transform(std::string_view input) {
  std::string text = trim_copy(input);
  glm::mat3 transform(1.0f);
  size_t index = 0u;

  auto skip_whitespace = [&]() {
    while (index < text.size() &&
           (std::isspace(static_cast<unsigned char>(text[index])) ||
            text[index] == ',')) {
      ++index;
    }
  };

  while (index < text.size()) {
    skip_whitespace();
    if (index >= text.size()) {
      break;
    }

    size_t name_end = index;
    while (name_end < text.size() &&
           std::isalpha(static_cast<unsigned char>(text[name_end]))) {
      ++name_end;
    }

    ASTRA_ENSURE(name_end == index, "Invalid SVG transform list: ", input);
    const std::string name = lower_copy(std::string_view(text).substr(index, name_end - index));
    index = name_end;
    skip_whitespace();

    ASTRA_ENSURE(index >= text.size() || text[index] != '(', "Invalid SVG transform syntax: ", input);
    ++index;

    const size_t value_start = index;
    size_t depth = 1u;
    while (index < text.size() && depth > 0u) {
      if (text[index] == '(') {
        ++depth;
      } else if (text[index] == ')') {
        --depth;
      }
      ++index;
    }

    ASTRA_ENSURE(depth != 0u, "Unterminated SVG transform: ", input);
    const auto values = parse_number_list(
        std::string_view(text).substr(value_start, index - value_start - 1u)
    );

    glm::mat3 local(1.0f);
    if (name == "matrix") {
      ASTRA_ENSURE(values.size() != 6u, "Invalid SVG matrix transform: ", input);
      local = glm::mat3(
          values[0u], values[1u], 0.0f, values[2u], values[3u], 0.0f, values[4u], values[5u], 1.0f
      );
    } else if (name == "translate") {
      ASTRA_ENSURE(values.empty() || values.size() > 2u, "Invalid SVG translate transform: ", input);
      local = make_translation(values[0u], values.size() > 1u ? values[1u] : 0.0f);
    } else if (name == "scale") {
      ASTRA_ENSURE(values.empty() || values.size() > 2u, "Invalid SVG scale transform: ", input);
      local = make_scale(values[0u], values.size() > 1u ? values[1u] : values[0u]);
    } else if (name == "rotate") {
      ASTRA_ENSURE(values.size() != 1u && values.size() != 3u, "Invalid SVG rotate transform: ", input);
      const float radians = glm::radians(values[0u]);
      local = make_rotation(radians);
      if (values.size() == 3u) {
        local =
            make_translation(values[1u], values[2u]) * local *
            make_translation(-values[1u], -values[2u]);
      }
    } else if (name == "skewx") {
      ASTRA_ENSURE(values.size() != 1u, "Invalid SVG skewX transform: ", input);
      local = make_skew_x(glm::radians(values[0u]));
    } else if (name == "skewy") {
      ASTRA_ENSURE(values.size() != 1u, "Invalid SVG skewY transform: ", input);
      local = make_skew_y(glm::radians(values[0u]));
    } else {
      ASTRA_EXCEPTION("Unsupported SVG transform function: ", name);
    }

    transform = local * transform;
  }

  return transform;
}

inline void apply_presentation_attributes(
    const XmlNode &node,
    SvgStyleState &state
) {
  for (const XmlAttribute &attribute : node.attributes) {
    const std::string &name = attribute.name;
    const std::string &value = attribute.value;

    if (name == "style") {
      apply_style_attribute(state, value);
      continue;
    }

    if (name == "transform" || name == "d" || name == "x" || name == "y" ||
        name == "x1" || name == "y1" || name == "x2" || name == "y2" ||
        name == "cx" || name == "cy" || name == "r" || name == "rx" ||
        name == "ry" || name == "width" || name == "height" ||
        name == "points" || name == "viewBox" || name == "xmlns" ||
        name == "version" || name == "id") {
      continue;
    }

    apply_style_property(state, name, value);
  }
}

inline std::vector<glm::vec2> simplify_polygon(std::vector<glm::vec2> polygon) {
  std::vector<glm::vec2> simplified;
  simplified.reserve(polygon.size());

  for (const glm::vec2 &point : polygon) {
    if (!simplified.empty() && nearly_equal(simplified.back(), point)) {
      continue;
    }
    simplified.push_back(point);
  }

  if (simplified.size() > 1u && nearly_equal(simplified.front(), simplified.back())) {
    simplified.pop_back();
  }

  if (simplified.size() < 3u) {
    return {};
  }

  bool changed = true;
  while (changed && simplified.size() >= 3u) {
    changed = false;
    for (size_t index = 0u; index < simplified.size(); ++index) {
      const glm::vec2 &previous =
          simplified[(index + simplified.size() - 1u) % simplified.size()];
      const glm::vec2 &current = simplified[index];
      const glm::vec2 &next = simplified[(index + 1u) % simplified.size()];

      const glm::vec2 ab = current - previous;
      const glm::vec2 bc = next - current;
      if (glm::length(ab) <= k_svg_epsilon || glm::length(bc) <= k_svg_epsilon ||
          std::abs(cross(ab, bc)) <= k_svg_epsilon) {
        simplified.erase(simplified.begin() + static_cast<std::ptrdiff_t>(index));
        changed = true;
        break;
      }
    }
  }

  return simplified.size() >= 3u ? simplified : std::vector<glm::vec2>{};
}

inline std::vector<glm::vec2> simplify_polyline(std::vector<glm::vec2> points) {
  std::vector<glm::vec2> simplified;
  simplified.reserve(points.size());

  for (const glm::vec2 &point : points) {
    if (!simplified.empty() && nearly_equal(simplified.back(), point)) {
      continue;
    }
    simplified.push_back(point);
  }

  if (simplified.size() < 2u) {
    return {};
  }

  bool changed = true;
  while (changed && simplified.size() >= 3u) {
    changed = false;
    for (size_t index = 1u; index + 1u < simplified.size(); ++index) {
      const glm::vec2 &previous = simplified[index - 1u];
      const glm::vec2 &current = simplified[index];
      const glm::vec2 &next = simplified[index + 1u];

      const glm::vec2 ab = current - previous;
      const glm::vec2 bc = next - current;
      if (glm::length(ab) <= k_svg_epsilon || glm::length(bc) <= k_svg_epsilon ||
          std::abs(cross(ab, bc)) <= k_svg_epsilon) {
        simplified.erase(simplified.begin() + static_cast<std::ptrdiff_t>(index));
        changed = true;
        break;
      }
    }
  }

  return simplified.size() >= 2u ? simplified : std::vector<glm::vec2>{};
}

inline bool point_in_triangle(
    const glm::vec2 &point,
    const glm::vec2 &a,
    const glm::vec2 &b,
    const glm::vec2 &c
) {
  const glm::vec2 v0 = c - a;
  const glm::vec2 v1 = b - a;
  const glm::vec2 v2 = point - a;

  const float dot00 = glm::dot(v0, v0);
  const float dot01 = glm::dot(v0, v1);
  const float dot02 = glm::dot(v0, v2);
  const float dot11 = glm::dot(v1, v1);
  const float dot12 = glm::dot(v1, v2);

  const float denominator = dot00 * dot11 - dot01 * dot01;
  if (std::abs(denominator) <= k_svg_epsilon) {
    return false;
  }

  const float inv = 1.0f / denominator;
  const float u = (dot11 * dot02 - dot01 * dot12) * inv;
  const float v = (dot00 * dot12 - dot01 * dot02) * inv;
  return u >= -k_svg_epsilon && v >= -k_svg_epsilon &&
         (u + v) <= (1.0f + k_svg_epsilon);
}

inline std::vector<glm::vec2> triangulate_simple_polygon(
    std::vector<glm::vec2> polygon
) {
  polygon = simplify_polygon(std::move(polygon));
  if (polygon.size() < 3u) {
    return {};
  }

  if (is_clockwise(polygon)) {
    std::reverse(polygon.begin(), polygon.end());
  }

  std::vector<size_t> indices(polygon.size());
  for (size_t index = 0u; index < polygon.size(); ++index) {
    indices[index] = index;
  }

  std::vector<glm::vec2> triangles;
  triangles.reserve((polygon.size() - 2u) * 3u);

  size_t guard = 0u;
  while (indices.size() > 3u && guard < polygon.size() * polygon.size()) {
    bool ear_found = false;
    for (size_t index = 0u; index < indices.size(); ++index) {
      const size_t prev_index = indices[(index + indices.size() - 1u) % indices.size()];
      const size_t curr_index = indices[index];
      const size_t next_index = indices[(index + 1u) % indices.size()];

      const glm::vec2 &a = polygon[prev_index];
      const glm::vec2 &b = polygon[curr_index];
      const glm::vec2 &c = polygon[next_index];

      if (cross(b - a, c - b) <= k_svg_epsilon) {
        continue;
      }

      bool contains_vertex = false;
      for (size_t test = 0u; test < indices.size(); ++test) {
        const size_t vertex_index = indices[test];
        if (vertex_index == prev_index || vertex_index == curr_index ||
            vertex_index == next_index) {
          continue;
        }

        if (point_in_triangle(polygon[vertex_index], a, b, c)) {
          contains_vertex = true;
          break;
        }
      }

      if (contains_vertex) {
        continue;
      }

      triangles.push_back(a);
      triangles.push_back(b);
      triangles.push_back(c);
      indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(index));
      ear_found = true;
      break;
    }

    if (!ear_found) {
      break;
    }

    ++guard;
  }

  if (indices.size() == 3u) {
    triangles.push_back(polygon[indices[0u]]);
    triangles.push_back(polygon[indices[1u]]);
    triangles.push_back(polygon[indices[2u]]);
  }

  return triangles;
}

inline size_t rightmost_vertex_index(const std::vector<glm::vec2> &polygon) {
  size_t index = 0u;
  for (size_t candidate = 1u; candidate < polygon.size(); ++candidate) {
    if (polygon[candidate].x > polygon[index].x ||
        (nearly_equal(polygon[candidate].x, polygon[index].x) &&
         polygon[candidate].y < polygon[index].y)) {
      index = candidate;
    }
  }
  return index;
}

inline size_t find_outer_bridge_vertex(
    const std::vector<glm::vec2> &outer,
    const glm::vec2 &hole_point
) {
  float best_intersection_x = std::numeric_limits<float>::infinity();
  size_t best_vertex = 0u;
  bool found = false;

  for (size_t index = 0u; index < outer.size(); ++index) {
    const glm::vec2 &a = outer[index];
    const glm::vec2 &b = outer[(index + 1u) % outer.size()];

    if ((hole_point.y < std::min(a.y, b.y)) ||
        (hole_point.y > std::max(a.y, b.y)) ||
        nearly_equal(a.y, b.y)) {
      continue;
    }

    const float ratio = (hole_point.y - a.y) / (b.y - a.y);
    const float intersection_x = a.x + ratio * (b.x - a.x);
    if (intersection_x <= hole_point.x + k_svg_epsilon) {
      continue;
    }

    if (intersection_x < best_intersection_x) {
      best_intersection_x = intersection_x;
      best_vertex = a.x > b.x ? index : (index + 1u) % outer.size();
      found = true;
    }
  }

  if (!found) {
    best_vertex = rightmost_vertex_index(outer);
  }

  return best_vertex;
}

inline std::vector<glm::vec2> merge_hole_into_polygon(
    std::vector<glm::vec2> outer,
    std::vector<glm::vec2> hole
) {
  outer = simplify_polygon(std::move(outer));
  hole = simplify_polygon(std::move(hole));

  if (outer.empty() || hole.empty()) {
    return outer;
  }

  if (is_clockwise(outer)) {
    std::reverse(outer.begin(), outer.end());
  }
  if (!is_clockwise(hole)) {
    std::reverse(hole.begin(), hole.end());
  }

  const size_t hole_index = rightmost_vertex_index(hole);
  const size_t outer_index = find_outer_bridge_vertex(outer, hole[hole_index]);

  std::vector<glm::vec2> merged;
  merged.reserve(outer.size() + hole.size() + 2u);

  for (size_t index = 0u; index <= outer_index; ++index) {
    merged.push_back(outer[index]);
  }

  for (size_t offset = 0u; offset < hole.size(); ++offset) {
    merged.push_back(hole[(hole_index + offset) % hole.size()]);
  }
  merged.push_back(hole[hole_index]);
  merged.push_back(outer[outer_index]);

  for (size_t index = outer_index + 1u; index < outer.size(); ++index) {
    merged.push_back(outer[index]);
  }

  return simplify_polygon(std::move(merged));
}

inline std::vector<glm::vec2> triangulate_filled_contours(
    const std::vector<SvgContour> &contours
) {
  struct PreparedContour {
    std::vector<glm::vec2> points;
    int parent = -1;
    std::vector<size_t> children;
  };

  std::vector<PreparedContour> prepared;
  prepared.reserve(contours.size());

  for (const SvgContour &contour : contours) {
    std::vector<glm::vec2> polygon = simplify_polygon(contour.points);
    if (polygon.size() >= 3u) {
      prepared.push_back({std::move(polygon)});
    }
  }

  if (prepared.empty()) {
    return {};
  }

  for (size_t index = 0u; index < prepared.size(); ++index) {
    float best_area = std::numeric_limits<float>::infinity();
    for (size_t candidate = 0u; candidate < prepared.size(); ++candidate) {
      if (candidate == index) {
        continue;
      }

      const float candidate_area = std::abs(signed_area(prepared[candidate].points));
      const float index_area = std::abs(signed_area(prepared[index].points));
      if (candidate_area <= index_area) {
        continue;
      }

      if (!point_in_polygon(prepared[index].points.front(), prepared[candidate].points)) {
        continue;
      }

      if (candidate_area < best_area) {
        best_area = candidate_area;
        prepared[index].parent = static_cast<int>(candidate);
      }
    }
  }

  for (size_t index = 0u; index < prepared.size(); ++index) {
    if (prepared[index].parent >= 0) {
      prepared[prepared[index].parent].children.push_back(index);
    }
  }

  std::vector<glm::vec2> triangles;
  std::function<void(size_t, size_t)> append_triangles =
      [&](size_t contour_index, size_t depth) {
        if (depth % 2u == 0u) {
          std::vector<glm::vec2> polygon = prepared[contour_index].points;
          for (size_t child_index : prepared[contour_index].children) {
            polygon = merge_hole_into_polygon(
                std::move(polygon), prepared[child_index].points
            );
          }

          auto contour_triangles = triangulate_simple_polygon(std::move(polygon));
          triangles.insert(
              triangles.end(),
              contour_triangles.begin(),
              contour_triangles.end()
          );
        }

        for (size_t child_index : prepared[contour_index].children) {
          append_triangles(child_index, depth + 1u);
        }
      };

  for (size_t index = 0u; index < prepared.size(); ++index) {
    if (prepared[index].parent < 0) {
      append_triangles(index, 0u);
    }
  }

  return triangles;
}

inline void add_round_fan(
    std::vector<SvgColorVertex> &vertices,
    const glm::vec2 &center,
    const glm::vec2 &from,
    const glm::vec2 &to,
    const glm::vec4 &color
) {
  float start_angle = std::atan2(from.y, from.x);
  float end_angle = std::atan2(to.y, to.x);

  while (end_angle < start_angle) {
    end_angle += std::numbers::pi_v<float> * 2.0f;
  }

  const float delta = end_angle - start_angle;
  const int segments =
      std::max(3, static_cast<int>(std::ceil(delta / (std::numbers::pi_v<float> / 12.0f))));

  for (int index = 0; index < segments; ++index) {
    const float start = start_angle + delta * static_cast<float>(index) /
                                          static_cast<float>(segments);
    const float end = start_angle + delta * static_cast<float>(index + 1) /
                                        static_cast<float>(segments);

    vertices.push_back({center, color});
    vertices.push_back(
        {center + glm::vec2(std::cos(start), std::sin(start)) * glm::length(from),
         color}
    );
    vertices.push_back(
        {center + glm::vec2(std::cos(end), std::sin(end)) * glm::length(from),
         color}
    );
  }
}

inline void append_segment_quad(
    std::vector<SvgColorVertex> &vertices,
    const glm::vec2 &a,
    const glm::vec2 &b,
    const glm::vec2 &offset_a,
    const glm::vec2 &offset_b,
    const glm::vec4 &color
) {
  const glm::vec2 top_left = a + offset_a;
  const glm::vec2 bottom_left = a - offset_a;
  const glm::vec2 top_right = b + offset_b;
  const glm::vec2 bottom_right = b - offset_b;

  vertices.push_back({top_left, color});
  vertices.push_back({bottom_left, color});
  vertices.push_back({top_right, color});

  vertices.push_back({top_right, color});
  vertices.push_back({bottom_left, color});
  vertices.push_back({bottom_right, color});
}

inline SvgTriangleBatch build_stroke_batch(
    std::vector<glm::vec2> points,
    bool closed,
    float stroke_width,
    StrokeLineCap line_cap,
    const glm::vec4 &color
) {
  SvgTriangleBatch batch;
  points = closed ? simplify_polygon(std::move(points))
                  : simplify_polyline(std::move(points));
  if (points.size() < 2u || stroke_width <= 0.0f) {
    return batch;
  }

  if (!closed && points.size() < 2u) {
    return batch;
  }

  const float half_width = stroke_width * 0.5f;
  if (!closed && line_cap == StrokeLineCap::Square) {
    const glm::vec2 start_direction = glm::normalize(points[1u] - points[0u]);
    const glm::vec2 end_direction =
        glm::normalize(points.back() - points[points.size() - 2u]);
    points.front() -= start_direction * half_width;
    points.back() += end_direction * half_width;
  }

  const size_t segment_count = closed ? points.size() : points.size() - 1u;
  std::vector<glm::vec2> normals(segment_count);
  for (size_t index = 0u; index < segment_count; ++index) {
    const glm::vec2 direction =
        glm::normalize(points[(index + 1u) % points.size()] - points[index]);
    normals[index] = glm::vec2(-direction.y, direction.x);
  }

  std::vector<glm::vec2> offsets(points.size());
  for (size_t index = 0u; index < points.size(); ++index) {
    if (!closed && (index == 0u || index + 1u == points.size())) {
      offsets[index] = normals[index == 0u ? 0u : segment_count - 1u] * half_width;
      continue;
    }

    const glm::vec2 previous = normals[(index + segment_count - 1u) % segment_count];
    const glm::vec2 current = normals[index % segment_count];
    glm::vec2 averaged = glm::normalize(previous + current);
    if (!std::isfinite(averaged.x) || !std::isfinite(averaged.y)) {
      averaged = current;
    }

    const float dot_product = glm::dot(averaged, current);
    if (std::abs(dot_product) <= k_svg_epsilon) {
      offsets[index] = current * half_width;
    } else {
      offsets[index] = averaged * (half_width / dot_product);
    }
  }

  for (size_t index = 0u; index < segment_count; ++index) {
    append_segment_quad(
        batch.vertices,
        points[index],
        points[(index + 1u) % points.size()],
        offsets[index],
        offsets[(index + 1u) % points.size()],
        color
    );
  }

  if (!closed && line_cap == StrokeLineCap::Round) {
    const glm::vec2 start_direction = glm::normalize(points[1u] - points[0u]);
    const glm::vec2 end_direction =
        glm::normalize(points.back() - points[points.size() - 2u]);
    const glm::vec2 start_normal(-start_direction.y, start_direction.x);
    const glm::vec2 end_normal(-end_direction.y, end_direction.x);

    add_round_fan(
        batch.vertices,
        points.front(),
        -start_normal * half_width,
        start_normal * half_width,
        color
    );
    add_round_fan(
        batch.vertices,
        points.back(),
        end_normal * half_width,
        -end_normal * half_width,
        color
    );
  }

  return batch;
}

inline void append_cubic_curve(
    std::vector<glm::vec2> &points,
    const glm::vec2 &p0,
    const glm::vec2 &p1,
    const glm::vec2 &p2,
    const glm::vec2 &p3,
    int depth = 0
) {
  const float flatness =
      std::max(
          std::abs(cross(p1 - p0, p3 - p0)),
          std::abs(cross(p2 - p0, p3 - p0))
      );
  if (flatness <= k_curve_flatness || depth >= 8) {
    points.push_back(p3);
    return;
  }

  const glm::vec2 p01 = (p0 + p1) * 0.5f;
  const glm::vec2 p12 = (p1 + p2) * 0.5f;
  const glm::vec2 p23 = (p2 + p3) * 0.5f;
  const glm::vec2 p012 = (p01 + p12) * 0.5f;
  const glm::vec2 p123 = (p12 + p23) * 0.5f;
  const glm::vec2 p0123 = (p012 + p123) * 0.5f;

  append_cubic_curve(points, p0, p01, p012, p0123, depth + 1);
  append_cubic_curve(points, p0123, p123, p23, p3, depth + 1);
}

inline void append_quadratic_curve(
    std::vector<glm::vec2> &points,
    const glm::vec2 &p0,
    const glm::vec2 &p1,
    const glm::vec2 &p2,
    int depth = 0
) {
  const float flatness = std::abs(cross(p1 - p0, p2 - p0));
  if (flatness <= k_curve_flatness || depth >= 8) {
    points.push_back(p2);
    return;
  }

  const glm::vec2 p01 = (p0 + p1) * 0.5f;
  const glm::vec2 p12 = (p1 + p2) * 0.5f;
  const glm::vec2 p012 = (p01 + p12) * 0.5f;

  append_quadratic_curve(points, p0, p01, p012, depth + 1);
  append_quadratic_curve(points, p012, p12, p2, depth + 1);
}

inline float vector_angle(const glm::vec2 &lhs, const glm::vec2 &rhs) {
  const float dot_product = glm::clamp(glm::dot(lhs, rhs), -1.0f, 1.0f);
  const float angle = std::acos(dot_product);
  return cross(lhs, rhs) < 0.0f ? -angle : angle;
}

inline void append_arc_curve(
    std::vector<glm::vec2> &points,
    const glm::vec2 &start,
    float rx,
    float ry,
    float x_axis_rotation,
    bool large_arc,
    bool sweep,
    const glm::vec2 &end
) {
  rx = std::abs(rx);
  ry = std::abs(ry);

  if (rx <= k_svg_epsilon || ry <= k_svg_epsilon || nearly_equal(start, end)) {
    points.push_back(end);
    return;
  }

  const float phi = glm::radians(x_axis_rotation);
  const float cos_phi = std::cos(phi);
  const float sin_phi = std::sin(phi);

  const glm::vec2 midpoint = (start - end) * 0.5f;
  const float x1p = cos_phi * midpoint.x + sin_phi * midpoint.y;
  const float y1p = -sin_phi * midpoint.x + cos_phi * midpoint.y;

  float rx_sq = rx * rx;
  float ry_sq = ry * ry;
  const float x1p_sq = x1p * x1p;
  const float y1p_sq = y1p * y1p;

  const float lambda = x1p_sq / rx_sq + y1p_sq / ry_sq;
  if (lambda > 1.0f) {
    const float scale = std::sqrt(lambda);
    rx *= scale;
    ry *= scale;
    rx_sq = rx * rx;
    ry_sq = ry * ry;
  }

  const float numerator =
      rx_sq * ry_sq - rx_sq * y1p_sq - ry_sq * x1p_sq;
  const float denominator = rx_sq * y1p_sq + ry_sq * x1p_sq;
  const float factor =
      (large_arc == sweep ? -1.0f : 1.0f) *
      std::sqrt(std::max(0.0f, numerator / std::max(denominator, k_svg_epsilon)));

  const float cxp = factor * (rx * y1p / ry);
  const float cyp = factor * (-ry * x1p / rx);

  const glm::vec2 center(
      cos_phi * cxp - sin_phi * cyp + (start.x + end.x) * 0.5f,
      sin_phi * cxp + cos_phi * cyp + (start.y + end.y) * 0.5f
  );

  const glm::vec2 start_vector((x1p - cxp) / rx, (y1p - cyp) / ry);
  const glm::vec2 end_vector((-x1p - cxp) / rx, (-y1p - cyp) / ry);

  float start_angle = vector_angle(glm::vec2(1.0f, 0.0f), start_vector);
  float delta_angle = vector_angle(start_vector, end_vector);

  if (!sweep && delta_angle > 0.0f) {
    delta_angle -= std::numbers::pi_v<float> * 2.0f;
  } else if (sweep && delta_angle < 0.0f) {
    delta_angle += std::numbers::pi_v<float> * 2.0f;
  }

  const int segments = std::max(
      1, static_cast<int>(std::ceil(std::abs(delta_angle) / (std::numbers::pi_v<float> / 8.0f)))
  );

  for (int index = 1; index <= segments; ++index) {
    const float angle = start_angle +
                        delta_angle * static_cast<float>(index) /
                            static_cast<float>(segments);
    const float cos_angle = std::cos(angle);
    const float sin_angle = std::sin(angle);

    points.emplace_back(
        center.x + cos_phi * rx * cos_angle - sin_phi * ry * sin_angle,
        center.y + sin_phi * rx * cos_angle + cos_phi * ry * sin_angle
    );
  }
}

class SvgPathParser {
public:
  explicit SvgPathParser(std::string data) : m_data(std::move(data)) {}

  std::vector<SvgContour> parse() {
    while (true) {
      skip_delimiters();
      if (m_index >= m_data.size()) {
        break;
      }

      if (std::isalpha(static_cast<unsigned char>(m_data[m_index]))) {
        m_command = m_data[m_index++];
      } else {
        ASTRA_ENSURE(m_command == '\0', "Invalid SVG path data: ", m_data);
      }

      execute_command();
    }

    flush_contour(false);
    return m_contours;
  }

private:
  void skip_delimiters() {
    while (m_index < m_data.size() &&
           (std::isspace(static_cast<unsigned char>(m_data[m_index])) ||
            m_data[m_index] == ',')) {
      ++m_index;
    }
  }

  bool read_number(float &value) {
    skip_delimiters();
    if (m_index >= m_data.size()) {
      return false;
    }

    if (std::isalpha(static_cast<unsigned char>(m_data[m_index])) &&
        m_data[m_index] != 'e' && m_data[m_index] != 'E') {
      return false;
    }

    char *end = nullptr;
    const float parsed = std::strtof(m_data.c_str() + m_index, &end);
    if (end == m_data.c_str() + m_index) {
      return false;
    }

    value = parsed;
    m_index = static_cast<size_t>(end - m_data.c_str());
    return true;
  }

  bool read_flag(bool &value) {
    float parsed = 0.0f;
    if (!read_number(parsed)) {
      return false;
    }

    ASTRA_ENSURE(parsed != 0.0f && parsed != 1.0f, "Invalid SVG arc flag in path data: ", m_data);
    value = parsed > 0.5f;
    return true;
  }

  void begin_contour(const glm::vec2 &point) {
    flush_contour(false);
    m_current_contour.clear();
    m_current_contour.push_back(point);
    m_current_point = point;
    m_subpath_start = point;
  }

  void append_point(const glm::vec2 &point) {
    if (m_current_contour.empty()) {
      begin_contour(point);
      return;
    }

    if (!nearly_equal(m_current_contour.back(), point)) {
      m_current_contour.push_back(point);
    }
    m_current_point = point;
  }

  void flush_contour(bool closed) {
    if (m_current_contour.empty()) {
      return;
    }

    SvgContour contour;
    contour.points = m_current_contour;
    contour.closed = closed;
    m_contours.push_back(std::move(contour));
    m_current_contour.clear();
  }

  void execute_command() {
    const bool relative = std::islower(static_cast<unsigned char>(m_command));
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(m_command)))) {
      case 'M':
        execute_move(relative);
        break;
      case 'L':
        execute_line(relative);
        break;
      case 'H':
        execute_horizontal(relative);
        break;
      case 'V':
        execute_vertical(relative);
        break;
      case 'C':
        execute_cubic(relative);
        break;
      case 'S':
        execute_smooth_cubic(relative);
        break;
      case 'Q':
        execute_quadratic(relative);
        break;
      case 'T':
        execute_smooth_quadratic(relative);
        break;
      case 'A':
        execute_arc(relative);
        break;
      case 'Z':
        execute_close();
        break;
      default:
        ASTRA_EXCEPTION("Unsupported SVG path command: ", m_command);
    }
  }

  glm::vec2 resolve_point(float x, float y, bool relative) const {
    return relative ? m_current_point + glm::vec2(x, y) : glm::vec2(x, y);
  }

  void execute_move(bool relative) {
    float x = 0.0f;
    float y = 0.0f;
    ASTRA_ENSURE(!read_number(x) || !read_number(y), "Invalid SVG move command");

    begin_contour(resolve_point(x, y, relative));
    m_previous_command = 'M';
    m_last_cubic_control.reset();
    m_last_quadratic_control.reset();

    while (read_number(x) && read_number(y)) {
      append_point(resolve_point(x, y, relative));
      m_previous_command = 'L';
    }
  }

  void execute_line(bool relative) {
    float x = 0.0f;
    float y = 0.0f;
    while (read_number(x) && read_number(y)) {
      append_point(resolve_point(x, y, relative));
    }
    m_previous_command = 'L';
    m_last_cubic_control.reset();
    m_last_quadratic_control.reset();
  }

  void execute_horizontal(bool relative) {
    float x = 0.0f;
    while (read_number(x)) {
      const glm::vec2 next_point =
          relative ? glm::vec2(m_current_point.x + x, m_current_point.y)
                   : glm::vec2(x, m_current_point.y);
      append_point(next_point);
    }
    m_previous_command = 'H';
    m_last_cubic_control.reset();
    m_last_quadratic_control.reset();
  }

  void execute_vertical(bool relative) {
    float y = 0.0f;
    while (read_number(y)) {
      const glm::vec2 next_point =
          relative ? glm::vec2(m_current_point.x, m_current_point.y + y)
                   : glm::vec2(m_current_point.x, y);
      append_point(next_point);
    }
    m_previous_command = 'V';
    m_last_cubic_control.reset();
    m_last_quadratic_control.reset();
  }

  void execute_cubic(bool relative) {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 0.0f;
    float y2 = 0.0f;
    float x = 0.0f;
    float y = 0.0f;

    while (read_number(x1) && read_number(y1) && read_number(x2) &&
           read_number(y2) && read_number(x) && read_number(y)) {
      const glm::vec2 control1 = resolve_point(x1, y1, relative);
      const glm::vec2 control2 = resolve_point(x2, y2, relative);
      const glm::vec2 target = resolve_point(x, y, relative);
      append_cubic_curve(
          m_current_contour,
          m_current_point,
          control1,
          control2,
          target
      );
      m_current_point = target;
      m_last_cubic_control = control2;
      m_last_quadratic_control.reset();
      m_previous_command = 'C';
    }
  }

  void execute_smooth_cubic(bool relative) {
    float x2 = 0.0f;
    float y2 = 0.0f;
    float x = 0.0f;
    float y = 0.0f;

    while (read_number(x2) && read_number(y2) && read_number(x) && read_number(y)) {
      glm::vec2 control1 = m_current_point;
      if (m_previous_command == 'C' || m_previous_command == 'S') {
        control1 = m_current_point * 2.0f - m_last_cubic_control.value_or(m_current_point);
      }

      const glm::vec2 control2 = resolve_point(x2, y2, relative);
      const glm::vec2 target = resolve_point(x, y, relative);
      append_cubic_curve(
          m_current_contour,
          m_current_point,
          control1,
          control2,
          target
      );
      m_current_point = target;
      m_last_cubic_control = control2;
      m_last_quadratic_control.reset();
      m_previous_command = 'S';
    }
  }

  void execute_quadratic(bool relative) {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x = 0.0f;
    float y = 0.0f;

    while (read_number(x1) && read_number(y1) && read_number(x) && read_number(y)) {
      const glm::vec2 control = resolve_point(x1, y1, relative);
      const glm::vec2 target = resolve_point(x, y, relative);
      append_quadratic_curve(m_current_contour, m_current_point, control, target);
      m_current_point = target;
      m_last_quadratic_control = control;
      m_last_cubic_control.reset();
      m_previous_command = 'Q';
    }
  }

  void execute_smooth_quadratic(bool relative) {
    float x = 0.0f;
    float y = 0.0f;
    while (read_number(x) && read_number(y)) {
      glm::vec2 control = m_current_point;
      if (m_previous_command == 'Q' || m_previous_command == 'T') {
        control =
            m_current_point * 2.0f - m_last_quadratic_control.value_or(m_current_point);
      }

      const glm::vec2 target = resolve_point(x, y, relative);
      append_quadratic_curve(m_current_contour, m_current_point, control, target);
      m_current_point = target;
      m_last_quadratic_control = control;
      m_last_cubic_control.reset();
      m_previous_command = 'T';
    }
  }

  void execute_arc(bool relative) {
    float rx = 0.0f;
    float ry = 0.0f;
    float rotation = 0.0f;
    bool large_arc = false;
    bool sweep = false;
    float x = 0.0f;
    float y = 0.0f;

    while (read_number(rx) && read_number(ry) && read_number(rotation) &&
           read_flag(large_arc) && read_flag(sweep) && read_number(x) &&
           read_number(y)) {
      const glm::vec2 target = resolve_point(x, y, relative);
      append_arc_curve(
          m_current_contour,
          m_current_point,
          rx,
          ry,
          rotation,
          large_arc,
          sweep,
          target
      );
      m_current_point = target;
      m_last_cubic_control.reset();
      m_last_quadratic_control.reset();
      m_previous_command = 'A';
    }
  }

  void execute_close() {
    if (!m_current_contour.empty()) {
      if (!nearly_equal(m_current_contour.front(), m_current_contour.back())) {
        m_current_contour.push_back(m_current_contour.front());
      }
      flush_contour(true);
      m_current_point = m_subpath_start;
    }
    m_last_cubic_control.reset();
    m_last_quadratic_control.reset();
    m_previous_command = 'Z';
  }

  std::string m_data;
  size_t m_index = 0u;
  char m_command = '\0';
  char m_previous_command = '\0';
  glm::vec2 m_current_point = glm::vec2(0.0f);
  glm::vec2 m_subpath_start = glm::vec2(0.0f);
  std::optional<glm::vec2> m_last_cubic_control;
  std::optional<glm::vec2> m_last_quadratic_control;
  std::vector<glm::vec2> m_current_contour;
  std::vector<SvgContour> m_contours;
};

inline void transform_shape(SvgShape &shape, const glm::mat3 &transform) {
  for (SvgContour &contour : shape.contours) {
    for (glm::vec2 &point : contour.points) {
      point = transform_point(transform, point);
    }
  }
}

inline SvgShape build_rect_shape(const XmlNode &node) {
  const float width = parse_svg_length(xml_attribute_value(node, "width"), "rect width");
  const float height = parse_svg_length(xml_attribute_value(node, "height"), "rect height");
  ASTRA_ENSURE(width <= 0.0f || height <= 0.0f, "Invalid SVG rect size");

  const float x = has_xml_attribute(node, "x")
                      ? parse_svg_length(xml_attribute_value(node, "x"), "rect x")
                      : 0.0f;
  const float y = has_xml_attribute(node, "y")
                      ? parse_svg_length(xml_attribute_value(node, "y"), "rect y")
                      : 0.0f;

  const float rx = has_xml_attribute(node, "rx")
                       ? parse_svg_length(xml_attribute_value(node, "rx"), "rect rx")
                       : 0.0f;
  const float ry = has_xml_attribute(node, "ry")
                       ? parse_svg_length(xml_attribute_value(node, "ry"), "rect ry")
                       : 0.0f;
  ASTRA_ENSURE(rx > 0.0f || ry > 0.0f, "Rounded SVG rect corners are not supported");

  SvgShape shape;
  shape.contours.push_back(
      SvgContour{
          .points = {
              glm::vec2(x, y),
              glm::vec2(x + width, y),
              glm::vec2(x + width, y + height),
              glm::vec2(x, y + height),
          },
          .closed = true,
      }
  );
  return shape;
}

inline SvgShape build_circle_shape(const XmlNode &node) {
  const float cx = has_xml_attribute(node, "cx")
                       ? parse_svg_length(xml_attribute_value(node, "cx"), "circle cx")
                       : 0.0f;
  const float cy = has_xml_attribute(node, "cy")
                       ? parse_svg_length(xml_attribute_value(node, "cy"), "circle cy")
                       : 0.0f;
  const float r = parse_svg_length(xml_attribute_value(node, "r"), "circle r");
  ASTRA_ENSURE(r <= 0.0f, "Invalid SVG circle radius");

  const int segments = std::max(16, static_cast<int>(std::ceil(2.0f * std::numbers::pi_v<float> * r / 6.0f)));
  SvgContour contour;
  contour.closed = true;
  contour.points.reserve(static_cast<size_t>(segments));

  for (int index = 0; index < segments; ++index) {
    const float angle =
        (std::numbers::pi_v<float> * 2.0f * static_cast<float>(index)) /
        static_cast<float>(segments);
    contour.points.emplace_back(cx + std::cos(angle) * r, cy + std::sin(angle) * r);
  }

  SvgShape shape;
  shape.contours.push_back(std::move(contour));
  return shape;
}

inline SvgShape build_ellipse_shape(const XmlNode &node) {
  const float cx = has_xml_attribute(node, "cx")
                       ? parse_svg_length(xml_attribute_value(node, "cx"), "ellipse cx")
                       : 0.0f;
  const float cy = has_xml_attribute(node, "cy")
                       ? parse_svg_length(xml_attribute_value(node, "cy"), "ellipse cy")
                       : 0.0f;
  const float rx = parse_svg_length(xml_attribute_value(node, "rx"), "ellipse rx");
  const float ry = parse_svg_length(xml_attribute_value(node, "ry"), "ellipse ry");
  ASTRA_ENSURE(rx <= 0.0f || ry <= 0.0f, "Invalid SVG ellipse radius");

  const int segments = std::max(
      16,
      static_cast<int>(std::ceil(
          2.0f * std::numbers::pi_v<float> * std::max(rx, ry) / 6.0f
      ))
  );
  SvgContour contour;
  contour.closed = true;
  contour.points.reserve(static_cast<size_t>(segments));

  for (int index = 0; index < segments; ++index) {
    const float angle =
        (std::numbers::pi_v<float> * 2.0f * static_cast<float>(index)) /
        static_cast<float>(segments);
    contour.points.emplace_back(
        cx + std::cos(angle) * rx,
        cy + std::sin(angle) * ry
    );
  }

  SvgShape shape;
  shape.contours.push_back(std::move(contour));
  return shape;
}

inline SvgShape build_line_shape(const XmlNode &node) {
  SvgShape shape;
  shape.contours.push_back(
      SvgContour{
          .points =
              {
                  glm::vec2(
                      parse_svg_length(xml_attribute_value(node, "x1"), "line x1"),
                      parse_svg_length(xml_attribute_value(node, "y1"), "line y1")
                  ),
                  glm::vec2(
                      parse_svg_length(xml_attribute_value(node, "x2"), "line x2"),
                      parse_svg_length(xml_attribute_value(node, "y2"), "line y2")
                  ),
              },
          .closed = false,
      }
  );
  return shape;
}

inline SvgShape build_poly_shape(
    const XmlNode &node,
    bool closed
) {
  SvgShape shape;
  shape.contours.push_back(
      SvgContour{
          .points = parse_points(xml_attribute_value(node, "points")),
          .closed = closed,
      }
  );
  return shape;
}

inline SvgShape build_path_shape(const XmlNode &node) {
  SvgPathParser parser(std::string(xml_attribute_value(node, "d")));
  SvgShape shape;
  shape.contours = parser.parse();
  return shape;
}

inline SvgShape build_shape(const XmlNode &node) {
  const std::string &name = node.name;
  if (name == "path") {
    return build_path_shape(node);
  }
  if (name == "rect") {
    return build_rect_shape(node);
  }
  if (name == "circle") {
    return build_circle_shape(node);
  }
  if (name == "ellipse") {
    return build_ellipse_shape(node);
  }
  if (name == "line") {
    return build_line_shape(node);
  }
  if (name == "polyline") {
    return build_poly_shape(node, false);
  }
  if (name == "polygon") {
    return build_poly_shape(node, true);
  }

  ASTRA_EXCEPTION("Unsupported SVG shape node: ", name);
}

inline void append_fill_batches(
    const SvgShape &shape,
    const SvgStyleState &state,
    std::vector<SvgTriangleBatch> &batches
) {
  if (!state.fill_enabled) {
    return;
  }

  const glm::vec4 color = state.fill *
                          glm::vec4(1.0f, 1.0f, 1.0f, state.opacity * state.fill_opacity);
  if (color.a <= 0.0f) {
    return;
  }

  SvgTriangleBatch batch;
  const std::vector<glm::vec2> triangles = triangulate_filled_contours(shape.contours);
  batch.vertices.reserve(triangles.size());
  for (const glm::vec2 &vertex : triangles) {
    batch.vertices.push_back({vertex, color});
  }

  if (!batch.vertices.empty()) {
    batches.push_back(std::move(batch));
  }
}

inline void append_stroke_batches(
    const SvgShape &shape,
    const SvgStyleState &state,
    std::vector<SvgTriangleBatch> &batches
) {
  if (!state.stroke_enabled || state.stroke_width <= 0.0f) {
    return;
  }

  const glm::vec4 color = state.stroke *
                          glm::vec4(1.0f, 1.0f, 1.0f, state.opacity * state.stroke_opacity);
  if (color.a <= 0.0f) {
    return;
  }

  for (const SvgContour &contour : shape.contours) {
    SvgTriangleBatch batch = build_stroke_batch(
        contour.points,
        contour.closed,
        state.stroke_width,
        state.line_cap,
        color
    );
    if (!batch.vertices.empty()) {
      batches.push_back(std::move(batch));
    }
  }
}

inline void compile_node(
    const XmlNode &node,
    const SvgStyleState &inherited,
    std::vector<SvgTriangleBatch> &batches
) {
  const std::string &name = node.name;
  if (name.empty()) {
    return;
  }

  if (name == "title" || name == "desc" || name == "metadata") {
    return;
  }

  if (name == "defs") {
    return;
  }

  if (name == "linearGradient" || name == "radialGradient" || name == "clipPath" ||
      name == "mask" || name == "filter" || name == "text" || name == "image" ||
      name == "use" || name == "pattern" || name == "style" || name == "symbol") {
    ASTRA_EXCEPTION("Unsupported SVG feature node: ", name);
  }

  SvgStyleState state = inherited;
  apply_presentation_attributes(node, state);

  if (has_xml_attribute(node, "transform")) {
    state.transform = inherited.transform * parse_transform(xml_attribute_value(node, "transform"));
  }

  if (!state.visible) {
    return;
  }

  if (name == "svg" || name == "g") {
    for (const auto &child : node.children) {
      compile_node(*child, state, batches);
    }
    return;
  }

  SvgShape shape = build_shape(node);
  transform_shape(shape, state.transform);
  append_fill_batches(shape, state, batches);
  append_stroke_batches(shape, state, batches);
}

inline SvgDocumentMetrics parse_document_metrics(const XmlNode &root) {
  ASTRA_ENSURE(root.name != "svg", "Expected SVG root node");

  SvgDocumentMetrics metrics;
  std::optional<std::array<float, 4u>> view_box;

  if (has_xml_attribute(root, "viewBox")) {
    const auto values = parse_number_list(xml_attribute_value(root, "viewBox"));
    ASTRA_ENSURE(values.size() != 4u, "Invalid SVG viewBox");
    view_box = std::array<float, 4u>{values[0u], values[1u], values[2u], values[3u]};
  }

  if (has_xml_attribute(root, "width")) {
    metrics.width = parse_svg_length(xml_attribute_value(root, "width"), "svg width");
  }
  if (has_xml_attribute(root, "height")) {
    metrics.height = parse_svg_length(xml_attribute_value(root, "height"), "svg height");
  }

  if (view_box.has_value()) {
    ASTRA_ENSURE((*view_box)[2u] <= 0.0f || (*view_box)[3u] <= 0.0f, "Invalid SVG viewBox dimensions");

    if (metrics.width <= 0.0f) {
      metrics.width = (*view_box)[2u];
    }
    if (metrics.height <= 0.0f) {
      metrics.height = (*view_box)[3u];
    }

    const float scale_x = metrics.width / (*view_box)[2u];
    const float scale_y = metrics.height / (*view_box)[3u];
    metrics.root_transform =
        make_scale(scale_x, scale_y) *
        make_translation(-(*view_box)[0u], -(*view_box)[1u]);
  }

  ASTRA_ENSURE(metrics.width <= 0.0f || metrics.height <= 0.0f, "SVG root must define width/height or viewBox");
  return metrics;
}

inline SvgDocumentData compile_svg_string(std::string_view xml_text) {
  const XmlNode root = xml_detail::parse_document(xml_text);

  SvgDocumentMetrics metrics = parse_document_metrics(root);
  SvgStyleState root_state;
  root_state.transform = metrics.root_transform;
  apply_presentation_attributes(root, root_state);
  if (has_xml_attribute(root, "transform")) {
    root_state.transform = root_state.transform * parse_transform(xml_attribute_value(root, "transform"));
  }

  SvgDocumentData compiled;
  compiled.width = metrics.width;
  compiled.height = metrics.height;

  for (const auto &child : root.children) {
    compile_node(*child, root_state, compiled.batches);
  }

  return compiled;
}

inline SvgDocumentData compile_svg_file(const std::filesystem::path &path) {
  std::ifstream input(path);
  ASTRA_ENSURE(!input.good(), "Unable to open SVG file: ", path.string());

  std::stringstream buffer;
  buffer << input.rdbuf();
  return compile_svg_string(buffer.str());
}

} // namespace svg_detail

inline SvgDocumentData compile_svg_string(std::string_view xml_text) {
  return svg_detail::compile_svg_string(xml_text);
}

inline SvgDocumentData compile_svg_file(const std::filesystem::path &path) {
  return svg_detail::compile_svg_file(path);
}

} // namespace astralix
