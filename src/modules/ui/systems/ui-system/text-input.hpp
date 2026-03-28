#pragma once

#include "systems/ui-system/core.hpp"
#include <string>
#include <string_view>
#include <utility>

namespace astralix::ui_system_core {

size_t text_input_index_from_pointer(const ui::UIDocument::UINode &node,
                                     const ui::UILayoutContext &context,
                                     glm::vec2 point);
void sync_text_input_scroll(const Target &target,
                            const ui::UILayoutContext &context);
void set_text_input_selection_and_caret(const Target &target, size_t anchor,
                                        size_t focus,
                                        const ui::UILayoutContext &context);
void focus_text_input(const Target &target, const ui::UILayoutContext &context,
                      bool select_all);
std::string selected_text(const ui::UIDocument::UINode &node);
std::pair<size_t, size_t> edit_range(const ui::UIDocument::UINode &node);
bool apply_text_input_value(const Target &target, std::string next_text,
                            size_t caret_index,
                            const ui::UILayoutContext &context,
                            bool queue_change);

} // namespace astralix::ui_system_core
