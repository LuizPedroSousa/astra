#pragma once

#include "base-manager.hpp"
#include "layout-node.hpp"

namespace astralix::editor {

class LayoutRegistry : public BaseManager<LayoutRegistry> {
public:
  bool register_template(LayoutTemplate layout);
  const LayoutTemplate *find(std::string_view id) const;

private:
  std::vector<LayoutTemplate> m_layouts;
};

inline Ref<LayoutRegistry> layout_registry() {
  if (LayoutRegistry::get() == nullptr) {
    LayoutRegistry::init();
  }

  return LayoutRegistry::get();
}

} // namespace astralix::editor
