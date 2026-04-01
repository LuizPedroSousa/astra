#pragma once

#include "dsl/widgets/layout/view.hpp"

namespace astralix::ui::dsl {

inline NodeSpec row() {
  auto spec = view();
  spec.style(styles::row());
  return spec;
}

} // namespace astralix::ui::dsl
