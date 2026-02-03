#include "directional-strategy.hpp"
#include "components/light/light-component.hpp"
#include "components/resource/resource-component.hpp"
#include "components/transform/transform-component.hpp"
#include "events/key-codes.hpp"
#include "events/key-event.hpp"
#include "guid.hpp"
#include "log.hpp"
#include "time.hpp"
#include <cstddef>
#include <imgui.h>
#include <imgui_internal.h>

#include "imgui.h"

namespace astralix {

float distance = 6.0;
float near_plane = -10.0f, far_plane = 7.5f;

inline void draw_vec3_control(const std::string &label, glm::vec3 &values,
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
      GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
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

void DirectionalStrategy::update(IEntity *source, Object *object,
                                 EntityID &camera_id, size_t &index) {
  auto resource = object->get_component<ResourceComponent>();
  auto transform = source->get_component<TransformComponent>();

  auto shader = resource->shader();

  glm::mat4 lightProjection, lightView;
  glm::mat4 lightSpaceMatrix;

  auto up = glm::vec3(0.0, 1.0, 0.0);

  auto front = glm::vec3(0.0f);

  lightProjection = glm::ortho(-distance, distance, -distance, distance,
                               near_plane, far_plane);

  lightView = glm::lookAt(transform->position, front, up);

  lightSpaceMatrix = lightProjection * lightView;

  shader->set_int("light_type", 0);
  shader->set_vec3("directional_light.direction", transform->forward());
  shader->set_vec3("directional_light.position", transform->position);
  shader->set_vec3("directional_light.exposure.ambient", glm::vec3(0.2f));
  shader->set_vec3("directional_light.exposure.diffuse", glm::vec3(0.5f));
  shader->set_vec3("directional_light.exposure.specular", glm::vec3(0.5f));

  shader->set_matrix("light_space_matrix", lightSpaceMatrix);

} // namespace astralix
} // namespace astralix
