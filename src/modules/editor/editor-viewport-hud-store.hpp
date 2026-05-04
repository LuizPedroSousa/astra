#pragma once

#include "base-manager.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace astralix::editor {

enum class EditorViewportHudTone : uint8_t {
  Neutral = 0,
  Accent = 1,
  Success = 2,
};

struct EditorViewportHudChip {
  std::string label;
  EditorViewportHudTone tone = EditorViewportHudTone::Neutral;
};

struct EditorViewportHudSnapshot {
  std::string title;
  std::string target;
  std::optional<std::string> detail_line;
  std::vector<EditorViewportHudChip> chips;
  std::vector<std::string> hints;
  std::optional<std::string> transient_message;
};

class EditorViewportHudStore : public BaseManager<EditorViewportHudStore> {
public:
  const EditorViewportHudSnapshot &snapshot() const { return m_snapshot; }

  void set_snapshot(EditorViewportHudSnapshot snapshot);
  void show_transient_message(
      std::string message,
      double duration_seconds = 1.35
  );
  void advance(double dt);
  void clear();

  uint64_t revision() const { return m_revision; }

private:
  struct TransientState {
    std::string message;
    double remaining_seconds = 0.0;
  };

  void apply_transient(EditorViewportHudSnapshot &snapshot) const;

  EditorViewportHudSnapshot m_snapshot;
  std::optional<TransientState> m_transient_state;
  uint64_t m_revision = 1u;
};

inline Ref<EditorViewportHudStore> editor_viewport_hud_store() {
  if (EditorViewportHudStore::get() == nullptr) {
    EditorViewportHudStore::init();
  }

  return EditorViewportHudStore::get();
}

} // namespace astralix::editor
