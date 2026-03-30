#pragma once

#include "guid.hpp"

#include <glm/glm.hpp>

namespace astralix::editor {

struct Theme {
  glm::vec4 bunker_1000 = glm::vec4(0.102f, 0.114f, 0.149f, 1.0f);
  glm::vec4 bunker_950 = glm::vec4(0.141f, 0.157f, 0.200f, 1.0f);
  glm::vec4 bunker_900 = glm::vec4(0.184f, 0.204f, 0.255f, 1.0f);
  glm::vec4 bunker_800 = glm::vec4(0.220f, 0.239f, 0.302f, 1.0f);
  glm::vec4 bunker_700 = glm::vec4(0.255f, 0.275f, 0.349f, 1.0f);
  glm::vec4 bunker_600 = glm::vec4(0.310f, 0.329f, 0.416f, 1.0f);
  glm::vec4 bunker_500 = glm::vec4(0.396f, 0.420f, 0.522f, 1.0f);
  glm::vec4 bunker_300 = glm::vec4(0.675f, 0.702f, 0.765f, 1.0f);
  glm::vec4 bunker_100 = glm::vec4(0.914f, 0.925f, 0.949f, 1.0f);
  glm::vec4 bunker_50 = glm::vec4(0.969f, 0.973f, 0.980f, 1.0f);

  glm::vec4 sunset_950 = glm::vec4(0.243f, 0.090f, 0.024f, 1.0f);
  glm::vec4 sunset_900 = glm::vec4(0.435f, 0.188f, 0.059f, 1.0f);
  glm::vec4 sunset_800 = glm::vec4(0.545f, 0.231f, 0.051f, 1.0f);
  glm::vec4 sunset_700 = glm::vec4(0.698f, 0.298f, 0.035f, 1.0f);
  glm::vec4 sunset_600 = glm::vec4(0.875f, 0.396f, 0.027f, 1.0f);
  glm::vec4 sunset_500 = glm::vec4(0.976f, 0.482f, 0.031f, 1.0f);
  glm::vec4 sunset_400 = glm::vec4(1.000f, 0.608f, 0.200f, 1.0f);
  glm::vec4 sunset_300 = glm::vec4(1.000f, 0.757f, 0.400f, 1.0f);
  glm::vec4 sunset_200 = glm::vec4(1.000f, 0.855f, 0.612f, 1.0f);

  glm::vec4 cabaret_950 = glm::vec4(0.282f, 0.008f, 0.102f, 1.0f);
  glm::vec4 cabaret_700 = glm::vec4(0.718f, 0.063f, 0.294f, 1.0f);
  glm::vec4 cabaret_600 = glm::vec4(0.851f, 0.176f, 0.427f, 1.0f);
  glm::vec4 cabaret_500 = glm::vec4(0.914f, 0.247f, 0.522f, 1.0f);
  glm::vec4 cabaret_300 = glm::vec4(0.976f, 0.659f, 0.800f, 1.0f);

