#include "tools/shading/shading-panel-controller.hpp"

#include "components/material.hpp"
#include "components/model.hpp"
#include "dsl.hpp"
#include "editor-selection-store.hpp"
#include "editor-theme.hpp"
#include "fnv1a.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "managers/system-manager.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "resources/descriptors/model-descriptor.hpp"
#include "resources/model.hpp"
#include "serialization-context-readers.hpp"
#include "systems/render-system/render-system.hpp"
#include "widgets/graph-layout.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace astralix::editor {
namespace {

using namespace ui::dsl::styles;

const ShadingPanelTheme &shading_panel_theme() {
  static const ShadingPanelTheme theme{};
  return theme;
}

std::string compact_id(std::string_view value) {
  const size_t separator = value.rfind("::");
  if (separator == std::string_view::npos || separator + 2u >= value.size()) {
    return std::string(value);
  }
  return std::string(value.substr(separator + 2u));
}

uint64_t hash_graph_key(std::string_view category, std::string_view key) {
  uint64_t hash = fnv1a64_append_string(category);
  hash = fnv1a64_append_string(":", hash);
  return fnv1a64_append_string(key, hash);
}

uint64_t hash_graph_key(
    std::string_view category,
    std::string_view key_a,
    std::string_view key_b
) {
  uint64_t hash = hash_graph_key(category, key_a);
  hash = fnv1a64_append_string(":", hash);
  return fnv1a64_append_string(key_b, hash);
}

std::optional<ui::UIGraphId> parse_graph_id(std::string_view value) {
  ui::UIGraphId parsed = 0u;
  const auto *first = value.data();
  const auto *last = value.data() + value.size();
  const auto result = std::from_chars(first, last, parsed);
  if (result.ec != std::errc{} || result.ptr != last) {
    return std::nullopt;
  }
  return parsed;
}

ui::UIGraphDagLayoutOptions shading_graph_layout_options() {
  return ui::UIGraphDagLayoutOptions{
      .origin = glm::vec2(72.0f, 72.0f),
      .column_spacing = 340.0f,
      .row_spacing = 120.0f,
      .ordering_iterations = 6u,
      .collision_padding = 30.0f,
  };
}

ShadingPanelController::MaterialSlotSnapshot collect_material_slot(
    const ResourceDescriptorID &material_id,
    std::string label
) {
  ShadingPanelController::MaterialSlotSnapshot slot;
  slot.material_id = material_id;
  slot.label = std::move(label);

  auto manager = resource_manager();
  if (manager == nullptr) {
    return slot;
  }

  auto descriptor = manager->get_by_descriptor_id<MaterialDescriptor>(material_id);
  if (descriptor == nullptr) {
    return slot;
  }

  slot.loaded = true;
  slot.base_color_id = descriptor->base_color_id;
  slot.normal_id = descriptor->normal_id;
  slot.metallic_id = descriptor->metallic_id;
  slot.roughness_id = descriptor->roughness_id;
  slot.metallic_roughness_id = descriptor->metallic_roughness_id;
  slot.occlusion_id = descriptor->occlusion_id;
  slot.emissive_id = descriptor->emissive_id;
  slot.displacement_id = descriptor->displacement_id;
  slot.base_color_factor = descriptor->base_color_factor;
  slot.emissive_factor = descriptor->emissive_factor;
  slot.metallic_factor = descriptor->metallic_factor;
  slot.roughness_factor = descriptor->roughness_factor;
  slot.occlusion_strength = descriptor->occlusion_strength;
  slot.normal_scale = descriptor->normal_scale;
  slot.height_scale = descriptor->height_scale;
  slot.bloom_intensity = descriptor->bloom_intensity;
  slot.alpha_mask = descriptor->alpha_mask;
  slot.alpha_cutoff = descriptor->alpha_cutoff;
  slot.double_sided = descriptor->double_sided;

  return slot;
}

struct TextureSlotInfo {
  std::string_view name;
  std::string_view port_label;
  const std::optional<std::string> &descriptor_id;
  glm::vec4 color;
};

ui::UIGraphId principled_node_id_for(const std::string &material_id) {
  return hash_graph_key("principled-node", material_id);
}

void build_material_graph(
    const ShadingPanelController::MaterialSlotSnapshot &slot,
    ui::UIGraphViewModel &graph
) {
  const auto &theme = shading_panel_theme();

  const ui::UIGraphId output_node_id = hash_graph_key("output-node", slot.material_id);
  const ui::UIGraphId principled_node_id = principled_node_id_for(slot.material_id);

  std::vector<ui::UIGraphId> principled_input_ports;
  std::vector<ui::UIGraphId> principled_output_ports;
  std::vector<ui::UIGraphId> output_input_ports;

  const ui::UIGraphId principled_surface_out =
      hash_graph_key("principled-port-surface-out", slot.material_id);
  principled_output_ports.push_back(principled_surface_out);
  graph.ports.push_back(ui::UIGraphPort{
      .id = principled_surface_out,
      .node_id = principled_node_id,
      .direction = ui::UIGraphPortDirection::Output,
      .label = "BSDF",
      .color = theme.accent,
  });

  const ui::UIGraphId output_surface_in =
      hash_graph_key("output-port-surface-in", slot.material_id);
  output_input_ports.push_back(output_surface_in);
  graph.ports.push_back(ui::UIGraphPort{
      .id = output_surface_in,
      .node_id = output_node_id,
      .direction = ui::UIGraphPortDirection::Input,
      .label = "Surface",
      .color = theme.accent,
  });

  graph.edges.push_back(ui::UIGraphEdge{
      .id = hash_graph_key("edge-principled-to-output", slot.material_id),
      .from_port_id = principled_surface_out,
      .to_port_id = output_surface_in,
      .color = theme.accent,
      .thickness = 2.2f,
  });

  const TextureSlotInfo texture_slots[] = {
      {"base_color", "Base Color", slot.base_color_id, theme.accent},
      {"metallic", "Metallic", slot.metallic_id, theme.resource_port},
      {"roughness", "Roughness", slot.roughness_id, theme.resource_port},
      {"metallic_roughness", "Metallic Roughness", slot.metallic_roughness_id, theme.resource_port},
      {"normal", "Normal", slot.normal_id, glm::vec4(0.514f, 0.545f, 0.878f, 1.0f)},
      {"occlusion", "Occlusion", slot.occlusion_id, theme.resource_port},
      {"emissive", "Emissive", slot.emissive_id, theme.material_edge},
      {"displacement", "Displacement", slot.displacement_id, theme.resource_port},
  };

  for (const auto &texture_slot : texture_slots) {
    const ui::UIGraphId principled_input_port = hash_graph_key(
        "principled-port-in", slot.material_id, texture_slot.name
    );
    principled_input_ports.push_back(principled_input_port);
    graph.ports.push_back(ui::UIGraphPort{
        .id = principled_input_port,
        .node_id = principled_node_id,
        .direction = ui::UIGraphPortDirection::Input,
        .label = std::string(texture_slot.port_label),
        .color = texture_slot.color,
    });

    if (!texture_slot.descriptor_id.has_value()) {
      continue;
    }

    const std::string &texture_id = *texture_slot.descriptor_id;
    const ui::UIGraphId texture_node_id = hash_graph_key(
        "texture-node", slot.material_id, texture_slot.name
    );
    const ui::UIGraphId texture_output_port = hash_graph_key(
        "texture-port-out", slot.material_id, texture_slot.name
    );

    graph.nodes.push_back(ui::UIGraphNode{
        .id = texture_node_id,
        .title = compact_id(texture_id),
        .size_hint = glm::vec2(220.0f, 0.0f),
        .output_ports = {texture_output_port},
    });
    graph.ports.push_back(ui::UIGraphPort{
        .id = texture_output_port,
        .node_id = texture_node_id,
        .direction = ui::UIGraphPortDirection::Output,
        .label = "Color",
        .color = texture_slot.color,
    });
    graph.edges.push_back(ui::UIGraphEdge{
        .id = hash_graph_key("edge-texture", slot.material_id, texture_slot.name),
        .from_port_id = texture_output_port,
        .to_port_id = principled_input_port,
        .color = texture_slot.color,
        .thickness = 2.0f,
    });
  }

  graph.nodes.push_back(ui::UIGraphNode{
      .id = principled_node_id,
      .title = "Principled BSDF",
      .size_hint = glm::vec2(280.0f, 0.0f),
      .input_ports = std::move(principled_input_ports),
      .output_ports = std::move(principled_output_ports),
  });

  graph.nodes.push_back(ui::UIGraphNode{
      .id = output_node_id,
      .title = "Material Output",
      .size_hint = glm::vec2(220.0f, 0.0f),
      .input_ports = std::move(output_input_ports),
  });
}

} // namespace

