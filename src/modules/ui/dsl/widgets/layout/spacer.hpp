#pragma once

#include "dsl/widgets/layout/view.hpp"

namespace astralix::ui::dsl {

inline NodeSpec spacer(std::string name = {}) {
  auto spec = view(std::move(name));
  spec.style(styles::grow(1.0f));
  return spec;
}

} // namespace astralix::ui::dsl
