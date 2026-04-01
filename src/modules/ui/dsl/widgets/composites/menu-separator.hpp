#pragma once

#include "dsl/core.hpp"
#include "dsl/widgets/layout/view.hpp"

namespace astralix::ui::dsl {

inline NodeSpec menu_separator() {
  using namespace styles;

  return view().style(
      fill_x(),
      height(px(1.0f)),
      background(rgba(0.396f, 0.420f, 0.522f, 0.55f))
  );
}

} // namespace astralix::ui::dsl
