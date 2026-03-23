#include "console-layer.hpp"

#include "engine.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "managers/resource-manager.hpp"

namespace astralix {
void ConsoleLayer::start() {

  auto resource_manager = ResourceManager::get();

  resource_manager->load_textures(
      {Texture2D::create("icons::warn", "icons/warn.png"),
       Texture2D::create("icons::info", "icons/info.png"),
       Texture2D::create("icons::error", "icons/error.png")});
};

void ConsoleLayer::update() {

  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

  ImGui::Begin("Console", nullptr, window_flags);

  auto resource_manager = ResourceManager::get();

  if (ImGui::BeginMenuBar()) {

    if (ImGui::Button("Clear")) {
      // Adicione aqui a lógica para limpar o console
    }
    ImGui::SameLine();
    static char searchBuffer[256] = "";
    ImGui::InputText("Search", searchBuffer, sizeof(searchBuffer));

    ImGui::SameLine();

    ImVec2 icon_size = ImVec2(14, 14);
    float spacing = 4.0f;

    float line_height = ImGui::GetFontSize() + GImGui->Style.FramePadding.y;

    float icon_position_y =
        ImGui::GetCursorPosY() + (line_height - icon_size.y) * 0.25f;

    ImGui::SameLine();
    ImGui::Text("0");

    ImVec4 clearColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f); // Transparent color

    auto info_icon = resource_manager->get_texture_by_id("icons::info");

    auto warn_icon = resource_manager->get_texture_by_id("icons::warn");

    auto error_icon = resource_manager->get_texture_by_id("icons::error");

    ImGui::PushStyleColor(ImGuiCol_Button,
                          clearColor); // Set the button color to transparent
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, clearColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, clearColor);

    ImGui::SetCursorPosY(icon_position_y);

    if (ImGui::ImageButton(info_icon->get_resource_id().c_str(),
                           info_icon->get_renderer_id(), icon_size)) {
    }

    ImGui::PopStyleColor(3); // Pop the style colors

    ImGui::SameLine();
    ImGui::SetCursorPosY(icon_position_y);
    ImGui::Text("0");

    ImGui::SameLine(0.0f, spacing);
    ImGui::SetCursorPosY(icon_position_y);
    if (ImGui::ImageButton(warn_icon->get_resource_id().c_str(),
                           warn_icon->get_renderer_id(), icon_size)) {
    }

    ImGui::SameLine(0.0f, spacing);

    ImGui::SetCursorPosY(icon_position_y);

    ImGui::SameLine();
    ImGui::Text("0");
    if (ImGui::ImageButton(error_icon->get_resource_id().c_str(),
                           error_icon->get_renderer_id(), icon_size)) {
    }

    ImGui::EndMenuBar();
  }

  ImGui::End();
}
} // namespace astralix