void ShadingPanelController::mount(const PanelMountContext &context) {
  m_runtime = context.runtime;
  m_default_font_id = context.default_font_id;
  m_default_font_size = context.default_font_size;
  m_last_selection_revision = editor_selection_store()->revision();
  refresh(true);
}

void ShadingPanelController::unmount() {
  m_runtime = nullptr;
  m_graph_widget = ui::im::k_invalid_widget_id;
}

void ShadingPanelController::update(const PanelUpdateContext &) { refresh(); }

std::optional<uint64_t> ShadingPanelController::render_version() const {
  return m_render_revision;
}

void ShadingPanelController::load_state(Ref<SerializationContext> state) {
  if (state == nullptr) {
    return;
  }

  m_saved_node_positions.clear();

  const auto zoom = serialization::context::read_float((*state)["view_transform"]["zoom"]);
  const auto pan_x =
      serialization::context::read_float((*state)["view_transform"]["pan"]["x"]);
  const auto pan_y =
      serialization::context::read_float((*state)["view_transform"]["pan"]["y"]);
  const auto min_zoom =
      serialization::context::read_float((*state)["view_transform"]["min_zoom"]);
  const auto max_zoom =
      serialization::context::read_float((*state)["view_transform"]["max_zoom"]);
  if (zoom.has_value() && pan_x.has_value() && pan_y.has_value()) {
    m_view_transform = ui::UIViewTransform2D{
        .pan = glm::vec2(*pan_x, *pan_y),
        .zoom = *zoom,
        .min_zoom = min_zoom.value_or(0.30f),
        .max_zoom = max_zoom.value_or(2.50f),
    };
  }

  auto node_positions = (*state)["node_positions"];
  const int positions_size = node_positions.size();
  for (int index = 0; index < positions_size; ++index) {
    const auto node_id_value =
        serialization::context::read_string(node_positions[index]["id"]);
    const auto x = serialization::context::read_float(node_positions[index]["x"]);
    const auto y = serialization::context::read_float(node_positions[index]["y"]);
    if (!node_id_value.has_value() || !x.has_value() || !y.has_value()) {
      continue;
    }

    const auto node_id = parse_graph_id(*node_id_value);
    if (!node_id.has_value()) {
      continue;
    }

    m_saved_node_positions.emplace(*node_id, glm::vec2(*x, *y));
  }

  const auto active_slot =
      serialization::context::read_float((*state)["active_slot_index"]);
  if (active_slot.has_value()) {
    m_snapshot.active_slot_index = static_cast<size_t>(*active_slot);
  }

  const auto mode_value =
      serialization::context::read_float((*state)["mode"]);
  if (mode_value.has_value()) {
    m_mode = static_cast<ShadingMode>(
        std::clamp(static_cast<int>(*mode_value), 0, 1)
    );
  }
}

void ShadingPanelController::save_state(Ref<SerializationContext> state) const {
  if (state == nullptr) {
    return;
  }

  if (m_view_transform.has_value()) {
    (*state)["view_transform"]["pan"]["x"] = m_view_transform->pan.x;
    (*state)["view_transform"]["pan"]["y"] = m_view_transform->pan.y;
    (*state)["view_transform"]["zoom"] = m_view_transform->zoom;
    (*state)["view_transform"]["min_zoom"] = m_view_transform->min_zoom;
    (*state)["view_transform"]["max_zoom"] = m_view_transform->max_zoom;
  }

  (*state)["active_slot_index"] = static_cast<float>(m_snapshot.active_slot_index);
  (*state)["mode"] = static_cast<float>(m_mode);

  size_t index = 0u;
  for (const auto &[node_id, position] : m_saved_node_positions) {
    auto node_ctx = (*state)["node_positions"][static_cast<int>(index++)];
    node_ctx["id"] = std::to_string(node_id);
    node_ctx["x"] = position.x;
    node_ctx["y"] = position.y;
  }
}

void ShadingPanelController::refresh(bool force) {
  auto selection_store = editor_selection_store();
  if (selection_store->revision() != m_last_selection_revision) {
    force = true;
  }
  m_last_selection_revision = selection_store->revision();

  Snapshot next_snapshot{};

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (scene == nullptr) {
    if (!force && !m_snapshot.has_scene) {
      return;
    }
    m_snapshot = std::move(next_snapshot);
    rebuild_graph();
    mark_render_dirty();
    return;
  }

  next_snapshot.has_scene = true;

  const auto selected_entity_id = selection_store->selected_entity();
  if (!selected_entity_id.has_value() ||
      !scene->world().contains(*selected_entity_id)) {
    if (!force && m_snapshot.has_scene && !m_snapshot.has_entity) {
      return;
    }
    m_snapshot = std::move(next_snapshot);
    rebuild_graph();
    mark_render_dirty();
    return;
  }

  auto entity = scene->world().entity(*selected_entity_id);
  next_snapshot.has_entity = true;
  next_snapshot.entity_name = std::string(entity.name());

  std::vector<ResourceDescriptorID> resolved_material_ids;

  auto *material_slots = entity.get<rendering::MaterialSlots>();
  if (material_slots != nullptr && !material_slots->materials.empty()) {
    resolved_material_ids = material_slots->materials;
  }

  if (resolved_material_ids.empty()) {
    auto *model_ref = entity.get<rendering::ModelRef>();
    if (model_ref != nullptr) {
      auto manager = resource_manager();
      for (const auto &resource_id : model_ref->resource_ids) {
        auto descriptor =
            manager->get_by_descriptor_id<ModelDescriptor>(resource_id);
        if (descriptor != nullptr && !descriptor->material_ids.empty()) {
          resolved_material_ids = descriptor->material_ids;
          break;
        }

        auto model = manager->get_by_descriptor_id<Model>(resource_id);
        if (model != nullptr && !model->materials.empty()) {
          resolved_material_ids = model->materials;
          break;
        }
      }
    }
  }

  for (size_t slot_index = 0u; slot_index < resolved_material_ids.size();
       ++slot_index) {
    std::string label = resolved_material_ids.size() == 1u
                            ? "Material"
                            : "Slot " + std::to_string(slot_index);
    next_snapshot.material_slots.push_back(collect_material_slot(
        resolved_material_ids[slot_index],
        std::move(label)
    ));
  }

  auto *shader_binding = entity.get<rendering::ShaderBinding>();
  if (shader_binding != nullptr && !shader_binding->shader.empty()) {
    for (auto &slot : next_snapshot.material_slots) {
      slot.shader_id = shader_binding->shader;
    }
  }

  next_snapshot.active_slot_index = std::min(
      m_snapshot.active_slot_index,
      next_snapshot.material_slots.empty()
          ? 0u
          : next_snapshot.material_slots.size() - 1u
  );

  bool changed = force;
  if (!changed) {
    changed = next_snapshot.entity_name != m_snapshot.entity_name ||
              next_snapshot.material_slots.size() != m_snapshot.material_slots.size();
  }
  if (!changed) {
    for (size_t i = 0u; i < next_snapshot.material_slots.size(); ++i) {
      if (next_snapshot.material_slots[i].material_id !=
          m_snapshot.material_slots[i].material_id) {
        changed = true;
        break;
      }
    }
  }

  if (!changed) {
    return;
  }

  m_snapshot = std::move(next_snapshot);
  rebuild_graph();
  mark_render_dirty();
}

void ShadingPanelController::rebuild_graph() {
  m_snapshot.graph = ui::UIGraphViewModel{};

  if (m_snapshot.material_slots.empty()) {
    m_saved_node_positions.clear();
    return;
  }

  const size_t slot_index = std::min(
      m_snapshot.active_slot_index,
      m_snapshot.material_slots.size() - 1u
  );
  const auto &active_slot = m_snapshot.material_slots[slot_index];

  if (!active_slot.loaded) {
    m_saved_node_positions.clear();
    return;
  }

  build_material_graph(active_slot, m_snapshot.graph);

  std::unordered_set<ui::UIGraphId> fixed_node_ids;
  fixed_node_ids.reserve(m_saved_node_positions.size());
  for (auto &node : m_snapshot.graph.nodes) {
    const auto position_it = m_saved_node_positions.find(node.id);
    if (position_it == m_saved_node_positions.end()) {
      continue;
    }
    node.position = position_it->second;
    fixed_node_ids.insert(node.id);
  }

  ui::layout_graph_dag(
      m_snapshot.graph,
      fixed_node_ids,
      shading_graph_layout_options()
  );

  m_saved_node_positions.clear();
  for (const auto &node : m_snapshot.graph.nodes) {
    m_saved_node_positions.emplace(node.id, node.position);
  }
}

