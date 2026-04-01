#include "layouts/layout-registry.hpp"

namespace astralix::editor {

bool LayoutRegistry::register_template(LayoutTemplate layout) {
  if (layout.id.empty()) {
    return false;
  }

  if (find(layout.id) != nullptr) {
    return false;
  }

  m_layouts.push_back(std::move(layout));
  return true;
}

const LayoutTemplate *LayoutRegistry::find(std::string_view id) const {
  for (const auto &layout : m_layouts) {
    if (layout.id == id) {
      return &layout;
    }
  }

  return nullptr;
}

} // namespace astralix::editor
