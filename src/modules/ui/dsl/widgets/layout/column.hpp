#pragma once

#include "dsl/widgets/layout/view.hpp"

namespace astralix::ui::dsl {

inline NodeSpec column() {
  auto spec = view();
  spec.style(styles::column());
  return spec;
}

} // namespace astralix::ui::dsl
