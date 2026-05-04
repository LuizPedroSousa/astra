#pragma once

#include "panels/panel-controller.hpp"
#include "types.hpp"
#include "unordered_map"
#include "widgets/graph-view.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace astralix {
struct MaterialDescriptor;
}

namespace astralix::editor {

enum class ShadingMode : uint8_t {
  Object = 0u,
  World = 1u,
};

class ShadingPanelController final : public PanelController {
public:
  static constexpr PanelMinimumSize kMinimumSize{
      .width = 520.0f,
      .height = 360.0f,
  };

  PanelMinimumSize minimum_size() const override { return kMinimumSize; }
  void render(ui::im::Frame &ui) override;
  void mount(const PanelMountContext &context) override;
  void unmount() override;
  void update(const PanelUpdateContext &context) override;
  std::optional<uint64_t> render_version() const override;
  void load_state(Ref<SerializationContext> state) override;
  void save_state(Ref<SerializationContext> state) const override;

  struct MaterialSlotSnapshot {
    std::string material_id;
    std::string label;
    bool loaded = false;
    std::optional<std::string> base_color_id;
    std::optional<std::string> normal_id;
    std::optional<std::string> metallic_id;
    std::optional<std::string> roughness_id;
    std::optional<std::string> metallic_roughness_id;
    std::optional<std::string> occlusion_id;
    std::optional<std::string> emissive_id;
    std::optional<std::string> displacement_id;
    glm::vec4 base_color_factor = glm::vec4(1.0f);
    glm::vec3 emissive_factor = glm::vec3(0.0f);
    float metallic_factor = 1.0f;
    float roughness_factor = 1.0f;
    float occlusion_strength = 1.0f;
    float normal_scale = 1.0f;
    float height_scale = 0.02f;
    float bloom_intensity = 0.0f;
    bool alpha_mask = false;
    float alpha_cutoff = 0.5f;
    bool double_sided = false;
    std::optional<std::string> shader_id;
  };

  struct Snapshot {
    bool has_scene = false;
    bool has_entity = false;
    std::string entity_name;
    std::vector<MaterialSlotSnapshot> material_slots;
    size_t active_slot_index = 0u;
    ui::UIGraphViewModel graph;
  };

private:
  void refresh(bool force = false);
  void rebuild_graph();
  void render_object_mode(ui::im::Frame &ui, ui::im::Children &root);
  void render_world_mode(ui::im::Frame &ui, ui::im::Children &root);
  void render_material_properties(ui::im::Children &parent);
  void apply_material_change(std::function<void(astralix::MaterialDescriptor &)> mutator);
  void mark_render_dirty() { ++m_render_revision; }

  ui::im::Runtime *m_runtime = nullptr;
  ResourceDescriptorID m_default_font_id = "fonts::roboto";
  float m_default_font_size = 16.0f;
  ui::im::WidgetId m_graph_widget = ui::im::k_invalid_widget_id;
  Snapshot m_snapshot;
  ShadingMode m_mode = ShadingMode::Object;
  std::optional<ui::UIGraphId> m_selected_node_id;
  std::unordered_map<ui::UIGraphId, glm::vec2> m_saved_node_positions;
  uint64_t m_last_selection_revision = 0u;
  uint64_t m_render_revision = 1u;
  std::optional<ui::UIViewTransform2D> m_view_transform =
      ui::UIViewTransform2D{
          .pan = glm::vec2(20.0f, 24.0f),
          .zoom = 0.70f,
          .min_zoom = 0.30f,
          .max_zoom = 2.50f,
      };
};

} // namespace astralix::editor