  glm::vec4 storm_1100 = glm::vec4(0.024f, 0.031f, 0.047f, 1.0f);
  glm::vec4 storm_1050 = glm::vec4(0.035f, 0.043f, 0.067f, 0.72f);
  glm::vec4 storm_1000 =
      glm::vec4(0.067f, 0.082f, 0.114f, 0.98f);
  glm::vec4 storm_950 =
      glm::vec4(0.098f, 0.125f, 0.173f, 0.96f);
  glm::vec4 storm_900 =
      glm::vec4(0.122f, 0.149f, 0.200f, 0.98f);
  glm::vec4 storm_800 = glm::vec4(0.220f, 0.259f, 0.333f, 1.0f);
  glm::vec4 storm_500 = glm::vec4(0.514f, 0.545f, 0.639f, 1.0f);
  glm::vec4 storm_300 = glm::vec4(0.627f, 0.682f, 0.773f, 1.0f);
  glm::vec4 storm_50 =
      glm::vec4(0.949f, 0.961f, 0.980f, 1.0f);
  glm::vec4 success = glm::vec4(0.576f, 0.945f, 0.576f, 1.0f);
  glm::vec4 success_soft = glm::vec4(0.078f, 0.259f, 0.110f, 0.92f);
};

inline const Theme k_theme{};

inline glm::vec4 theme_alpha(const glm::vec4 &color, float value) {
  return glm::vec4(color.r, color.g, color.b, value);
}

struct WorkspaceShellTheme {
  glm::vec4 backdrop = k_theme.storm_1050;
  glm::vec4 bar_background = k_theme.storm_1000;
  glm::vec4 panel_background = k_theme.storm_950;
  glm::vec4 panel_raised_background = k_theme.storm_900;
  glm::vec4 panel_border = k_theme.storm_800;
  glm::vec4 accent = k_theme.sunset_500;
  glm::vec4 accent_soft = theme_alpha(k_theme.sunset_600, 0.25f);
  glm::vec4 text_primary = k_theme.storm_50;
  glm::vec4 text_muted = k_theme.storm_300;
};

struct ViewportPanelTheme {
  glm::vec4 surface = k_theme.storm_1100;
};

struct ConsolePanelTheme {
  glm::vec4 panel_background = theme_alpha(k_theme.bunker_950, 0.98f);
  glm::vec4 panel_border = k_theme.bunker_800;
  glm::vec4 accent = k_theme.sunset_500;
  glm::vec4 accent_pressed = k_theme.sunset_700;
  glm::vec4 text_primary = k_theme.bunker_50;
  glm::vec4 text_muted = k_theme.bunker_300;
  glm::vec4 handle = k_theme.bunker_900;
  glm::vec4 prompt_background = theme_alpha(k_theme.sunset_950, 0.96f);
  glm::vec4 prompt_text = k_theme.sunset_300;
  glm::vec4 placeholder_text = k_theme.storm_500;
  ResourceDescriptorID mono_font = "fonts::noto_sans_mono";
};

struct RuntimePanelTheme {
  glm::vec4 shell_background = theme_alpha(k_theme.bunker_1000, 0.98f);
  glm::vec4 panel_background = theme_alpha(k_theme.bunker_950, 0.98f);
  glm::vec4 panel_border = k_theme.bunker_800;
  glm::vec4 card_background = theme_alpha(k_theme.bunker_900, 0.92f);
  glm::vec4 card_border = k_theme.bunker_700;
  glm::vec4 text_primary = k_theme.bunker_50;
  glm::vec4 text_muted = k_theme.bunker_300;
  glm::vec4 accent = k_theme.sunset_500;
  glm::vec4 accent_soft = theme_alpha(k_theme.sunset_950, 0.94f);
  glm::vec4 success = k_theme.success;
  glm::vec4 success_soft = k_theme.success_soft;
};

struct SceneHierarchyPanelTheme {
  glm::vec4 shell_background = theme_alpha(k_theme.bunker_1000, 0.98f);
  glm::vec4 panel_background = theme_alpha(k_theme.bunker_950, 0.98f);
  glm::vec4 panel_border = k_theme.bunker_800;
  glm::vec4 card_background = theme_alpha(k_theme.bunker_900, 0.92f);
  glm::vec4 card_border = k_theme.bunker_700;
  glm::vec4 row_background = theme_alpha(k_theme.bunker_900, 0.84f);
  glm::vec4 row_border = theme_alpha(k_theme.bunker_700, 0.92f);
  glm::vec4 row_selected_background = theme_alpha(k_theme.sunset_950, 0.96f);
  glm::vec4 row_selected_border = k_theme.sunset_500;
  glm::vec4 text_primary = k_theme.bunker_50;
  glm::vec4 text_muted = k_theme.bunker_300;
  glm::vec4 accent = k_theme.sunset_500;
  glm::vec4 accent_soft = theme_alpha(k_theme.sunset_950, 0.94f);
  glm::vec4 success = k_theme.success;
  glm::vec4 success_soft = k_theme.success_soft;
  glm::vec4 subdued = k_theme.bunker_500;
  glm::vec4 subdued_soft = theme_alpha(k_theme.bunker_900, 0.96f);
};

struct InspectorPanelTheme {
  glm::vec4 shell_background = theme_alpha(k_theme.bunker_1000, 0.98f);
  glm::vec4 panel_background = theme_alpha(k_theme.bunker_950, 0.98f);
  glm::vec4 panel_border = k_theme.bunker_800;
  glm::vec4 card_background = theme_alpha(k_theme.bunker_900, 0.92f);
  glm::vec4 card_border = k_theme.bunker_700;
  glm::vec4 input_background = theme_alpha(k_theme.bunker_1000, 0.74f);
  glm::vec4 input_border = k_theme.bunker_700;
  glm::vec4 text_primary = k_theme.bunker_50;
  glm::vec4 text_muted = k_theme.bunker_300;
  glm::vec4 accent = k_theme.sunset_500;
  glm::vec4 accent_soft = theme_alpha(k_theme.sunset_950, 0.94f);
  glm::vec4 success = k_theme.success;
  glm::vec4 success_soft = k_theme.success_soft;
  glm::vec4 subdued = k_theme.bunker_500;
  glm::vec4 subdued_soft = theme_alpha(k_theme.bunker_900, 0.96f);
};

} // namespace astralix::editor
