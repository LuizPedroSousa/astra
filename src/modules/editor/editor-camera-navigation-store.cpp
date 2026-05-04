#include "editor-camera-navigation-store.hpp"

namespace astralix::editor {

void EditorCameraNavigationStore::set_preset(
    EditorCameraNavigationPreset preset
) {
  if (m_preset == preset) {
    return;
  }

  m_preset = preset;
  ++m_revision;
}

} // namespace astralix::editor
