#pragma once

#include "document.hpp"
#include <optional>

namespace astralix::ui {

void layout_document(UIDocument &document, const UILayoutContext &context);
std::optional<UIHitResult> hit_test_document(const UIDocument &document, glm::vec2 point);
void build_draw_list(UIDocument &document, const UILayoutContext &context);

} // namespace astralix::ui
