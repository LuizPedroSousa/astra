#pragma once

#include "dsl/widgets/layout/view.hpp"

namespace astralix::ui::dsl {

inline NodeSpec spacer() {
  auto spec = view();
  spec.style(styles::grow(1.0f));
  return spec;
}

} // namespace astralix::ui::dsl
