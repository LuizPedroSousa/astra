#pragma once

#include "disclosure.hpp"
#include "panels/panel-controller.hpp"
#include "virtual-list.hpp"
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
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
  ui::dsl::NodeSpec build() override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
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

private:
  void update();
  struct RowNodes {
    ui::UINodeId slot = ui::k_invalid_node_id;
    ui::UIDisclosureNodes disclosure;
    ui::UINodeId indicator = ui::k_invalid_node_id;
    ui::UINodeId meta = ui::k_invalid_node_id;
    ui::UINodeId badge = ui::k_invalid_node_id;
    ui::UINodeId primary = ui::k_invalid_node_id;
    ui::UINodeId secondary = ui::k_invalid_node_id;
  };

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

  void refresh(bool force = false);
  ui::UINodeId create_row_slot(size_t slot_index);
  void bind_row_slot(size_t slot_index, size_t visible_index);
  void sync_virtual_list(bool force);
  void toggle_row_expanded(size_t source_index);
  void clear_history_navigation();
  void navigate_history(int direction);
  void refresh_suggestions(bool open_popup);
  bool suggestions_open() const;
  void sync_filter_ui();
  void apply_filter_chip_style(
      ui::UINodeId trigger_node,
      ui::UINodeId summary_node,
      const glm::vec4 &accent,
      bool active
  );
  std::string source_filter_summary() const;
  std::string severity_filter_summary() const;
  bool source_filter_is_default() const;
  bool severity_filter_is_default() const;
  bool severity_filter_enabled(size_t index) const;
  bool popover_open(ui::UINodeId node_id) const;
  void close_filter_popovers();
  void toggle_source_filter_popover();
  void toggle_severity_filter_popover();
  void set_input_capture(bool captures_input);
  void scroll_to_bottom();
  void set_input_text(const std::string &value);
  float measure_text_width(float font_size, std::string_view text) const;
  float measure_line_height(float font_size) const;
  static std::string text_metric_cache_key(
      uint32_t pixel_size,
      std::string_view text
  );

  Ref<ui::UIDocument> m_document = nullptr;
  ui::UINodeId m_root_node = ui::k_invalid_node_id;
  ui::UINodeId m_filters_row_node = ui::k_invalid_node_id;
  ui::UINodeId m_follow_tail_toggle_node = ui::k_invalid_node_id;
  ui::UINodeId m_expand_details_toggle_node = ui::k_invalid_node_id;
  ui::UINodeId m_source_chip_trigger_node = ui::k_invalid_node_id;
  ui::UINodeId m_source_chip_summary_node = ui::k_invalid_node_id;
  ui::UINodeId m_source_popover_node = ui::k_invalid_node_id;
  std::array<ui::UINodeId, 3u> m_source_filter_option_nodes{
      ui::k_invalid_node_id,
      ui::k_invalid_node_id,
      ui::k_invalid_node_id,
  };
  ui::UINodeId m_density_node = ui::k_invalid_node_id;
  ui::UINodeId m_severity_chip_trigger_node = ui::k_invalid_node_id;
  ui::UINodeId m_severity_chip_summary_node = ui::k_invalid_node_id;
  ui::UINodeId m_severity_popover_node = ui::k_invalid_node_id;
  std::array<ui::UINodeId, 5u> m_severity_filter_option_nodes{
      ui::k_invalid_node_id,
      ui::k_invalid_node_id,
      ui::k_invalid_node_id,
      ui::k_invalid_node_id,
      ui::k_invalid_node_id,
  };
  ui::UINodeId m_severity_node = ui::k_invalid_node_id;
  ui::UINodeId m_source_filters_node = ui::k_invalid_node_id;
  ui::UINodeId m_log_scroll_node = ui::k_invalid_node_id;
  ui::UINodeId m_input_node = ui::k_invalid_node_id;
  ResourceDescriptorID m_default_font_id;
  float m_default_font_size = 16.0f;

  std::string m_input_value;
  std::string m_history_navigation_draft;
  std::optional<size_t> m_history_navigation_index;
  std::optional<size_t> m_expanded_source_index;
  std::vector<RowNodes> m_row_slots;
  std::vector<VisibleEntry> m_visible_entries;
  std::unordered_set<size_t> m_collapsed_source_indices;
  std::unique_ptr<ui::VirtualListController> m_virtual_list;
  uint64_t m_entries_version = 0u;
  bool m_force_follow_on_next_refresh = false;
  bool m_force_scroll_to_bottom_once = false;
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
  mutable std::unordered_map<uint32_t, float> m_line_height_cache;
  mutable std::unordered_map<std::string, float> m_text_width_cache;
};

} // namespace astralix::editor
