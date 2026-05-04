#include "tools/context-toolbox/context-toolbox-panel-controller.hpp"

#include "context-tool-registry.hpp"
#include "dsl.hpp"
#include "editor-context-store.hpp"
#include "editor-theme.hpp"
#include "fnv1a.hpp"
#include "tools/context-toolbox/styles.hpp"

#include <algorithm>
#include <utility>

namespace astralix::editor {
namespace {

const ContextToolDefinition *find_tool_definition(
    const std::vector<ContextToolDefinition> &tools,
    std::string_view tool_id
) {
  const auto it = std::find_if(
      tools.begin(),
      tools.end(),
      [tool_id](const ContextToolDefinition &tool) {
        return tool.id == tool_id;
      }
  );
  return it != tools.end() ? &(*it) : nullptr;
}

} // namespace

void ContextToolboxPanelController::render(ui::im::Frame &ui) {
  using namespace ui::dsl::styles;

  const ContextToolboxTheme theme;
  const auto &tools = context_tool_registry()->tools_for_context(
      editor_context_store()->active_context()
  );
  const std::string_view active_tool_id = editor_context_store()->active_tool_id();

  m_tool_widget_ids.clear();

  auto root = ui.column("root").style(context_toolbox_styles::root_style(theme));
  std::string previous_group;

  for (size_t index = 0u; index < tools.size(); ++index) {
    const ContextToolDefinition &tool = tools[index];
    if (index > 0u && tool.group != previous_group) {
      root.view("group-separator-" + std::to_string(index))
          .style(context_toolbox_styles::group_separator_style(theme));
      root.spacer("group-gap-" + std::to_string(index))
          .style(height(px(6.0f)));
    }

    previous_group = tool.group;
    const bool active = tool.id == active_tool_id;

    auto button = root.pressable("tool-" + std::to_string(index))
                      .on_click([tool_id = tool.id]() {
                        editor_context_store()->set_active_tool_id(tool_id);
                      })
                      .style(context_toolbox_styles::button_style(theme, active));

    if (active) {
      button.view("active-indicator")
          .style(context_toolbox_styles::active_indicator_style(theme));
    }

    button.image("icon", tool.icon)
        .style(context_toolbox_styles::icon_style(theme, active));

    m_tool_widget_ids.insert_or_assign(tool.id, button.widget_id());
  }

  if (m_hovered_tool_id.has_value()) {
    const auto *hovered_tool = find_tool_definition(tools, *m_hovered_tool_id);
    const auto hovered_widget = m_tool_widget_ids.find(*m_hovered_tool_id);
    if (hovered_tool != nullptr && hovered_widget != m_tool_widget_ids.end()) {
      auto tooltip =
          static_cast<ui::im::Children &>(root).popover("tooltip").popover(
              ui::im::PopoverState{
                  .open = true,
                  .anchor_widget_id = hovered_widget->second,
                  .placement = ui::UIPopupPlacement::RightStart,
                  .depth = 0u,
                  .close_on_outside_click = false,
                  .close_on_escape = false,
              }
          );
      tooltip.style(context_toolbox_styles::tooltip_style(theme));

      auto row = tooltip.row("tooltip-row").style(items_center().gap(8.0f));
      row.text("label", hovered_tool->tooltip)
          .style(
              font_id(m_default_font_id)
                  .font_size(std::max(12.0f, m_default_font_size * 0.74f))
                  .text_color(theme.tooltip_text)
          );

      if (hovered_tool->shortcut_label.has_value()) {
        row.text("shortcut", *hovered_tool->shortcut_label)
            .style(
                context_toolbox_styles::shortcut_badge_style(theme)
                    .font_id(m_default_font_id)
                    .font_size(std::max(11.0f, m_default_font_size * 0.68f))
            );
      }
    }
  }
}

void ContextToolboxPanelController::mount(const PanelMountContext &context) {
  m_runtime = context.runtime;
  m_default_font_id = context.default_font_id;
  m_default_font_size = context.default_font_size;
  m_tool_widget_ids.clear();
  m_hovered_tool_id.reset();
}

void ContextToolboxPanelController::unmount() {
  m_runtime = nullptr;
  m_tool_widget_ids.clear();
  m_hovered_tool_id.reset();
}

void ContextToolboxPanelController::update(const PanelUpdateContext &context) {
  if (auto *provider = context_tool_registry()->find_provider(
          editor_context_store()->active_context()
      );
      provider != nullptr) {
    provider->sync(context.dt);
  }

  sync_hover_state();
}

std::optional<uint64_t> ContextToolboxPanelController::render_version() const {
  uint64_t hash = k_fnv1a64_offset_basis;
  hash = fnv1a64_append_value(hash, editor_context_store()->revision());
  hash = fnv1a64_append_value(hash, m_render_revision);
  return hash;
}

void ContextToolboxPanelController::load_state(Ref<SerializationContext>) {}

void ContextToolboxPanelController::save_state(Ref<SerializationContext>) const {}

void ContextToolboxPanelController::sync_hover_state() {
  if (m_runtime == nullptr || m_runtime->document() == nullptr ||
      m_tool_widget_ids.empty()) {
    set_hovered_tool(std::nullopt);
    return;
  }

  const ui::UINodeId hot_node = m_runtime->document()->hot_node();
  for (const auto &[tool_id, widget_id] : m_tool_widget_ids) {
    if (m_runtime->node_id_for(widget_id) == hot_node) {
      set_hovered_tool(tool_id);
      return;
    }
  }

  set_hovered_tool(std::nullopt);
}

void ContextToolboxPanelController::set_hovered_tool(
    std::optional<std::string> tool_id
) {
  if (m_hovered_tool_id == tool_id) {
    return;
  }

  m_hovered_tool_id = std::move(tool_id);
  ++m_render_revision;
}

} // namespace astralix::editor
