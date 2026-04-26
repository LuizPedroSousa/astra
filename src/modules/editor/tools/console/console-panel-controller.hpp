#pragma once

#include "disclosure.hpp"
#include "panels/panel-controller.hpp"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace astralix::editor {

class ConsolePanelController : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 640.0f,
      .height = 320.0f,
  };

  enum class SeverityFilter : uint8_t {
    All = 0,
    Info,
    Warning,
    Error,
    Debug,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;
  void load_state(Ref<SerializationContext> state) override;
  void save_state(Ref<SerializationContext> state) const override;

  void reset();
  void set_open(bool open);

  void set_input_value(std::string value);
  void accept_suggestion(std::string value);
  void summon_suggestions();
  void submit_command(std::string value);
  void clear_entries();

  bool follow_tail() const { return m_follow_tail; }
  void set_follow_tail(bool follow_tail);

  bool expand_all_details() const { return m_expand_all_details; }
  void set_expand_all_details(bool expand_all_details);

  float density() const { return m_density; }
  void set_density(float density);

  void toggle_severity_all();
  void toggle_severity_option(size_t index);

  bool source_filter_enabled(size_t index) const;
  void set_source_filter_enabled(size_t index, bool enabled);

  struct VisibleEntry {
    size_t source_index = 0u;
    std::string meta_text;
    std::string badge_text;
    std::string primary_text;
    std::string secondary_text;
    glm::vec4 background = glm::vec4(0.0f);
    glm::vec4 border = glm::vec4(0.0f);
    glm::vec4 meta_color = glm::vec4(1.0f);
    glm::vec4 badge_background = glm::vec4(0.0f);
    glm::vec4 badge_text_color = glm::vec4(1.0f);
    glm::vec4 primary_color = glm::vec4(1.0f);
    glm::vec4 secondary_background = glm::vec4(0.0f);
    glm::vec4 secondary_border = glm::vec4(0.0f);
    glm::vec4 secondary_color = glm::vec4(1.0f);
    bool expandable = false;
    bool expanded = false;
    float collapsed_height = 0.0f;
    float expanded_height = 0.0f;
    float width = 0.0f;
  };

private:
  void invalidate_runtime_handles();
  void update();
  void refresh(bool force = false);
  void render_visible_entries(ui::im::Children &parent);
  void toggle_row_expanded(size_t source_index);
  void clear_history_navigation();
  void navigate_history(int direction);
  void refresh_suggestions(bool open_popup);
  bool suggestions_open() const;
  std::string source_filter_summary() const;
  std::string severity_filter_summary() const;
  bool source_filter_is_default() const;
  bool severity_filter_is_default() const;
  bool severity_filter_enabled(size_t index) const;
  void close_filter_popovers();
  void toggle_source_filter_popover();
  void toggle_severity_filter_popover();
  void set_input_capture(bool captures_input);
  void scroll_to_bottom();
  void set_input_text(const std::string &value);
  float measure_text_width(float font_size, std::string_view text) const;
  float measure_line_height(float font_size) const;
  void mark_render_dirty() { ++m_render_revision; }
  static std::string text_metric_cache_key(
      uint32_t pixel_size,
      std::string_view text
  );

  ui::im::Runtime *m_runtime = nullptr;
  ui::im::WidgetId m_log_scroll_widget = ui::im::k_invalid_widget_id;
  ui::im::WidgetId m_input_widget = ui::im::k_invalid_widget_id;
  ui::im::WidgetId m_source_chip_widget = ui::im::k_invalid_widget_id;
  ui::im::WidgetId m_severity_chip_widget = ui::im::k_invalid_widget_id;
  ResourceDescriptorID m_default_font_id;
  float m_default_font_size = 16.0f;

  std::string m_input_value;
  std::vector<std::string> m_input_suggestions;
  std::string m_input_autocomplete;
  std::string m_history_navigation_draft;
  std::optional<size_t> m_history_navigation_index;
  std::optional<size_t> m_expanded_source_index;
  std::vector<VisibleEntry> m_visible_entries;
  float m_visible_entries_content_width = 0.0f;
  std::unordered_set<size_t> m_collapsed_source_indices;
  uint64_t m_entries_version = 0u;
  uint64_t m_render_revision = 1u;
  bool m_force_follow_on_next_refresh = false;
  bool m_force_scroll_to_bottom_once = false;
  bool m_source_filter_popover_open = false;
  bool m_severity_filter_popover_open = false;
  bool m_suggestions_open = false;
  bool m_request_input_focus = false;
  bool m_request_input_select_to_end = false;
  bool m_follow_tail = true;
  bool m_expand_all_details = false;
  bool m_show_log_entries = true;
  bool m_show_command_entries = true;
  bool m_show_output_entries = true;
  float m_density = 0.5f;
  bool m_severity_filter_all = true;
  bool m_severity_filter_info = false;
  bool m_severity_filter_warning = false;
  bool m_severity_filter_error = false;
  bool m_severity_filter_debug = false;
  bool m_header_frozen = false;
};

} // namespace astralix::editor
