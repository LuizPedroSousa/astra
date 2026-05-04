#include "editor-viewport-hud-store.hpp"

#include <algorithm>

namespace astralix::editor {
namespace {

bool same_optional_string(
    const std::optional<std::string> &lhs,
    const std::optional<std::string> &rhs
) {
  if (lhs.has_value() != rhs.has_value()) {
    return false;
  }

  return !lhs.has_value() || *lhs == *rhs;
}

bool same_chip(
    const EditorViewportHudChip &lhs,
    const EditorViewportHudChip &rhs
) {
  return lhs.label == rhs.label && lhs.tone == rhs.tone;
}

bool same_chips(
    const std::vector<EditorViewportHudChip> &lhs,
    const std::vector<EditorViewportHudChip> &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t index = 0u; index < lhs.size(); ++index) {
    if (!same_chip(lhs[index], rhs[index])) {
      return false;
    }
  }

  return true;
}

bool same_strings(
    const std::vector<std::string> &lhs,
    const std::vector<std::string> &rhs
) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t index = 0u; index < lhs.size(); ++index) {
    if (lhs[index] != rhs[index]) {
      return false;
    }
  }

  return true;
}

bool same_snapshot(
    const EditorViewportHudSnapshot &lhs,
    const EditorViewportHudSnapshot &rhs
) {
  return lhs.title == rhs.title &&
         lhs.target == rhs.target &&
         same_optional_string(lhs.detail_line, rhs.detail_line) &&
         same_chips(lhs.chips, rhs.chips) &&
         same_strings(lhs.hints, rhs.hints) &&
         same_optional_string(lhs.transient_message, rhs.transient_message);
}

} // namespace

void EditorViewportHudStore::apply_transient(
    EditorViewportHudSnapshot &snapshot
) const {
  snapshot.transient_message =
      m_transient_state.has_value()
          ? std::optional<std::string>(m_transient_state->message)
          : std::nullopt;
}

void EditorViewportHudStore::set_snapshot(EditorViewportHudSnapshot snapshot) {
  apply_transient(snapshot);
  if (same_snapshot(m_snapshot, snapshot)) {
    return;
  }

  m_snapshot = std::move(snapshot);
  ++m_revision;
}

void EditorViewportHudStore::show_transient_message(
    std::string message,
    double duration_seconds
) {
  const std::optional<std::string> previous_message = m_snapshot.transient_message;
  m_transient_state = TransientState{
      .message = std::move(message),
      .remaining_seconds = std::max(duration_seconds, 0.0),
  };
  apply_transient(m_snapshot);

  if (previous_message != m_snapshot.transient_message) {
    ++m_revision;
  }
}

void EditorViewportHudStore::advance(double dt) {
  if (!m_transient_state.has_value() || dt <= 0.0) {
    return;
  }

  m_transient_state->remaining_seconds =
      std::max(0.0, m_transient_state->remaining_seconds - dt);
  if (m_transient_state->remaining_seconds > 0.0) {
    return;
  }

  m_transient_state.reset();
  if (m_snapshot.transient_message.has_value()) {
    m_snapshot.transient_message.reset();
    ++m_revision;
  }
}

void EditorViewportHudStore::clear() {
  if (m_snapshot.title.empty() && m_snapshot.target.empty() &&
      !m_snapshot.detail_line.has_value() && m_snapshot.chips.empty() &&
      m_snapshot.hints.empty() && !m_snapshot.transient_message.has_value() &&
      !m_transient_state.has_value()) {
    return;
  }

  m_snapshot = {};
  m_transient_state.reset();
  ++m_revision;
}

} // namespace astralix::editor