void ShadingPanelController::apply_material_change(
    std::function<void(astralix::MaterialDescriptor &)> mutator
) {
  if (m_snapshot.material_slots.empty()) {
    return;
  }

  const size_t slot_index = std::min(
      m_snapshot.active_slot_index,
      m_snapshot.material_slots.size() - 1u
  );
  auto &active_slot = m_snapshot.material_slots[slot_index];
  if (!active_slot.loaded) {
    return;
  }

  auto manager = resource_manager();
  if (manager == nullptr) {
    return;
  }

  auto descriptor =
      manager->get_by_descriptor_id<MaterialDescriptor>(active_slot.material_id);
  if (descriptor == nullptr) {
    return;
  }

  mutator(*descriptor);

  active_slot.base_color_factor = descriptor->base_color_factor;
  active_slot.emissive_factor = descriptor->emissive_factor;
  active_slot.metallic_factor = descriptor->metallic_factor;
  active_slot.roughness_factor = descriptor->roughness_factor;
  active_slot.occlusion_strength = descriptor->occlusion_strength;
  active_slot.normal_scale = descriptor->normal_scale;
  active_slot.height_scale = descriptor->height_scale;
  active_slot.bloom_intensity = descriptor->bloom_intensity;
  active_slot.alpha_mask = descriptor->alpha_mask;
  active_slot.alpha_cutoff = descriptor->alpha_cutoff;
  active_slot.double_sided = descriptor->double_sided;
  mark_render_dirty();
}

