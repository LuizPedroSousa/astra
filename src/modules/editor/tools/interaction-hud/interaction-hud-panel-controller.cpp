#include "tools/interaction-hud/interaction-hud-panel-controller.hpp"

#include "dsl.hpp"
#include "editor-theme.hpp"
#include "editor-viewport-hud-store.hpp"

namespace astralix::editor {
namespace {

using namespace ui::dsl::styles;

bool hud_snapshot_visible(const EditorViewportHudSnapshot &snapshot) {
  return !snapshot.title.empty() || !snapshot.target.empty() ||
         snapshot.detail_line.has_value() || !snapshot.chips.empty() ||
         !snapshot.hints.empty() || snapshot.transient_message.has_value();
}

std::string joined_hints(const std::vector<std::string> &hints) {
  std::string result;
  for (size_t index = 0u; index < hints.size(); ++index) {
    if (index > 0u) {
      result += " · ";
    }
    result += hints[index];
  }
  return result;
}

ui::dsl::StyleBuilder hud_card_style(
    const ViewportPanelTheme &viewport_theme
) {
  return fill_x()
      .padding(12.0f)
      .gap(8.0f)
      .radius(14.0f)
      .background(viewport_theme.hud_background)
      .border(1.0f, viewport_theme.hud_border);
}

ui::dsl::StyleBuilder target_chip_style(
    const ViewportPanelTheme &theme
) {
  return items_center()
      .padding_xy(10.0f, 5.0f)
      .radius(8.0f)
      .background(theme.hud_chip_background)
      .border(1.0f, theme.hud_chip_border);
}

ui::dsl::StyleBuilder transient_chip_style(
    const ViewportPanelTheme &theme
) {
  return items_center()
      .padding_xy(10.0f, 5.0f)
      .radius(8.0f)
      .background(theme.hud_transient_background)
      .border(1.0f, theme.hud_transient_border);
}

ui::dsl::StyleBuilder hud_chip_style(
    const ViewportPanelTheme &theme,
    EditorViewportHudTone tone
) {
  glm::vec4 background = theme.hud_chip_background;
  glm::vec4 border = theme.hud_chip_border;
  glm::vec4 text = theme.hud_chip_text;

  switch (tone) {
    case EditorViewportHudTone::Accent:
      background = theme.hud_chip_accent_background;
      border = theme.hud_chip_accent_border;
      text = theme.hud_chip_accent_text;
      break;
    case EditorViewportHudTone::Success:
      background = theme.hud_chip_success_background;
      border = theme.hud_chip_success_border;
      text = theme.hud_chip_success_text;
      break;
    case EditorViewportHudTone::Neutral:
    default:
      break;
  }

  return items_center()
      .padding_xy(8.0f, 4.0f)
      .radius(7.0f)
      .background(background)
      .border(1.0f, border)
      .text_color(text);
}

} // namespace

void InteractionHudPanelController::render(ui::im::Frame &ui) {
  const WorkspaceShellTheme shell_theme;
  const ViewportPanelTheme viewport_theme;
  const auto &snapshot = editor_viewport_hud_store()->snapshot();

  auto root = ui.column("root").style(
      fill()
          .padding_bottom(12.0f)
          .gap(10.0f)
  );

  if (
      auto empty = root.column("empty").style(
                                           fill()
                                               .justify_center()
                                               .items_center()
                                               .padding(12.0f)
                                               .gap(6.0f)
                                               .radius(14.0f)
                                               .background(shell_theme.panel_background)
                                               .border(1.0f, shell_theme.panel_border)
      )
                       .visible(!hud_snapshot_visible(snapshot))
  ) {
    empty.text("title", "No Active Interaction")
        .style(font_size(15.0f).text_color(shell_theme.text_primary));
    empty.text(
             "body",
             "Transform and navigation status will appear here."
    )
        .style(font_size(12.0f).text_color(shell_theme.text_muted));
    return;
  }

  auto card = root.column("card").style(
      hud_card_style(viewport_theme)
  );

  if (snapshot.transient_message.has_value()) {
    auto transient =
        card.row("transient").style(transient_chip_style(viewport_theme));
    transient.text("label", *snapshot.transient_message)
        .style(font_size(11.0f).text_color(viewport_theme.hud_transient_text));
  }

  auto header = card.row("header").style(fill_x().items_center().gap(8.0f));
  header.text(
            "title",
            snapshot.title.empty() ? std::string("Interaction") : snapshot.title
  )
      .style(font_size(14.0f).text_color(viewport_theme.hud_text_primary));

  if (!snapshot.target.empty()) {
    header.spacer("spacer");
    auto target = header.row("target").style(target_chip_style(viewport_theme));
    target.text("label", snapshot.target)
        .style(font_size(11.0f).text_color(viewport_theme.hud_text_muted));
  }

  if (snapshot.detail_line.has_value()) {
    card.text("detail", *snapshot.detail_line)
        .style(
            font_id(viewport_theme.hud_mono_font)
                .font_size(11.5f)
                .text_color(viewport_theme.hud_detail_text)
        );
  }

  if (!snapshot.chips.empty()) {
    auto chips = card.row("chips").style(items_center().gap(6.0f));
    for (size_t index = 0u; index < snapshot.chips.size(); ++index) {
      const auto &chip = snapshot.chips[index];
      auto chip_node =
          chips.row("chip_" + std::to_string(index))
              .style(hud_chip_style(viewport_theme, chip.tone));
      chip_node.text("label", chip.label).style(font_size(10.5f));
    }
  }

  if (!snapshot.hints.empty()) {
    card.text("hints", joined_hints(snapshot.hints))
        .style(
            font_size(10.5f)
                .text_color(viewport_theme.hud_text_muted)
        );
  }
}

void InteractionHudPanelController::update(const PanelUpdateContext &context) {
  editor_viewport_hud_store()->advance(context.dt);
}

std::optional<uint64_t> InteractionHudPanelController::render_version() const {
  return editor_viewport_hud_store()->revision();
}

} // namespace astralix::editor
