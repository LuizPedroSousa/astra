#pragma once

#include "base.hpp"
#include "layouts/layout-node.hpp"
#include "project.hpp"
#include "serialization-context.hpp"
#include "workspace-definition.hpp"
#include <filesystem>
#include <optional>
#include <unordered_map>

namespace astralix::editor {

inline constexpr int k_workspace_snapshot_version = 5;

struct WorkspacePanelState {
  std::string provider_id;
  std::string title;
  bool open = true;
  std::optional<WorkspacePanelResolvedFrame> floating_frame;
  std::optional<WorkspaceDockSlot> dock_slot;
  std::string state_blob;
};

struct WorkspaceSnapshot {
  int version = k_workspace_snapshot_version;
  std::string workspace_id;
  LayoutNode root;
  std::unordered_map<std::string, WorkspacePanelState> panels;
};

class WorkspaceStore {
public:
  explicit WorkspaceStore(Ref<Project> project);

  SerializationFormat storage_format() const;
  std::string storage_extension() const;
  Ref<SerializationContext> create_context() const;

  std::optional<std::string> load_active_workspace_id() const;
  void save_active_workspace_id(std::string_view workspace_id);

  std::optional<WorkspaceSnapshot> load_snapshot(std::string_view workspace_id) const;
  void save_snapshot(const WorkspaceSnapshot &snapshot);

  std::string encode_panel_state(Ref<SerializationContext> state) const;
  std::optional<Ref<SerializationContext>>
  decode_panel_state(std::string_view state_blob) const;

private:
  std::filesystem::path editor_root() const;
  std::filesystem::path workspaces_root() const;
  std::filesystem::path session_path() const;
  std::filesystem::path snapshot_path(std::string_view workspace_id) const;

  std::optional<Ref<SerializationContext>>
  read_context_file(const std::filesystem::path &path) const;
  void write_context_file(const std::filesystem::path &path, Ref<SerializationContext> ctx) const;

  void write_session(Ref<SerializationContext> ctx, std::string_view workspace_id) const;
  std::optional<std::string> read_session(Ref<SerializationContext> ctx) const;

  void write_snapshot(Ref<SerializationContext> ctx, const WorkspaceSnapshot &snapshot) const;
  WorkspaceSnapshot read_snapshot(Ref<SerializationContext> ctx) const;

  void write_layout_node(Ref<SerializationContext> ctx, const LayoutNode &node) const;
  LayoutNode read_layout_node(ContextProxy ctx) const;

  Ref<Project> m_project = nullptr;
};

} // namespace astralix::editor
