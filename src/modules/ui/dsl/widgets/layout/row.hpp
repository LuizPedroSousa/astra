#pragma once

#include "dsl/widgets/layout/view.hpp"

namespace astralix::ui::dsl {

inline NodeSpec row(std::string name = {}) {
  auto spec = view(std::move(name));
  spec.style(styles::row());
  return spec;
}

} // namespace astralix::ui::dsl
