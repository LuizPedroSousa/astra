#pragma once

#include "immediate.hpp"
#include "panels/panel-controller.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace astralix::editor {

class ContextToolboxPanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 48.0f,
      .height = 200.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;
  void load_state(Ref<SerializationContext> state) override;
  void save_state(Ref<SerializationContext> state) const override;

private:
  void sync_hover_state();
  void set_hovered_tool(std::optional<std::string> tool_id);

  ui::im::Runtime *m_runtime = nullptr;
  ResourceDescriptorID m_default_font_id;
  float m_default_font_size = 16.0f;
  std::unordered_map<std::string, ui::im::WidgetId> m_tool_widget_ids;
  std::optional<std::string> m_hovered_tool_id;
  uint64_t m_render_revision = 1u;
};

} // namespace astralix::editor
