#pragma once

#include "foundations.hpp"
#include "layout.hpp"
#include "text-metrics.hpp"

#include <optional>
#include <string_view>

namespace astralix::ui {

float clamp_dimension(
    float value,
    UILength min_value,
    UILength max_value,
    float basis,
    float rem_basis,
    float intrinsic_value
);

float resolve_length(
    UILength value,
    float basis,
    float rem_basis,
    float auto_value = 0.0f
);

float measure_label_width(
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    std::string_view text
);

float measure_line_height(
    const UIDocument::UINode &node,
    const UILayoutContext &context
);

UIRect
resolve_single_line_text_rect(const UIRect &content_bounds, float line_height);

void apply_self_clip(UIDrawCommand &command, const UIDocument::UINode &node);
void apply_content_clip(UIDrawCommand &command, const UIDocument::UINode &node);
std::optional<UIRect> child_clip_rect(const UIDocument::UINode &node);

void append_text_commands(
    UIDocument &document,
    UINodeId node_id,
    const UIRect &text_rect,
    const UIDocument::UINode &node,
    const UILayoutContext &context,
    const UIResolvedStyle &resolved,
    std::string_view text,
    glm::vec4 text_color,
    float text_scroll_x,
    bool draw_selection,
    bool draw_caret
);

} // namespace astralix::ui
