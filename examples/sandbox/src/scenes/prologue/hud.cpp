#include "hud.hpp"
#include "prologue.hpp"

#include "astralix/modules/ui/document.hpp"
#include "astralix/modules/ui/dsl.hpp"
#include "astralix/modules/ui/types.hpp"

using namespace astralix;
using namespace ui::dsl;
using namespace ui::dsl::styles;

namespace {

constexpr const char *fps_text_label = "FPS: --";
constexpr const char *entities_count_text_label = "Entities Count: --";
constexpr const char *bodies_text_label = "Bodies: --";
constexpr const char *controls_text_label =
    "F5 reset scene | F6 pause physics | ` open console";

NodeSpec make_label(const char *name, const char *value, glm::vec4 color, ui::UINodeId *target = nullptr) {
  auto spec = text(value, name).style(text_color(color), font_size(18.0f));
  if (target != nullptr) {
    spec.bind(*target);
  }

  return spec;
}

NodeSpec build_stats_panel(Prologue *scene, HudDocumentState &state) {
  return column("panel")
      .style(
          panel()
              .draggable()
              .padding(18.0f)
              .gap(10.0f)
              .radius(14.0f)
              .background(rgba(0.03f, 0.08f, 0.14f, 0.84f))
              .border(1.0f, rgba(0.43f, 0.64f, 0.86f, 0.35f))
              .items_start()
      )
      .children(
          column("stats_handle")
              .style(
                  items_start()
                      .gap(10.0f)
                      .drag_handle()
              )
              .children(
                  make_label("fps", fps_text_label, rgba(0.85f, 1.0f, 0.8f, 1.0f), &state.fps),
                  make_label("entities", entities_count_text_label, rgba(0.85f, 1.0f, 0.8f, 1.0f), &state.entities),
                  make_label("bodies", bodies_text_label, rgba(0.85f, 1.0f, 0.8f, 1.0f), &state.bodies),
                  make_label("controls", controls_text_label, rgba(0.9f, 0.92f, 1.0f, 1.0f))
              ),
          row("hud_actions")
              .style(
                  gap(5.0f)
              )
              .children(
                  button(
                      "Spawn Cube", [scene]() { scene->request_spawn_cube(); }, "spawn"
                  ),
                  button(
                      "Reset Scene",
                      [scene]() { scene->request_reset_scene(); },
                      "reset_scene"
                  ),
                  button(
                      "Toggle resizable split",
                      [scene]() { scene->request_toggle_resizable_split_view(); },
                      "reset_scene"
                  )
              )
      );
}

NodeSpec build_split_demo(HudDocumentState &state) {
  return column("split_demo")
      .bind(state.resizable_split_demo)
      .style(
          absolute()
              .draggable()
              .top(px(24.0f))
              .right(px(24.0f))
              .width(px(320.0f))
              .height(px(220.0f))
              .padding(12.0f)
              .gap(10.0f)
              .radius(14.0f)
              .background(rgba(0.03f, 0.08f, 0.14f, 0.88f))
              .border(1.0f, rgba(0.42f, 0.63f, 0.84f, 0.38f))
              .resizable_all()
              .handle(8.0f)
              .corner(18.0f)
      )
      .visible(false)
      .children(
          row("split_demo_header")
              .style(
                  fill_x()
                      .items_center()
                      .drag_handle()
              )
              .children(
                  text("Resizable Split View", "split_demo_title")
                      .style(
                          font_size(16.0f)
                              .text_color(rgba(0.88f, 0.96f, 1.0f, 1.0f))
                      )
              ),
          row("split_demo_body")
              .style(
                  flex(1.0f)
                      .gap(0.0f)
                      .radius(12.0f)
                      .overflow_hidden()
                      .background(rgba(0.01f, 0.04f, 0.08f, 0.72f))
              )
              .children(
                  column("split_left")
                      .style(
                          width(px(120.0f))
                              .padding(12.0f)
                              .gap(6.0f)
                              .background(rgba(0.08f, 0.14f, 0.22f, 0.94f))
                      )
                      .children(
                          text("Left Pane", "split_left_title"),
                          text("Drag the divider or panel edges.", "split_left_body")
                      ),
                  splitter("vertical_splitter"),
                  column("split_right")
                      .style(
                          flex(1.0f)
                              .gap(0.0f)
                              .background(rgba(0.04f, 0.09f, 0.16f, 0.9f))
                      )
                      .children(
                          column("split_top")
                              .style(
                                  height(px(74.0f))
                                      .padding(12.0f)
                                      .gap(6.0f)
                                      .background(rgba(0.08f, 0.16f, 0.27f, 0.94f))
                              )
                              .children(
                                  text("Top Pane", "split_top_title"),
                                  text("Nested splitter below.", "split_top_body")
                              ),
                          splitter("horizontal_splitter"),
                          column("split_bottom")
                              .style(
                                  flex(1.0f)
                                      .padding(12.0f)
                                      .gap(6.0f)
                                      .background(
                                          rgba(0.06f, 0.12f, 0.2f, 0.94f)
                                      )
                              )
                              .children(
                                  text("Bottom Pane", "split_bottom_title"),
                                  text("Pixel-based splitter sizing.", "split_bottom_body")
                              )
                      )
              )
      );
}

NodeSpec build_console_panel(Prologue *scene, HudDocumentState &state) {
  return column("console_root")
      .bind(state.console_root)
      .style(
          flex(1.0f)
              .absolute()
              .right(px(0.0f))
              .bottom(px(0.0f))
              .padding(14.0f)
              .gap(12.0f)
              .height(px(400.0f))
              .radius(14.0f)
              .background(rgba(0.02f, 0.05f, 0.09f, 0.92f))
              .border(1.0f, rgba(0.4f, 0.57f, 0.76f, 0.4f))
              .resizable_all()
              .handle(8.0f)
              .corner(20.0f)
      )
      .visible(false)
      .children(
          row("console_header")
              .style(
                  fill_x()
                      .items_center()
                      .gap(10.0f)
                      .drag_handle()
              )
              .children(
                  text("Console", "console_title")
                      .style(
                          font_size(18.0f)
                              .text_color(
                                  rgba(0.9f, 0.97f, 1.0f, 1.0f)
                              )
                      ),
                  spacer("console_spacer"),
                  text("Type help. Esc closes the console.", "console_hint")
                      .style(
                          font_size(14.0f)
                              .text_color(
                                  rgba(0.67f, 0.76f, 0.86f, 0.95f)
                              )
                      ),
                  icon_button(
                      "icons::adjust",
                      [scene]() { scene->console().toggle_filters_expanded(); },
                      "console_filters_toggle"
                  ),
                  icon_button(
                      "icons::clear",
                      [scene]() { scene->console().clear_entries(); },
                      "console_clear"
                  )
              ),
          row("console_settings")
              .bind(state.console_settings)
              .style(
                  fill_x()
                      .items_center()
                      .gap(14.0f)
              )
              .children(
                  checkbox(
                      "Follow tail",
                      scene->console().follow_tail(),
                      "console_follow_tail"
                  )
                      .style(
                          accent_color(rgba(0.58f, 0.79f, 1.0f, 1.0f))
                              .control_gap(8.0f)
                              .padding_xy(0.0f, 4.0f)
                      )
                      .on_toggle([scene](bool checked) {
                        scene->console().set_follow_tail(checked);
                      }),
                  checkbox("Show details", scene->console().show_details(), "console_show_details")
                      .style(
                          accent_color(rgba(0.58f, 0.79f, 1.0f, 1.0f))
                              .control_gap(8.0f)
                              .padding_xy(0.0f, 4.0f)
                      )
                      .on_toggle([scene](bool checked) {
                        scene->console().set_show_details(checked);
                      }),
                  spacer("console_settings_spacer"),
                  text("Severity", "console_severity_label")
                      .style(
                          font_size(14.0f)
                              .text_color(
                                  rgba(0.67f, 0.76f, 0.86f, 0.95f)
                              )
                      ),
                  segmented_control({"All", "Info", "Warn", "Error", "Debug"}, scene->console().severity_filter_index(), "console_severity")
                      .bind(state.console_severity)
                      .style(
                          padding(4.0f)
                              .accent_color(
                                  rgba(0.58f, 0.79f, 1.0f, 1.0f)
                              )
                      )
                      .on_select([scene](size_t index, const std::string &) {
                        scene->console().set_severity_filter_index(index);
                      }),
                  text("Source", "console_source_label")
                      .style(
                          font_size(14.0f)
                              .text_color(
                                  rgba(0.67f, 0.76f, 0.86f, 0.95f)
                              )
                      ),
                  chip_group({"Logs", "Commands", "Output"}, {scene->console().source_filter_enabled(0u), scene->console().source_filter_enabled(1u), scene->console().source_filter_enabled(2u)}, "console_sources")
                      .bind(state.console_sources)
                      .style(
                          accent_color(rgba(0.58f, 0.79f, 1.0f, 1.0f))
                      )
                      .on_chip_toggle([scene](size_t index, const std::string &, bool enabled) {
                        scene->console().set_source_filter_enabled(index, enabled);
                      }),
                  text("Density", "console_density_label")
                      .style(
                          font_size(14.0f)
                              .text_color(
                                  rgba(0.67f, 0.76f, 0.86f, 0.95f)
                              )
                      ),
                  slider(scene->console().density(), 0.0f, 1.0f, "console_density")
                      .step(0.1f)
                      .style(
                          width(px(168.0f))
                              .accent_color(rgba(0.58f, 0.79f, 1.0f, 1.0f))
                              .slider_track_thickness(6.0f)
                              .slider_thumb_radius(8.0f)
                      )
                      .on_value_change([scene](float value) {
                        scene->console().set_density(value);
                      })
              ),

          scroll_view("console_log_scroll")
              .bind(state.console_log_scroll)
              .style(
                  fill_x()
                      .flex(1.0f)
                      .padding(10.0f)
                      .gap(8.0f)
                      .radius(12.0f)
                      .border(1.0f, rgba(0.28f, 0.42f, 0.58f, 0.3f))
                      .background(rgba(0.01f, 0.04f, 0.08f, 0.88f))
                      .scroll_both()
                      .scrollbar_auto()
              ),

          text_input({}, "Type a command and press Enter", "console_input")
              .bind(state.console_input)
              .style(
                  fill_x()
                      .height(px(40.0f))
                      .padding_xy(12.0f, 10.0f)
                      .background(rgba(0.01f, 0.04f, 0.08f, 0.9f))
                      .border(1.0f, rgba(0.28f, 0.42f, 0.58f, 0.3f))
                      .focused(ui::dsl::styles::state().border(2.0f, rgba(0.58f, 0.79f, 1.0f, 1.0f)))
              )
              .on_change([scene](const std::string &value) {
                scene->console().set_input_value(value);
              })
              .on_submit([scene](const std::string &value) {
                scene->console().submit_command(value);
              })
              .on_key_input([scene](const ui::UIKeyInputEvent &event) {
                scene->console().handle_input_key(event);
              })
      );
}

} // namespace

HudDocumentState build_hud_document(Prologue *scene) {
  HudDocumentState state;
  state.document = ui::UIDocument::create();

  mount(
      *state.document,
      row("root")
          .style(
              fill()
                  .padding(24.0f)
                  .gap(18.0f)
                  .items_end()
                  .justify_start()
                  .relative()
          )
          .children(
              build_stats_panel(scene, state),
              build_split_demo(state),
              build_console_panel(scene, state)
          )
  );

  return state;
}
