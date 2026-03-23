#include "properties-layer.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "components/camera/camera-component.hpp"
#include "components/light/light-component.hpp"
#include "components/material/material-component.hpp"
#include "components/mesh-collision/mesh-collision-component.hpp"
#include "components/mesh/mesh-component.hpp"
#include "components/model/model-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/rigidbody/rigidbody-component.hpp"
#include "components/transform/transform-component.hpp"

#include "UI/imgui-widgets.hpp"

#include "engine.hpp"

#include "managers/layer-manager.hpp"
#include "managers/resource-manager.hpp"

#include "glm/gtc/type_ptr.hpp"

#include "imgui.h"

namespace astralix {

void PropertiesLayer::start() {
  ResourceManager::get()->load_textures(
      {Texture2D::create("icons::transform", "icons/transform.png"),
       Texture2D::create("icons::camera", "icons/camera.png"),
       Texture2D::create("icons::rigidbody", "icons/rigidbody.png"),
       Texture2D::create("icons::light", "icons/light.png"),
       Texture2D::create("icons::mesh", "icons/mesh.png"),
       Texture2D::create("icons::skybox", "icons/skybox.png"),
       Texture2D::create("icons::settings", "icons/adjust.png")});
}

void PropertiesLayer::update() {
  ImGui::Begin("Properties");

  if (LayerManager::get()->get_layer_context()->has_selected_entity()) {
    auto entity_id =
        LayerManager::get()->get_layer_context()->selected_entity_id;
    auto entity = EntityManager::get()->get_entity(entity_id);

    float line_height =
        ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 0.25f;

    float spacing_y = ImGui::GetFrameHeight() + line_height - 14 * 0.5f;
    ImVec2 content_region_available = ImGui::GetContentRegionAvail();

    ImGui::Dummy(ImVec2(4.0f, 0.0f));
    ImGui::BeginGroup();

    auto cube_icon = ResourceManager::get()->get_texture_by_id("icons::cube");

    ImGui::Dummy(ImVec2(10.0f, 0.0f));
    ImGui::SetCursorPosY(spacing_y);
    ImGui::Image(cube_icon->get_renderer_id(), ImVec2{18, 18});

    ImGui::SameLine();

    ImGui::SameLine();
    ImGui::SetCursorPosY(ImGui::GetFrameHeight() + line_height * 0.5f);
    ImGui::Text(entity->name.c_str());

    ImGui::EndGroup();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    draw_components(entity);
    UI::draw_add_component(entity);
  }

  ImGui::End();
}

template <typename T, typename UIFunction>
void PropertiesLayer::draw_component(IEntity *entity, const char *icon_name,
                                     UIFunction uiFunction) {
  if (entity->has_component<T>()) {
    T *component = entity->get_component<T>();
    ImVec2 content_region_available = ImGui::GetContentRegionAvail();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{2, 2});

    float spacing = 4.0f;

    float line_height =
        ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 0.25f;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));

    ImGui::Separator();

    ImGui::PopStyleVar(2);

    ImGui::SameLine(0.0f, 0.0f);

    bool open = UI::tree_node(component->name.c_str(), icon_name,
                              ImVec2{16, 16}, false);

    ImGui::PopStyleVar();

    ImGui::SameLine(content_region_available.x - line_height * 0.5f);

    auto settings_icon =
        ResourceManager::get()->get_texture_by_id("icons::settings");

    if (ImGui::ImageButton(settings_icon->get_resource_id().c_str(),
                           settings_icon->get_renderer_id(),
                           ImVec2{line_height, line_height})) {
      ImGui::OpenPopup("component_settings");
    }
    if (open) {
      uiFunction(component);
      ImGui::TreePop();
    }

    bool remove_component = false;

    if (ImGui::BeginPopup("component_settings")) {

      if (component->is_removable) {
        if (ImGui::MenuItem("Remove component")) {
          remove_component = true;
        }
      }

      ImGui::EndPopup();
    }

    if (remove_component)
      entity->remove_component<T>();
  }
}

void PropertiesLayer::draw_components(IEntity *entity) {
  draw_component<TransformComponent>(
      entity, "icons::transform", [](TransformComponent *component) {
        UI::draw_vec3_control("Translation", component->position);
        // glm::vec3 rotation = glm::degrees(component->rotation);
        // UI::draw_vec3_control("Rotation", rotation);
        // component->rotation = glm::radians(rotation);
        UI::draw_vec3_control("Scale", component->scale, 1.0f);
      });

  draw_component<MaterialComponent>(entity, "icons::cube",
                                    [](MaterialComponent *component) {

                                    });

  draw_component<MeshComponent>(entity, "icons::mesh",
                                [](MeshComponent *component) {

                                });

  draw_component<CameraComponent>(
      entity, "icons::camera", [](CameraComponent *component) {
        ImGuiIO &io = ImGui::GetIO();
        auto bold_font = io.Fonts->Fonts[0];

        ImGui::PushID("Sensibility");

        ImGui::Columns(2);
        ImGui::SetColumnWidth(0, 2.0f);
        ImGui::Text("Sensibility");
        ImGui::NextColumn();

        ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

        float line_height =
            ImGui::GetFontSize() + GImGui->Style.FramePadding.y * 2.0f;
        ImVec2 button_size = {line_height + 3.0f, line_height};

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4{0.2f, 0.35f, 0.9f, 1.0f});
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
        ImGui::PushFont(bold_font);

        if (ImGui::Button("Sensibility", button_size)) {
          // component->sensitivity = 0.1f;
        }

        ImGui::PopFont();
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        // ImGui::DragFloat("##Sensibility", &component->sensitivity, 0.1f,
        // 0.0f,
        //                  0.0f, "%.2f");
        ImGui::PopItemWidth();

        ImGui::PopStyleVar();

        ImGui::Columns(1);

        ImGui::PopID();
      });

  draw_component<RigidBodyComponent>(entity, "icons::rigidbody",
                                     [](RigidBodyComponent *component) {

                                     });

  draw_component<LightComponent>(entity, "icons::light",
                                 [](LightComponent *component) {});
}

} // namespace astralix
