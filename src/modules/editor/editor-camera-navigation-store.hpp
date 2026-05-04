#pragma once

#include "base-manager.hpp"

#include <cstdint>

namespace astralix::editor {

enum class EditorCameraNavigationPreset : uint8_t {
  Free = 0,
  Orbit = 1,
};

class EditorCameraNavigationStore
    : public BaseManager<EditorCameraNavigationStore> {
public:
  EditorCameraNavigationPreset preset() const { return m_preset; }

  void set_preset(EditorCameraNavigationPreset preset);

  void toggle_preset() {
    set_preset(
        m_preset == EditorCameraNavigationPreset::Free
            ? EditorCameraNavigationPreset::Orbit
            : EditorCameraNavigationPreset::Free
    );
  }

  uint64_t revision() const { return m_revision; }

private:
  EditorCameraNavigationPreset m_preset = EditorCameraNavigationPreset::Free;
  uint64_t m_revision = 1u;
};

inline Ref<EditorCameraNavigationStore> editor_camera_navigation_store() {
  if (EditorCameraNavigationStore::get() == nullptr) {
    EditorCameraNavigationStore::init();
  }

  return EditorCameraNavigationStore::get();
}

} // namespace astralix::editor