void ShadingPanelController::render_material_properties(
    ui::im::Children &parent
) {
  const auto &theme = shading_panel_theme();

  if (m_snapshot.material_slots.empty()) {
    return;
  }

  const size_t slot_index = std::min(
      m_snapshot.active_slot_index,
      m_snapshot.material_slots.size() - 1u
  );
  const auto &active_slot = m_snapshot.material_slots[slot_index];
  if (!active_slot.loaded) {
    return;
  }

  bool principled_selected = m_selected_node_id.has_value() &&
      *m_selected_node_id == principled_node_id_for(active_slot.material_id);

  if (!principled_selected) {
    return;
  }

  auto panel = parent.column("material_properties")
                   .style(
                       width(px(260.0f))
                           .fill_y()
                           .padding(12.0f)
                           .gap(10.0f)
                           .radius(12.0f)
                           .background(theme.card_background)
                           .border(1.0f, theme.card_border)
                   );

  panel.text("header", "Principled BSDF")
      .style(font_size(13.0f).text_color(theme.text_primary));

  auto slider_style = width(px(140.0f))
                          .accent_color(theme.accent)
                          .slider_track_thickness(4.0f)
                          .slider_thumb_radius(5.0f);

  const auto render_slider_row =
      [&](std::string_view row_id, std::string_view label,
          float value, float min_value, float max_value, float step,
          auto on_change) {
        auto row = panel.row(std::string(row_id))
                       .style(fill_x().items_center().gap(8.0f));
        row.text("label", std::string(label))
            .style(width(px(90.0f)).font_size(11.5f).text_color(theme.text_muted));
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_slider_row(
      "metallic_row", "Metallic",
      active_slot.metallic_factor, 0.0f, 1.0f, 0.01f,
      [this](float value) {
        apply_material_change([&](MaterialDescriptor &descriptor) {
          descriptor.metallic_factor = value;
        });
      }
  );
  render_slider_row(
      "roughness_row", "Roughness",
      active_slot.roughness_factor, 0.0f, 1.0f, 0.01f,
      [this](float value) {
        apply_material_change([&](MaterialDescriptor &descriptor) {
          descriptor.roughness_factor = value;
        });
      }
  );
  render_slider_row(
      "normal_scale_row", "Normal",
      active_slot.normal_scale, 0.0f, 5.0f, 0.01f,
      [this](float value) {
        apply_material_change([&](MaterialDescriptor &descriptor) {
          descriptor.normal_scale = value;
        });
      }
  );
  render_slider_row(
      "occlusion_row", "Occlusion",
      active_slot.occlusion_strength, 0.0f, 1.0f, 0.01f,
      [this](float value) {
        apply_material_change([&](MaterialDescriptor &descriptor) {
          descriptor.occlusion_strength = value;
        });
      }
  );
  render_slider_row(
      "height_row", "Height",
      active_slot.height_scale, 0.0f, 0.2f, 0.001f,
      [this](float value) {
        apply_material_change([&](MaterialDescriptor &descriptor) {
          descriptor.height_scale = value;
        });
      }
  );
  render_slider_row(
      "bloom_row", "Bloom",
      active_slot.bloom_intensity, 0.0f, 4.0f, 0.05f,
      [this](float value) {
        apply_material_change([&](MaterialDescriptor &descriptor) {
          descriptor.bloom_intensity = value;
        });
      }
  );
  render_slider_row(
      "alpha_cutoff_row", "Alpha Cutoff",
      active_slot.alpha_cutoff, 0.0f, 1.0f, 0.01f,
      [this](float value) {
        apply_material_change([&](MaterialDescriptor &descriptor) {
          descriptor.alpha_cutoff = value;
        });
      }
  );

  auto toggles = panel.column("toggles").style(fill_x().gap(6.0f));
  toggles
      .checkbox("alpha_mask", "Alpha Mask", active_slot.alpha_mask)
      .on_toggle([this](bool checked) {
        apply_material_change([&](MaterialDescriptor &descriptor) {
          descriptor.alpha_mask = checked;
        });
      });
  toggles
      .checkbox("double_sided", "Double Sided", active_slot.double_sided)
      .on_toggle([this](bool checked) {
        apply_material_change([&](MaterialDescriptor &descriptor) {
          descriptor.double_sided = checked;
        });
      });
}

void ShadingPanelController::render_object_mode(
    ui::im::Frame &ui,
    ui::im::Children &root
) {
  const auto &theme = shading_panel_theme();

  auto heading = root.row("heading")
                     .style(fill_x().gap(8.0f).items_center());

  if (m_snapshot.has_entity) {
    heading.text("entity_name", m_snapshot.entity_name)
        .style(font_size(14.0f).text_color(theme.text_primary));

    if (m_snapshot.material_slots.size() > 1u) {
      heading.text("separator", "|")
          .style(font_size(12.0f).text_color(theme.text_muted));

      for (size_t slot_index = 0u; slot_index < m_snapshot.material_slots.size();
           ++slot_index) {
        const auto &slot = m_snapshot.material_slots[slot_index];
        const bool active = slot_index == m_snapshot.active_slot_index;
        auto slot_button = heading.text(
            "slot_" + std::to_string(slot_index),
            slot.label
        );
        slot_button.style(
            font_size(12.0f)
                .text_color(active ? theme.accent : theme.text_muted)
                .padding_xy(8.0f, 4.0f)
                .radius(4.0f)
                .background(active ? theme.accent_soft : glm::vec4(0.0f))
        );
        if (!active) {
          slot_button.on_click([this, slot_index]() {
            m_snapshot.active_slot_index = slot_index;
            m_selected_node_id.reset();
            rebuild_graph();
            mark_render_dirty();
          });
        }
      }
    }
  } else {
    heading.text("subtitle", m_snapshot.has_scene ? "No entity selected" : "No active scene")
        .style(font_size(12.5f).text_color(theme.text_muted));
  }

  if (m_snapshot.has_entity && !m_snapshot.material_slots.empty()) {
    const size_t slot_index = std::min(
        m_snapshot.active_slot_index,
        m_snapshot.material_slots.size() - 1u
    );
    const auto &active_slot = m_snapshot.material_slots[slot_index];

    heading.spacer("spacer");
    heading.text("material_id", compact_id(active_slot.material_id))
        .style(font_size(11.5f).text_color(theme.text_muted));

    if (active_slot.shader_id.has_value()) {
      heading.text("shader_sep", "|")
          .style(font_size(11.5f).text_color(theme.text_muted));
      heading.text("shader_id", compact_id(*active_slot.shader_id))
          .style(font_size(11.5f).text_color(theme.shader_edge));
    }
  }

  auto content = root.row("content").style(flex(1.0f).gap(8.0f));

  auto shell = content.column("graph_shell").style(flex(1.0f).min_height(px(280.0f)).padding(10.0f).gap(8.0f).radius(18.0f).background(theme.card_background).border(1.0f, theme.card_border));

  if (!m_snapshot.has_scene) {
    auto empty = shell.column("empty").style(
        fill().items_center().justify_center().gap(8.0f)
            .radius(14.0f).background(theme.graph_background)
            .border(1.0f, theme.graph_border)
    );
    empty.text("title", "No active scene")
        .style(font_size(18.0f).text_color(theme.text_primary));
    empty.text("body", "Load a scene to inspect entity materials.")
        .style(font_size(12.5f).text_color(theme.text_muted));
    return;
  }

  if (!m_snapshot.has_entity) {
    auto empty = shell.column("empty").style(
        fill().items_center().justify_center().gap(8.0f)
            .radius(14.0f).background(theme.graph_background)
            .border(1.0f, theme.graph_border)
    );
    empty.text("title", "No entity selected")
        .style(font_size(18.0f).text_color(theme.text_primary));
    empty.text("body", "Select an entity to view its material graph.")
        .style(font_size(12.5f).text_color(theme.text_muted));
    return;
  }

  if (m_snapshot.material_slots.empty()) {
    auto empty = shell.column("empty").style(
        fill().items_center().justify_center().gap(8.0f)
            .radius(14.0f).background(theme.graph_background)
            .border(1.0f, theme.graph_border)
    );
    empty.text("title", "No materials")
        .style(font_size(18.0f).text_color(theme.text_primary));
    empty.text("body", "The selected entity has no material slots assigned.")
        .style(font_size(12.5f).text_color(theme.text_muted));
    return;
  }

  const size_t slot_index = std::min(
      m_snapshot.active_slot_index,
      m_snapshot.material_slots.size() - 1u
  );
  const auto &active_slot = m_snapshot.material_slots[slot_index];

  if (!active_slot.loaded) {
    auto empty = shell.column("empty").style(
        fill().items_center().justify_center().gap(8.0f)
            .radius(14.0f).background(theme.graph_background)
            .border(1.0f, theme.graph_border)
    );
    empty.text("title", "Material not loaded")
        .style(font_size(18.0f).text_color(theme.text_primary));
    empty.text("body", active_slot.material_id)
        .style(font_size(12.5f).text_color(theme.text_muted));
    return;
  }

  auto graph = shell.graph_view(
                        "material_graph",
                        ui::GraphViewSpec{
                            .model = m_snapshot.graph,
                        }
  )
                   .style(
                       fill()
                           .radius(2.0f)
                           .background(theme.graph_background)
                           .border(1.0f, theme.graph_border)
                   )
                   .on_view_transform_change(
                       [this](const ui::UIViewTransformChangeEvent &event) {
                         m_view_transform = event.current;
                       }
                   )
                   .on_node_move(
                       [this](ui::UIGraphId node_id, glm::vec2 position) {
                         m_saved_node_positions[node_id] = position;
                         for (auto &node : m_snapshot.graph.nodes) {
                           if (node.id != node_id) {
                             continue;
                           }
                           node.position = position;
                           break;
                         }
                       }
                   )
                   .on_selection_change(
                       [this](const ui::UIGraphSelection &selection) {
                         if (selection.node_ids.empty()) {
                           m_selected_node_id.reset();
                         } else {
                           m_selected_node_id = selection.node_ids.front();
                         }
                         mark_render_dirty();
                       }
                   );

  m_graph_widget = graph.widget_id();
  if (m_view_transform.has_value()) {
    ui.set_view_transform(m_graph_widget, *m_view_transform);
  }

  render_material_properties(content);
}

void ShadingPanelController::render_world_mode(
    ui::im::Frame &,
    ui::im::Children &root
) {
  const auto &theme = shading_panel_theme();

  const auto apply_scene_override = [this](auto mutator) {
    auto scene_manager = SceneManager::get();
    if (scene_manager == nullptr) {
      return;
    }

    auto *scene = scene_manager->get_active_scene();
    if (scene == nullptr) {
      return;
    }

    auto next = scene->render_overrides();
    mutator(next);
    scene->set_render_overrides(std::move(next));
    mark_render_dirty();
  };

  const auto apply_ssgi_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.ssgi.value_or(SSGIConfig{});
      mutator(value);
      overrides.ssgi = value;
    });
  };

  const auto apply_ssr_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.ssr.value_or(SSRConfig{});
      mutator(value);
      overrides.ssr = value;
    });
  };

  const auto apply_volumetric_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.volumetric.value_or(VolumetricFogConfig{});
      mutator(value);
      overrides.volumetric = value;
    });
  };

  const auto apply_lens_flare_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.lens_flare.value_or(LensFlareConfig{});
      mutator(value);
      overrides.lens_flare = value;
    });
  };

  const auto apply_eye_adaptation_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.eye_adaptation.value_or(EyeAdaptationConfig{});
      mutator(value);
      overrides.eye_adaptation = value;
    });
  };

  const auto apply_motion_blur_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.motion_blur.value_or(MotionBlurConfig{});
      mutator(value);
      overrides.motion_blur = value;
    });
  };

  const auto apply_chromatic_aberration_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.chromatic_aberration.value_or(ChromaticAberrationConfig{});
      mutator(value);
      overrides.chromatic_aberration = value;
    });
  };

  const auto apply_vignette_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.vignette.value_or(VignetteConfig{});
      mutator(value);
      overrides.vignette = value;
    });
  };

  const auto apply_film_grain_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.film_grain.value_or(FilmGrainConfig{});
      mutator(value);
      overrides.film_grain = value;
    });
  };

  const auto apply_depth_of_field_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.depth_of_field.value_or(DepthOfFieldConfig{});
      mutator(value);
      overrides.depth_of_field = value;
    });
  };

  const auto apply_god_rays_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.god_rays.value_or(GodRaysConfig{});
      mutator(value);
      overrides.god_rays = value;
    });
  };

  const auto apply_cas_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.cas.value_or(CASConfig{});
      mutator(value);
      overrides.cas = value;
    });
  };

  const auto apply_taa_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.taa.value_or(TAAConfig{});
      mutator(value);
      overrides.taa = value;
    });
  };

  const auto apply_tonemapping_change = [apply_scene_override](auto &&mutator) {
    apply_scene_override([&](SceneRenderOverrides &overrides) {
      auto value = overrides.tonemapping.value_or(TonemappingConfig{});
      mutator(value);
      overrides.tonemapping = value;
    });
  };

  auto system_manager = SystemManager::get();
  RenderSystem *render_system = nullptr;
  if (system_manager != nullptr) {
    render_system = system_manager->get_system<RenderSystem>();
  }

  if (render_system == nullptr) {
    auto empty = root.column("empty").style(
        fill().items_center().justify_center().gap(8.0f)
    );
    empty.text("title", "Render system unavailable")
        .style(font_size(18.0f).text_color(theme.text_primary));
    empty.text("body", "No render system is active in the current project.")
        .style(font_size(12.5f).text_color(theme.text_muted));
    return;
  }

  auto scroll = root.scroll_view("world_scroll").style(flex(1.0f));
  auto content = scroll.column("world_content")
                     .style(fill_x().padding(4.0f).gap(14.0f));

  SceneRenderOverrides scene_overrides;
  auto *active_scene = SceneManager::get() != nullptr
                           ? SceneManager::get()->get_active_scene()
                           : nullptr;
  if (active_scene != nullptr) {
    scene_overrides = active_scene->render_overrides();
  }

  const auto &ssgi = scene_overrides.ssgi.value_or(render_system->ssgi_config());
  auto tuning = content.column("ssgi_tuning")
                    .style(
                        fill_x()
                            .gap(10.0f)
                            .padding(12.0f)
                            .radius(12.0f)
                            .background(theme.card_background)
                            .border(1.0f, theme.card_border)
                    );
  auto tuning_header =
      tuning.row("header").style(fill_x().items_center().gap(10.0f));
  tuning_header.text("title", "SSGI Tuning")
      .style(font_size(14.0f).text_color(theme.text_primary));
  tuning_header.spacer("spacer");
  tuning_header.text(
                   "resolution",
                   ssgi.full_resolution ? "Full resolution" : "Half resolution"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  auto toggles = tuning.row("toggles").style(fill_x().items_center().gap(10.0f));
  toggles
      .checkbox("enabled", "Enabled", ssgi.enabled)
      .on_toggle([apply_ssgi_change](bool checked) {
        apply_ssgi_change([&](SSGIConfig &config) { config.enabled = checked; });
      });
  toggles
      .checkbox("full_resolution", "Full Resolution", ssgi.full_resolution)
      .on_toggle([apply_ssgi_change](bool checked) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.full_resolution = checked;
        });
      });
  toggles
      .checkbox("temporal", "Temporal", ssgi.temporal)
      .on_toggle([apply_ssgi_change](bool checked) {
        apply_ssgi_change([&](SSGIConfig &config) { config.temporal = checked; });
      });

  auto slider_style = width(px(220.0f))
                          .accent_color(theme.accent)
                          .slider_track_thickness(5.0f)
                          .slider_thumb_radius(6.0f);
  const auto render_slider_row =
      [&](std::string_view row_id, std::string_view label,
          float value, float min_value, float max_value, float step,
          auto on_change) {
        auto row =
            tuning.row(std::string(row_id)).style(fill_x().items_center().gap(10.0f));
        row.text("label", std::string(label))
            .style(width(px(160.0f)).font_size(12.0f).text_color(theme.text_muted));
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_slider_row(
      "intensity_row", "Intensity",
      ssgi.intensity, 0.0f, 4.0f, 0.05f,
      [apply_ssgi_change](float value) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.intensity = value;
        });
      }
  );
  render_slider_row(
      "radius_row", "Radius",
      ssgi.radius, 0.1f, 6.0f, 0.05f,
      [apply_ssgi_change](float value) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.radius = value;
        });
      }
  );
  render_slider_row(
      "thickness_row", "Thickness",
      ssgi.thickness, 0.01f, 1.0f, 0.01f,
      [apply_ssgi_change](float value) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.thickness = value;
        });
      }
  );
  render_slider_row(
      "max_distance_row", "Max Distance",
      ssgi.max_distance, 0.1f, 8.0f, 0.05f,
      [apply_ssgi_change](float value) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.max_distance = value;
        });
      }
  );
  render_slider_row(
      "history_weight_row", "History Weight",
      ssgi.history_weight, 0.0f, 0.98f, 0.01f,
      [apply_ssgi_change](float value) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.history_weight = value;
        });
      }
  );
  render_slider_row(
      "normal_reject_row", "Normal Reject",
      ssgi.normal_reject_dot, 0.0f, 1.0f, 0.01f,
      [apply_ssgi_change](float value) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.normal_reject_dot = value;
        });
      }
  );
  render_slider_row(
      "position_reject_row", "Depth Reject",
      ssgi.position_reject_distance, 0.01f, 2.0f, 0.01f,
      [apply_ssgi_change](float value) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.position_reject_distance = value;
        });
      }
  );
  render_slider_row(
      "directions_row", "Directions",
      static_cast<float>(ssgi.directions), 1.0f, 16.0f, 1.0f,
      [apply_ssgi_change](float value) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.directions = std::clamp(
              static_cast<int>(std::lround(value)), 1, 16
          );
        });
      }
  );
  render_slider_row(
      "steps_row", "Steps",
      static_cast<float>(ssgi.steps_per_direction), 1.0f, 8.0f, 1.0f,
      [apply_ssgi_change](float value) {
        apply_ssgi_change([&](SSGIConfig &config) {
          config.steps_per_direction = std::clamp(
              static_cast<int>(std::lround(value)), 1, 8
          );
        });
      }
  );

  const auto &ssr = scene_overrides.ssr.value_or(render_system->ssr_config());
  auto ssr_tuning = content.column("ssr_tuning")
                        .style(
                            fill_x()
                                .gap(10.0f)
                                .padding(12.0f)
                                .radius(12.0f)
                                .background(theme.card_background)
                                .border(1.0f, theme.card_border)
                        );
  auto ssr_header =
      ssr_tuning.row("header").style(fill_x().items_center().gap(10.0f));
  ssr_header.text("title", "SSR")
      .style(font_size(14.0f).text_color(theme.text_primary));
  ssr_header.spacer("spacer");
  ssr_header.text(
                "status",
                ssr.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  ssr_tuning
      .checkbox("enabled", "Enabled", ssr.enabled)
      .on_toggle([apply_ssr_change](bool checked) {
        apply_ssr_change([&](SSRConfig &config) {
          config.enabled = checked;
        });
      });

  const auto render_ssr_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = ssr_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_ssr_slider_row(
      "intensity_row", "Intensity",
      ssr.intensity, 0.0f, 4.0f, 0.05f,
      [apply_ssr_change](float value) {
        apply_ssr_change([&](SSRConfig &config) {
          config.intensity = value;
        });
      }
  );
  render_ssr_slider_row(
      "max_distance_row", "Max Distance",
      ssr.max_distance, 1.0f, 200.0f, 1.0f,
      [apply_ssr_change](float value) {
        apply_ssr_change([&](SSRConfig &config) {
          config.max_distance = value;
        });
      }
  );
  render_ssr_slider_row(
      "thickness_row", "Thickness",
      ssr.thickness, 0.01f, 2.0f, 0.01f,
      [apply_ssr_change](float value) {
        apply_ssr_change([&](SSRConfig &config) {
          config.thickness = value;
        });
      }
  );
  render_ssr_slider_row(
      "max_steps_row", "Max Steps",
      static_cast<float>(ssr.max_steps), 4.0f, 128.0f, 1.0f,
      [apply_ssr_change](float value) {
        apply_ssr_change([&](SSRConfig &config) {
          config.max_steps = std::clamp(
              static_cast<int>(std::lround(value)), 4, 128
          );
        });
      }
  );
  render_ssr_slider_row(
      "stride_row", "Stride",
      ssr.stride, 0.1f, 10.0f, 0.1f,
      [apply_ssr_change](float value) {
        apply_ssr_change([&](SSRConfig &config) {
          config.stride = value;
        });
      }
  );
  render_ssr_slider_row(
      "roughness_cutoff_row", "Roughness Cutoff",
      ssr.roughness_cutoff, 0.0f, 1.0f, 0.01f,
      [apply_ssr_change](float value) {
        apply_ssr_change([&](SSRConfig &config) {
          config.roughness_cutoff = value;
        });
      }
  );

  const auto &volumetric = scene_overrides.volumetric.value_or(render_system->volumetric_config());
  auto volumetric_tuning = content.column("volumetric_tuning")
                               .style(
                                   fill_x()
                                       .gap(10.0f)
                                       .padding(12.0f)
                                       .radius(12.0f)
                                       .background(theme.card_background)
                                       .border(1.0f, theme.card_border)
                               );
  auto volumetric_header = volumetric_tuning.row("header").style(
      fill_x().items_center().gap(10.0f)
  );
  volumetric_header.text("title", "Volumetric Fog")
      .style(font_size(14.0f).text_color(theme.text_primary));
  volumetric_header.spacer("spacer");
  volumetric_header.text(
                       "status",
                       volumetric.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  auto volumetric_toggles = volumetric_tuning.row("toggles").style(
      fill_x().items_center().gap(10.0f)
  );
  volumetric_toggles
      .checkbox("enabled", "Enabled", volumetric.enabled)
      .on_toggle([apply_volumetric_change](bool checked) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.enabled = checked;
        });
      });
  volumetric_toggles
      .checkbox("temporal", "Temporal", volumetric.temporal)
      .on_toggle([apply_volumetric_change](bool checked) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.temporal = checked;
        });
      });

  const auto render_volumetric_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = volumetric_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_volumetric_slider_row(
      "intensity_row", "Intensity",
      volumetric.intensity, 0.0f, 4.0f, 0.05f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.intensity = value;
        });
      }
  );
  render_volumetric_slider_row(
      "density_row", "Density",
      volumetric.density, 0.0f, 1.0f, 0.005f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.density = value;
        });
      }
  );
  render_volumetric_slider_row(
      "scattering_row", "Scattering",
      volumetric.scattering, 0.0f, 1.0f, 0.01f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.scattering = value;
        });
      }
  );
  render_volumetric_slider_row(
      "max_distance_row", "Max Distance",
      volumetric.max_distance, 1.0f, 200.0f, 1.0f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.max_distance = value;
        });
      }
  );
  render_volumetric_slider_row(
      "max_steps_row", "Max Steps",
      static_cast<float>(volumetric.max_steps), 4.0f, 128.0f, 1.0f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.max_steps = std::clamp(
              static_cast<int>(std::lround(value)), 4, 128
          );
        });
      }
  );
  render_volumetric_slider_row(
      "fog_base_height_row", "Base Height",
      volumetric.fog_base_height, -50.0f, 50.0f, 0.5f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.fog_base_height = value;
        });
      }
  );
  render_volumetric_slider_row(
      "height_falloff_row", "Height Falloff",
      volumetric.height_falloff_rate, 0.0f, 2.0f, 0.01f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.height_falloff_rate = value;
        });
      }
  );
  render_volumetric_slider_row(
      "noise_scale_row", "Noise Scale",
      volumetric.noise_scale, 0.0f, 1.0f, 0.01f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.noise_scale = value;
        });
      }
  );
  render_volumetric_slider_row(
      "noise_weight_row", "Noise Weight",
      volumetric.noise_weight, 0.0f, 1.0f, 0.01f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.noise_weight = value;
        });
      }
  );
  render_volumetric_slider_row(
      "wind_speed_row", "Wind Speed",
      volumetric.wind_speed, 0.0f, 5.0f, 0.05f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.wind_speed = value;
        });
      }
  );
  render_volumetric_slider_row(
      "temporal_blend_row", "Temporal Blend",
      volumetric.temporal_blend_weight, 0.0f, 0.98f, 0.01f,
      [apply_volumetric_change](float value) {
        apply_volumetric_change([&](VolumetricFogConfig &config) {
          config.temporal_blend_weight = value;
        });
      }
  );

  const auto &lens_flare = scene_overrides.lens_flare.value_or(render_system->lens_flare_config());
  auto lens_flare_tuning = content.column("lens_flare_tuning")
                               .style(
                                   fill_x()
                                       .gap(10.0f)
                                       .padding(12.0f)
                                       .radius(12.0f)
                                       .background(theme.card_background)
                                       .border(1.0f, theme.card_border)
                               );
  auto lens_flare_header = lens_flare_tuning.row("header").style(
      fill_x().items_center().gap(10.0f)
  );
  lens_flare_header.text("title", "Lens Flare")
      .style(font_size(14.0f).text_color(theme.text_primary));
  lens_flare_header.spacer("spacer");
  lens_flare_header.text(
                       "status",
                       lens_flare.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  lens_flare_tuning
      .checkbox("enabled", "Enabled", lens_flare.enabled)
      .on_toggle([apply_lens_flare_change](bool checked) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.enabled = checked;
        });
      });

  const auto render_lens_flare_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = lens_flare_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_lens_flare_slider_row(
      "intensity_row", "Intensity",
      lens_flare.intensity, 0.0f, 4.0f, 0.05f,
      [apply_lens_flare_change](float value) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.intensity = value;
        });
      }
  );
  render_lens_flare_slider_row(
      "threshold_row", "Threshold",
      lens_flare.threshold, 0.0f, 4.0f, 0.05f,
      [apply_lens_flare_change](float value) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.threshold = value;
        });
      }
  );
  render_lens_flare_slider_row(
      "ghost_count_row", "Ghost Count",
      static_cast<float>(lens_flare.ghost_count), 0.0f, 8.0f, 1.0f,
      [apply_lens_flare_change](float value) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.ghost_count = std::clamp(
              static_cast<int>(std::lround(value)), 0, 8
          );
        });
      }
  );
  render_lens_flare_slider_row(
      "ghost_dispersal_row", "Ghost Dispersal",
      lens_flare.ghost_dispersal, 0.0f, 1.0f, 0.01f,
      [apply_lens_flare_change](float value) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.ghost_dispersal = value;
        });
      }
  );
  render_lens_flare_slider_row(
      "ghost_weight_row", "Ghost Weight",
      lens_flare.ghost_weight, 0.0f, 2.0f, 0.05f,
      [apply_lens_flare_change](float value) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.ghost_weight = value;
        });
      }
  );
  render_lens_flare_slider_row(
      "halo_radius_row", "Halo Radius",
      lens_flare.halo_radius, 0.0f, 1.0f, 0.01f,
      [apply_lens_flare_change](float value) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.halo_radius = value;
        });
      }
  );
  render_lens_flare_slider_row(
      "halo_weight_row", "Halo Weight",
      lens_flare.halo_weight, 0.0f, 2.0f, 0.05f,
      [apply_lens_flare_change](float value) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.halo_weight = value;
        });
      }
  );
  render_lens_flare_slider_row(
      "halo_thickness_row", "Halo Thickness",
      lens_flare.halo_thickness, 0.0f, 0.5f, 0.01f,
      [apply_lens_flare_change](float value) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.halo_thickness = value;
        });
      }
  );
  render_lens_flare_slider_row(
      "chromatic_aberration_row", "Chromatic Aberration",
      lens_flare.chromatic_aberration, 0.0f, 0.1f, 0.005f,
      [apply_lens_flare_change](float value) {
        apply_lens_flare_change([&](LensFlareConfig &config) {
          config.chromatic_aberration = value;
        });
      }
  );

  const auto &god_rays = scene_overrides.god_rays.value_or(render_system->god_rays_config());
  auto god_rays_tuning = content.column("god_rays_tuning")
                             .style(
                                 fill_x()
                                     .gap(10.0f)
                                     .padding(12.0f)
                                     .radius(12.0f)
                                     .background(theme.card_background)
                                     .border(1.0f, theme.card_border)
                             );
  auto god_rays_header = god_rays_tuning.row("header").style(
      fill_x().items_center().gap(10.0f)
  );
  god_rays_header.text("title", "God Rays")
      .style(font_size(14.0f).text_color(theme.text_primary));
  god_rays_header.spacer("spacer");
  god_rays_header.text(
                     "status",
                     god_rays.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  god_rays_tuning
      .checkbox("enabled", "Enabled", god_rays.enabled)
      .on_toggle([apply_god_rays_change](bool checked) {
        apply_god_rays_change([&](GodRaysConfig &config) {
          config.enabled = checked;
        });
      });

  const auto render_god_rays_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = god_rays_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_god_rays_slider_row(
      "intensity_row", "Intensity",
      god_rays.intensity, 0.0f, 4.0f, 0.05f,
      [apply_god_rays_change](float value) {
        apply_god_rays_change([&](GodRaysConfig &config) {
          config.intensity = value;
        });
      }
  );
  render_god_rays_slider_row(
      "decay_row", "Decay",
      god_rays.decay, 0.8f, 1.0f, 0.005f,
      [apply_god_rays_change](float value) {
        apply_god_rays_change([&](GodRaysConfig &config) {
          config.decay = value;
        });
      }
  );
  render_god_rays_slider_row(
      "density_row", "Density",
      god_rays.density, 0.0f, 2.0f, 0.01f,
      [apply_god_rays_change](float value) {
        apply_god_rays_change([&](GodRaysConfig &config) {
          config.density = value;
        });
      }
  );
  render_god_rays_slider_row(
      "weight_row", "Weight",
      god_rays.weight, 0.0f, 1.0f, 0.01f,
      [apply_god_rays_change](float value) {
        apply_god_rays_change([&](GodRaysConfig &config) {
          config.weight = value;
        });
      }
  );
  render_god_rays_slider_row(
      "threshold_row", "Threshold",
      god_rays.threshold, 0.0f, 4.0f, 0.05f,
      [apply_god_rays_change](float value) {
        apply_god_rays_change([&](GodRaysConfig &config) {
          config.threshold = value;
        });
      }
  );
  render_god_rays_slider_row(
      "samples_row", "Samples",
      static_cast<float>(god_rays.samples), 8.0f, 128.0f, 1.0f,
      [apply_god_rays_change](float value) {
        apply_god_rays_change([&](GodRaysConfig &config) {
          config.samples = std::clamp(
              static_cast<int>(std::lround(value)), 8, 128
          );
        });
      }
  );

  const auto &eye_adaptation = scene_overrides.eye_adaptation.value_or(render_system->eye_adaptation_config());
  auto eye_adaptation_tuning = content.column("eye_adaptation_tuning")
                                   .style(
                                       fill_x()
                                           .gap(10.0f)
                                           .padding(12.0f)
                                           .radius(12.0f)
                                           .background(theme.card_background)
                                           .border(1.0f, theme.card_border)
                                   );
  auto eye_adaptation_header = eye_adaptation_tuning.row("header").style(
      fill_x().items_center().gap(10.0f)
  );
  eye_adaptation_header.text("title", "Eye Adaptation")
      .style(font_size(14.0f).text_color(theme.text_primary));
  eye_adaptation_header.spacer("spacer");
  eye_adaptation_header.text(
                          "status",
                          eye_adaptation.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  eye_adaptation_tuning
      .checkbox("enabled", "Enabled", eye_adaptation.enabled)
      .on_toggle([apply_eye_adaptation_change](bool checked) {
        apply_eye_adaptation_change([&](EyeAdaptationConfig &config) {
          config.enabled = checked;
        });
      });

  const auto render_eye_adaptation_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = eye_adaptation_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_eye_adaptation_slider_row(
      "min_log_row", "Min Log Luminance",
      eye_adaptation.min_log_luminance, -16.0f, 4.0f, 0.1f,
      [apply_eye_adaptation_change](float value) {
        apply_eye_adaptation_change([&](EyeAdaptationConfig &config) {
          config.min_log_luminance =
              std::min(value, config.max_log_luminance - 0.1f);
        });
      }
  );
  render_eye_adaptation_slider_row(
      "max_log_row", "Max Log Luminance",
      eye_adaptation.max_log_luminance, -8.0f, 16.0f, 0.1f,
      [apply_eye_adaptation_change](float value) {
        apply_eye_adaptation_change([&](EyeAdaptationConfig &config) {
          config.max_log_luminance =
              std::max(value, config.min_log_luminance + 0.1f);
        });
      }
  );
  render_eye_adaptation_slider_row(
      "speed_up_row", "Speed Up",
      eye_adaptation.adaptation_speed_up, 0.0f, 10.0f, 0.05f,
      [apply_eye_adaptation_change](float value) {
        apply_eye_adaptation_change([&](EyeAdaptationConfig &config) {
          config.adaptation_speed_up = value;
        });
      }
  );
  render_eye_adaptation_slider_row(
      "speed_down_row", "Speed Down",
      eye_adaptation.adaptation_speed_down, 0.0f, 10.0f, 0.05f,
      [apply_eye_adaptation_change](float value) {
        apply_eye_adaptation_change([&](EyeAdaptationConfig &config) {
          config.adaptation_speed_down = value;
        });
      }
  );
  render_eye_adaptation_slider_row(
      "key_value_row", "Key Value",
      eye_adaptation.key_value, 0.01f, 2.0f, 0.01f,
      [apply_eye_adaptation_change](float value) {
        apply_eye_adaptation_change([&](EyeAdaptationConfig &config) {
          config.key_value = value;
        });
      }
  );
  render_eye_adaptation_slider_row(
      "low_percentile_row", "Low Percentile",
      eye_adaptation.low_percentile, 0.0f, 1.0f, 0.01f,
      [apply_eye_adaptation_change](float value) {
        apply_eye_adaptation_change([&](EyeAdaptationConfig &config) {
          config.low_percentile = std::clamp(value, 0.0f, config.high_percentile);
        });
      }
  );
  render_eye_adaptation_slider_row(
      "high_percentile_row", "High Percentile",
      eye_adaptation.high_percentile, 0.0f, 1.0f, 0.01f,
      [apply_eye_adaptation_change](float value) {
        apply_eye_adaptation_change([&](EyeAdaptationConfig &config) {
          config.high_percentile =
              std::clamp(value, config.low_percentile, 1.0f);
        });
      }
  );

  const auto &motion_blur = scene_overrides.motion_blur.value_or(render_system->motion_blur_config());
  auto motion_blur_tuning = content.column("motion_blur_tuning")
                                .style(
                                    fill_x()
                                        .gap(10.0f)
                                        .padding(12.0f)
                                        .radius(12.0f)
                                        .background(theme.card_background)
                                        .border(1.0f, theme.card_border)
                                );
  auto motion_blur_header = motion_blur_tuning.row("header").style(
      fill_x().items_center().gap(10.0f)
  );
  motion_blur_header.text("title", "Motion Blur")
      .style(font_size(14.0f).text_color(theme.text_primary));
  motion_blur_header.spacer("spacer");
  motion_blur_header.text(
                        "status",
                        motion_blur.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  motion_blur_tuning
      .checkbox("enabled", "Enabled", motion_blur.enabled)
      .on_toggle([apply_motion_blur_change](bool checked) {
        apply_motion_blur_change([&](MotionBlurConfig &config) {
          config.enabled = checked;
        });
      });

  const auto render_motion_blur_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = motion_blur_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_motion_blur_slider_row(
      "intensity_row", "Intensity",
      motion_blur.intensity, 0.0f, 3.0f, 0.05f,
      [apply_motion_blur_change](float value) {
        apply_motion_blur_change([&](MotionBlurConfig &config) {
          config.intensity = value;
        });
      }
  );
  render_motion_blur_slider_row(
      "max_samples_row", "Max Samples",
      static_cast<float>(motion_blur.max_samples), 2.0f, 32.0f, 1.0f,
      [apply_motion_blur_change](float value) {
        apply_motion_blur_change([&](MotionBlurConfig &config) {
          config.max_samples = std::clamp(
              static_cast<int>(std::lround(value)), 2, 32
          );
        });
      }
  );
  render_motion_blur_slider_row(
      "depth_threshold_row", "Depth Threshold",
      motion_blur.depth_threshold, 0.5f, 50.0f, 0.5f,
      [apply_motion_blur_change](float value) {
        apply_motion_blur_change([&](MotionBlurConfig &config) {
          config.depth_threshold = value;
        });
      }
  );

  const auto &chromatic_aberration = scene_overrides.chromatic_aberration.value_or(render_system->chromatic_aberration_config());
  auto chromatic_aberration_tuning =
      content.column("chromatic_aberration_tuning")
          .style(
              fill_x()
                  .gap(10.0f)
                  .padding(12.0f)
                  .radius(12.0f)
                  .background(theme.card_background)
                  .border(1.0f, theme.card_border)
          );
  auto chromatic_aberration_header =
      chromatic_aberration_tuning.row("header").style(
          fill_x().items_center().gap(10.0f)
      );
  chromatic_aberration_header.text("title", "Chromatic Aberration")
      .style(font_size(14.0f).text_color(theme.text_primary));
  chromatic_aberration_header.spacer("spacer");
  chromatic_aberration_header.text(
                                "status",
                                chromatic_aberration.enabled ? "Enabled"
                                                            : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  chromatic_aberration_tuning
      .checkbox("enabled", "Enabled", chromatic_aberration.enabled)
      .on_toggle([apply_chromatic_aberration_change](bool checked) {
        apply_chromatic_aberration_change(
            [&](ChromaticAberrationConfig &config) {
              config.enabled = checked;
            }
        );
      });

  const auto render_chromatic_aberration_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = chromatic_aberration_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_chromatic_aberration_slider_row(
      "intensity_row", "Intensity",
      chromatic_aberration.intensity, 0.0f, 0.05f, 0.001f,
      [apply_chromatic_aberration_change](float value) {
        apply_chromatic_aberration_change(
            [&](ChromaticAberrationConfig &config) {
              config.intensity = value;
            }
        );
      }
  );

  const auto &vignette = scene_overrides.vignette.value_or(render_system->vignette_config());
  auto vignette_tuning = content.column("vignette_tuning")
                             .style(
                                 fill_x()
                                     .gap(10.0f)
                                     .padding(12.0f)
                                     .radius(12.0f)
                                     .background(theme.card_background)
                                     .border(1.0f, theme.card_border)
                             );
  auto vignette_header =
      vignette_tuning.row("header").style(fill_x().items_center().gap(10.0f));
  vignette_header.text("title", "Vignette")
      .style(font_size(14.0f).text_color(theme.text_primary));
  vignette_header.spacer("spacer");
  vignette_header.text(
                     "status",
                     vignette.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  vignette_tuning
      .checkbox("enabled", "Enabled", vignette.enabled)
      .on_toggle([apply_vignette_change](bool checked) {
        apply_vignette_change([&](VignetteConfig &config) {
          config.enabled = checked;
        });
      });

  const auto render_vignette_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = vignette_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_vignette_slider_row(
      "intensity_row", "Intensity",
      vignette.intensity, 0.0f, 1.0f, 0.01f,
      [apply_vignette_change](float value) {
        apply_vignette_change([&](VignetteConfig &config) {
          config.intensity = value;
        });
      }
  );
  render_vignette_slider_row(
      "smoothness_row", "Smoothness",
      vignette.smoothness, 0.0f, 1.0f, 0.01f,
      [apply_vignette_change](float value) {
        apply_vignette_change([&](VignetteConfig &config) {
          config.smoothness = value;
        });
      }
  );
  render_vignette_slider_row(
      "roundness_row", "Roundness",
      vignette.roundness, 0.0f, 2.0f, 0.01f,
      [apply_vignette_change](float value) {
        apply_vignette_change([&](VignetteConfig &config) {
          config.roundness = value;
        });
      }
  );

  const auto &film_grain = scene_overrides.film_grain.value_or(render_system->film_grain_config());
  auto film_grain_tuning = content.column("film_grain_tuning")
                               .style(
                                   fill_x()
                                       .gap(10.0f)
                                       .padding(12.0f)
                                       .radius(12.0f)
                                       .background(theme.card_background)
                                       .border(1.0f, theme.card_border)
                               );
  auto film_grain_header =
      film_grain_tuning.row("header").style(fill_x().items_center().gap(10.0f));
  film_grain_header.text("title", "Film Grain")
      .style(font_size(14.0f).text_color(theme.text_primary));
  film_grain_header.spacer("spacer");
  film_grain_header.text(
                       "status",
                       film_grain.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  film_grain_tuning
      .checkbox("enabled", "Enabled", film_grain.enabled)
      .on_toggle([apply_film_grain_change](bool checked) {
        apply_film_grain_change([&](FilmGrainConfig &config) {
          config.enabled = checked;
        });
      });

  const auto render_film_grain_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = film_grain_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_film_grain_slider_row(
      "intensity_row", "Intensity",
      film_grain.intensity, 0.0f, 0.5f, 0.005f,
      [apply_film_grain_change](float value) {
        apply_film_grain_change([&](FilmGrainConfig &config) {
          config.intensity = value;
        });
      }
  );

  const auto &cas = scene_overrides.cas.value_or(render_system->cas_config());
  auto cas_tuning = content.column("cas_tuning")
                        .style(
                            fill_x()
                                .gap(10.0f)
                                .padding(12.0f)
                                .radius(12.0f)
                                .background(theme.card_background)
                                .border(1.0f, theme.card_border)
                        );
  auto cas_header = cas_tuning.row("header").style(
      fill_x().items_center().gap(10.0f)
  );
  cas_header.text("title", "Contrast Adaptive Sharpening")
      .style(font_size(14.0f).text_color(theme.text_primary));
  cas_header.spacer("spacer");
  cas_header.text(
                 "status",
                 cas.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  cas_tuning
      .checkbox("enabled", "Enabled", cas.enabled)
      .on_toggle([apply_cas_change](bool checked) {
        apply_cas_change([&](CASConfig &config) {
          config.enabled = checked;
        });
      });

  const auto render_cas_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = cas_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_cas_slider_row(
      "sharpness_row", "Sharpness",
      cas.sharpness, 0.0f, 1.0f, 0.05f,
      [apply_cas_change](float value) {
        apply_cas_change([&](CASConfig &config) {
          config.sharpness = value;
        });
      }
  );
  render_cas_slider_row(
      "contrast_row", "Contrast",
      cas.contrast, 0.0f, 2.0f, 0.05f,
      [apply_cas_change](float value) {
        apply_cas_change([&](CASConfig &config) {
          config.contrast = value;
        });
      }
  );
  render_cas_slider_row(
      "sharpening_limit_row", "Sharpening Limit",
      cas.sharpening_limit, 0.01f, 1.0f, 0.01f,
      [apply_cas_change](float value) {
        apply_cas_change([&](CASConfig &config) {
          config.sharpening_limit = value;
        });
      }
  );

  const auto &taa = scene_overrides.taa.value_or(render_system->taa_config());
  auto taa_tuning = content.column("taa_tuning")
                        .style(
                            fill_x()
                                .gap(10.0f)
                                .padding(12.0f)
                                .radius(12.0f)
                                .background(theme.card_background)
                                .border(1.0f, theme.card_border)
                        );
  auto taa_header =
      taa_tuning.row("header").style(fill_x().items_center().gap(10.0f));
  taa_header.text("title", "Temporal Anti-Aliasing")
      .style(font_size(14.0f).text_color(theme.text_primary));
  taa_header.spacer("spacer");
  taa_header.text(
                "status",
                taa.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  taa_tuning
      .checkbox("enabled", "Enabled", taa.enabled)
      .on_toggle([apply_taa_change](bool checked) {
        apply_taa_change([&](TAAConfig &config) {
          config.enabled = checked;
        });
      });

  {
    auto row = taa_tuning.row("blend_factor_row").style(
        fill_x().items_center().gap(10.0f)
    );
    row.text("label", "Blend Factor")
        .style(
            width(px(160.0f))
                .font_size(12.0f)
                .text_color(theme.text_muted)
        );
    row.slider("value", taa.blend_factor, 0.01f, 0.5f)
        .step(0.01f)
        .style(slider_style)
        .on_value_change([apply_taa_change](float value) {
          apply_taa_change([&](TAAConfig &config) {
            config.blend_factor = value;
          });
        });
  }

  const auto &tonemapping = scene_overrides.tonemapping.value_or(render_system->tonemapping_config());
  auto tonemapping_tuning = content.column("tonemapping_tuning")
                                .style(
                                    fill_x()
                                        .gap(10.0f)
                                        .padding(12.0f)
                                        .radius(12.0f)
                                        .background(theme.card_background)
                                        .border(1.0f, theme.card_border)
                                );
  auto tonemapping_header =
      tonemapping_tuning.row("header").style(fill_x().items_center().gap(10.0f));
  tonemapping_header.text("title", "Tonemapping")
      .style(font_size(14.0f).text_color(theme.text_primary));
  tonemapping_header.spacer("spacer");
  {
    const char *operator_name = "Reinhard";
    if (tonemapping.tonemap_operator == TonemapOperator::AgX) {
      operator_name = "AgX";
    } else if (tonemapping.tonemap_operator == TonemapOperator::ACES) {
      operator_name = "ACES";
    }
    tonemapping_header.text("status", operator_name)
        .style(font_size(11.5f).text_color(theme.text_muted));
  }

  tonemapping_tuning.segmented_control(
      "operator_selector",
      {"Reinhard", "AgX", "ACES"},
      static_cast<size_t>(tonemapping.tonemap_operator)
  )
      .style(font_size(12.0f))
      .on_select([apply_tonemapping_change](size_t index, const std::string &) {
        apply_tonemapping_change([&](TonemappingConfig &config) {
          config.tonemap_operator = static_cast<TonemapOperator>(index);
        });
      });

  const auto render_tonemapping_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = tonemapping_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_tonemapping_slider_row(
      "gamma_row", "Gamma",
      tonemapping.gamma, 1.0f, 3.0f, 0.01f,
      [apply_tonemapping_change](float value) {
        apply_tonemapping_change([&](TonemappingConfig &config) {
          config.gamma = value;
        });
      }
  );
  render_tonemapping_slider_row(
      "bloom_strength_row", "Bloom Strength",
      tonemapping.bloom_strength, 0.0f, 1.0f, 0.005f,
      [apply_tonemapping_change](float value) {
        apply_tonemapping_change([&](TonemappingConfig &config) {
          config.bloom_strength = value;
        });
      }
  );

  const auto &depth_of_field = scene_overrides.depth_of_field.value_or(render_system->depth_of_field_config());
  auto depth_of_field_tuning = content.column("depth_of_field_tuning")
                                   .style(
                                       fill_x()
                                           .gap(10.0f)
                                           .padding(12.0f)
                                           .radius(12.0f)
                                           .background(theme.card_background)
                                           .border(1.0f, theme.card_border)
                                   );
  auto depth_of_field_header =
      depth_of_field_tuning.row("header").style(fill_x().items_center().gap(10.0f));
  depth_of_field_header.text("title", "Depth of Field")
      .style(font_size(14.0f).text_color(theme.text_primary));
  depth_of_field_header.spacer("spacer");
  depth_of_field_header.text(
                           "status",
                           depth_of_field.enabled ? "Enabled" : "Disabled"
  )
      .style(font_size(11.5f).text_color(theme.text_muted));

  depth_of_field_tuning
      .checkbox("enabled", "Enabled", depth_of_field.enabled)
      .on_toggle([apply_depth_of_field_change](bool checked) {
        apply_depth_of_field_change([&](DepthOfFieldConfig &config) {
          config.enabled = checked;
        });
      });

  const auto render_depth_of_field_slider_row =
      [&](std::string_view row_id, std::string_view label, float value,
          float min_value, float max_value, float step, auto on_change) {
        auto row = depth_of_field_tuning.row(std::string(row_id)).style(
            fill_x().items_center().gap(10.0f)
        );
        row.text("label", std::string(label))
            .style(
                width(px(160.0f))
                    .font_size(12.0f)
                    .text_color(theme.text_muted)
            );
        row.slider("value", value, min_value, max_value)
            .step(step)
            .style(slider_style)
            .on_value_change(on_change);
      };

  render_depth_of_field_slider_row(
      "focus_distance_row", "Focus Distance",
      depth_of_field.focus_distance, 0.1f, 100.0f, 0.1f,
      [apply_depth_of_field_change](float value) {
        apply_depth_of_field_change([&](DepthOfFieldConfig &config) {
          config.focus_distance = value;
        });
      }
  );
  render_depth_of_field_slider_row(
      "focus_range_row", "Focus Range",
      depth_of_field.focus_range, 0.1f, 50.0f, 0.1f,
      [apply_depth_of_field_change](float value) {
        apply_depth_of_field_change([&](DepthOfFieldConfig &config) {
          config.focus_range = value;
        });
      }
  );
  render_depth_of_field_slider_row(
      "max_blur_radius_row", "Max Blur Radius",
      depth_of_field.max_blur_radius, 1.0f, 15.0f, 0.5f,
      [apply_depth_of_field_change](float value) {
        apply_depth_of_field_change([&](DepthOfFieldConfig &config) {
          config.max_blur_radius = value;
        });
      }
  );
  render_depth_of_field_slider_row(
      "sample_count_row", "Sample Count",
      static_cast<float>(depth_of_field.sample_count), 4.0f, 64.0f, 4.0f,
      [apply_depth_of_field_change](float value) {
        apply_depth_of_field_change([&](DepthOfFieldConfig &config) {
          config.sample_count = std::clamp(
              static_cast<int>(std::lround(value)), 4, 64
          );
        });
      }
  );
}

void ShadingPanelController::render(ui::im::Frame &ui) {
  const auto &theme = shading_panel_theme();
  m_graph_widget = ui::im::k_invalid_widget_id;

  auto root = ui.column("shading_panel").style(fill().padding(14.0f).gap(12.0f).background(theme.panel_background));

  auto toolbar = root.row("toolbar").style(fill_x().gap(10.0f).items_center());
  toolbar.segmented_control(
      "mode_selector",
      {"Object", "World"},
      static_cast<size_t>(m_mode)
  )
      .style(font_size(12.0f))
      .on_select([this](size_t index, const std::string &) {
        m_mode = static_cast<ShadingMode>(index);
        mark_render_dirty();
      });

  switch (m_mode) {
    case ShadingMode::Object:
      render_object_mode(ui, root);
      break;
    case ShadingMode::World:
      render_world_mode(ui, root);
      break;
  }
}

} // namespace astralix::editor
