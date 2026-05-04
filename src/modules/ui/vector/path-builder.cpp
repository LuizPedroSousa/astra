#include "vector/path-builder.hpp"

#include <utility>

namespace astralix::ui {
namespace {

constexpr float k_circle_cubic_kappa = 0.5522847498307936f;

} // namespace

UIPathBuilder::UIPathBuilder(UIPathStyle style) { m_command.style = style; }

UIPathBuilder &UIPathBuilder::set_style(const UIPathStyle &style) {
  m_command.style = style;
  return *this;
}

UIPathBuilder &UIPathBuilder::clear() {
  m_command.elements.clear();
  return *this;
}

UIPathBuilder &UIPathBuilder::move_to(glm::vec2 point) {
  m_command.elements.push_back(UIPathElement{
      .verb = UIPathVerb::MoveTo,
      .p0 = point,
  });
  return *this;
}

UIPathBuilder &UIPathBuilder::line_to(glm::vec2 point) {
  m_command.elements.push_back(UIPathElement{
      .verb = UIPathVerb::LineTo,
      .p0 = point,
  });
  return *this;
}

UIPathBuilder &UIPathBuilder::cubic_to(
    glm::vec2 control_point_a,
    glm::vec2 control_point_b,
    glm::vec2 point
) {
  m_command.elements.push_back(UIPathElement{
      .verb = UIPathVerb::CubicTo,
      .p0 = control_point_a,
      .p1 = control_point_b,
      .p2 = point,
  });
  return *this;
}

UIPathBuilder &UIPathBuilder::close() {
  m_command.elements.push_back(UIPathElement{
      .verb = UIPathVerb::Close,
  });
  return *this;
}

UIPathBuilder &UIPathBuilder::append_circle(glm::vec2 center, float radius) {
  if (radius <= 0.0f) {
    return *this;
  }

  const float control = radius * k_circle_cubic_kappa;
  move_to(center + glm::vec2(radius, 0.0f));
  cubic_to(
      center + glm::vec2(radius, control),
      center + glm::vec2(control, radius),
      center + glm::vec2(0.0f, radius)
  );
  cubic_to(
      center + glm::vec2(-control, radius),
      center + glm::vec2(-radius, control),
      center + glm::vec2(-radius, 0.0f)
  );
  cubic_to(
      center + glm::vec2(-radius, -control),
      center + glm::vec2(-control, -radius),
      center + glm::vec2(0.0f, -radius)
  );
  cubic_to(
      center + glm::vec2(control, -radius),
      center + glm::vec2(radius, -control),
      center + glm::vec2(radius, 0.0f)
  );
  close();
  return *this;
}

UIPathBuilder &UIPathBuilder::append_rect(const UIRect &rect) {
  if (rect.width <= 0.0f || rect.height <= 0.0f) {
    return *this;
  }

  move_to(glm::vec2(rect.x, rect.y));
  line_to(glm::vec2(rect.x + rect.width, rect.y));
  line_to(glm::vec2(rect.x + rect.width, rect.y + rect.height));
  line_to(glm::vec2(rect.x, rect.y + rect.height));
  close();
  return *this;
}

UIPathCommand UIPathBuilder::release() {
  UIPathCommand command = std::move(m_command);
  m_command = UIPathCommand{};
  return command;
}

} // namespace astralix::ui
