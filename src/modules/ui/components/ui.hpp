#pragma once

#include "base.hpp"
#include "document.hpp"

namespace astralix::rendering {

enum class UIRootSizing : uint8_t {
  MatchWindow,
};

struct UIRoot {
  Ref<ui::UIDocument> document = nullptr;
  ResourceDescriptorID default_font_id;
  float default_font_size = 16.0f;
  int sort_order = 0;
  bool input_enabled = true;
  bool visible = true;
  UIRootSizing sizing = UIRootSizing::MatchWindow;
};

} // namespace astralix::rendering
