#pragma once

#include "astralix/modules/ui/document.hpp"
#include <glm/glm.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

using namespace astralix;

struct ConsoleRowNodes {
  ui::UINodeId row = ui::k_invalid_node_id;
  ui::UINodeId primary = ui::k_invalid_node_id;
  ui::UINodeId secondary = ui::k_invalid_node_id;
};

class ConsoleController {
public:
  enum class SeverityFilter : uint8_t {
    All = 0,
    Info,
    Warning,
    Error,
    Debug,
  };

  struct Nodes {
    Ref<ui::UIDocument> document = nullptr;
    ui::UINodeId root = ui::k_invalid_node_id;
    ui::UINodeId filters_row = ui::k_invalid_node_id;
    ui::UINodeId severity = ui::k_invalid_node_id;
    ui::UINodeId source_filters = ui::k_invalid_node_id;
    ui::UINodeId log_scroll = ui::k_invalid_node_id;
    ui::UINodeId input = ui::k_invalid_node_id;
  };

  void init(Nodes nodes);
  void reset();

  void set_open(bool open);
  void update();

  void set_input_value(std::string value);
  void submit_command(std::string value);
  void handle_input_key(const ui::UIKeyInputEvent &event);
  void clear_entries();

  bool follow_tail() const { return m_follow_tail; }
  void set_follow_tail(bool follow_tail);

  bool show_details() const { return m_show_details; }
  void set_show_details(bool show_details);

  float density() const { return m_density; }
  void set_density(float density);

  SeverityFilter severity_filter() const { return m_severity_filter; }
  size_t severity_filter_index() const;
  void set_severity_filter_index(size_t index);

  bool filters_expanded() const { return m_filters_expanded; }
  void toggle_filters_expanded();
  bool source_filter_enabled(size_t index) const;
  void set_source_filter_enabled(size_t index, bool enabled);

private:
  void refresh(bool force = false);
  void ensure_row_capacity(size_t count);
  void scroll_to_bottom();
  void set_input_text(const std::string &value);
  void navigate_history(int direction);

  Ref<ui::UIDocument> m_document = nullptr;
  ui::UINodeId m_root_node = ui::k_invalid_node_id;
  ui::UINodeId m_filters_row_node = ui::k_invalid_node_id;
  ui::UINodeId m_severity_node = ui::k_invalid_node_id;
  ui::UINodeId m_source_filters_node = ui::k_invalid_node_id;
  ui::UINodeId m_log_scroll_node = ui::k_invalid_node_id;
  ui::UINodeId m_input_node = ui::k_invalid_node_id;

  std::string m_input_value;
  std::string m_history_draft;
  std::optional<size_t> m_history_index;
  std::vector<ConsoleRowNodes> m_rows;
  uint64_t m_entries_version = 0u;
  bool m_force_follow_on_next_refresh = false;
  bool m_follow_tail = true;
  bool m_show_details = true;
  bool m_filters_expanded = true;
  bool m_show_log_entries = true;
  bool m_show_command_entries = true;
  bool m_show_output_entries = true;
  float m_density = 0.5f;
  SeverityFilter m_severity_filter = SeverityFilter::All;
};
