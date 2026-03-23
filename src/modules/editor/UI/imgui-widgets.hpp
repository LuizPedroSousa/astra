#pragma once

#include "base.hpp"

#include "components/camera/camera-component.hpp"
#include "components/light/light-component.hpp"
#include "components/light/strategies/directional-strategy.hpp"
#include "components/light/strategies/point-strategy.hpp"
#include "components/light/strategies/spotlight-strategy.hpp"
#include "components/material/material-component.hpp"
#include "components/mesh-collision/mesh-collision-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/rigidbody/rigidbody-component.hpp"
#include "components/transform/transform-component.hpp"

#include "entities/ientity.hpp"
#include "imgui.h"
#include "layers/managers/layer-manager.hpp"
#include <imgui_internal.h>

#include "managers/resource-manager.hpp"

namespace astralix::UI {

static bool
tree_node(const char *label, const char *icon_name,
          ImVec2 icon_size = ImVec2(14, 14), bool is_item_active = false,
          std::function<void(std::function<void()>)> on_open = nullptr,
          std::function<void(std::function<void()>)> on_close = nullptr,
          ImVec4 icon_uv = {1, 0, 0, 1}) {
  const ImGuiStyle &style = ImGui::GetStyle();
  ImGuiStorage *storage = ImGui::GetStateStorage();

  LayerContext *context = LayerManager::get()->get_layer_context();
  ImU32 id = ImGui::GetID(label);
  int opened = storage->GetInt(id, 0);
  float cursor_x = ImGui::GetCursorPosX();

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 2.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 10.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
  ImGui::BeginGroup();

  if (ImGui::InvisibleButton(
          label,
          ImVec2(-1, ImGui::GetFontSize() + style.FramePadding.y * 2.0f))) {

    int *p_opened = storage->GetIntRef(id, 0);
    *p_opened = !*p_opened;

    auto toggle = [&]() { opened = *p_opened; };

    if (on_open != nullptr && *p_opened != 0) {
      on_open(toggle);
    } else if (on_close != nullptr) {
      on_close(toggle);
    } else {
      toggle();
    }
  }

  bool hovered = ImGui::IsItemHovered();
  bool active = ImGui::IsItemActive() || is_item_active;

  ImVec2 rect_min = ImGui::GetItemRectMin();
  ImVec2 rect_max = ImGui::GetItemRectMax();

  if (hovered || active) {
    auto hover_color = ImColor(
        ImGui::GetStyle()
            .Colors[active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered]);

    ImGui::GetWindowDrawList()->AddRectFilled(
        ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), hover_color);

    if (active) {
      ImVec2 bottom_left = {ImGui::GetItemRectMin().x,
                            ImGui::GetItemRectMax().y - 1.0f};
      ImVec2 bottom_right = {ImGui::GetItemRectMax().x,
                             ImGui::GetItemRectMax().y - 1.0f};

      auto active_color = ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]);

      ImGui::GetWindowDrawList()->AddLine(bottom_left, bottom_right,
                                          active_color, 1.0f);
    }
  }

  ImGui::SameLine(cursor_x);

  auto icon = ResourceManager::get()->get_texture_by_id(icon_name);

  float spacing = 2.0f;

  float line_height = ImGui::GetFontSize() + GImGui->Style.FramePadding.y;

  float icon_position_y =
      ImGui::GetCursorPosY() + (line_height - icon_size.y) * 0.5f;

  ImVec4 transparent_color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

  ImGui::PushStyleColor(ImGuiCol_Button, transparent_color);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, transparent_color);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, transparent_color);
  ImGui::SetCursorPosY(icon_position_y);
  ImGui::ImageButton(icon->get_resource_id().c_str(), icon->get_renderer_id(),
                     icon_size, {icon_uv.x, icon_uv.y}, {icon_uv.z, icon_uv.w});
  ImGui::PopStyleColor(3);

  ImGui::SameLine(0.0f, spacing);

  ImGui::SetCursorPosY(icon_position_y);
  ImGui::Text(label);
  ImGui::EndGroup();
  ImGui::PopStyleVar(3);

  if (opened) {
    ImGui::TreePush(label);
  }
  return opened != 0;
}

static void draw_vec3_control(const std::string &label, glm::vec3 &values,
                              float reset_value = 0.0f,
                              float column_width = 100.0f) {
  ImGuiIO &io = ImGui::GetIO();
  auto boldFont = io.Fonts->Fonts[0];

  ImGui::PushID(label.c_str());

  ImGui::Columns(2);
  ImGui::SetColumnWidth(0, column_width);
  ImGui::Text(label.c_str());
  ImGui::NextColumn();

  ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

  float line_height =
      ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
  ImVec2 button_size = {line_height + 3.0f, line_height};

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
  ImGui::PushFont(boldFont);
  if (ImGui::Button("X", button_size))
    values.x = reset_value;
  ImGui::PopFont();
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::PopItemWidth();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.8f, 0.3f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.7f, 0.2f, 1.0f});
  ImGui::PushFont(boldFont);
  if (ImGui::Button("Y", button_size))
    values.y = reset_value;
  ImGui::PopFont();
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::PopItemWidth();
  ImGui::SameLine();

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ImVec4{0.2f, 0.35f, 0.9f, 1.0f});
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
  ImGui::PushFont(boldFont);
  if (ImGui::Button("Z", button_size))
    values.z = reset_value;
  ImGui::PopFont();
  ImGui::PopStyleColor(3);

  ImGui::SameLine();
  ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
  ImGui::PopItemWidth();

  ImGui::PopStyleVar();

  ImGui::Columns(1);

  ImGui::PopID();
}

