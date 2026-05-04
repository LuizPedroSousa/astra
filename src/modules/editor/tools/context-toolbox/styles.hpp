#pragma once

#include "dsl.hpp"
#include "editor-theme.hpp"

namespace astralix::editor::context_toolbox_styles {

ui::dsl::StyleBuilder root_style(const ContextToolboxTheme &theme);
ui::dsl::StyleBuilder button_style(
    const ContextToolboxTheme &theme,
    bool active
);
ui::dsl::StyleBuilder active_indicator_style(
    const ContextToolboxTheme &theme
);
ui::dsl::StyleBuilder icon_style(
    const ContextToolboxTheme &theme,
    bool active
);
ui::dsl::StyleBuilder group_separator_style(
    const ContextToolboxTheme &theme
);
ui::dsl::StyleBuilder tooltip_style(const ContextToolboxTheme &theme);
ui::dsl::StyleBuilder shortcut_badge_style(
    const ContextToolboxTheme &theme
);

} // namespace astralix::editor::context_toolbox_styles
