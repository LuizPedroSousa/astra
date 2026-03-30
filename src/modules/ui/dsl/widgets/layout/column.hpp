#pragma once

#include "dsl/widgets/layout/view.hpp"

namespace astralix::ui::dsl {

inline NodeSpec column(std::string name = {}) {
  auto spec = view(std::move(name));
  spec.style(styles::column());
  return spec;
}

} // namespace astralix::ui::dsl
