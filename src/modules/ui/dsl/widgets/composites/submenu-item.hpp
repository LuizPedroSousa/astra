#pragma once

#include "dsl/core.hpp"
#include "dsl/widgets/actions/pressable.hpp"
#include "dsl/widgets/content/text.hpp"

#include <utility>

namespace astralix::ui::dsl {

inline NodeSpec submenu_item(
    std::string label,
    std::function<void()> on_open
) {
  using namespace styles;

  return pressable()
      .style(
          fill_x()
              .flex_row()
              .items_center()
              .justify_start()
              .padding_xy(12.0f, 8.0f)
              .gap(10.0f)
              .radius(10.0f)
              .hover(state().background(rgba(0.973f, 0.482f, 0.031f, 0.14f)))
              .pressed(
                  state().background(rgba(0.973f, 0.482f, 0.031f, 0.20f))
              )
              .focused(
                  state().border(1.0f, rgba(1.0f, 0.608f, 0.200f, 0.85f))
              )
      )
      .on_click(std::move(on_open))
      .children(
          text(std::move(label))
              .style(
                  grow(1.0f),
                  font_size(13.0f),
                  text_color(rgba(0.969f, 0.973f, 0.980f, 1.0f))
              ),
          text(">")
              .style(
                  font_size(13.0f),
                  text_color(rgba(0.675f, 0.702f, 0.765f, 1.0f))
              )
      );
}

} // namespace astralix::ui::dsl