template <typename T, typename... Args>
static void add_component_menu_item(const std::string &name, IEntity &entity,
                                    Args &&...args) {

  if (!entity.has_component<T>()) {
    if (ImGui::MenuItem(name.c_str())) {
      entity.add_component<T>(std::forward<Args>(args)...);
      ImGui::CloseCurrentPopup();
    }
  }
}

static void draw_add_component(IEntity *entity) {
  ImGui::PushItemWidth(-1);

  if (ImGui::Button("Add Component"))
    ImGui::OpenPopup("AddComponent");

  if (ImGui::BeginPopup("AddComponent")) {
    add_component_menu_item<MaterialComponent>("Material", *entity);
    add_component_menu_item<RigidBodyComponent>("RigidBody", *entity);
    add_component_menu_item<MeshComponent>("Mesh", *entity);
    add_component_menu_item<CameraComponent>("Camera", *entity);

    ImGui::EndPopup();
  }

  ImGui::PopItemWidth();
}

static void draw_menu_item(const char *title, std::function<void()> callback) {

  if (ImGui::MenuItem(title)) {
    callback();
  }
};

static void draw_menu(const char *title, std::function<void()> callback) {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 6));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);

  ImGuiStyle &style = ImGui::GetStyle();

  float border_size = style.WindowBorderSize;
  style.WindowBorderSize = 0.0f;

  style.Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0); // remove border

  if (ImGui::BeginMenu(title)) {

    callback();

    ImGui::EndMenu();
  }

  ImGui::PopStyleVar(2);

  style.WindowBorderSize = border_size;
}

static void draw_sub_menu(
    const char *title,
    std::vector<std::tuple<const char *, std::function<void()>>> items) {

  draw_menu(title, [items]() {
    for (int i = 0; i < items.size(); i++) {
      auto name = std::get<0>(items[i]);
      auto fn = std::get<1>(items[i]);

      draw_menu_item(name, fn);
    }
  });
}

static void entity_templates() {

  auto entity_manager = EntityManager::get();
  auto layer_context = LayerManager::get()->get_layer_context();

  auto create_object_with_mesh = [&](const char *name, Mesh mesh) {
    auto entity = entity_manager->add_entity<Object>(name);

    entity->add_component<MeshComponent>()->attach_mesh(mesh);
    entity->get_component<ResourceComponent>()
        ->attach_shader("shaders::lighting")
        ->attach_texture({"textures::default", "materials[0].diffuse"})
        ->attach_texture({"textures::default", "materials[0].specular"});

    entity->start();
  };

  auto create_light = [&](const char *name, Scope<LightStrategy> strategy) {
    auto entity = entity_manager->add_entity<Object>(name);
    auto camera_obj =
        entity_manager->get_entity_with_component<CameraComponent>();

    // entity->add_component<LightComponent>(std::move(strategy),
    //                                       camera_obj->get_entity_id());
  };

  UI::draw_menu_item("Create Empty", [&]() {
    entity_manager->add_entity<Object>("GameObject");
  });
  UI::draw_sub_menu(
      "3D Object",
      {{"Cube", [&]() { create_object_with_mesh("Cube", Mesh::cube(2.0f)); }},
       {
           "Sphere",
           [&]() { create_object_with_mesh("Sphere", Mesh::sphere()); },
       },
       {
           "Capsule",
           [&]() { create_object_with_mesh("Capsule", Mesh::capsule()); },
       },
       {
           "Quad",
           [&]() { create_object_with_mesh("Quad", Mesh::quad()); },
       },
       {
           "Plane",
           [&]() { create_object_with_mesh("Plane", Mesh::plane()); },
       }});

  UI::draw_sub_menu("Light", {

                                 {"Directional light",
                                  [&]() {
                                    // create_light("Directional Light",
                                    //              create_scope<DirectionalStrategy>());
                                  }},
                                 {"Point light",
                                  [&]() {
                                    // create_light("Point Light",
                                    // create_scope<PointStrategy>());
                                  }},
                                 {"Spotlight",
                                  [&]() {
                                    // create_light("Spotlight",
                                    //              create_scope<SpotLightStrategy>());
                                  }}

                             });
}

static void entity_components() {
  auto entity_manager = EntityManager::get();
  auto layer_context = LayerManager::get()->get_layer_context();

  UI::draw_menu_item("Add", [&]() {});

  UI::draw_sub_menu("Mesh",
                    {

                        {"Mesh Renderer",
                         [&]() {
                           auto entity = entity_manager->get_entity(
                               layer_context->selected_entity_id);

                           entity->add_component<MeshComponent>();
                         }}}

  );

  UI::draw_sub_menu(
      "Physics",
      {

          {"RigidBody",
           [&]() {
             auto entity =
                 entity_manager->get_entity(layer_context->selected_entity_id);

             entity->add_component<RigidBodyComponent>();
           }},

          {"Mesh Collision",
           [&]() {
             auto entity =
                 entity_manager->get_entity(layer_context->selected_entity_id);

             entity->add_component<MeshCollisionComponent>();
           }}

      }

  );

  UI::draw_sub_menu(
      "Rendering",
      {

          {"Camera",
           [&]() {
             auto entity =
                 entity_manager->get_entity(layer_context->selected_entity_id);

             entity->add_component<CameraComponent>();
           }},

          {"Skybox",
           [&]() {
             auto entity =
                 entity_manager->get_entity(layer_context->selected_entity_id);
           }}}

  );
}

} // namespace astralix::UI
