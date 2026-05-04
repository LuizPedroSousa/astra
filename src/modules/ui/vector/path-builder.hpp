#pragma once

#include "types.hpp"

namespace astralix::ui {

class UIPathBuilder {
public:
  explicit UIPathBuilder(UIPathStyle style = {});

  UIPathBuilder &set_style(const UIPathStyle &style);
  UIPathBuilder &clear();

  UIPathBuilder &move_to(glm::vec2 point);
  UIPathBuilder &line_to(glm::vec2 point);
  UIPathBuilder &cubic_to(
      glm::vec2 control_point_a,
      glm::vec2 control_point_b,
      glm::vec2 point
  );
  UIPathBuilder &close();

  UIPathBuilder &append_circle(glm::vec2 center, float radius);
  UIPathBuilder &append_rect(const UIRect &rect);

  const UIPathCommand &command() const { return m_command; }
  UIPathCommand build() const { return m_command; }
  UIPathCommand release();

private:
  UIPathCommand m_command{};
};

} // namespace astralix::ui
