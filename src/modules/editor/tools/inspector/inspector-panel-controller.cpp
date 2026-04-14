#include "fields.hpp"
#include "styles.hpp"

#include "editor-selection-store.hpp"
#include "entities/serializers/scene-snapshot.hpp"
#include "managers/resource-manager.hpp"
#include "managers/scene-manager.hpp"
#include "resources/descriptors/material-descriptor.hpp"
#include "trace.hpp"

#include <algorithm>
#include <utility>

namespace astralix::editor {
namespace panel = inspector_panel;

namespace {

std::string optional_descriptor_label(
    const std::optional<ResourceDescriptorID> &descriptor_id
) {
  return descriptor_id.has_value() ? *descriptor_id : std::string("(none)");
}

serialization::ComponentSnapshot
build_material_properties_snapshot(const ResourceDescriptorID &material_id) {
  serialization::ComponentSnapshot snapshot{
      .name = std::string(panel::k_material_properties_component_name)};
  snapshot.fields.push_back({"material_id", material_id});

  auto manager = resource_manager();
  auto material = manager != nullptr
                      ? manager->get_by_descriptor_id<MaterialDescriptor>(material_id)
                      : nullptr;
  const bool loaded = material != nullptr;
  snapshot.fields.push_back({"descriptor_loaded", loaded});

  if (!loaded) {
    return snapshot;
  }

  snapshot.fields.push_back(
      {"base_color_id", optional_descriptor_label(material->base_color_id)}
  );
  snapshot.fields.push_back(
      {"normal_id", optional_descriptor_label(material->normal_id)}
  );
  snapshot.fields.push_back(
      {"metallic_id", optional_descriptor_label(material->metallic_id)}
  );
  snapshot.fields.push_back(
      {"roughness_id", optional_descriptor_label(material->roughness_id)}
  );
  snapshot.fields.push_back({"metallic_roughness_id",
                             optional_descriptor_label(
                                 material->metallic_roughness_id
                             )});
  snapshot.fields.push_back(
      {"occlusion_id", optional_descriptor_label(material->occlusion_id)}
  );
  snapshot.fields.push_back(
      {"emissive_id", optional_descriptor_label(material->emissive_id)}
  );
  snapshot.fields.push_back(
      {"displacement_id", optional_descriptor_label(material->displacement_id)}
  );
  snapshot.fields.push_back(
      {"base_color_factor.x", material->base_color_factor.x}
  );
  snapshot.fields.push_back(
      {"base_color_factor.y", material->base_color_factor.y}
  );
  snapshot.fields.push_back(
      {"base_color_factor.z", material->base_color_factor.z}
  );
  snapshot.fields.push_back(
      {"base_color_factor.w", material->base_color_factor.w}
  );
  snapshot.fields.push_back({"emissive_factor.x", material->emissive_factor.x});
  snapshot.fields.push_back({"emissive_factor.y", material->emissive_factor.y});
  snapshot.fields.push_back({"emissive_factor.z", material->emissive_factor.z});
  snapshot.fields.push_back({"metallic_factor", material->metallic_factor});
  snapshot.fields.push_back({"roughness_factor", material->roughness_factor});
  snapshot.fields.push_back(
      {"occlusion_strength", material->occlusion_strength}
  );
  snapshot.fields.push_back({"normal_scale", material->normal_scale});
  snapshot.fields.push_back({"bloom_intensity", material->bloom_intensity});
  return snapshot;
}

void append_material_properties_snapshot(
    std::vector<serialization::ComponentSnapshot> &components
) {
  const auto *material_slots =
      panel::find_component_snapshot(components, "MaterialSlots");
  if (material_slots == nullptr) {
    return;
  }

  const auto material_id =
      serialization::fields::read_string(material_slots->fields, "material_0");
  if (!material_id.has_value() || material_id->empty()) {
    return;
  }

  auto insert_position = components.end();
  for (auto it = components.begin(); it != components.end(); ++it) {
    if (it->name == "MaterialSlots") {
      insert_position = std::next(it);
      break;
    }
  }

  components.insert(
      insert_position, build_material_properties_snapshot(*material_id)
  );
}

} // namespace

void InspectorPanelController::mount(const PanelMountContext &context) {
  static_cast<void>(context);
  m_last_selection_revision = editor_selection_store()->revision();
  refresh(true);
}

void InspectorPanelController::unmount() {
  m_snapshot = {};
  m_add_component_options.clear();
  m_add_component_lookup.clear();
  m_pending_add_component_name.clear();
  m_scalar_drafts.clear();
  m_group_drafts.clear();
  m_component_expansion.clear();
  mark_render_dirty();
}

void InspectorPanelController::update(const PanelUpdateContext &) { refresh(); }

std::optional<uint64_t> InspectorPanelController::render_version() const {
  return m_render_revision;
}

InspectorPanelController::InspectedEntitySnapshot
InspectorPanelController::collect_snapshot() const {
  InspectedEntitySnapshot snapshot;

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  if (scene == nullptr) {
    return snapshot;
  }

  snapshot.has_scene = true;
  snapshot.scene_name = scene->get_name();

  const auto selected_entity_id = editor_selection_store()->selected_entity();
  if (!selected_entity_id.has_value() ||
      !scene->world().contains(*selected_entity_id)) {
    return snapshot;
  }

  snapshot.entity_id = *selected_entity_id;
  auto entity = scene->world().entity(*selected_entity_id);
  snapshot.entity_name = std::string(entity.name());
  snapshot.entity_active = entity.active();
  snapshot.components = serialization::collect_entity_component_snapshots(entity);
  append_material_properties_snapshot(snapshot.components);
  return snapshot;
}

void InspectorPanelController::refresh(bool force) {
  auto selection_store = editor_selection_store();
  if (selection_store->revision() != m_last_selection_revision) {
    force = true;
  }

  InspectedEntitySnapshot next_snapshot = collect_snapshot();
  if (selection_store->selected_entity().has_value() &&
      !next_snapshot.entity_id.has_value()) {
    selection_store->clear_selected_entity();
    next_snapshot = collect_snapshot();
    force = true;
  }

  if (!force && panel::snapshots_equal(m_snapshot, next_snapshot)) {
    m_last_selection_revision = selection_store->revision();
    return;
  }

  m_snapshot = std::move(next_snapshot);
  m_last_selection_revision = selection_store->revision();
  sync_add_component_controls();
  rebuild_component_cards();
  mark_render_dirty();
}

void InspectorPanelController::sync_add_component_controls() {
  const std::string previous_pending = m_pending_add_component_name;
  m_add_component_options.clear();
  m_add_component_lookup.clear();
  m_pending_add_component_name.clear();

  auto scene_manager = SceneManager::get();
  Scene *scene =
      scene_manager != nullptr ? scene_manager->get_active_scene() : nullptr;
  const bool has_selection = scene != nullptr && m_snapshot.entity_id.has_value() &&
                             scene->world().contains(*m_snapshot.entity_id);

  if (!has_selection) {
    return;
  }

  auto entity = scene->world().entity(*m_snapshot.entity_id);
  const auto *descriptors = panel::component_descriptors();
  for (size_t index = 0u; index < panel::component_descriptor_count(); ++index) {
    const auto &descriptor = descriptors[index];
    if (!descriptor.visible || descriptor.can_add == nullptr) {
      continue;
    }

    if (descriptor.can_add(entity)) {
      const std::string label = panel::humanize_token(descriptor.name);
      m_add_component_lookup.emplace(label, descriptor.name);
      m_add_component_options.push_back(label);
    }
  }

  std::sort(m_add_component_options.begin(), m_add_component_options.end());
  for (const auto &label : m_add_component_options) {
    const auto it = m_add_component_lookup.find(label);
    if (it != m_add_component_lookup.end() &&
        it->second == previous_pending) {
      m_pending_add_component_name = previous_pending;
      break;
    }
  }

  if (m_pending_add_component_name.empty() && !m_add_component_options.empty()) {
    const auto it = m_add_component_lookup.find(m_add_component_options.front());
    if (it != m_add_component_lookup.end()) {
      m_pending_add_component_name = it->second;
    }
  }
}

void InspectorPanelController::render(ui::im::Frame &ui) {
  ASTRA_PROFILE_N("InspectorPanel::render");
  using namespace ui::dsl::styles;

  const InspectorPanelTheme theme;
  const bool has_selection = m_snapshot.entity_id.has_value();
  const std::string selection_title =
      has_selection
          ? (m_snapshot.entity_name.empty() ? std::string("Unnamed Entity")
                                            : m_snapshot.entity_name)
          : (m_snapshot.has_scene ? std::string("Nothing selected")
                                  : std::string("Selection unavailable"));
  const std::string entity_id_label =
      has_selection
          ? "Entity ID " + static_cast<std::string>(*m_snapshot.entity_id)
          : std::string("Entity ID --");
  const std::string empty_title =
      !m_snapshot.has_scene ? std::string("No active scene")
                            : std::string("Nothing selected");
  const std::string empty_body =
      !m_snapshot.has_scene
          ? std::string("SceneManager is not exposing an active scene yet.")
          : std::string(
                "Select an entity in Scene Hierarchy to inspect and edit it."
            );

  size_t add_component_selected_index = 0u;
  for (size_t index = 0u; index < m_add_component_options.size(); ++index) {
    const auto it = m_add_component_lookup.find(m_add_component_options[index]);
    if (it != m_add_component_lookup.end() &&
        it->second == m_pending_add_component_name) {
      add_component_selected_index = index;
      break;
    }
  }

  auto root = ui.column("root").style(
      fill().background(theme.shell_background).padding(12.0f).gap(10.0f)
  );
  {
  ASTRA_PROFILE_N("InspectorPanel::summary");
  auto summary = root.column("summary").style(
      fill_x()
          .padding(14.0f)
          .gap(8.0f)
          .radius(12.0f)
          .background(theme.panel_background)
          .border(1.0f, theme.panel_border)
  );
  auto header_row = summary.row("header").style(fill_x().items_center().gap(8.0f));
  auto header_title = header_row.column("title").style(items_start().gap(3.0f));
  header_title.text("heading", "Inspector")
      .style(font_size(18.0f).text_color(theme.text_primary));
  header_title.text("body", "Inspect and edit the selected scene entity.")
      .style(font_size(12.0f).text_color(theme.text_muted));
  header_row.spacer("spacer");
  auto scene_chip = header_row.row("scene-chip").style(
      items_center()
          .padding_xy(12.0f, 6.0f)
          .background(theme.accent_soft)
          .border(1.0f, theme.accent)
          .radius(8.0f)
  );
  scene_chip
      .text(
          "label",
          m_snapshot.has_scene ? m_snapshot.scene_name : std::string("No active scene")
      )
      .style(font_size(12.0f).text_color(theme.accent));

  summary.text("selection-title", selection_title)
      .style(
          font_size(16.0f).text_color(
              has_selection ? theme.text_primary : theme.text_muted
          )
      );
  summary.text(
      "component-count",
      panel::component_count_label(
          panel::visible_component_count(m_snapshot.components)
      )
  )
      .style(font_size(12.0f).text_color(theme.text_muted));
  summary.text("entity-id", entity_id_label)
      .style(font_size(12.0f).text_color(theme.text_muted));
  summary
      .text_input(
          "entity-name",
          has_selection ? m_snapshot.entity_name : std::string{},
          "Entity name"
      )
      .enabled(has_selection)
      .select_all_on_focus(true)
      .on_change([this](const std::string &value) { set_entity_name(value); })
      .style(panel::input_field_style(theme));
  summary
      .checkbox(
          "entity-active", "Entity is active", has_selection && m_snapshot.entity_active
      )
      .enabled(has_selection)
      .style(panel::checkbox_field_style(theme))
      .on_toggle([this](bool active) { set_entity_active(active); });

  }

  {
  ASTRA_PROFILE_N("InspectorPanel::add_component_row");
  auto add_row = root.row("add-row").style(fill_x().items_center().gap(8.0f));
  add_row
      .select(
          "add-component-select",
          m_add_component_options,
          add_component_selected_index,
          "Add component"
      )
      .enabled(has_selection && !m_add_component_options.empty())
      .on_select([this](size_t, const std::string &value) {
        const auto it = m_add_component_lookup.find(value);
        const std::string next_pending =
            it != m_add_component_lookup.end() ? it->second : std::string{};
        if (m_pending_add_component_name == next_pending) {
          return;
        }

        m_pending_add_component_name = next_pending;
        mark_render_dirty();
      })
      .style(panel::input_field_style(theme).flex(1.0f));

  auto add_button = add_row.pressable("add-component-button")
                        .enabled(
                            has_selection && !m_pending_add_component_name.empty()
                        )
                        .on_click([this]() {
                          add_component(m_pending_add_component_name);
                        })
                        .style(
                            panel::compact_button_style(theme)
                                .items_center()
                                .justify_center()
                                .cursor_pointer()
                        );
  add_button.text("label", "Add")
      .style(font_size(12.5f).text_color(theme.text_primary));

  }

  if (auto scroll = root.scroll_view("component-scroll").visible(has_selection).style(
      fill_x()
          .flex(1.0f)
          .padding(ui::UIEdges{
              .left = 6.0f,
              .top = 6.0f,
              .right = 6.0f,
              .bottom = 10.0f,
          })
          .gap(8.0f)
          .background(theme.panel_background)
          .border(1.0f, theme.panel_border)
          .radius(12.0f)
  )) {
    ASTRA_PROFILE_N("InspectorPanel::component_cards");
    auto stack = scroll.column("component-stack").style(fill_x().gap(4.0f));
    render_component_cards(stack);
  }

  if (auto empty = root.column("empty-state").visible(!has_selection).style(
      fill_x()
          .flex(1.0f)
          .justify_center()
          .items_center()
          .padding(18.0f)
          .gap(8.0f)
          .radius(12.0f)
          .background(theme.panel_background)
          .border(1.0f, theme.panel_border)
  )) {
    empty.text("title", empty_title)
        .style(font_size(16.0f).text_color(theme.text_primary));
    empty.text("body", empty_body)
        .style(font_size(12.0f).text_color(theme.text_muted));
  }
}

} // namespace astralix::editor
